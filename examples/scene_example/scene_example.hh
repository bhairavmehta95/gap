/*
 *  Copyright (C) 2018 João Borrego
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *      
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*!
    \file examples/scene_example/scene_example.hh
    \brief Random scene generation example

    Generates a scene with up to 10 objects in a 4x4 grid.

    \author João Borrego : jsbruglie
    \author Rui Figueiredo : ruipimentelfigueiredo
*/

// Includes

// Gazebo
#include <gazebo/gazebo_config.h>
#include <gazebo/transport/transport.hh>
#include <gazebo/msgs/msgs.hh>
#include <gazebo/gazebo_client.hh>

// Custom messages
#include "camera_utils_request.pb.h"
#include "camera_utils_response.pb.h"
#include "visual_utils_request.pb.h"
#include "visual_utils_response.pb.h"
#include "world_utils_request.pb.h"
#include "world_utils_response.pb.h"

// Utilities
#include "utils.hh"
// Object class
#include "ObjectGrid.hh"

// I/O streams
#include <iostream>
// File streams
#include <fstream>
// Iterating over the contents of a dir
#include <boost/filesystem.hpp>
// Protecting variables
#include <mutex>
// Set container
#include <set>
// Sleep
#include <chrono>
#include <thread>
// Regular expressions
#include <regex>
// Linear algebra
#include <Eigen/Dense>
// INT MAX
#include <climits>

/// OpenCV 2
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc.hpp"

//////////////////////////////////////////////////

// Macros

/// Matches name field in <model name=""> XML tag
#define REGEX_XML_MODEL "<model name=(\"([^\"]|\"\")*\")>"
/// Matches string enclosed in <uid> XML tags
#define REGEX_XML_UID   "<uid>[\\s\\S]*?<\\/uid>"

//////////////////////////////////////////////////

// Macros for custom messages

// Camera utils

/// Request to move camera to given pose
#define MOVE_REQUEST            camera_utils::msgs::CameraUtilsRequest::MOVE
/// Response acknowledging move camera request
#define MOVE_RESPONSE           camera_utils::msgs::CameraUtilsResponse::MOVE
/// Request to capture a frame and save it to disk
#define CAPTURE_REQUEST         camera_utils::msgs::CameraUtilsRequest::CAPTURE
/// Response acknowledging captured frame
#define CAPTURE_RESPONSE        camera_utils::msgs::CameraUtilsResponse::CAPTURE
/// Request 3D to 2D point projection
#define PROJECTION_REQUEST      camera_utils::msgs::CameraUtilsRequest::PROJECTION
/// Response 3D to 2D point projection
#define PROJECTION_RESPONSE     camera_utils::msgs::CameraUtilsResponse::PROJECTION

// Visual utils

/// Request update
#define UPDATE      visual_utils::msgs::VisualUtilsRequest::UPDATE
/// Visual updated response
#define UPDATED     visual_utils::msgs::VisualUtilsResponse::UPDATED

// World utils

/// Spawn entity
#define SPAWN           world_utils::msgs::WorldUtilsRequest::SPAWN
/// Move entity
#define WORLD_MOVE      world_utils::msgs::WorldUtilsRequest::MOVE
/// Start or stop physcis simulation
#define PHYSICS         world_utils::msgs::WorldUtilsRequest::PHYSICS
/// \breief TODO
#define SUCCESS         world_utils::msgs::WorldUtilsResponse::SUCCESS

/// Spawn custom object
#define CUSTOM          world_utils::msgs::Object::CUSTOM
/// Spawn custom light object
#define CUSTOM_LIGHT    world_utils::msgs::Object::CUSTOM_LIGHT

//////////////////////////////////////////////////

// API Topics

