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

#include <camera/Camera.h>

#include "jsapi.h"
#include "GonkCameraControl.h"
#include "DOMCameraManager.h"
#include "CameraCommon.h"

// From nsDOMCameraManager, but gonk-specific!
nsresult
nsDOMCameraManager::GetNumberOfCameras(int32_t& aDeviceCount)
{
  aDeviceCount = android::Camera::getNumberOfCameras();
  return NS_OK;
}

nsresult
nsDOMCameraManager::GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName)
{
  int32_t count = android::Camera::getNumberOfCameras();
  DOM_CAMERA_LOGI("GetCameraName : getNumberOfCameras() returned %d\n", count);
  if (aDeviceNum > count) {
    DOM_CAMERA_LOGE("GetCameraName : invalid device number");
    return NS_ERROR_NOT_AVAILABLE;
  }

  android::CameraInfo info;
  int rv = android::Camera::getCameraInfo(aDeviceNum, &info);
  if (rv != 0) {
    DOM_CAMERA_LOGE("GetCameraName : get_camera_info(%d) failed: %d\n", aDeviceNum, rv);
    return NS_ERROR_NOT_AVAILABLE;
  }

  switch (info.facing) {
    case CAMERA_FACING_BACK:
      aDeviceName.Assign("back");
      break;

    case CAMERA_FACING_FRONT:
      aDeviceName.Assign("front");
      break;

    default:
      aDeviceName.Assign("extra-camera-");
      aDeviceName.AppendInt(aDeviceNum);
      break;
  }
  return NS_OK;
}

/* void getListOfCameras ([optional] out unsigned long aCount, [array, size_is (aCount), retval] out string aCameras); */
NS_IMETHODIMP
nsDOMCameraManager::GetListOfCameras(uint32_t *aCount, char * **aCameras)
{
  int32_t count = android::Camera::getNumberOfCameras();

  DOM_CAMERA_LOGI("GetListOfCameras : getNumberOfCameras() returned %d\n", count);
  if (count < 1) {
    *aCameras = nullptr;
    *aCount = 0;
    return NS_OK;
  }

  // Allocate 2 extra slots to reserve space for 'front' and 'back' cameras
  // at the front of the array--we will collapse any empty slots below.
  int32_t arraySize = count + 2;
  char** cameras = static_cast<char**>(NS_Alloc(arraySize * sizeof(char*)));
  for (int32_t i = 0; i < arraySize; ++i) {
    cameras[i] = nullptr;
  }

  uint32_t extraIndex = 2;
  bool gotFront = false;
  bool gotBack = false;

  for (int32_t i = 0; i < count; ++i) {
    nsCString cameraName;
    nsresult result = GetCameraName(i, cameraName);
    if (result != NS_OK) {
      continue;
    }

    // The first camera we find named 'back' gets slot 0; and the first
    // we find named 'front' gets slot 1.  All others appear after these.
    uint32_t index;
    if (!gotBack && !cameraName.Compare("back")) {
      index = 0;
      gotBack = true;
    } else if (!gotFront && !cameraName.Compare("front")) {
      index = 1;
      gotFront = true;
    } else {
      index = extraIndex++;
    }

    MOZ_ASSERT(index < arraySize);
    cameras[index] = ToNewCString(cameraName);
  }

  // Make a forward pass over the array to compact it; after this loop,
  // 'offset' will contain the number of nullptrs in the array, which
  // we use to adjust the value returned in 'aCount'.
  int32_t offset = 0;
  for (int32_t i = 0; i < arraySize; ++i) {
    if (cameras[i] == nullptr) {
      offset++;
    } else if (offset != 0) {
      cameras[i - offset] = cameras[i];
      cameras[i] = nullptr;
    }
  }
  MOZ_ASSERT(offset >= 2);

  *aCameras = cameras;
  *aCount = arraySize - offset;
  return NS_OK;
}
