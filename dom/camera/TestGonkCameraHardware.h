/*
 * Copyright (C) 2013-2015 Mozilla Foundation
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

#ifndef DOM_CAMERA_TESTGONKCAMERAHARDWARE_H
#define DOM_CAMERA_TESTGONKCAMERAHARDWARE_H

#include "GonkCameraHwMgr.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventListener.h"
#include "mozilla/CondVar.h"

namespace mozilla {

class TestGonkCameraHardware : public android::GonkCameraHardware
{
public:
  virtual nsresult Init() MOZ_OVERRIDE;
  virtual int AutoFocus() MOZ_OVERRIDE;
  virtual int StartFaceDetection() MOZ_OVERRIDE;
  virtual int StopFaceDetection() MOZ_OVERRIDE;
  virtual int TakePicture() MOZ_OVERRIDE;
  virtual void CancelTakePicture() MOZ_OVERRIDE;
  virtual int StartPreview() MOZ_OVERRIDE;
  virtual void StopPreview() MOZ_OVERRIDE;
  virtual int PushParameters(const mozilla::GonkCameraParameters& aParams) MOZ_OVERRIDE;
  virtual nsresult PullParameters(mozilla::GonkCameraParameters& aParams) MOZ_OVERRIDE;
  virtual int StartRecording() MOZ_OVERRIDE;
  virtual int StopRecording() MOZ_OVERRIDE;
  virtual int StoreMetaDataInBuffers(bool aEnabled) MOZ_OVERRIDE;
  virtual int PushParameters(const android::CameraParameters& aParams) MOZ_OVERRIDE;
  virtual void PullParameters(android::CameraParameters& aParams) MOZ_OVERRIDE;

  TestGonkCameraHardware(mozilla::nsGonkCameraControl* aTarget,
                         uint32_t aCameraId,
                         const android::sp<android::Camera>& aCamera);

protected:
  virtual ~TestGonkCameraHardware();

  class ControlMessage;
  class PushParametersDelegate;
  class PullParametersDelegate;

  nsresult WaitWhileRunningOnMainThread(nsRefPtr<ControlMessage> aRunnable);

  nsCOMPtr<nsIDOMEventListener> mDomListener;
  nsCOMPtr<nsIThread> mCameraThread;
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  nsresult mStatus;

private:
  TestGonkCameraHardware(const TestGonkCameraHardware&) = delete;
  TestGonkCameraHardware& operator=(const TestGonkCameraHardware&) = delete;
};

} // namespace mozilla

#endif // DOM_CAMERA_TESTGONKCAMERAHARDWARE_H
