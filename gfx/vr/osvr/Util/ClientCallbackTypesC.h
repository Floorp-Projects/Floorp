/** @file
    @brief Header

    Must be c-safe!

    GENERATED - do not edit by hand!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_ClientCallbackTypesC_h_GUID_4D43A675_C8A4_4BBF_516F_59E6C785E4EF
#define INCLUDED_ClientCallbackTypesC_h_GUID_4D43A675_C8A4_4BBF_516F_59E6C785E4EF

/* Internal Includes */
#include <osvr/Util/ClientReportTypesC.h>
#include <osvr/Util/ImagingReportTypesC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/TimeValueC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup ClientKit
    @{
*/

/** @name Report callback types
    @{
*/

/* generated file - do not edit! */
/** @brief C function type for a Pose callback */
typedef void (*OSVR_PoseCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_PoseReport *report);
/** @brief C function type for a Position callback */
typedef void (*OSVR_PositionCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_PositionReport *report);
/** @brief C function type for a Orientation callback */
typedef void (*OSVR_OrientationCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_OrientationReport *report);
/** @brief C function type for a Velocity callback */
typedef void (*OSVR_VelocityCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_VelocityReport *report);
/** @brief C function type for a LinearVelocity callback */
typedef void (*OSVR_LinearVelocityCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_LinearVelocityReport *report);
/** @brief C function type for a AngularVelocity callback */
typedef void (*OSVR_AngularVelocityCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_AngularVelocityReport *report);
/** @brief C function type for a Acceleration callback */
typedef void (*OSVR_AccelerationCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_AccelerationReport *report);
/** @brief C function type for a LinearAcceleration callback */
typedef void (*OSVR_LinearAccelerationCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_LinearAccelerationReport *report);
/** @brief C function type for a AngularAcceleration callback */
typedef void (*OSVR_AngularAccelerationCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_AngularAccelerationReport *report);
/** @brief C function type for a Button callback */
typedef void (*OSVR_ButtonCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_ButtonReport *report);
/** @brief C function type for a Analog callback */
typedef void (*OSVR_AnalogCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_AnalogReport *report);
/** @brief C function type for a Imaging callback */
typedef void (*OSVR_ImagingCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_ImagingReport *report);
/** @brief C function type for a Location2D callback */
typedef void (*OSVR_Location2DCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_Location2DReport *report);
/** @brief C function type for a Direction callback */
typedef void (*OSVR_DirectionCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_DirectionReport *report);
/** @brief C function type for a EyeTracker2D callback */
typedef void (*OSVR_EyeTracker2DCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_EyeTracker2DReport *report);
/** @brief C function type for a EyeTracker3D callback */
typedef void (*OSVR_EyeTracker3DCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_EyeTracker3DReport *report);
/** @brief C function type for a EyeTrackerBlink callback */
typedef void (*OSVR_EyeTrackerBlinkCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_EyeTrackerBlinkReport *report);
/** @brief C function type for a NaviVelocity callback */
typedef void (*OSVR_NaviVelocityCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_NaviVelocityReport *report);
/** @brief C function type for a NaviPosition callback */
typedef void (*OSVR_NaviPositionCallback)(void *userdata,
                                     const struct OSVR_TimeValue *timestamp,
                                     const struct OSVR_NaviPositionReport *report);

/** @} */

/** @} */

OSVR_EXTERN_C_END

#endif
