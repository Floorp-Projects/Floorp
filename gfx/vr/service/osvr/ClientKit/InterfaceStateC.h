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

#ifndef INCLUDED_InterfaceStateC_h_GUID_8F85D178_74B9_4AA9_4E9E_243089411408
#define INCLUDED_InterfaceStateC_h_GUID_8F85D178_74B9_4AA9_4E9E_243089411408

/* Internal Includes */
#include <osvr/ClientKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/AnnotationMacrosC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>
#include <osvr/Util/ClientReportTypesC.h>
#include <osvr/Util/TimeValueC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

#define OSVR_CALLBACK_METHODS(TYPE)                                      \
  /** @brief Get TYPE state from an interface, returning failure if none \
   * exists */                                                           \
  OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrGet##TYPE##State(            \
      OSVR_ClientInterface iface, struct OSVR_TimeValue* timestamp,      \
      OSVR_##TYPE##State* state);

OSVR_CALLBACK_METHODS(Pose)
OSVR_CALLBACK_METHODS(Position)
OSVR_CALLBACK_METHODS(Orientation)
OSVR_CALLBACK_METHODS(Velocity)
OSVR_CALLBACK_METHODS(LinearVelocity)
OSVR_CALLBACK_METHODS(AngularVelocity)
OSVR_CALLBACK_METHODS(Acceleration)
OSVR_CALLBACK_METHODS(LinearAcceleration)
OSVR_CALLBACK_METHODS(AngularAcceleration)
OSVR_CALLBACK_METHODS(Button)
OSVR_CALLBACK_METHODS(Analog)
OSVR_CALLBACK_METHODS(Location2D)
OSVR_CALLBACK_METHODS(Direction)
OSVR_CALLBACK_METHODS(EyeTracker2D)
OSVR_CALLBACK_METHODS(EyeTracker3D)
OSVR_CALLBACK_METHODS(EyeTrackerBlink)
OSVR_CALLBACK_METHODS(NaviVelocity)
OSVR_CALLBACK_METHODS(NaviPosition)

#undef OSVR_CALLBACK_METHODS

OSVR_EXTERN_C_END

#endif
