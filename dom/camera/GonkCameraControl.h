/*
 * Copyright (C) 2012-2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_CAMERA_GONKCAMERACONTROL_H
#define DOM_CAMERA_GONKCAMERACONTROL_H

#include "base/basictypes.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/ReentrantMonitor.h"
#include "DeviceStorage.h"
#include "CameraControlImpl.h"
#include "CameraCommon.h"
#include "GonkCameraHwMgr.h"
#include "GonkCameraParameters.h"

#ifdef MOZ_WIDGET_GONK
#include <media/MediaProfiles.h>
#include <camera/Camera.h>
#include "GonkRecorder.h"
#else
#include "FallbackCameraPlatform.h"
#endif

class nsITimer;

namespace android {
  class GonkCameraHardware;
  class GonkRecorder;
  class GonkCameraSource;
}

namespace mozilla {

namespace layers {
  class TextureClient;
  class ImageContainer;
}

class GonkRecorderProfile;
class GonkRecorderProfileManager;

class nsGonkCameraControl : public CameraControlImpl
{
public:
  nsGonkCameraControl(uint32_t aCameraId);

  void OnAutoFocusMoving(bool aIsMoving);
  void OnAutoFocusComplete(bool aSuccess, bool aExpired);
  void OnFacesDetected(camera_frame_metadata_t* aMetaData);
  void OnTakePictureComplete(uint8_t* aData, uint32_t aLength);
  void OnTakePictureError();
  void OnRateLimitPreview(bool aLimit);
  void OnNewPreviewFrame(layers::TextureClient* aBuffer);
#ifdef MOZ_WIDGET_GONK
  void OnRecorderEvent(int msg, int ext1, int ext2);
#endif
  void OnSystemError(CameraControlListener::SystemContext aWhere, nsresult aError);

  // See ICameraControl.h for getter/setter return values.
  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) override;
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) override;
  virtual nsresult Set(uint32_t aKey, double aValue) override;
  virtual nsresult Get(uint32_t aKey, double& aValue) override;
  virtual nsresult Set(uint32_t aKey, int32_t aValue) override;
  virtual nsresult Get(uint32_t aKey, int32_t& aValue) override;
  virtual nsresult Set(uint32_t aKey, int64_t aValue) override;
  virtual nsresult Get(uint32_t aKey, int64_t& aValue) override;
  virtual nsresult Set(uint32_t aKey, bool aValue) override;
  virtual nsresult Get(uint32_t aKey, bool& aValue) override;
  virtual nsresult Set(uint32_t aKey, const Size& aValue) override;
  virtual nsresult Get(uint32_t aKey, Size& aValue) override;
  virtual nsresult Set(uint32_t aKey, const nsTArray<Region>& aRegions) override;
  virtual nsresult Get(uint32_t aKey, nsTArray<Region>& aRegions) override;

  virtual nsresult SetLocation(const Position& aLocation) override;

  virtual nsresult Get(uint32_t aKey, nsTArray<Size>& aSizes) override;
  virtual nsresult Get(uint32_t aKey, nsTArray<nsString>& aValues) override;
  virtual nsresult Get(uint32_t aKey, nsTArray<double>& aValues) override;

  virtual nsresult GetRecorderProfiles(nsTArray<nsString>& aProfiles) override;
  virtual ICameraControl::RecorderProfile* 
    GetProfileInfo(const nsAString& aProfile) override;

  nsresult PushParameters();
  nsresult PullParameters();

protected:
  ~nsGonkCameraControl();

  using CameraControlImpl::OnRateLimitPreview;
  using CameraControlImpl::OnNewPreviewFrame;
  using CameraControlImpl::OnAutoFocusComplete;
  using CameraControlImpl::OnFacesDetected;
  using CameraControlImpl::OnTakePictureComplete;
  using CameraControlImpl::OnConfigurationChange;
  using CameraControlImpl::OnUserError;

  typedef nsTArray<Size>::index_type SizeIndex;

  virtual void BeginBatchParameterSet() override;
  virtual void EndBatchParameterSet() override;

  nsresult Initialize();

  nsresult ValidateConfiguration(const Configuration& aConfig, Configuration& aValidatedConfig);
  nsresult SetConfigurationInternal(const Configuration& aConfig);
  nsresult SetPictureConfiguration(const Configuration& aConfig);
  nsresult SetVideoConfiguration(const Configuration& aConfig);
  nsresult StartInternal(const Configuration* aInitialConfig);
  nsresult StartPreviewInternal();
  nsresult StopInternal();

  template<class T> nsresult SetAndPush(uint32_t aKey, const T& aValue);

  // See CameraControlImpl.h for these methods' return values.
  virtual nsresult StartImpl(const Configuration* aInitialConfig = nullptr) override;
  virtual nsresult SetConfigurationImpl(const Configuration& aConfig) override;
  virtual nsresult StopImpl() override;
  virtual nsresult StartPreviewImpl() override;
  virtual nsresult StopPreviewImpl() override;
  virtual nsresult AutoFocusImpl() override;
  virtual nsresult StartFaceDetectionImpl() override;
  virtual nsresult StopFaceDetectionImpl() override;
  virtual nsresult TakePictureImpl() override;
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) override;
  virtual nsresult StopRecordingImpl() override;
  virtual nsresult ResumeContinuousFocusImpl() override;
  virtual nsresult PushParametersImpl() override;
  virtual nsresult PullParametersImpl() override;

  nsresult SetupRecording(int aFd, int aRotation, uint64_t aMaxFileSizeBytes,
                          uint64_t aMaxVideoLengthMs);
  nsresult SetupRecordingFlash(bool aAutoEnableLowLightTorch);
  nsresult SelectCaptureAndPreviewSize(const Size& aPreviewSize, const Size& aCaptureSize,
                                       const Size& aMaxSize, uint32_t aCaptureSizeKey);
  nsresult MaybeAdjustVideoSize();
  nsresult PausePreview();
  nsresult GetSupportedSize(const Size& aSize, const nsTArray<Size>& supportedSizes, Size& best);

  nsresult LoadRecorderProfiles();
  static PLDHashOperator Enumerate(const nsAString& aProfileName,
                                   RecorderProfile* aProfile,
                                   void* aUserArg);

  friend class SetPictureSize;
  friend class SetThumbnailSize;
  nsresult SetPictureSize(const Size& aSize);
  nsresult SetPictureSizeImpl(const Size& aSize);
  nsresult SetThumbnailSize(const Size& aSize);
  nsresult UpdateThumbnailSize();
  nsresult SetThumbnailSizeImpl(const Size& aSize);

  friend class android::GonkCameraSource;
  android::sp<android::GonkCameraHardware> GetCameraHw();

  int32_t RationalizeRotation(int32_t aRotation);

  uint32_t                  mCameraId;

  android::sp<android::GonkCameraHardware> mCameraHw;

  Size                      mLastThumbnailSize;
  Size                      mLastRecorderSize;
  uint32_t                  mPreviewFps;
  bool                      mResumePreviewAfterTakingPicture;
  bool                      mFlashSupported;
  bool                      mLuminanceSupported;
  bool                      mAutoFlashModeOverridden;
  bool                      mSeparateVideoAndPreviewSizesSupported;
  Atomic<uint32_t>          mDeferConfigUpdate;
  GonkCameraParameters      mParams;

  nsRefPtr<mozilla::layers::ImageContainer> mImageContainer;

#ifdef MOZ_WIDGET_GONK
  nsRefPtr<android::GonkRecorder> mRecorder;
#endif
  // Touching mRecorder happens inside this monitor because the destructor
  // can run on any thread, and we need to be able to clean up properly if
  // GonkCameraControl goes away.
  ReentrantMonitor          mRecorderMonitor;

  // Supported recorder profiles
  nsRefPtrHashtable<nsStringHashKey, RecorderProfile> mRecorderProfiles;

  nsRefPtr<DeviceStorageFile> mVideoFile;
  nsString                  mFileFormat;

  bool                      mAutoFocusPending;
  nsCOMPtr<nsITimer>        mAutoFocusCompleteTimer;
  int32_t                   mAutoFocusCompleteExpired;

  // Guards against calling StartPreviewImpl() while in OnTakePictureComplete().
  ReentrantMonitor          mReentrantMonitor;

private:
  nsGonkCameraControl(const nsGonkCameraControl&) = delete;
  nsGonkCameraControl& operator=(const nsGonkCameraControl&) = delete;
};

// camera driver callbacks
void OnRateLimitPreview(nsGonkCameraControl* gc, bool aLimit);
void OnTakePictureComplete(nsGonkCameraControl* gc, uint8_t* aData, uint32_t aLength);
void OnTakePictureError(nsGonkCameraControl* gc);
void OnAutoFocusComplete(nsGonkCameraControl* gc, bool aSuccess);
void OnAutoFocusMoving(nsGonkCameraControl* gc, bool aIsMoving);
void OnFacesDetected(nsGonkCameraControl* gc, camera_frame_metadata_t* aMetaData);
void OnNewPreviewFrame(nsGonkCameraControl* gc, layers::TextureClient* aBuffer);
void OnShutter(nsGonkCameraControl* gc);
void OnSystemError(nsGonkCameraControl* gc,
                   CameraControlListener::SystemContext aWhere,
                   int32_t aArg1, int32_t aArg2);

} // namespace mozilla

#endif // DOM_CAMERA_GONKCAMERACONTROL_H
