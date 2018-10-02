/** @file
    @brief Header

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

#ifndef INCLUDED_RadialDistortionParametersC_h_GUID_925BCEB1_BACA_4DA7_5133_FFF560C72EBD
#define INCLUDED_RadialDistortionParametersC_h_GUID_925BCEB1_BACA_4DA7_5133_FFF560C72EBD

/* Internal Includes */
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/Vec2C.h>
#include <osvr/Util/Vec3C.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup UtilMath
@{
*/

/** @brief Parameters for a per-color-component radial distortion shader
*/
typedef struct OSVR_RadialDistortionParameters {
    /** @brief Vector of K1 coefficients for the R, G, B channels*/
    OSVR_Vec3 k1;
    /** @brief Center of projection for the radial distortion, relative to the
        bounds of this surface.
        */
    OSVR_Vec2 centerOfProjection;
} OSVR_RadialDistortionParameters;

OSVR_EXTERN_C_END

#endif
