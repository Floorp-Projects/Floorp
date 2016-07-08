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

   if either 'maxFileSizeBytes' or 'maxVideoLengthMs' is missing or zero,
   that limit will be disabled; if either value is out of range, it will
   be clamped from 0 to the upper limit for an 'unsigned long long'.
*/
dictionary CameraStartRecordingOptions
{
  long rotation = 0;
  [Clamp]
  unsigned long long maxFileSizeBytes = 0;
  [Clamp]
  unsigned long long maxVideoLengthMs = 0;

  /* If startRecording() is called with flashMode set to "auto" and the
     camera has determined that the scene is poorly lit, the flash mode
     will be automatically changed to "torch" until stopRecording() is
     called. During this time, flashMode will reflect the new setting. If
     flashMode is changed while recording is in progress, the new setting
     will be left as-is on stopRecording(). If the camera does not
     support this setting, it will be ignored. */
  boolean autoEnableLowLightTorch = false;

  /* If true, a poster JPG will be created from the recording and issued
     via the poster event. PosterCreated or PosterFailed recording state
     changes will indicate whether or not it was created. */
  boolean createPoster = false;
};

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

     if this setter is called with no arguments, the camera will
     determine metering areas on its own. */
  [Throws]
  sequence<CameraRegion> getMeteringAreas();
  [Throws]
  void setMeteringAreas(optional sequence<CameraRegion> meteringAreas);

  /* an array of one or more objects that define where the camera will
     perform auto-focusing, with the same definition as meteringAreas.

     if the array contains more than capabilities.maxFocusAreas, extra
     areas will be ignored.

     if this setter is called with no arguments, the camera will
     determine focus areas on its own. */
  [Throws]
  sequence<CameraRegion> getFocusAreas();
  [Throws]
  void setFocusAreas(optional sequence<CameraRegion> focusAreas);

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

  /* over- or under-expose the image; acceptable values must range from
     minExposureCompensation to maxExposureCompensation in steps of
     stepExposureCompensation. Invalid values will be rounded to the nearest
     valid value; out-of-bounds values will be limited to the range
     supported by the camera. */
  [Throws]
  attribute double          exposureCompensation;

  /* one of the values chosen from capabilities.isoModes; default
     value is "auto" if supported. */
  [Throws]
  attribute DOMString       isoMode;

  /* the event dispatched on the camera's shutter event, to trigger
     a shutter sound and/or a visual shutter indicator.

     contains no event-specific data. */
  attribute EventHandler    onshutter;

  /* the event dispatched when the camera hardware is closed; this may
     be due to a system failure, another process taking over the camera,
     or a call to release().

     The event has a 'reason' attribute that will be one of the following
     string values:
       - SystemFailure    : the camera subsystem failed and was closed;
       - HardwareReleased : a call to release() was successful;
       - NotAvailable     : the camera hardware is in use by another process.
  */
  attribute EventHandler    onclose;

  /* the event dispatched when the recorder changes state, either because
     the recording process encountered an error, or because one of the
     recording limits (see CameraStartRecordingOptions) was reached.

     event type is CameraStateChangeEvent where:
         'newState' is one of the following states:
             'Started' if the recording has begun capturing data
             'Stopped' when the recording has completed (success and failure)
             'Paused' if the recording is paused
             'Resumed' if the recording is resumed after pausing
             'PosterCreated' if a poster was requested and created
             'PosterFailed' if a poster was requested and failed to create
             'FileSizeLimitReached' if stopped due to file size limit
             'VideoLengthLimitReached' if stopped due to a time limit
             'TrackCompleted' if audio or video track complete when stopping
             'TrackFailed' if audio or video track incomplete when stopping
             'MediaRecorderFailed' if failed due to local error
             'MediaServerFailed' if failed due to media server */
  attribute EventHandler    onrecorderstatechange;

  /* the event dispatched when a poster is successfully captured; it is of the
     type BlobEvent, where the data attribute contains the poster. */
  attribute EventHandler    onposter;

  /* the event dispatched when the viewfinder stops or starts,
     useful for synchronizing other UI elements.

     event type is CameraStateChangeEvent where:
         'newState' is the new preview state */
  attribute EventHandler    onpreviewstatechange;

  /* the size of the picture to be returned by a call to takePicture();
     an object with 'height' and 'width' properties that corresponds to
     one of the options returned by capabilities.pictureSizes.

     note that unlike when one uses setConfiguration instead to update the
     picture size, this will not recalculate the ideal preview size. */
  [Throws]
  CameraSize getPictureSize();
  [Throws]
  void setPictureSize(optional CameraSize size);

  /* if the image blob to be returned by takePicture() supports lossy
     compression, this setting controls the quality-size trade-off;
     valid values range from 0.0 for smallest size/worst quality to 1.0
     for largest size/best quality. Note that depending on the range of
     values supported by the underlying platform, this attribute may not
     'get' the exact value that was previously 'set'. If this setting is
     not supported, it is ignored. */
  [Throws]
  attribute double          pictureQuality;

  /* the size of the thumbnail to be included in the picture returned
     by a call to takePicture(), assuming the chosen fileFormat supports
     one; an object with 'height' and 'width' properties that corresponds
     to one of the options returned by capabilities.pictureSizes.

     this setting should be considered a hint: the implementation will
     respect it when possible, and override it if necessary. */
  [Throws]
  CameraSize getThumbnailSize();
  [Throws]
  void setThumbnailSize(optional CameraSize size);

  /* the angle, in degrees, that the image sensor is mounted relative
     to the display; e.g. if 'sensorAngle' is 270 degrees (or -90 degrees),
     then the preview stream needs to be rotated +90 degrees to have the
     same orientation as the real world. */
  [Constant, Cached]
  readonly attribute long   sensorAngle;

  /* the mode the camera will use to determine the correct exposure of
     the scene; supported modes are exposed by capabilities.meteringModes. */
  [Throws]
  attribute DOMString       meteringMode;

  /* tell the camera to attempt to focus the image */
  [Throws]
  Promise<boolean> autoFocus();

  /* the event dispatched whenever the focus state changes due to calling
     autoFocus or due to continuous autofocus.

     if continuous autofocus is supported and focusMode is set to enable it,
     then this event is dispatched whenever the camera decides to start and
     stop moving the focus position; it can be used to update a UI element to
     indicate that the camera is still trying to focus, or has finished. Some
     platforms do not support this event, in which case the callback is never
     invoked.

     event type is CameraStateChangeEvent where:
         'newState' is one of the following states:
             'focused' if the focus is now set
             'focusing' if the focus is moving
             'unfocused' if last attempt to focus failed */
  attribute EventHandler    onfocus;

  /* capture an image and return it as a blob to the 'onSuccess' callback;
     if the camera supports it, this may be invoked while the camera is
     already recording video.

     invoking this function will stop the preview stream, which must be
     manually restarted by calling resumePreview(). */
  [Throws]
  Promise<Blob> takePicture(optional CameraPictureOptions options);

  /* the event dispatched when a picture is successfully taken; it is of the
     type BlobEvent, where the data attribute contains the picture. */
  attribute EventHandler    onpicture;

  /* start recording video; 'options' is a CameraStartRecordingOptions object.
     If the success/error callbacks are not used, one may determine success by
     waiting for the recorderstatechange event. */
  [Throws]
  Promise<void> startRecording(CameraStartRecordingOptions options,
                               DeviceStorage storageArea,
                               DOMString filename);

  /* stop recording video. */
  [Throws]
  void stopRecording();

  /* pause recording video. The camera remains active but audio and video
     frames are no longer saved in the output file. If called when not
     recording or already paused, it fails silently. */
  [Throws]
  void pauseRecording();

  /* resume recording video while paused. If called when not recording or
     not paused, it fails silently. */
  [Throws]
  void resumeRecording();

  /* call in or after the takePicture() onSuccess callback to
     resume the camera preview stream. */
  [Throws]
  void resumePreview();

  /* release the camera so that other applications can use it; you should
     probably call this whenever the camera is not longer in the foreground
     (depending on your usage model).

     once this is called, the camera control object is to be considered
     defunct; a new instance will need to be created to access the camera. */
  [Throws]
  Promise<void> release();

  /* changes the camera configuration on the fly. */
  [Throws]
  Promise<CameraConfiguration> setConfiguration(optional CameraConfiguration configuration);

  /* the event dispatched when the camera is successfully configured.

     event type is CameraConfigurationEvent which has the same members as
     CameraConfiguration. */
  attribute EventHandler onconfigurationchange;

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
   the value will be set to null. The x and y coordinates are bounded by the
   range (-1000, 1000) where:
       { x: -1000, y: -1000 } is the top-left corner
       { x:  1000, y:  1000 } is the bottom-right corner

   'rightEye' is the coordinates of the detected right eye; null if not
   supported or detected. Same boundary conditions as 'leftEye'.

   'mouth' is the coordinates of the detected mouth; null if not supported or
   detected. Same boundary conditions as 'leftEye'.
*/
[Pref="camera.control.face_detection.enabled",
 Func="DOMCameraDetectedFace::HasSupport",
 Constructor(optional CameraDetectedFaceInit initDict)]
interface CameraDetectedFace
{
  readonly attribute unsigned long id;

  readonly attribute unsigned long score;

  readonly attribute DOMRect bounds;

  readonly attribute boolean hasLeftEye;
  readonly attribute DOMPoint? leftEye;

  readonly attribute boolean hasRightEye;
  readonly attribute DOMPoint? rightEye;

  readonly attribute boolean hasMouth;
  readonly attribute DOMPoint? mouth;
};

dictionary CameraDetectedFaceInit
{
  unsigned long id = 0;
  unsigned long score = 100;
  CameraRegion bounds;
  boolean hasLeftEye = false;
  DOMPointInit leftEye;
  boolean hasRightEye = false;
  DOMPointInit rightEye;
  boolean hasMouth = false;
  DOMPointInit mouth;
};

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

  /* CameraFacesDetectedEvent */
  [Pref="camera.control.face_detection.enabled"]
  attribute EventHandler    onfacesdetected;
};
