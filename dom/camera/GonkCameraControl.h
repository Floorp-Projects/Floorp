/*
 * Copyright (C) 2012 Mozilla Foundation
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
#include "prrwlock.h"
#include <media/MediaProfiles.h>
#include "DeviceStorage.h"
#include "nsIDOMCameraManager.h"
#include "DOMCameraControl.h"
#include "CameraControlImpl.h"
#include "CameraCommon.h"
#include "GonkRecorder.h"
#include "GonkCameraHwMgr.h"

namespace android {
class GonkCameraHardware;
class MediaProfiles;
class GonkRecorder;
}

namespace mozilla {

namespace layers {
class GraphicBufferLocked;
}

class GonkRecorderProfile;
class GonkRecorderProfileManager;

class nsGonkCameraControl : public CameraControlImpl
{
public:
  nsGonkCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);
  void DispatchInit(nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);
  nsresult Init();

  const char* GetParameter(const char* aKey);
  const char* GetParameterConstChar(uint32_t aKey);
  double GetParameterDouble(uint32_t aKey);
  int32_t GetParameterInt32(uint32_t aKey);
  void GetParameter(uint32_t aKey, nsTArray<idl::CameraRegion>& aRegions);
  void GetParameter(uint32_t aKey, nsTArray<idl::CameraSize>& aSizes);
  void GetParameter(uint32_t aKey, idl::CameraSize& aSize);
  void SetParameter(const char* aKey, const char* aValue);
  void SetParameter(uint32_t aKey, const char* aValue);
  void SetParameter(uint32_t aKey, double aValue);
  void SetParameter(uint32_t aKey, const nsTArray<idl::CameraRegion>& aRegions);
  void SetParameter(uint32_t aKey, int aValue);
  void SetParameter(uint32_t aKey, const idl::CameraSize& aSize);
  nsresult GetVideoSizes(nsTArray<idl::CameraSize>& aVideoSizes);
  nsresult PushParameters();

  void AutoFocusComplete(bool aSuccess);
  void TakePictureComplete(uint8_t* aData, uint32_t aLength);
  void TakePictureError();
  void HandleRecorderEvent(int msg, int ext1, int ext2);

protected:
  ~nsGonkCameraControl();

  nsresult GetPreviewStreamImpl(GetPreviewStreamTask* aGetPreviewStream);
  nsresult StartPreviewImpl(StartPreviewTask* aStartPreview);
  nsresult StopPreviewImpl(StopPreviewTask* aStopPreview);
  nsresult StopPreviewInternal(bool aForced = false);
  nsresult AutoFocusImpl(AutoFocusTask* aAutoFocus);
  nsresult TakePictureImpl(TakePictureTask* aTakePicture);
  nsresult StartRecordingImpl(StartRecordingTask* aStartRecording);
  nsresult StopRecordingImpl(StopRecordingTask* aStopRecording);
  nsresult PushParametersImpl();
  nsresult PullParametersImpl();
  nsresult GetPreviewStreamVideoModeImpl(GetPreviewStreamVideoModeTask* aGetPreviewStreamVideoMode);
  nsresult ReleaseHardwareImpl(ReleaseHardwareTask* aReleaseHardware);
  already_AddRefed<RecorderProfileManager> GetRecorderProfileManagerImpl();
  already_AddRefed<GonkRecorderProfileManager> GetGonkRecorderProfileManager();

  nsresult SetupRecording(int aFd, int aRotation, int64_t aMaxFileSizeBytes, int64_t aMaxVideoLengthMs);
  nsresult SetupVideoMode(const nsAString& aProfile);
  void SetPreviewSize(uint32_t aWidth, uint32_t aHeight);
  void SetThumbnailSize(uint32_t aWidth, uint32_t aHeight);
  void UpdateThumbnailSize();
  void SetPictureSize(uint32_t aWidth, uint32_t aHeight);

  android::sp<android::GonkCameraHardware> mCameraHw;
  double                    mExposureCompensationMin;
  double                    mExposureCompensationStep;
  bool                      mDeferConfigUpdate;
  PRRWLock*                 mRwLock;
  android::CameraParameters mParams;
  uint32_t                  mWidth;
  uint32_t                  mHeight;
  uint32_t                  mLastPictureWidth;
  uint32_t                  mLastPictureHeight;
  uint32_t                  mLastThumbnailWidth;
  uint32_t                  mLastThumbnailHeight;

  enum {
    PREVIEW_FORMAT_UNKNOWN,
    PREVIEW_FORMAT_YUV420P,
    PREVIEW_FORMAT_YUV420SP
  };
  uint32_t                  mFormat;

  uint32_t                  mFps;
  uint32_t                  mDiscardedFrameCount;

  android::MediaProfiles*   mMediaProfiles;
  nsRefPtr<android::GonkRecorder> mRecorder;

  // camcorder profile settings for the desired quality level
  nsRefPtr<GonkRecorderProfileManager> mProfileManager;
  nsRefPtr<GonkRecorderProfile> mRecorderProfile;

  nsRefPtr<DeviceStorageFile> mVideoFile;

private:
  nsGonkCameraControl(const nsGonkCameraControl&) MOZ_DELETE;
  nsGonkCameraControl& operator=(const nsGonkCameraControl&) MOZ_DELETE;
};

// camera driver callbacks
void ReceiveImage(nsGonkCameraControl* gc, uint8_t* aData, uint32_t aLength);
void ReceiveImageError(nsGonkCameraControl* gc);
void AutoFocusComplete(nsGonkCameraControl* gc, bool aSuccess);
void ReceiveFrame(nsGonkCameraControl* gc, layers::GraphicBufferLocked* aBuffer);
void OnShutter(nsGonkCameraControl* gc);
void OnClosed(nsGonkCameraControl* gc);

} // namespace mozilla

#endif // DOM_CAMERA_GONKCAMERACONTROL_H
