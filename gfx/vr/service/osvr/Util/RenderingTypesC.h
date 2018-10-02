/** @file
    @brief Header with integer types for Viewer, Eye, and Surface
   counts/indices, as well as viewport information.

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

#ifndef INCLUDED_RenderingTypesC_h_GUID_6689A6CA_76AC_48AC_A0D0_2902BC95AC35
#define INCLUDED_RenderingTypesC_h_GUID_6689A6CA_76AC_48AC_A0D0_2902BC95AC35

/* Internal Includes */
#include <osvr/Util/StdInt.h>
#include <osvr/Util/APIBaseC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup PluginKit
@{
*/

/** @brief A count or index for a display input in a display config.
*/
typedef uint8_t OSVR_DisplayInputCount;

/** @brief The integer type used in specification of size or location of a
    display input, in pixels.
*/
typedef int32_t OSVR_DisplayDimension;

/** @brief The integer type specifying a number of viewers in a system.

    A "head" is a viewer (though not all viewers are necessarily heads).

    The count is output from osvrClientGetNumViewers().

    When used as an ID/index, it is zero-based, so values range from 0 to (count
    - 1) inclusive.

    The most frequent count is 1, though higher values are theoretically
    possible. If you do not handle higher values, do still check and alert the
    user if their system reports a higher number, as your application may not
    behave as the user expects.
*/
typedef uint32_t OSVR_ViewerCount;

/** @brief The integer type specifying the number of eyes (viewpoints) of a
    viewer.

    The count for a given viewer is output from osvrClientGetNumEyesForViewer().

    When used as an ID/index, it is zero-based,so values range from 0 to (count
    - 1) inclusive, for a given viewer.

    Use as an ID/index is not meaningful except in conjunction with the ID of
    the corresponding viewer. (that is, there is no overall "eye 0", but "viewer
    0, eye 0" is meaningful.)

    In practice, the most frequent counts are 1 (e.g. mono) and 2 (e.g. stereo),
    and for example the latter results in eyes with ID 0 and 1 for the viewer.
    There is no innate or consistent semantics/meaning ("left" or "right") to
    indices guaranteed at this time, and applications should not try to infer
    any.
*/
typedef uint8_t OSVR_EyeCount;

/** @brief The integer type specifying the number of surfaces seen by a viewer's
    eye.

    The count for a given viewer and eye is output from
    osvrClientGetNumSurfacesForViewerEye(). Note that the count is not
    necessarily equal between eyes of a viewer.

    When used as an ID/index, it is zero-based, so values range from 0 to (count
    - 1) inclusive, for a given viewer and eye.

    Use as an ID/index is not meaningful except in conjunction with the IDs of
    the corresponding viewer and eye. (that is, there is no overall "surface 0",
    but "viewer 0, eye 0, surface 0" is meaningful.)
*/
typedef uint32_t OSVR_SurfaceCount;

/** @brief The integer type used in specification of size or location of a
    viewport.
*/
typedef int32_t OSVR_ViewportDimension;

/** @brief The integer type used to indicate relative priorities of a display
    distortion strategy. Negative values are defined to mean that strategy is
    unavailable.

    @sa OSVR_DISTORTION_PRIORITY_UNAVAILABLE
*/
typedef int32_t OSVR_DistortionPriority;

/** @brief The constant to return as an OSVR_DistortionPriority if a given
    strategy is not available for a surface.

    @sa OSVR_DistortionPriority
*/
#define OSVR_DISTORTION_PRIORITY_UNAVAILABLE (-1)

/** @} */

OSVR_EXTERN_C_END

#endif
