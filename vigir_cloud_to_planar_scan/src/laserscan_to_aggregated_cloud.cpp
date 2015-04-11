//=================================================================================================
// Copyright (c) 2012, Stefan Kohlbrecher, TU Darmstadt
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Simulation, Systems Optimization and Robotics
//       group, TU Darmstadt nor the names of its contributors may be used to
//       endorse or promote products derived from this software without
//       specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//=================================================================================================

#include <ros/ros.h>
#include <laser_geometry/laser_geometry.h>

#include <tf/transform_listener.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <pcl_conversions/pcl_conversions.h>

#include <pcl_ros/transforms.h>

//#include <filters/filter_chain.h>

class LaserscanToPointcloud
{
public:

  LaserscanToPointcloud()
  {
    ros::NodeHandle nh_;

    prior_roll_angle_ = 0.0;

    scan_sub_ = nh_.subscribe("cloud", 10, &LaserscanToPointcloud::scanCallback, this);
    point_cloud2_pub_ = nh_.advertise<sensor_msgs::PointCloud2>("cloud_out",10,false);

    ros::NodeHandle pnh_("~");
    //pnh_.param("max_range", p_max_range_, 29.0);
    //pnh_.param("min_range", p_min_range_, 0.0);

    //filter_chain_.configure("scan_filter_chain", pnh_);

    pnh_.param("use_high_fidelity_projection", p_use_high_fidelity_projection_, true);

    if (p_use_high_fidelity_projection_){
      pnh_.param("target_frame", p_target_frame_, std::string("NO_TARGET_FRAME_SPECIFIED"));

      if (p_target_frame_ == "NO_TARGET_FRAME_SPECIFIED"){
        ROS_ERROR("No target frame specified! Needs to be set for high fidelity projection to work");
        p_use_high_fidelity_projection_ = false;
        return;
      }

      tfl_.reset(new tf::TransformListener());
      wait_duration_ = ros::Duration(0.5);

    }
  }

  void scanCallback (const sensor_msgs::PointCloud2::ConstPtr& cloud_in)
  {

    //ros::Time scan_time = cloud_in->header.stamp;


    if (tfl_->waitForTransform(p_target_frame_, cloud_in->header.frame_id, cloud_in->header.stamp, wait_duration_)){
      tf::StampedTransform transform;
      tfl_->lookupTransform(p_target_frame_, cloud_in->header.frame_id, cloud_in->header.stamp, transform);

      //ROS_INFO("Lookup %s %s", p_target_frame_.c_str(), cloud_in->header.frame_id.c_str());

      double roll, pitch, yaw;
      tf::Matrix3x3(transform.getRotation()).getRPY(roll, pitch, yaw);
      //ROS_INFO("roll: %f pitch: %f yaw: %f", roll, pitch, yaw);

      if (((prior_roll_angle_ < (-M_PI*0.5)) && (roll > (-M_PI*0.5))) ||
          ((prior_roll_angle_ < ( M_PI*0.5)) && (roll > ( M_PI*0.5)))){

        pcl::PointCloud<pcl::PointXYZ> tmp_agg_cloud;

        for (size_t i=0; i < cloud_agg_.size(); ++i){
          if (tmp_agg_cloud.empty()){
            tmp_agg_cloud = *cloud_agg_[i];
          }else{
            tmp_agg_cloud += *cloud_agg_[i];
          }
        }

        pcl::toROSMsg(tmp_agg_cloud, cloud2_);

        cloud2_.header.frame_id = p_target_frame_;
        cloud2_.header.stamp = ros::Time::now();
        point_cloud2_pub_.publish(cloud2_);
        cloud_agg_.clear();



      }else{


        pcl::PointCloud<pcl::PointXYZ> pc_tmp;



        pcl::fromROSMsg(*cloud_in, pc_tmp);


        Eigen::Matrix4f sensorToWorld;
        pcl_ros::transformAsMatrix(transform, sensorToWorld);

        boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > pc;
        pc.reset(new pcl::PointCloud<pcl::PointXYZ>());

        pcl::transformPointCloud(pc_tmp, *pc, sensorToWorld);

        cloud_agg_.push_back(pc);
      }

      prior_roll_angle_ = roll;

    }

    /*
    if (p_use_high_fidelity_projection_){

      ros::Time end_time   = scan_in->header.stamp + ros::Duration().fromSec(scan_in->ranges.size()*scan_in->time_increment) ;

      if(tfl_->waitForTransform(p_target_frame_, scan_in->header.frame_id, scan_in->header.stamp, wait_duration_) &&
         tfl_->waitForTransform(p_target_frame_, scan_in->header.frame_id, end_time, wait_duration_)){
        projector_.transformLaserScanToPointCloud(p_target_frame_, *scan_to_convert, cloud2_, *tfl_, p_max_range_, laser_geometry::channel_option::Intensity);
      }else{
        ROS_WARN("Timed out waiting for transform between %s and %s for %f seconds. Unable to transform laser scan.",p_target_frame_.c_str(), scan_in->header.frame_id.c_str(), wait_duration_.toSec());
        return;
      }
    }else{
      projector_.projectLaser(*scan_to_convert, cloud2_, p_max_range_, laser_geometry::channel_option::Intensity);
    }
    */
  }

protected:
  ros::Subscriber scan_sub_;
  ros::Publisher point_cloud2_pub_;

  boost::shared_ptr<tf::TransformListener> tfl_;
  ros::Duration wait_duration_;

  //double p_max_range_;
  //double p_min_range_;
  bool p_use_high_fidelity_projection_;
  std::string p_target_frame_;

  double prior_roll_angle_;

  //laser_geometry::LaserProjection projector_;

  sensor_msgs::PointCloud2 cloud2_;
  //sensor_msgs::LaserScan scan_min_range_;

  //sensor_msgs::LaserScan scan_filtered_;
  
  //filters::FilterChain<sensor_msgs::LaserScan> filter_chain_;

  std::vector<boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > > cloud_agg_;
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "vigir_laserscan_to_pointcloud_node");

  LaserscanToPointcloud ls;

  ros::spin();
}
