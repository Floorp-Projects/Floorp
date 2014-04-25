/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Camera regions are used to set focus and metering areas;
   the coordinates are referenced to the sensor:
     (-1000, -1000) is the top-left corner
     (1000, 1000) is the bottom-right corner
   The weight of the region can range from 0 to 1000. */
dictionary CameraRegion
{
  long top = -1000;
  long left = -1000;
  long bottom = 1000;
  long right = 1000;
  unsigned long weight = 1000;
};

/* The position information to record in the image header.
   'NaN' indicates the information is not available. */
dictionary CameraPosition
{
  unrestricted double latitude = NaN;
  unrestricted double longitude = NaN;
  unrestricted double altitude = NaN;
  unrestricted double timestamp = NaN;
};

/*
  Options for takePicture().
*/
dictionary CameraPictureOptions
{
  /* an object with a combination of 'height' and 'width' properties
     chosen from CameraCapabilities.pictureSizes */
  CameraSize pictureSize = null;

  /* one of the file formats chosen from
     CameraCapabilities.fileFormats */
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
  CameraPosition position = null;

  /* the number of seconds from January 1, 1970 UTC.  This can be
     different from the positional timestamp (above). */
  // XXXbz this should really accept a date too, no?
  long long dateTime = 0;
};

/* These properties affect the actual video recording, e.g.
      {
         rotation: 0,
         maxFileSizeBytes: 1024 * 1024,
         maxVideoLengthMs: 0
      }

   'rotation' is the degrees clockwise to rotate the recorded video; if
   this options is not supported, it will be ignored; if this option is
   missing, the default is 0.

   'maxFileSizeBytes' is the maximum size in bytes to which the recorded
   video file will be allowed to grow.

   'maxVideoLengthMs' is the maximum length in milliseconds to which the
   recorded video will be allowed to grow.

   if either 'maxFileSizeBytes' or 'maxVideoLengthMs' is missing, zero,
   or negative, that limit will be disabled.
*/
dictionary CameraStartRecordingOptions
{
  long      rotation = 0;
  long long maxFileSizeBytes = 0;
  long long maxVideoLengthMs = 0;

  /* If startRecording() is called with flashMode set to "auto" and the
     camera has determined that the scene is poorly lit, the flash mode
     will be automatically changed to "torch" until stopRecording() is
     called. During this time, flashMode will reflect the new setting. If
     flashMode is changed while recording is in progress, the new setting
     will be left as-is on stopRecording(). If the camera does not
     support this setting, it will be ignored. */
  boolean autoEnableLowLightTorch = false;
};

callback CameraSetConfigurationCallback = void (CameraConfiguration configuration);
callback CameraAutoFocusCallback = void (boolean focused);
callback CameraTakePictureCallback = void (Blob picture);
callback CameraStartRecordingCallback = void ();
callback CameraShutterCallback = void ();
callback CameraClosedCallback = void ();
callback CameraReleaseCallback = void ();
callback CameraRecorderStateChange = void (DOMString newState);
callback CameraPreviewStateChange = void (DOMString newState);
callback CameraAutoFocusMovingCallback = void (boolean isMoving);

/*
    attributes here affect the preview, any pictures taken, and/or
    any video recorded by the camera.
*/
[Func="nsDOMCameraControl::HasSupport"]
interface CameraControl : MediaStream
{
  [Constant, Cached]
  readonly attribute CameraCapabilities capabilities;

  /* one of the values chosen from capabilities.effects;
     default is "none" */
  [Throws]
  attribute DOMString       effect;

  /* one of the values chosen from capabilities.whiteBalanceModes;
     default is "auto" */
  [Throws]
  attribute DOMString       whiteBalanceMode;

  /* one of the values chosen from capabilities.sceneModes;
     default is "auto" */
  [Throws]
  attribute DOMString       sceneMode;

  /* one of the values chosen from capabilities.flashModes;
     default is "auto" */
  [Throws]
  attribute DOMString       flashMode;

  /* one of the values chosen from capabilities.focusModes;
     default is "auto", if supported, or "fixed" */
  [Throws]
  attribute DOMString       focusMode;

  /* one of the values chosen from capabilities.zoomRatios; other
     values will be rounded to the nearest supported value;
     default is 1.0 */
  [Throws]
  attribute double          zoom;

  /* an array of one or more objects that define where the
     camera will perform light metering, each defining the properties:
      {
          top: -1000,
          left: -1000,
          bottom: 1000,
          right: 1000,
          weight: 1000
      }

      'top', 'left', 'bottom', and 'right' all range from -1000 at
      the top-/leftmost of the sensor to 1000 at the bottom-/rightmost
      of the sensor.

      objects missing one or more of these properties will be ignored;
      if the array contains more than capabilities.maxMeteringAreas,
      extra areas will be ignored.

      this attribute can be set to null to allow the camera to determine
      where to perform light metering. */
  [Throws]
  attribute any             meteringAreas;

