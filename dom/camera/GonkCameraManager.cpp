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

#include "ICameraControl.h"

#include <camera/Camera.h>

#include "CameraCommon.h"
#include "GonkCameraControl.h"
#include "mozilla/Preferences.h"
#include "TestGonkCameraControl.h"

using namespace mozilla;

// From ICameraControl, gonk-specific management functions
nsresult
ICameraControl::GetNumberOfCameras(int32_t& aDeviceCount)
{
  aDeviceCount = android::Camera::getNumberOfCameras();
  return NS_OK;
}

nsresult
ICameraControl::GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName)
{
  int32_t count = android::Camera::getNumberOfCameras();
  int32_t deviceNum = static_cast<int32_t>(aDeviceNum);

  DOM_CAMERA_LOGI("GetCameraName : getNumberOfCameras() returned %d\n", count);
  if (deviceNum < 0 || deviceNum > count) {
    DOM_CAMERA_LOGE("GetCameraName : invalid device number (%u)\n", aDeviceNum);
    return NS_ERROR_INVALID_ARG;
  }

  android::CameraInfo info;
  int rv = android::Camera::getCameraInfo(deviceNum, &info);
  if (rv != 0) {
    DOM_CAMERA_LOGE("GetCameraName : get_camera_info(%d) failed: %d\n", deviceNum, rv);
    return NS_ERROR_NOT_AVAILABLE;
  }

  switch (info.facing) {
    case CAMERA_FACING_BACK:
      aDeviceName.AssignLiteral("back");
      break;

    case CAMERA_FACING_FRONT:
      aDeviceName.AssignLiteral("front");
      break;

    default:
      aDeviceName.AssignLiteral("extra-camera-");
      aDeviceName.AppendInt(deviceNum);
      break;
  }
  return NS_OK;
}

nsresult
ICameraControl::GetListOfCameras(nsTArray<nsString>& aList)
{
  int32_t count = android::Camera::getNumberOfCameras();
  DOM_CAMERA_LOGI("getListOfCameras : getNumberOfCameras() returned %d\n", count);
  if (count <= 0) {
    aList.Clear();
    return NS_OK;
  }

  // Allocate 2 extra slots to reserve space for 'front' and 'back' cameras
  // at the front of the array--we will collapse any empty slots below.
  aList.SetLength(2);
  uint32_t extraIdx = 2;
  bool gotFront = false;
  bool gotBack = false;
  while (count--) {
    nsCString cameraName;
    nsresult result = GetCameraName(count, cameraName);
    if (result != NS_OK) {
      continue;
    }

    // The first camera we find named 'back' gets slot 0; and the first
    // we find named 'front' gets slot 1.  All others appear after these.
    if (cameraName.EqualsLiteral("back")) {
      CopyUTF8toUTF16(cameraName, aList[0]);
      gotBack = true;
    } else if (cameraName.EqualsLiteral("front")) {
      CopyUTF8toUTF16(cameraName, aList[1]);
      gotFront = true;
    } else {
      CopyUTF8toUTF16(cameraName, *aList.InsertElementAt(extraIdx));
      extraIdx++;
    }
  }

  if (!gotFront) {
    aList.RemoveElementAt(1);
  }

  if (!gotBack) {
    aList.RemoveElementAt(0);
  }

  return NS_OK;
}

static const char* sTestModeEnabled = "camera.control.test.enabled";

// implementation-specific camera factory
already_AddRefed<ICameraControl>
ICameraControl::Create(uint32_t aCameraId)
{
  const nsAdoptingCString& test = Preferences::GetCString(sTestModeEnabled);
  nsRefPtr<nsGonkCameraControl> control;
  if (test.EqualsASCII("control")) {
    NS_WARNING("Using test CameraControl layer");
    control = new TestGonkCameraControl(aCameraId);
  } else {
    control = new nsGonkCameraControl(aCameraId);
  }
  return control.forget();
}
