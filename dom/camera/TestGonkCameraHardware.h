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
  virtual int AutoFocus() MOZ_OVERRIDE;
  virtual int TakePicture() MOZ_OVERRIDE;
  virtual int StartPreview() MOZ_OVERRIDE;
  virtual int PushParameters(const mozilla::GonkCameraParameters& aParams) MOZ_OVERRIDE;
  virtual nsresult PullParameters(mozilla::GonkCameraParameters& aParams) MOZ_OVERRIDE;
  virtual int StartRecording() MOZ_OVERRIDE;
  virtual int StopRecording() MOZ_OVERRIDE;
  virtual int SetListener(const sp<GonkCameraListener>& aListener) MOZ_OVERRIDE;
  virtual int StoreMetaDataInBuffers(bool aEnabled) MOZ_OVERRIDE;

  virtual int
  PushParameters(const CameraParameters& aParams) MOZ_OVERRIDE
  {
    return GonkCameraHardware::PushParameters(aParams);
  }

  virtual void
  PullParameters(CameraParameters& aParams) MOZ_OVERRIDE
  {
    GonkCameraHardware::PullParameters(aParams);
  }

  TestGonkCameraHardware(mozilla::nsGonkCameraControl* aTarget,
                         uint32_t aCameraId,
                         const sp<Camera>& aCamera);
  virtual ~TestGonkCameraHardware();

  virtual nsresult Init() MOZ_OVERRIDE;

protected:
  const nsCString TestCase();
  const nsCString GetExtraParameters();
  bool IsTestCaseInternal(const char* aTest, const char* aFile, int aLine);
  int TestCaseError(int aDefaultError);

private:
  TestGonkCameraHardware(const TestGonkCameraHardware&) MOZ_DELETE;
  TestGonkCameraHardware& operator=(const TestGonkCameraHardware&) MOZ_DELETE;
};

#define IsTestCase(test)  IsTestCaseInternal((test), __FILE__, __LINE__)

} // namespace android

#endif // DOM_CAMERA_TESTGONKCAMERAHARDWARE_H