  /* an array of one or more objects that define where the camera will
     perform auto-focusing, with the same definition as meteringAreas.

     if the array contains more than capabilities.maxFocusAreas, extra
     areas will be ignored.

     this attribute can be set to null to allow the camera to determine
     where to focus. */
  [Throws]
  attribute any             focusAreas;

  /* focal length in millimetres */
  [Throws]
  readonly attribute double focalLength;

  /* the distances in metres to where the image subject appears to be
     in focus.  'focusDistanceOptimum' is where the subject will appear
     sharpest; the difference between 'focusDistanceFar' and
     'focusDistanceNear' is the image's depth of field.

     'focusDistanceFar' may be infinity. */
  [Throws]
  readonly attribute double focusDistanceNear;
  [Throws]
  readonly attribute double focusDistanceOptimum;
  [Throws]
  readonly attribute unrestricted double focusDistanceFar;

  /* 'compensation' is optional, and if missing, will
     set the camera to use automatic exposure compensation.

     acceptable values must range from minExposureCompensation
     to maxExposureCompensation in steps of stepExposureCompensation;
     invalid values will be rounded to the nearest valid value. */
  [Throws]
  void setExposureCompensation(optional double compensation);
  [Throws]
  readonly attribute unrestricted double exposureCompensation;

  /* one of the values chosen from capabilities.isoModes; default
     value is "auto" if supported. */
  [Throws]
  attribute DOMString       isoMode;

  /* the function to call on the camera's shutter event, to trigger
     a shutter sound and/or a visual shutter indicator. */
  attribute CameraShutterCallback? onShutter;

  /* the function to call when the camera hardware is closed
     by the underlying framework, e.g. when another app makes a more
     recent call to get the camera. */
  attribute CameraClosedCallback? onClosed;

  /* the function to call when the recorder changes state, either because
     the recording process encountered an error, or because one of the
     recording limits (see CameraStartRecordingOptions) was reached. */
  attribute CameraRecorderStateChange? onRecorderStateChange;

  /* the function to call when the viewfinder stops or starts,
     useful for synchronizing other UI elements. */
  attribute CameraPreviewStateChange? onPreviewStateChange;

  /* the size of the picture to be returned by a call to takePicture();
     an object with 'height' and 'width' properties that corresponds to
     one of the options returned by capabilities.pictureSizes. */
  [Throws]
  attribute any              pictureSize;

  /* the size of the thumbnail to be included in the picture returned
     by a call to takePicture(), assuming the chosen fileFormat supports
     one; an object with 'height' and 'width' properties that corresponds
     to one of the options returned by capabilities.pictureSizes.

     this setting should be considered a hint: the implementation will
     respect it when possible, and override it if necessary. */
  [Throws]
  attribute any             thumbnailSize;

  /* the angle, in degrees, that the image sensor is mounted relative
     to the display; e.g. if 'sensorAngle' is 270 degrees (or -90 degrees),
     then the preview stream needs to be rotated +90 degrees to have the
     same orientation as the real world. */
  readonly attribute long   sensorAngle;

  /* tell the camera to attempt to focus the image */
  [Throws]
  void autoFocus(CameraAutoFocusCallback onSuccess, optional CameraErrorCallback onError);

  /* if continuous autofocus is supported and focusMode is set to enable it,
     then this function is called whenever the camera decides to start and
     stop moving the focus position; it can be used to update a UI element to
     indicate that the camera is still trying to focus, or has finished. Some
     platforms do not support this event, in which case the callback is never
     invoked. */
  [Pref="camera.control.autofocus_moving_callback.enabled"]
  attribute CameraAutoFocusMovingCallback? onAutoFocusMoving;

  /* capture an image and return it as a blob to the 'onSuccess' callback;
     if the camera supports it, this may be invoked while the camera is
     already recording video.

     invoking this function will stop the preview stream, which must be
     manually restarted (e.g. by calling .play() on it). */
  [Throws]
  void takePicture(CameraPictureOptions aOptions,
                   CameraTakePictureCallback onSuccess,
                   optional CameraErrorCallback onError);

  /* start recording video; 'aOptions' is a
     CameraStartRecordingOptions object. */
  [Throws]
  void startRecording(CameraStartRecordingOptions aOptions,
                      DeviceStorage storageArea,
                      DOMString filename,
                      CameraStartRecordingCallback onSuccess,
                      optional CameraErrorCallback onError);

  /* stop precording video. */
  [Throws]
  void stopRecording();

