/** @file
    @brief Header controlling the OSVR transformation hierarchy

    Must be c-safe!

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_TransformsC_h_GUID_5B5B7438_42D4_4095_E48A_90E2CC13498E
#define INCLUDED_TransformsC_h_GUID_5B5B7438_42D4_4095_E48A_90E2CC13498E

/* Internal Includes */
#include <osvr/ClientKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup ClientKit
    @{
*/

/** @brief Updates the internal "room to world" transformation (applied to all
    tracker data for this client context instance) based on the user's head
    orientation, so that the direction the user is facing becomes -Z to your
    application. Only rotates about the Y axis (yaw).

    Note that this method internally calls osvrClientUpdate() to get a head pose
    so your callbacks may be called during its execution!

    @param ctx Client context
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientSetRoomRotationUsingHead(OSVR_ClientContext ctx);

/** @brief Clears/resets the internal "room to world" transformation back to an
   identity transformation - that is, clears the effect of any other
   manipulation of the room to world transform.

    @param ctx Client context
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientClearRoomToWorldTransform(OSVR_ClientContext ctx);

/** @} */
OSVR_EXTERN_C_END

#endif
