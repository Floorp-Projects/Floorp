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
#include "nsIDOMCameraManager.h"
#include "DOMCameraControl.h"
#include "CameraControlImpl.h"
#include "CameraCommon.h"
#include "GonkRecorder.h"

namespace mozilla {

namespace layers {
class GraphicBufferLocked;
}

class nsGonkCameraControl : public CameraControlImpl
{
public:
  nsGonkCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId);
  nsresult Init();

  const char* GetParameter(const char* aKey);
  const char* GetParameterConstChar(uint32_t aKey);
  double GetParameterDouble(uint32_t aKey);
  void GetParameter(uint32_t aKey, nsTArray<dom::CameraRegion>& aRegions);
  void SetParameter(const char* aKey, const char* aValue);
  void SetParameter(uint32_t aKey, const char* aValue);
  void SetParameter(uint32_t aKey, double aValue);
  void SetParameter(uint32_t aKey, const nsTArray<dom::CameraRegion>& aRegions);
  nsresult PushParameters();

  nsresult SetupRecording(int aFd);
  nsresult SetupVideoMode();

  void AutoFocusComplete(bool aSuccess);
  void TakePictureComplete(uint8_t* aData, uint32_t aLength);

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

  void SetPreviewSize(uint32_t aWidth, uint32_t aHeight);

  uint32_t                  mHwHandle;
  double                    mExposureCompensationMin;
  double                    mExposureCompensationStep;
  bool                      mDeferConfigUpdate;
  PRRWLock*                 mRwLock;
  android::CameraParameters mParams;
  uint32_t                  mWidth;
  uint32_t                  mHeight;

  enum {
    PREVIEW_FORMAT_UNKNOWN,
    PREVIEW_FORMAT_YUV420P,
    PREVIEW_FORMAT_YUV420SP
  };
  uint32_t                  mFormat;

  uint32_t                  mFps;
  uint32_t                  mDiscardedFrameCount;

  android::MediaProfiles*   mMediaProfiles;
  android::GonkRecorder*    mRecorder;

  uint32_t                  mVideoRotation;
  uint32_t                  mVideoWidth;
  uint32_t                  mVideoHeight;
  nsString                  mVideoFile;

  // camcorder profile settings for the desired quality level
  int mDuration;        // max recording duration (ignored)
  int mVideoFileFormat; // output file format
  int mVideoCodec;      // video encoder
  int mVideoBitRate;    // video bit rate
  int mVideoFrameRate;  // video frame rate
  int mVideoFrameWidth; // video frame width
  int mVideoFrameHeight;// video frame height
  int mAudioCodec;      // audio encoder
  int mAudioBitRate;    // audio bit rate
  int mAudioSampleRate; // audio sample rate
  int mAudioChannels;   // number of audio channels

private:
  nsGonkCameraControl(const nsGonkCameraControl&) MOZ_DELETE;
  nsGonkCameraControl& operator=(const nsGonkCameraControl&) MOZ_DELETE;
};

// camera driver callbacks
void ReceiveImage(nsGonkCameraControl* gc, uint8_t* aData, uint32_t aLength);
void AutoFocusComplete(nsGonkCameraControl* gc, bool aSuccess);
void ReceiveFrame(nsGonkCameraControl* gc, layers::GraphicBufferLocked* aBuffer);
void OnShutter(nsGonkCameraControl* gc);
void OnClosed(nsGonkCameraControl* gc);

} // namespace mozilla

#endif // DOM_CAMERA_GONKCAMERACONTROL_H
