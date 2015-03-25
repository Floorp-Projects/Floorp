/*
 * Copyright (C) 2013-2014 Mozilla Foundation
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

namespace android {

class TestGonkCameraHardware : public android::GonkCameraHardware
{
public:
  virtual int AutoFocus() override;
  virtual int StartFaceDetection() override;
  virtual int StopFaceDetection() override;
  virtual int TakePicture() override;
  virtual int StartPreview() override;
  virtual int PushParameters(const mozilla::GonkCameraParameters& aParams) override;
  virtual nsresult PullParameters(mozilla::GonkCameraParameters& aParams) override;
  virtual int StartRecording() override;
  virtual int StopRecording() override;
  virtual int SetListener(const sp<GonkCameraListener>& aListener) override;
  virtual int StoreMetaDataInBuffers(bool aEnabled) override;

  virtual int
  PushParameters(const CameraParameters& aParams) override
  {
    return GonkCameraHardware::PushParameters(aParams);
  }

  virtual void
  PullParameters(CameraParameters& aParams) override
  {
    GonkCameraHardware::PullParameters(aParams);
  }

  TestGonkCameraHardware(mozilla::nsGonkCameraControl* aTarget,
                         uint32_t aCameraId,
                         const sp<Camera>& aCamera);
  virtual ~TestGonkCameraHardware();

  virtual nsresult Init() override;

protected:
  const nsCString TestCase();
  const nsCString GetExtraParameters();
  bool IsTestCaseInternal(const char* aTest, const char* aFile, int aLine);
  int TestCaseError(int aDefaultError);

  int StartAutoFocusMoving(bool aIsMoving);
  void InjectFakeSystemFailure();

private:
  TestGonkCameraHardware(const TestGonkCameraHardware&) = delete;
  TestGonkCameraHardware& operator=(const TestGonkCameraHardware&) = delete;
};

#define IsTestCase(test)  IsTestCaseInternal((test), __FILE__, __LINE__)

} // namespace android

#endif // DOM_CAMERA_TESTGONKCAMERAHARDWARE_H
