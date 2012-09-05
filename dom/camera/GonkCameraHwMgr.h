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

#ifndef DOM_CAMERA_GONKCAMERAHWMGR_H
#define DOM_CAMERA_GONKCAMERAHWMGR_H

#include "libcameraservice/CameraHardwareInterface.h"
#include "binder/IMemory.h"
#include "mozilla/ReentrantMonitor.h"

#include "GonkCameraControl.h"
#include "CameraCommon.h"

#include "GonkNativeWindow.h"

// config
#define GIHM_TIMING_RECEIVEFRAME    0
#define GIHM_TIMING_OVERALL         1

using namespace mozilla;
using namespace android;

namespace mozilla {

typedef class nsGonkCameraControl GonkCamera;

class GonkCameraHardware : GonkNativeWindowNewFrameCallback
{
protected:
  GonkCameraHardware(GonkCamera* aTarget, uint32_t aCamera);
  ~GonkCameraHardware();
  void Init();

  static void     DataCallback(int32_t aMsgType, const sp<IMemory> &aDataPtr, camera_frame_metadata_t* aMetadata, void* aUser);
  static void     NotifyCallback(int32_t aMsgType, int32_t ext1, int32_t ext2, void* aUser);

public:
  virtual void    OnNewFrame() MOZ_OVERRIDE;

  static void     ReleaseHandle(uint32_t aHwHandle);
  static uint32_t GetHandle(GonkCamera* aTarget, uint32_t aCamera);
  static int      AutoFocus(uint32_t aHwHandle);
  static void     CancelAutoFocus(uint32_t aHwHandle);
  static int      TakePicture(uint32_t aHwHandle);
  static void     CancelTakePicture(uint32_t aHwHandle);
  static int      StartPreview(uint32_t aHwHandle);
  static void     StopPreview(uint32_t aHwHandle);
  static int      PushParameters(uint32_t aHwHandle, const CameraParameters& aParams);
  static void     PullParameters(uint32_t aHwHandle, CameraParameters& aParams);

protected:
  static GonkCameraHardware*    sHw;
  static uint32_t               sHwHandle;

  static GonkCameraHardware*    GetHardware(uint32_t aHwHandle)
  {
    if (aHwHandle == sHwHandle) {
      /**
       * In the initial case, sHw will be null and sHwHandle will be 0,
       * so even if this function is called with aHwHandle = 0, the
       * result will still be null.
       */
      return sHw;
    }
    return nullptr;
  }

  // Instance wrapper to make member function access easier.
  int StartPreview();

  uint32_t                      mCamera;
  bool                          mClosing;
  mozilla::ReentrantMonitor     mMonitor;
  uint32_t                      mNumFrames;
  sp<CameraHardwareInterface>   mHardware;
  GonkCamera*                   mTarget;
  camera_module_t*              mModule;
  sp<ANativeWindow>             mWindow;
#if GIHM_TIMING_OVERALL
  struct timespec               mStart;
  struct timespec               mAutoFocusStart;
#endif
  bool                          mInitialized;

  bool IsInitialized()
  {
    return mInitialized;
  }

private:
  GonkCameraHardware(const GonkCameraHardware&) MOZ_DELETE;
  GonkCameraHardware& operator=(const GonkCameraHardware&) MOZ_DELETE;
};

} // namespace mozilla

#endif // GONK_IMPL_HW_MGR_H
