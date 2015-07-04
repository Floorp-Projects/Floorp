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
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "GonkCameraSource.h"

namespace android {
class MOZ_EXPORT MediaBuffer;
}

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
    , mCallbackMonitor("GonkCamera.CallbackMonitor")
    , mCameraControl(nullptr)
    , mRotation(0)
    , mBackCamera(false)
    , mOrientationChanged(true) // Correct the orientation at first time takePhoto.
    {
      Init();
    }

  virtual nsresult Allocate(const dom::MediaTrackConstraints &aConstraints,
                            const MediaEnginePrefs &aPrefs,
                            const nsString& aDeviceId) override;
  virtual nsresult Deallocate() override;
  virtual nsresult Start(SourceMediaStream* aStream, TrackID aID) override;
  virtual nsresult Stop(SourceMediaStream* aSource, TrackID aID) override;
  virtual void NotifyPull(MediaStreamGraph* aGraph,
                          SourceMediaStream* aSource,
                          TrackID aId,
                          StreamTime aDesiredTime) override;

  void OnHardwareStateChange(HardwareState aState, nsresult aReason) override;
  void GetRotation();
  bool OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight) override;
  void OnUserError(UserContext aContext, nsresult aError) override;
  void OnTakePictureComplete(const uint8_t* aData, uint32_t aLength, const nsAString& aMimeType) override;

  void AllocImpl();
  void DeallocImpl();
  void StartImpl(webrtc::CaptureCapability aCapability);
  void StopImpl();
  uint32_t ConvertPixelFormatToFOURCC(int aFormat);
  void RotateImage(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight);
  void Notify(const mozilla::hal::ScreenConfiguration& aConfiguration);

  nsresult TakePhoto(PhotoCallback* aCallback) override;

  // It sets the correct photo orientation via camera parameter according to
  // current screen orientation.
  nsresult UpdatePhotoOrientation();

  // It adds aBuffer to current preview image and sends this image to MediaStreamDirectListener
  // via AppendToTrack(). Due to MediaBuffer is limited resource, it will clear
  // image's MediaBuffer by calling GonkCameraImage::ClearMediaBuffer() before leaving
  // this function.
  nsresult OnNewMediaBufferFrame(android::MediaBuffer* aBuffer);

protected:
  ~MediaEngineGonkVideoSource()
  {
    Shutdown();
  }
  // Initialize the needed Video engine interfaces.
  void Init();
  void Shutdown();
  size_t NumCapabilities() override;
  // Initialize the recording frame (MediaBuffer) callback and Gonk camera.
  // MediaBuffer will transfers to MediaStreamGraph via AppendToTrack.
  nsresult InitDirectMediaBuffer();

  mozilla::ReentrantMonitor mCallbackMonitor; // Monitor for camera callback handling
  // This is only modified on MainThread (AllocImpl and DeallocImpl)
  nsRefPtr<ICameraControl> mCameraControl;
  nsRefPtr<dom::File> mLastCapture;

  android::sp<android::GonkCameraSource> mCameraSource;

  // These are protected by mMonitor in parent class
  nsTArray<nsRefPtr<PhotoCallback>> mPhotoCallbacks;
  int mRotation;
  int mCameraAngle; // See dom/base/ScreenOrientation.h
  bool mBackCamera;
  bool mOrientationChanged; // True when screen rotates.

  RefPtr<layers::TextureClientRecycleAllocator> mTextureClientAllocator;
};

} // namespace mozilla

#endif // MediaEngineGonkVideoSource_h_
