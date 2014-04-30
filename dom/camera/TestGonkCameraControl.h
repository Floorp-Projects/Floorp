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

  virtual nsresult Start(const Configuration* aConfig = nullptr) MOZ_OVERRIDE;
  virtual nsresult Stop() MOZ_OVERRIDE;
  virtual nsresult SetConfiguration(const Configuration& aConfig) MOZ_OVERRIDE;
  virtual nsresult StartPreview() MOZ_OVERRIDE;
  virtual nsresult StopPreview() MOZ_OVERRIDE;
  virtual nsresult AutoFocus() MOZ_OVERRIDE;
  virtual nsresult StartFaceDetection() MOZ_OVERRIDE;
  virtual nsresult StopFaceDetection() MOZ_OVERRIDE;
  virtual nsresult TakePicture() MOZ_OVERRIDE;
  virtual nsresult StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                  const StartRecordingOptions* aOptions) MOZ_OVERRIDE;
  virtual nsresult StopRecording() MOZ_OVERRIDE;

protected:
  virtual ~TestGonkCameraControl();

  virtual nsresult StartImpl(const Configuration* aInitialConfig = nullptr) MOZ_OVERRIDE;
  virtual nsresult StopImpl() MOZ_OVERRIDE;
  virtual nsresult SetConfigurationImpl(const Configuration& aConfig) MOZ_OVERRIDE;
  virtual nsresult StartPreviewImpl() MOZ_OVERRIDE;
  virtual nsresult StopPreviewImpl() MOZ_OVERRIDE;
  virtual nsresult AutoFocusImpl() MOZ_OVERRIDE;
  virtual nsresult StartFaceDetectionImpl() MOZ_OVERRIDE;
  virtual nsresult StopFaceDetectionImpl() MOZ_OVERRIDE;
  virtual nsresult TakePictureImpl() MOZ_OVERRIDE;
  virtual nsresult StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions = nullptr) MOZ_OVERRIDE;
  virtual nsresult StopRecordingImpl() MOZ_OVERRIDE;

  nsresult ForceMethodFailWithCodeInternal(const char* aFile, int aLine);
  nsresult ForceAsyncFailWithCodeInternal(const char* aFile, int aLine);

private:
  TestGonkCameraControl(const TestGonkCameraControl&) MOZ_DELETE;
  TestGonkCameraControl& operator=(const TestGonkCameraControl&) MOZ_DELETE;
};

#define ForceMethodFailWithCode() ForceMethodFailWithCodeInternal(__FILE__, __LINE__)
#define ForceAsyncFailWithCode()  ForceAsyncFailWithCodeInternal(__FILE__, __LINE__)

} // namespace mozilla

#endif // DOM_CAMERA_TESTGONKCAMERACONTROL_H
