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

/* Pre-emptive camera configuration options. */
dictionary CameraConfiguration
{
  CameraMode mode = "unspecified";
  CameraSize previewSize = null;
  DOMString recorderProfile = ""; // one of the profiles reported by
                                  // CameraControl.capabilities.recorderProfiles
};

callback CameraErrorCallback = void (DOMString error);

callback GetCameraCallback = void (CameraControl camera,
                                   CameraConfiguration configuration);

[Func="nsDOMCameraManager::HasSupport"]
interface CameraManager
{
  /* get a camera instance; 'camera' is one of the camera
     identifiers returned by getListOfCameras() below.
  */
  [Throws]
  void getCamera(DOMString camera,
                 CameraConfiguration initialConfiguration,
                 GetCameraCallback callback,
                 optional CameraErrorCallback errorCallback);

  /* return an array of camera identifiers, e.g.
       [ "front", "back" ]
   */
  [Throws]
  sequence<DOMString> getListOfCameras();
};
