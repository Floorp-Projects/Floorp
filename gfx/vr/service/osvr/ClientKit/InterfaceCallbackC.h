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

#ifndef INCLUDED_InterfaceCallbacksC_h_GUID_8F16E6CB_F998_4ABC_5B6B_4FC1E4B71BC9
#define INCLUDED_InterfaceCallbacksC_h_GUID_8F16E6CB_F998_4ABC_5B6B_4FC1E4B71BC9

/* Internal Includes */
#include <osvr/ClientKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/AnnotationMacrosC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>
#include <osvr/Util/ClientCallbackTypesC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

#define OSVR_INTERFACE_CALLBACK_METHOD(TYPE)                          \
  /** @brief Register a callback for TYPE reports on an interface */  \
  OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrRegister##TYPE##Callback( \
      OSVR_ClientInterface iface, OSVR_##TYPE##Callback cb, void* userdata);

OSVR_INTERFACE_CALLBACK_METHOD(Pose)
OSVR_INTERFACE_CALLBACK_METHOD(Position)
OSVR_INTERFACE_CALLBACK_METHOD(Orientation)
OSVR_INTERFACE_CALLBACK_METHOD(Velocity)
OSVR_INTERFACE_CALLBACK_METHOD(LinearVelocity)
OSVR_INTERFACE_CALLBACK_METHOD(AngularVelocity)
OSVR_INTERFACE_CALLBACK_METHOD(Acceleration)
OSVR_INTERFACE_CALLBACK_METHOD(LinearAcceleration)
OSVR_INTERFACE_CALLBACK_METHOD(AngularAcceleration)
OSVR_INTERFACE_CALLBACK_METHOD(Button)
OSVR_INTERFACE_CALLBACK_METHOD(Analog)
OSVR_INTERFACE_CALLBACK_METHOD(Imaging)
OSVR_INTERFACE_CALLBACK_METHOD(Location2D)
OSVR_INTERFACE_CALLBACK_METHOD(Direction)
OSVR_INTERFACE_CALLBACK_METHOD(EyeTracker2D)
OSVR_INTERFACE_CALLBACK_METHOD(EyeTracker3D)
OSVR_INTERFACE_CALLBACK_METHOD(EyeTrackerBlink)
OSVR_INTERFACE_CALLBACK_METHOD(NaviVelocity)
OSVR_INTERFACE_CALLBACK_METHOD(NaviPosition)

#undef OSVR_INTERFACE_CALLBACK_METHOD

OSVR_EXTERN_C_END

#endif
