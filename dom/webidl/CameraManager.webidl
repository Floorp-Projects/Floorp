/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum CameraMode { "unspecified", "picture", "video" };

/* Used for the dimensions of a captured picture,
   a preview stream, a video capture stream, etc. */
dictionary CameraSize
{
  unsigned long width = 0;
  unsigned long height = 0;
};

/* Pre-emptive camera configuration options. If 'mode' is set to "unspecified",
   the camera will not be configured immediately. If the 'mode' is set to
   "video" or "picture", then the camera automatically configures itself and
   will be ready for use upon return.

   The remaining parameters are optional and are considered hints by the
   camera. The application should use the values returned in the
   GetCameraCallback configuration because while the camera makes a best effort
   to adhere to the requested values, it may need to change them to ensure
   optimal behavior.

   If not specified, 'pictureSize' and 'recorderProfile' default to the best or
   highest resolutions supported by the camera hardware.

   To determine 'previewSize', one should generally provide the size of the
   element which will contain the preview rather than guess which supported
   preview size is the best. If not specified, 'previewSize' defaults to the
   inner window size. */
dictionary CameraConfiguration
{
  CameraMode mode = "picture";
  CameraSize previewSize = null;
  CameraSize pictureSize = null;

  /* one of the profiles reported by
     CameraControl.capabilities.recorderProfiles
  */
  DOMString recorderProfile = "default";
};

[Func="nsDOMCameraManager::HasSupport"]
interface CameraManager
{
  /* get a camera instance; 'camera' is one of the camera
     identifiers returned by getListOfCameras() below.
  */
  [Throws]
  Promise<CameraGetPromiseData> getCamera(DOMString camera,
                                          optional CameraConfiguration initialConfiguration);

  /* return an array of camera identifiers, e.g.
       [ "front", "back" ]
   */
  [Throws]
  sequence<DOMString> getListOfCameras();
};