  /* call in or after the takePicture() onSuccess callback to
     resume the camera preview stream. */
  [Throws]
  void resumePreview();

  /* release the camera so that other applications can use it; you should
     probably call this whenever the camera is not longer in the foreground
     (depending on your usage model).

     the callbacks are optional, unless you really need to know when
     the hardware is ultimately released.

     once this is called, the camera control object is to be considered
     defunct; a new instance will need to be created to access the camera. */
  [Throws]
  void release(optional CameraReleaseCallback onSuccess,
               optional CameraErrorCallback onError);

  /* changes the camera configuration on the fly;
     'configuration' is of type CameraConfiguration.

     XXXmikeh the 'configuration' argument needs to be optional, else
     the WebIDL compiler throws: "WebIDL.WebIDLError: error: Dictionary
     argument or union argument containing a dictionary not followed by
     a required argument must be optional"
  */
  [Throws]
  void setConfiguration(optional CameraConfiguration configuration,
                        optional CameraSetConfigurationCallback onSuccess,
                        optional CameraErrorCallback onError);

  /* if focusMode is set to either 'continuous-picture' or 'continuous-video',
     then calling autoFocus() will trigger its onSuccess callback immediately
     if the camera was either successfully focused, or if no focus could be
     acquired; if the focus acquisition is still in progress, the onSuccess
     callback will be invoked later, its argument indicating success or
     failure.

     once autoFocus() is called with a continuous autofocus mode set, the
     continuous autofocus process is stopped and focus is locked in the
     current state until this method is called.
  */
  [Throws]
  void resumeContinuousFocus();
};

/* The coordinates of a point, relative to the camera sensor, of the center of
   detected facial features. As with CameraRegions:
     { x: -1000, y: -1000 } is the top-left corner
     { x:  1000, y:  1000 } is the bottom-right corner
   x and y can range from -1000 to 1000.
*/
[Pref="camera.control.face_detection.enabled", Func="DOMCameraPoint::HasSupport"]
interface CameraPoint
{
  attribute long x;
  attribute long y;
};

/* The information of the each face detected by a camera device, e.g.
     {
       id: 1,
       score: 80,
       bound: { left:   -203,
                top:    -400,
                right:   300,
                bottom:  250 },
       leftEye:  { x:  -100,
                   y:  -200 },
       rightEye: { x:   100,
                   y:   100 },
       mouth:    { x:   150,
                   y:   150 } }

   'id' is an unique value per face while the face is visible to the tracker.
   If the face leaves the viewfinder and then returns, it will be assigned
   a new value.

   'score' is the confidence level for the detection of the face.
   This range is 1 to 100, where 100 is the highest confidence.

   'bounds' is the bounds of the face. It is guaranteed left < right and
   top < bottom. The coordinates can be smaller than -1000 or bigger than 1000.
   But at least one vertex will be within (-1000, -1000) and (1000, 1000).

   'leftEye' is the coordinates of the centre of the left eye. The coordinates
   are in the same space as the ones for 'bounds'. This is an optional field
   and may not be supported on all devices. If it is not supported or detected,
   the value will be set to null.

   'rightEye' is the coordinates of the detected right eye; null if not
   supported or detected.

   'mouth' is the coordinates of the detected mouth; null if not supported or
   detected.
*/
[Pref="camera.control.face_detection.enabled", Func="DOMCameraDetectedFace::HasSupport"]
interface CameraDetectedFace
{
  readonly attribute unsigned long id;

  readonly attribute unsigned long score;

  readonly attribute DOMRect bounds;

  readonly attribute boolean hasLeftEye;
  readonly attribute CameraPoint? leftEye;

  readonly attribute boolean hasRightEye;
  readonly attribute CameraPoint? rightEye;

  readonly attribute boolean hasMouth;
  readonly attribute CameraPoint? mouth;
};

callback CameraFaceDetectionCallback = void (sequence<CameraDetectedFace> faces);

partial interface CameraControl
{
  /* Starts the face detection. This should be called after the preview is
     started. The camera will periodically call 'onFacesDetected' with a
     sequence of zero or one or more detected faces in the preview frame.

     How often the callback is invoked is implementation dependent.

     This method throws an exception if face detection fails to start.
  */
  [Throws, Pref="camera.control.face_detection.enabled"]
  void startFaceDetection();

  /* Stops the face detection.

     This method throws an exception if face detection can't be stopped.
  */
  [Throws, Pref="camera.control.face_detection.enabled"]
  void stopFaceDetection();

  /* Callback for faces detected in the preview frame. If no faces are
     detected, the callback is invoked with an empty sequence. */
  [Pref="camera.control.face_detection.enabled"]
  attribute CameraFaceDetectionCallback? onFacesDetected;
};
