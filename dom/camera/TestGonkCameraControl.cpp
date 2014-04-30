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

#include "TestGonkCameraControl.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

static const char* sMethodErrorOverride = "camera.control.test.method.error";
static const char* sAsyncErrorOverride = "camera.control.test.async.error";

TestGonkCameraControl::TestGonkCameraControl(uint32_t aCameraId)
  : nsGonkCameraControl(aCameraId)
{
  DOM_CAMERA_LOGA("v===== Created TestGonkCameraControl =====v\n");
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
}

TestGonkCameraControl::~TestGonkCameraControl()
{
  DOM_CAMERA_LOGA("^===== Destroyed TestGonkCameraControl =====^\n");
}

nsresult
TestGonkCameraControl::ForceMethodFailWithCodeInternal(const char* aFile, int aLine)
{
  nsresult rv =
    static_cast<nsresult>(Preferences::GetInt(sMethodErrorOverride,
                                              static_cast<int32_t>(NS_OK)));
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGI("[%s:%d] CameraControl method error override: 0x%x\n",
      aFile, aLine, rv);
  }
  return rv;
}

nsresult
TestGonkCameraControl::ForceAsyncFailWithCodeInternal(const char* aFile, int aLine)
{
  nsresult rv =
    static_cast<nsresult>(Preferences::GetInt(sAsyncErrorOverride,
                                              static_cast<int32_t>(NS_OK)));
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGI("[%s:%d] CameraControl async error override: 0x%x\n",
      aFile, aLine, rv);
  }
  return rv;
}

nsresult
TestGonkCameraControl::Start(const Configuration* aConfig)
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::Start(aConfig);
}

nsresult
TestGonkCameraControl::StartImpl(const Configuration* aInitialConfig)
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartImpl(aInitialConfig);
}

nsresult
TestGonkCameraControl::Stop()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::Stop();
}

nsresult
TestGonkCameraControl::StopImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopImpl();
}

nsresult
TestGonkCameraControl::SetConfiguration(const Configuration& aConfig)
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::SetConfiguration(aConfig);
}

nsresult
TestGonkCameraControl::SetConfigurationImpl(const Configuration& aConfig)
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::SetConfigurationImpl(aConfig);
}

nsresult
TestGonkCameraControl::StartPreview()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartPreview();
}

nsresult
TestGonkCameraControl::StartPreviewImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartImpl();
}

nsresult
TestGonkCameraControl::StopPreview()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopPreview();
}

nsresult
TestGonkCameraControl::StopPreviewImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartImpl();
}

nsresult
TestGonkCameraControl::AutoFocus()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::AutoFocus();
}

nsresult
TestGonkCameraControl::AutoFocusImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::AutoFocusImpl();
}

nsresult
TestGonkCameraControl::StartFaceDetection()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartFaceDetection();
}

nsresult
TestGonkCameraControl::StartFaceDetectionImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartFaceDetectionImpl();
}

nsresult
TestGonkCameraControl::StopFaceDetection()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopFaceDetection();
}

nsresult
TestGonkCameraControl::StopFaceDetectionImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopFaceDetectionImpl();
}

nsresult
TestGonkCameraControl::TakePicture()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::TakePicture();
}

nsresult
TestGonkCameraControl::TakePictureImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::TakePictureImpl();
}

nsresult
TestGonkCameraControl::StartRecording(DeviceStorageFileDescriptor* aFileDescriptor,
                                      const StartRecordingOptions* aOptions)
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartRecording(aFileDescriptor, aOptions);
}

nsresult
TestGonkCameraControl::StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                          const StartRecordingOptions* aOptions)
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StartRecordingImpl(aFileDescriptor, aOptions);
}

nsresult
TestGonkCameraControl::StopRecording()
{
  nsresult rv = ForceMethodFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopRecording();
}

nsresult
TestGonkCameraControl::StopRecordingImpl()
{
  nsresult rv = ForceAsyncFailWithCode();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return nsGonkCameraControl::StopRecordingImpl();
}
