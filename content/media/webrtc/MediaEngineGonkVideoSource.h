/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEngineGonkVideoSource_h_
#define MediaEngineGonkVideoSource_h_

#ifndef MOZ_B2G_CAMERA
#error MediaEngineGonkVideoSource is only available when MOZ_B2G_CAMERA is defined.
#endif

#include "CameraControlListener.h"
#include "MediaEngineCameraVideoSource.h"

#include "mozilla/Hal.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/dom/File.h"

namespace mozilla {

/**
 * The B2G implementation of the MediaEngine interface.
 *
 * On B2G platform, member data may accessed from different thread after construction:
 *
 * MediaThread:
 * mState, mImage, mWidth, mHeight, mCapability, mPrefs, mDeviceName, mUniqueId, mInitDone,
 * mSources, mImageContainer, mSources, mState, mImage, mLastCapture.
 *
 * CameraThread:
 * mDOMCameraControl, mCaptureIndex, mCameraThread, mWindowId, mCameraManager,
 * mNativeCameraControl, mPreviewStream, mState, mLastCapture, mWidth, mHeight
 *
 * Where mWidth, mHeight, mImage, mPhotoCallbacks, mRotation, mCameraAngle and
 * mBackCamera are protected by mMonitor (in parent MediaEngineCameraVideoSource)
 * mState, mLastCapture is protected by mCallbackMonitor
 * Other variable is accessed only from single thread
 */
class MediaEngineGonkVideoSource : public MediaEngineCameraVideoSource
                                 , public mozilla::hal::ScreenConfigurationObserver
                                 , public CameraControlListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  MediaEngineGonkVideoSource(int aIndex)
    : MediaEngineCameraVideoSource(aIndex, "GonkCamera.Monitor")
    , mCameraControl(nullptr)
    , mCallbackMonitor("GonkCamera.CallbackMonitor")
    , mRotation(0)
    , mBackCamera(false)
    , mOrientationChanged(true) // Correct the orientation at first time takePhoto.
    {
      Init();
    }

  virtual nsresult Allocate(const VideoTrackConstraintsN &aConstraints,
                            const MediaEnginePrefs &aPrefs) MOZ_OVERRIDE;
  virtual nsresult Deallocate() MOZ_OVERRIDE;
  virtual nsresult Start(SourceMediaStream* aStream, TrackID aID) MOZ_OVERRIDE;
  virtual nsresult Stop(SourceMediaStream* aSource, TrackID aID) MOZ_OVERRIDE;
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream* aSource,
                          TrackID aId,
                          StreamTime aDesiredTime,
                          TrackTicks& aLastEndTime) MOZ_OVERRIDE;

  void OnHardwareStateChange(HardwareState aState);
  void GetRotation();
  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void OnUserError(UserContext aContext, nsresult aError);
  void OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType);

  void AllocImpl();
  void DeallocImpl();
  void StartImpl(webrtc::CaptureCapability aCapability);
  void StopImpl();
  uint32_t ConvertPixelFormatToFOURCC(int aFormat);
  void RotateImage(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration);

  nsresult TakePhoto(PhotoCallback* aCallback) MOZ_OVERRIDE;

  // It sets the correct photo orientation via camera parameter according to
  // current screen orientation.
  nsresult UpdatePhotoOrientation();

protected:
  ~MediaEngineGonkVideoSource()
  {
    Shutdown();
  }
  // Initialize the needed Video engine interfaces.
  void Init();
  void Shutdown();
  void ChooseCapability(const VideoTrackConstraintsN& aConstraints,
                        const MediaEnginePrefs& aPrefs);

  mozilla::ReentrantMonitor mCallbackMonitor; // Monitor for camera callback handling
  // This is only modified on MainThread (AllocImpl and DeallocImpl)
  nsRefPtr<ICameraControl> mCameraControl;
  nsCOMPtr<nsIDOMFile> mLastCapture;

  // These are protected by mMonitor in parent class
  nsTArray<nsRefPtr<PhotoCallback>> mPhotoCallbacks;
  int mRotation;
  int mCameraAngle; // See dom/base/ScreenOrientation.h
  bool mBackCamera;
  bool mOrientationChanged; // True when screen rotates.
};

} // namespace mozilla

#endif // MediaEngineGonkVideoSource_h_
