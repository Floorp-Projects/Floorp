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

#ifndef INCLUDED_DisplayC_h_GUID_8658EDC9_32A2_49A2_5F5C_10F67852AE74
#define INCLUDED_DisplayC_h_GUID_8658EDC9_32A2_49A2_5F5C_10F67852AE74

/* Internal Includes */
#include <osvr/ClientKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>
#include <osvr/Util/RenderingTypesC.h>
#include <osvr/Util/MatrixConventionsC.h>
#include <osvr/Util/Pose3C.h>
#include <osvr/Util/BoolC.h>
#include <osvr/Util/RadialDistortionParametersC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN
/** @addtogroup ClientKit
    @{
    @name Display API
    @{
*/

/** @brief Opaque type of a display configuration. */
typedef struct OSVR_DisplayConfigObject *OSVR_DisplayConfig;

/** @brief Allocates a display configuration object populated with data from the
    OSVR system.

    Before this call will succeed, your application will need to be correctly
   and fully connected to an OSVR server. You may consider putting this call in
   a loop alternating with osvrClientUpdate() until this call succeeds.

    Data provided by a display configuration object:

    - The logical display topology (number and relationship of viewers, eyes,
        and surfaces), which remains constant throughout the life of the
        configuration object. (A method of notification of change here is TBD).
    - Pose data for viewers (not required for rendering) and pose/view data for
        eyes (used for rendering) which is based on tracker data: if used, these
        should be queried every frame.
    - Projection matrix data for surfaces, which while in current practice may
        be relatively unchanging, we are not guaranteeing them to be constant:
        these should be queried every frame.
    - Video-input-relative viewport size/location for a surface: would like this
        to be variable, but probably not feasible. If you have input, please
        comment on the dev mailing list.
    - Per-surface distortion strategy priorities/availabilities: constant. Note
        the following, though...
    - Per-surface distortion strategy parameters: variable, request each frame.
        (Could make constant with a notification if needed?)

    Important note: While most of this data is immediately available if you are
   successful in getting a display config object, the pose-based data (viewer
   pose, eye pose, eye view matrix) needs tracker state, so at least one (and in
   practice, typically more) osvrClientUpdate() must be performed before a new
   tracker report is available to populate that state. See
   osvrClientCheckDisplayStartup() to query if all startup data is available.

    @todo Decide if relative viewport should be constant in a display config,
   and update docs accordingly.

    @todo Decide if distortion params should be constant in a display config,
   and update docs accordingly.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed or some other
    error occurred, in which case the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetDisplay(OSVR_ClientContext ctx, OSVR_DisplayConfig *disp);

/** @brief Frees a display configuration object. The corresponding context must
    still be open.

    If you fail to call this, it will be automatically called as part of
    clean-up when the corresponding context is closed.

    @return OSVR_RETURN_FAILURE if a null config was passed, or if the given
    display object was already freed.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientFreeDisplay(OSVR_DisplayConfig disp);

/** @brief Checks to see if a display is fully configured and ready, including
    having received its first pose update.

    Once this first succeeds, it will continue to succeed for the lifetime of
    the display config object, so it is not necessary to keep calling once you
    get a successful result.

    @return OSVR_RETURN_FAILURE if a null config was passed, or if the given
    display config object was otherwise not ready for full use.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientCheckDisplayStartup(OSVR_DisplayConfig disp);

/** @brief A display config can have one or more display inputs to pass pixels
    over (HDMI/DVI connections, etc): retrieve the number of display inputs in
   the current configuration.

    @param disp Display config object.
    @param[out] numDisplayInputs Number of display inputs in the logical display
    topology, **constant** throughout the active, valid lifetime of a display
    config object.

    @sa OSVR_DisplayInputCount

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in
   which case the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetNumDisplayInputs(
    OSVR_DisplayConfig disp, OSVR_DisplayInputCount *numDisplayInputs);

/** @brief Retrieve the pixel dimensions of a given display input for a display
   config

    @param disp Display config object.
    @param displayInputIndex The zero-based index of the display input.
    @param[out] width Width (in pixels) of the display input.
    @param[out] height Height (in pixels) of the display input.

    The out parameters are **constant** throughout the active, valid lifetime of
    a display config object.

    @sa OSVR_DisplayDimension

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in
   which case the output arguments are unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetDisplayDimensions(
    OSVR_DisplayConfig disp, OSVR_DisplayInputCount displayInputIndex,
    OSVR_DisplayDimension *width, OSVR_DisplayDimension *height);

/** @brief A display config can have one (or theoretically more) viewers:
    retrieve the viewer count.

    @param disp Display config object.
    @param[out] viewers Number of viewers in the logical display topology,
    **constant** throughout the active, valid lifetime of a display config
    object.

    @sa OSVR_ViewerCount

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
   the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetNumViewers(OSVR_DisplayConfig disp, OSVR_ViewerCount *viewers);

/** @brief Get the pose of a viewer in a display config.

    Note that there may not necessarily be any surfaces rendered from this pose
    (it's the unused "center" eye in a stereo configuration, for instance) so
    only use this if it makes integration into your engine or existing
    applications (not originally designed for stereo) easier.

    Will only succeed if osvrClientCheckDisplayStartup() succeeds.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed or no pose was
    yet available, in which case the pose argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetViewerPose(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_Pose3 *pose);

/** @brief Each viewer in a display config can have one or more "eyes" which
    have a substantially similar pose: get the count.

    @param disp Display config object.
    @param viewer Viewer ID
    @param[out] eyes Number of eyes for this viewer in the logical display
   topology, **constant** throughout the active, valid lifetime of a display
   config object

    @sa OSVR_EyeCount

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetNumEyesForViewer(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount *eyes);

/** @brief Get the "viewpoint" for the given eye of a viewer in a display
   config.

    Will only succeed if osvrClientCheckDisplayStartup() succeeds.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param[out] pose Room-space pose (not relative to pose of the viewer)

    @return OSVR_RETURN_FAILURE if invalid parameters were passed or no pose was
    yet available, in which case the pose argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyePose(OSVR_DisplayConfig disp, OSVR_ViewerCount viewer,
                           OSVR_EyeCount eye, OSVR_Pose3 *pose);

/** @brief Get the view matrix (inverse of pose) for the given eye of a
    viewer in a display config - matrix of **doubles**.

    Will only succeed if osvrClientCheckDisplayStartup() succeeds.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param flags Bitwise OR of matrix convention flags (see @ref MatrixFlags)
    @param[out] mat Pass a double[::OSVR_MATRIX_SIZE] to get the transformation
    matrix from room space to eye space (not relative to pose of the viewer)

    @return OSVR_RETURN_FAILURE if invalid parameters were passed or no pose was
    yet available, in which case the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetViewerEyeViewMatrixd(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_MatrixConventions flags, double *mat);

/** @brief Get the view matrix (inverse of pose) for the given eye of a
    viewer in a display config - matrix of **floats**.

    Will only succeed if osvrClientCheckDisplayStartup() succeeds.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param flags Bitwise OR of matrix convention flags (see @ref MatrixFlags)
    @param[out] mat Pass a float[::OSVR_MATRIX_SIZE] to get the transformation
    matrix from room space to eye space (not relative to pose of the viewer)

    @return OSVR_RETURN_FAILURE if invalid parameters were passed or no pose was
    yet available, in which case the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetViewerEyeViewMatrixf(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_MatrixConventions flags, float *mat);

/** @brief Each eye of each viewer in a display config has one or more surfaces
    (aka "screens") on which content should be rendered.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param[out] surfaces Number of surfaces (numbered [0, surfaces - 1]) for the
    given viewer and eye. **Constant** throughout the active, valid lifetime of
    a display config object.

    @sa OSVR_SurfaceCount

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode osvrClientGetNumSurfacesForViewerEye(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount *surfaces);

/** @brief Get the dimensions/location of the viewport **within the display
    input** for a surface seen by an eye of a viewer in a display config. (This
    does not include other video inputs that may be on a single virtual desktop,
    etc. or explicitly account for display configurations that use multiple
    video inputs. It does not necessarily indicate that a viewport in the sense
    of glViewport must be created with these parameters, though the parameter
    order matches for convenience.)

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] left Output: Distance in pixels from the left of the video input
    to the left of the viewport.
    @param[out] bottom Output: Distance in pixels from the bottom of the video
    input to the bottom of the viewport.
    @param[out] width Output: Width of viewport in pixels.
    @param[out] height Output: Height of viewport in pixels.


    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output arguments are unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetRelativeViewportForViewerEyeSurface(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, OSVR_ViewportDimension *left,
    OSVR_ViewportDimension *bottom, OSVR_ViewportDimension *width,
    OSVR_ViewportDimension *height);

/** @brief Get the index of the display input for a surface seen by an eye of a
   viewer in a display config.

    This is the OSVR-assigned display input: it may not (and in practice,
    usually will not) match any platform-specific display indices. This function
    exists to associate surfaces with video inputs as enumerated by
    osvrClientGetNumDisplayInputs().

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] displayInput Zero-based index of the display input pixels for
    this surface are tranmitted over.

    This association is **constant** throughout the active, valid lifetime of a
    display config object.

    @sa osvrClientGetNumDisplayInputs(),
    osvrClientGetRelativeViewportForViewerEyeSurface()

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which
    case the output argument is unmodified.
 */
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceDisplayInputIndex(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, OSVR_DisplayInputCount *displayInput);

