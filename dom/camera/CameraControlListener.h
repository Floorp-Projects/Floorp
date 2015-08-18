/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERACONTROLLISTENER_H
#define DOM_CAMERA_CAMERACONTROLLISTENER_H

#include <stdint.h>
#include "ICameraControl.h"

namespace mozilla {

namespace dom {
  class BlobImpl;
} // namespace dom

namespace layers {
  class Image;
} // namespace layers

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
    kHardwareUninitialized,
    kHardwareClosed,
    kHardwareOpen,
    kHardwareOpenFailed
  };
  // aReason:
  //    NS_OK : state change was expected and normal;
  //    NS_ERROR_FAILURE : one or more system-level components failed and
  //                       the camera was closed;
  //    NS_ERROR_NOT_AVAILABLE : the hardware is in use by another process
  //                             and cannot be acquired, or another process
  //                             was given access to the camera hardware.
  virtual void OnHardwareStateChange(HardwareState aState, nsresult aReason) { }

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
    kRecorderPaused,
    kRecorderResumed,
    kPosterCreated,
    kPosterFailed,
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
  virtual void OnRateLimitPreview(bool aLimit) { }
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
  virtual void OnTakePictureComplete(const uint8_t* aData, uint32_t aLength, const nsAString& aMimeType) { }
  virtual void OnFacesDetected(const nsTArray<ICameraControl::Face>& aFaces) { }
  virtual void OnPoster(dom::BlobImpl* aBlobImpl) { }

  enum UserContext
  {
    kInStartCamera,
    kInStopCamera,
    kInAutoFocus,
    kInStartFaceDetection,
    kInStopFaceDetection,
    kInTakePicture,
    kInStartRecording,
    kInStopRecording,
    kInPauseRecording,
    kInResumeRecording,
    kInSetConfiguration,
    kInStartPreview,
    kInStopPreview,
    kInSetPictureSize,
    kInSetThumbnailSize,
    kInResumeContinuousFocus,
    kInUnspecified
  };
  // Error handler for problems arising due to user-initiated actions.
  virtual void OnUserError(UserContext aContext, nsresult aError) { }

  enum SystemContext
  {
    kSystemService
  };
  // Error handler for problems arising due to system failures, not triggered
  // by something the CameraControl API user did.
  virtual void OnSystemError(SystemContext aContext, nsresult aError) { }
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERACONTROLLISTENER_H
