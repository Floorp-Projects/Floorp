/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERACONTROLLISTENER_H
#define DOM_CAMERA_CAMERACONTROLLISTENER_H

#include <stdint.h>
#include "ICameraControl.h"

namespace mozilla {

namespace layers {
  class Image;
}

class CameraControlListener
{
public:
  CameraControlListener()
  {
    MOZ_COUNT_CTOR(CameraControlListener);
  }

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~CameraControlListener()
  {
    MOZ_COUNT_DTOR(CameraControlListener);
  }

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CameraControlListener);

  enum HardwareState
  {
    kHardwareOpen,
    kHardwareClosed
  };
  virtual void OnHardwareStateChange(HardwareState aState) { }

  enum PreviewState
  {
    kPreviewStopped,
    kPreviewPaused,
    kPreviewStarted
  };
  virtual void OnPreviewStateChange(PreviewState aState) { }

  enum RecorderState
  {
    kRecorderStopped,
    kRecorderStarted,
#ifdef MOZ_B2G_CAMERA
    kFileSizeLimitReached,
    kVideoLengthLimitReached,
    kTrackCompleted,
    kTrackFailed,
    kMediaRecorderFailed,
    kMediaServerFailed
#endif
  };
  enum { kNoTrackNumber = -1 };
  virtual void OnRecorderStateChange(RecorderState aState, int32_t aStatus, int32_t aTrackNum) { }

  virtual void OnShutter() { }
  virtual bool OnNewPreviewFrame(layers::Image* aFrame, uint32_t aWidth, uint32_t aHeight)
  {
    return false;
  }

  class CameraListenerConfiguration : public ICameraControl::Configuration
  {
  public:
    uint32_t mMaxMeteringAreas;
    uint32_t mMaxFocusAreas;
  };
  virtual void OnConfigurationChange(const CameraListenerConfiguration& aConfiguration) { }

  virtual void OnAutoFocusComplete(bool aAutoFocusSucceeded) { }
  virtual void OnAutoFocusMoving(bool aIsMoving) { }
  virtual void OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType) { }
  virtual void OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces) { }

  enum CameraErrorContext
  {
    kInStartCamera,
    kInStopCamera,
    kInAutoFocus,
    kInStartFaceDetection,
    kInStopFaceDetection,
    kInTakePicture,
    kInStartRecording,
    kInStopRecording,
    kInSetConfiguration,
    kInStartPreview,
    kInStopPreview,
    kInResumeContinuousFocus,
    kInUnspecified
  };
  enum CameraError
  {
    kErrorApiFailed,
    kErrorInitFailed,
    kErrorInvalidConfiguration,
    kErrorServiceFailed,
    kErrorSetPictureSizeFailed,
    kErrorSetThumbnailSizeFailed,
    kErrorUnknown
  };
  virtual void OnError(CameraErrorContext aContext, CameraError aError) { }
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERACONTROLLISTENER_H
