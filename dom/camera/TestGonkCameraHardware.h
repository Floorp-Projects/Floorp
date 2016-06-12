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
#include "nsIDOMEventListener.h"
#include "mozilla/CondVar.h"

namespace mozilla {

class TestGonkCameraHardware : public android::GonkCameraHardware
{
#ifndef MOZ_WIDGET_GONK
  NS_DECL_ISUPPORTS_INHERITED
#endif

public:
  virtual nsresult Init() override;
  virtual int AutoFocus() override;
  virtual int CancelAutoFocus() override;
  virtual int StartFaceDetection() override;
  virtual int StopFaceDetection() override;
  virtual int TakePicture() override;
  virtual void CancelTakePicture() override;
  virtual int StartPreview() override;
  virtual void StopPreview() override;
  virtual int PushParameters(const mozilla::GonkCameraParameters& aParams) override;
  virtual nsresult PullParameters(mozilla::GonkCameraParameters& aParams) override;
  virtual int StartRecording() override;
  virtual int StopRecording() override;
  virtual int StoreMetaDataInBuffers(bool aEnabled) override;
#ifdef MOZ_WIDGET_GONK
  virtual int PushParameters(const android::CameraParameters& aParams) override;
  virtual void PullParameters(android::CameraParameters& aParams) override;
#endif

  TestGonkCameraHardware(mozilla::nsGonkCameraControl* aTarget,
                         uint32_t aCameraId,
                         const android::sp<android::Camera>& aCamera);

protected:
  virtual ~TestGonkCameraHardware();

  class ControlMessage;
  class PushParametersDelegate;
  class PullParametersDelegate;

  nsresult WaitWhileRunningOnMainThread(RefPtr<ControlMessage> aRunnable);

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
