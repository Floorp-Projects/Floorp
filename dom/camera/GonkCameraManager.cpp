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

#include "jsapi.h"
#include "libcameraservice/CameraHardwareInterface.h"
#include "GonkCameraControl.h"
#include "DOMCameraManager.h"

#define DOM_CAMERA_LOG_LEVEL  3
#include "CameraCommon.h"

// From nsDOMCameraManager, but gonk-specific!

/* [implicit_jscontext] jsval getListOfCameras (); */
NS_IMETHODIMP
nsDOMCameraManager::GetListOfCameras(JSContext* cx, JS::Value* _retval)
{
  JSObject* a = JS_NewArrayObject(cx, 0, nullptr);
  camera_module_t* module;
  PRUint32 index = 0;
  PRUint32 count;

  if (!a) {
    DOM_CAMERA_LOGE("getListOfCameras : Could not create array object");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (hw_get_module(CAMERA_HARDWARE_MODULE_ID, (const hw_module_t**)&module) < 0) {
    DOM_CAMERA_LOGE("getListOfCameras : Could not load camera HAL module");
    return NS_ERROR_NOT_AVAILABLE;
  }

  count = module->get_number_of_cameras();
  DOM_CAMERA_LOGI("getListOfCameras : get_number_of_cameras() returned %d\n", count);
  while (count--) {
    struct camera_info info;
    int rv = module->get_camera_info(count, &info);
    if (rv != 0) {
      DOM_CAMERA_LOGE("getListOfCameras : get_camera_info(%d) failed: %d\n", count, rv);
      continue;
    }

    JSString* v;
    jsval jv;

    switch (info.facing) {
      case CAMERA_FACING_BACK:
        v = JS_NewStringCopyZ(cx, "back");
        index = 0;
        break;

      case CAMERA_FACING_FRONT:
        v = JS_NewStringCopyZ(cx, "front");
        index = 1;
        break;

      default:
        // TODO: handle extra cameras in getCamera().
        {
          static PRUint32 extraIndex = 2;
          nsCString s;
          s.AppendPrintf("extra-camera-%d", count);
          v = JS_NewStringCopyZ(cx, s.get());
          index = extraIndex++;
        }
        break;
    }
    if (!v) {
      DOM_CAMERA_LOGE("getListOfCameras : out of memory populating camera list");
      delete a;
      return NS_ERROR_NOT_AVAILABLE;
    }
    jv = STRING_TO_JSVAL(v);
    if (!JS_SetElement(cx, a, index, &jv)) {
      DOM_CAMERA_LOGE("getListOfCameras : failed building list of cameras");
      delete a;
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  *_retval = OBJECT_TO_JSVAL(a);
  return NS_OK;
}

using namespace mozilla;

NS_IMETHODIMP
GetCameraTask::Run()
{
  nsCOMPtr<nsICameraControl> cameraControl = new nsGonkCameraControl(mCameraId, mCameraThread);

  DOM_CAMERA_LOGI("%s:%d\n", __func__, __LINE__);

  return NS_DispatchToMainThread(new GetCameraResult(cameraControl, mOnSuccessCb));
}
