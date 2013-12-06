/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface CameraCapabilities;
interface GetCameraCallback;
interface CameraErrorCallback;
interface CameraShutterCallback;
interface CameraClosedCallback;
interface CameraRecorderStateChange;
interface CameraAutoFocusCallback;
interface CameraTakePictureCallback;
interface CameraPreviewStateChange;
interface CameraPreviewStreamCallback;
interface CameraStartRecordingCallback;
interface CameraReleaseCallback;

/*
    attributes here affect the preview, any pictures taken, and/or
    any video recorded by the camera.
*/
interface CameraControl {
    readonly attribute CameraCapabilities capabilities;

    /* one of the values chosen from capabilities.effects;
       default is "none" */
    [Throws]
    attribute DOMString         effect;

    /* one of the values chosen from capabilities.whiteBalanceModes;
       default is "auto" */
    [Throws]
    attribute DOMString         whiteBalanceMode;

    /* one of the values chosen from capabilities.sceneModes;
       default is "auto" */
    [Throws]
    attribute DOMString         sceneMode;

    /* one of the values chosen from capabilities.flashModes;
       default is "auto" */
    [Throws]
    attribute DOMString         flashMode;

    /* one of the values chosen from capabilities.focusModes;
       default is "auto", if supported, or "fixed" */
    [Throws]
    attribute DOMString         focusMode;

    /* one of the values chosen from capabilities.zoomRatios; other
       values will be rounded to the nearest supported value;
       default is 1.0 */
    [Throws]
    attribute double            zoom;

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
    readonly attribute double   focalLength;

    /* the distances in metres to where the image subject appears to be
       in focus.  'focusDistanceOptimum' is where the subject will appear
       sharpest; the difference between 'focusDistanceFar' and
       'focusDistanceNear' is the image's depth of field.

       'focusDistanceFar' may be infinity. */
    [Throws]
    readonly attribute double   focusDistanceNear;
    [Throws]
    readonly attribute double   focusDistanceOptimum;
    [Throws]
    readonly attribute unrestricted double   focusDistanceFar;

    /* 'compensation' is optional, and if missing, will
       set the camera to use automatic exposure compensation.

       acceptable values must range from minExposureCompensation
       to maxExposureCompensation in steps of stepExposureCompensation;
       invalid values will be rounded to the nearest valid value. */
    [Throws]
    void setExposureCompensation(optional double compensation);
    [Throws]
    readonly attribute unrestricted double   exposureCompensation;

    /* the function to call on the camera's shutter event, to trigger
       a shutter sound and/or a visual shutter indicator. */
    [Throws]
    attribute CameraShutterCallback? onShutter;

    /* the function to call when the camera hardware is closed
       by the underlying framework, e.g. when another app makes a more
       recent call to get the camera. */
    [Throws]
    attribute CameraClosedCallback? onClosed;

    /* the function to call when the recorder changes state, either because
       the recording process encountered an error, or because one of the
       recording limits (see CameraStartRecordingOptions) was reached. */
    [Throws]
    attribute CameraRecorderStateChange? onRecorderStateChange;
    attribute CameraPreviewStateChange? onPreviewStateChange;

    /* the size of the picture to be returned by a call to takePicture();
       an object with 'height' and 'width' properties that corresponds to
       one of the options returned by capabilities.pictureSizes. */
    [Throws]
    attribute any pictureSize;

    /* the size of the thumbnail to be included in the picture returned
       by a call to takePicture(), assuming the chose fileFormat supports
       one; an object with 'height' and 'width' properties that corresponds
       to one of the options returned by capabilities.pictureSizes.
       
       this setting should be considered a hint: the implementation will
       respect it when possible, and override it if necessary. */
    [Throws]
    attribute any thumbnailSize;

    /* the angle, in degrees, that the image sensor is mounted relative
       to the display; e.g. if 'sensorAngle' is 270 degrees (or -90 degrees),
       then the preview stream needs to be rotated +90 degrees to have the
       same orientation as the real world. */
    readonly attribute long sensorAngle;

    /* tell the camera to attempt to focus the image */
    [Throws]
    void autoFocus(CameraAutoFocusCallback onSuccess, optional CameraErrorCallback onError);

    /* capture an image and return it as a blob to the 'onSuccess' callback;
       if the camera supports it, this may be invoked while the camera is
       already recording video.

       invoking this function will stop the preview stream, which must be
       manually restarted (e.g. by calling .play() on it). */
    [Throws]
    void takePicture(CameraPictureOptions aOptions,
                     CameraTakePictureCallback onSuccess,
                     optional CameraErrorCallback onError);

    /* get a media stream to be used as a camera viewfinder in video mode;
       'aOptions' is an CameraRecorderOptions object. */
    [Throws]
    void getPreviewStreamVideoMode(any aOptions, CameraPreviewStreamCallback onSuccess, optional CameraErrorCallback onError);

    /* start recording video; 'aOptions' is a
       CameraStartRecordingOptions object. */
    [Throws]
    void startRecording(any aOptions, DeviceStorage storageArea, DOMString filename, CameraStartRecordingCallback onSuccess, optional CameraErrorCallback onError);

    /* stop precording video. */
    [Throws]
    void stopRecording();

    /* get a media stream to be used as a camera viewfinder; the options
       define the desired frame size of the preview, chosen from
       capabilities.previewSizes, e.g.:
        {
            height: 640,
            width:  480,
         }
    */
    [Throws]
    void getPreviewStream(any aOptions, CameraPreviewStreamCallback onSuccess, optional CameraErrorCallback onError);

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
    void release(optional CameraReleaseCallback onSuccess, optional CameraErrorCallback onError);
};