/** @brief Get the projection matrix for a surface seen by an eye of a viewer
    in a display config. (double version)

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param near Distance from viewpoint to near clipping plane - must be
    positive.
    @param far Distance from viewpoint to far clipping plane - must be positive
    and not equal to near, typically greater than near.
    @param flags Bitwise OR of matrix convention flags (see @ref MatrixFlags)
    @param[out] matrix Output projection matrix: supply an array of 16
    (::OSVR_MATRIX_SIZE) doubles.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceProjectionMatrixd(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, double near, double far,
    OSVR_MatrixConventions flags, double *matrix);

/** @brief Get the projection matrix for a surface seen by an eye of a viewer
    in a display config. (float version)

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param near Distance to near clipping plane - must be nonzero, typically
    positive.
    @param far Distance to far clipping plane - must be nonzero, typically
    positive and greater than near.
    @param flags Bitwise OR of matrix convention flags (see @ref MatrixFlags)
    @param[out] matrix Output projection matrix: supply an array of 16
    (::OSVR_MATRIX_SIZE) floats.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceProjectionMatrixf(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, float near, float far,
    OSVR_MatrixConventions flags, float *matrix);

/** @brief Get the clipping planes (positions at unit distance) for a surface
   seen by an eye of a viewer
    in a display config.

    This is only for use in integrations that cannot accept a fully-formulated
    projection matrix as returned by
    osvrClientGetViewerEyeSurfaceProjectionMatrixf() or
    osvrClientGetViewerEyeSurfaceProjectionMatrixd(), and may not necessarily
    provide the same optimizations.

    As all the planes are given at unit (1) distance, before passing these
   planes to a consuming function in your application/engine, you will typically
   divide them by your near clipping plane distance.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] left Distance to left clipping plane
    @param[out] right Distance to right clipping plane
    @param[out] bottom Distance to bottom clipping plane
    @param[out] top Distance to top clipping plane

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output arguments are unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceProjectionClippingPlanes(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, double *left, double *right, double *bottom,
    double *top);

/** @brief Determines if a surface seen by an eye of a viewer in a display
    config requests some distortion to be performed.

    This simply reports true or false, and does not specify which kind of
    distortion implementations have been parameterized for this display. For
    each distortion implementation your application supports, you'll want to
    call the corresponding priority function to find out if it is available.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] distortionRequested Output parameter: whether distortion is
    requested. **Constant** throughout the active, valid lifetime of a display
    config object.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientDoesViewerEyeSurfaceWantDistortion(OSVR_DisplayConfig disp,
                                             OSVR_ViewerCount viewer,
                                             OSVR_EyeCount eye,
                                             OSVR_SurfaceCount surface,
                                             OSVR_CBool *distortionRequested);

/** @brief Returns the priority/availability of radial distortion parameters for
    a surface seen by an eye of a viewer in a display config.

    If osvrClientDoesViewerEyeSurfaceWantDistortion() reports false, then the
    display does not request distortion of any sort, and thus neither this nor
    any other distortion strategy priority function will report an "available"
    priority.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] priority Output: the priority level. Negative values
    (canonically OSVR_DISTORTION_PRIORITY_UNAVAILABLE) indicate this technique
    not available, higher values indicate higher preference for the given
    technique based on the device's description. **Constant** throughout the
    active, valid lifetime of a display config object.

    @return OSVR_RETURN_FAILURE if invalid parameters were passed, in which case
    the output argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceRadialDistortionPriority(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, OSVR_DistortionPriority *priority);

/** @brief Returns the radial distortion parameters, if known/requested, for a
    surface seen by an eye of a viewer in a display config.

    Will only succeed if osvrClientGetViewerEyeSurfaceRadialDistortionPriority()
    reports a non-negative priority.

    @param disp Display config object
    @param viewer Viewer ID
    @param eye Eye ID
    @param surface Surface ID
    @param[out] params Output: the parameters for radial distortion

    @return OSVR_RETURN_FAILURE if this surface does not have these parameters
    described, or if invalid parameters were passed, in which case the output
    argument is unmodified.
*/
OSVR_CLIENTKIT_EXPORT OSVR_ReturnCode
osvrClientGetViewerEyeSurfaceRadialDistortion(
    OSVR_DisplayConfig disp, OSVR_ViewerCount viewer, OSVR_EyeCount eye,
    OSVR_SurfaceCount surface, OSVR_RadialDistortionParameters *params);

/** @}
    @}
*/

OSVR_EXTERN_C_END

#endif
