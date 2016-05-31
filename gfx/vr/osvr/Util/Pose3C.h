/** @file
    @brief Header

    Must be c-safe!

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

#ifndef INCLUDED_Pose3C_h_GUID_066CFCE2_229C_4194_5D2B_2602CCD5C439
#define INCLUDED_Pose3C_h_GUID_066CFCE2_229C_4194_5D2B_2602CCD5C439

/* Internal Includes */

/* Internal Includes */
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/Vec3C.h>
#include <osvr/Util/QuaternionC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup UtilMath
    @{
*/

/** @brief A structure defining a 3D (6DOF) rigid body pose: translation and
    rotation.
*/
typedef struct OSVR_Pose3 {
    /** @brief Position vector */
    OSVR_Vec3 translation;
    /** @brief Orientation as a unit quaternion */
    OSVR_Quaternion rotation;
} OSVR_Pose3;

/** @brief Set a pose to identity */
OSVR_INLINE void osvrPose3SetIdentity(OSVR_Pose3 *pose) {
    osvrQuatSetIdentity(&(pose->rotation));
    osvrVec3Zero(&(pose->translation));
}
/** @} */

OSVR_EXTERN_C_END

#endif
