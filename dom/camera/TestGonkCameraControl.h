/*
 * Copyright (C) 2014 Mozilla Foundation
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

#ifndef DOM_CAMERA_TESTGONKCAMERACONTROL_H
#define DOM_CAMERA_TESTGONKCAMERACONTROL_H

#include "GonkCameraControl.h"

namespace mozilla {

class TestGonkCameraControl : public nsGonkCameraControl
{
public:
  TestGonkCameraControl(uint32_t aCameraId);

  virtual nsresult Start(const Configuration* aConfig = nullptr) override;
  virtual nsresult Stop() override;
  virtual nsresult SetConfiguration(const Configuration& aConfig) override;
  virtual nsresult StartPreview() override;
  virtual nsresult StopPreview() override;
  virtual nsresult AutoFocus() override;
  virtual nsresult StartFaceDetection() override;
  virtual nsresult StopFaceDetection() override;
  virtual nsresult TakePicture() override;
  virtual nsresult StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                  const StartRecordingOptions* aOptions) override;
  virtual nsresult StopRecording() override;

protected:
  virtual ~TestGonkCameraControl();

  virtual nsresult StartImpl(const Configuration* aInitialConfig = nullptr) override;
  virtual nsresult StopImpl() override;
  virtual nsresult SetConfigurationImpl(const Configuration& aConfig) override;
  virtual nsresult StartPreviewImpl() override;
  virtual nsresult StopPreviewImpl() override;
  virtual nsresult AutoFocusImpl() override;
  virtual nsresult StartFaceDetectionImpl() override;
  virtual nsresult StopFaceDetectionImpl() override;
  virtual nsresult TakePictureImpl() override;
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) override;
  virtual nsresult StopRecordingImpl() override;

  nsresult ForceMethodFailWithCodeInternal(const char* aFile, int aLine);
  nsresult ForceAsyncFailWithCodeInternal(const char* aFile, int aLine);

private:
  TestGonkCameraControl(const TestGonkCameraControl&) = delete;
  TestGonkCameraControl& operator=(const TestGonkCameraControl&) = delete;
};

#define ForceMethodFailWithCode() ForceMethodFailWithCodeInternal(__FILE__, __LINE__)
#define ForceAsyncFailWithCode()  ForceAsyncFailWithCodeInternal(__FILE__, __LINE__)

} // namespace mozilla

#endif // DOM_CAMERA_TESTGONKCAMERACONTROL_H
