/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary CameraPictureOptions {

  /* an object with a combination of 'height' and 'width' properties
     chosen from nsICameraCapabilities.pictureSizes */
  // XXXbz this should be a CameraSize dictionary, but we don't have that yet.
  any pictureSize = null;

  /* one of the file formats chosen from
     nsICameraCapabilities.fileFormats */
  DOMString fileFormat = "";

  /* the rotation of the image in degrees, from 0 to 270 in
     steps of 90; this doesn't affect the image, only the
     rotation recorded in the image header.*/
  long rotation = 0;

  /* an object containing any or all of 'latitude', 'longitude',
     'altitude', and 'timestamp', used to record when and where
     the image was taken.  e.g.
     {
         latitude:  43.647118,
         longitude: -79.3943,
         altitude:  500
         // timestamp not specified, in this case, and
         // won't be included in the image header
     }

     can be null in the case where position information isn't
     available/desired.

     'altitude' is in metres; 'timestamp' is UTC, in seconds from
     January 1, 1970.
  */
  any position = null;

  /* the number of seconds from January 1, 1970 UTC.  This can be
     different from the positional timestamp (above). */
  // XXXbz this should really accept a date too, no?
  long long dateTime = 0;
};

// If we start using CameraPictureOptions here, remove it from DummyBinding.

interface GetCameraCallback;
interface CameraErrorCallback;

interface CameraManager {
  [Throws]
  void getCamera(any options, GetCameraCallback callback,
                 optional CameraErrorCallback errorCallback);
  [Throws]
  sequence<DOMString> getListOfCameras();
};