/// Topic monitored by CameraUtils plugin for incoming requests
#define CAMERA_UTILS_TOPIC          "~/gazebo-utils/camera_utils"
/// Topic for receiving replies from CameraUtils plugin
#define CAMERA_UTILS_RESPONSE_TOPIC "~/gazebo-utils/camera_utils/response"
/// Topic monitored by VisualUtils plugin for incoming requests
#define VISUAL_UTILS_TOPIC          "~/gazebo-utils/visual_utils"
/// Topic for receiving replies from VisualUtils plugin
#define VISUAL_UTILS_RESPONSE_TOPIC "~/gazebo-utils/visual_utils/response"
/// Topic monitored by WorldUtils plugin for incoming requests
#define WORLD_UTILS_TOPIC           "~/gazebo-utils/world_utils"
/// Topic for receiving replies from WorldUtils plugin
#define WORLD_UTILS_RESPONSE_TOPIC  "~/gazebo-utils/world_utils/response"

// Message pointer typedefs

/// Pointer to CameraUtils response message
typedef const boost::shared_ptr<const camera_utils::msgs::CameraUtilsResponse>
    CameraUtilsResponsePtr;
/// Pointer to VisualUtils request message
typedef const boost::shared_ptr<const visual_utils::msgs::VisualUtilsResponse>
    VisualUtilsResponsePtr;
/// Pointer to WorldUtils request message
typedef const boost::shared_ptr<const world_utils::msgs::WorldUtilsResponse>
    WorldUtilsResponsePtr;

//////////////////////////////////////////////////

/// Function prototypes

/// \brief Adds an SDF model to a WorldUtils request
/// \param msg  WorldUtils request message
/// \param file SDF file with model
void addModelFromFile(
    world_utils::msgs::WorldUtilsRequest & msg,
    const std::string & file);

/// \brief Add objects in global grid to WorldUtils spawn request
/// \param msg  WorldUtils request message
void addDynamicModels(world_utils::msgs::WorldUtilsRequest & msg);

/// \brief Add objects in global grid to VisualUtils update request
void updateObjects(visual_utils::msgs::VisualUtilsRequest & msg);

/// \brief Add move object command to WorldUtils request
/// \param name     Object name
/// \param is_light Whether object is a light
/// \param pose     New object pose
void addMoveObject(
    world_utils::msgs::WorldUtilsRequest & msg,
    const std::string & name,
    const bool is_light,
    const ignition::math::Pose3d & pose);

/// \brief Obtain random camera pose in dome
/// \return New random camera pose
ignition::math::Pose3d getRandomCameraPose();

/// \brief Obtain random light pose in dome
/// \return New random light pose
ignition::math::Pose3d getRandomLightPose();

/// \brief Send CameraUtils request to capture current scene
/// \param pub          Publisher for CameraUtils request topic
/// \param iteration    Current iteration
void captureScene(gazebo::transport::PublisherPtr pub, int iteration);

/// \brief Wait for camera to move to new pose
/// \return True if process should wait
bool waitForMove();

/// \brief Wait for visuals to update
/// \return True if process should wait
bool waitForVisuals();

/// \brief Wait for camera to save frame to disk
/// \return True if process should wait
bool waitForCamera();

/// \brief Wait for projected points
bool waitForProjections();

/// \brief Create set with names of existing objects
void createNameSet();

/// \brief Add 3D points to projection request
void addProjections(camera_utils::msgs::CameraUtilsRequest & msg);

/// \brief Move camera to global camera pose
void moveCamera(gazebo::transport::PublisherPtr pub);

/// \brief Callback function for CameraUtils response
/// \param _msg Incoming message
void onCameraUtilsResponse(CameraUtilsResponsePtr & _msg);

/// \brief Callback function for VisualUtils response
/// \param _msg Incoming message
void onVisualUtilsResponse(VisualUtilsResponsePtr & _msg);

/// \brief Callback function for WorldUtils response
/// \param _msg Incoming message
void onWorldUtilsResponse(WorldUtilsResponsePtr & _msg);

/// \brief Enables/disables physics engine
/// \param pub WorldUtils publisher ptr
/// \param enable Desired physics engine status
void setPhysics(gazebo::transport::PublisherPtr pub, bool enable);

/// \brief Debug function to visualise scene
/// Debug function to visualise acquired frame and object bounding boxes
void visualizeData(const std::string & image_dir, int iteration);

/// \brief Store current scene annotations
/// \param path         Path to dataset folder
/// \param iteration    Current iteration
void storeAnnotations(
    const std::string & path,
    const int iteration);