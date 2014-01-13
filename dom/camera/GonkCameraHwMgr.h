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

#include <binder/IMemory.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/threads.h>

#include "GonkCameraControl.h"
#include "CameraCommon.h"

#include "GonkCameraListener.h"
#include "GonkNativeWindow.h"
#include "mozilla/ReentrantMonitor.h"

// config
#define GIHM_TIMING_RECEIVEFRAME    0
#define GIHM_TIMING_OVERALL         1


namespace mozilla {
  class nsGonkCameraControl;
}

namespace android {

class GonkCameraHardware : public GonkNativeWindowNewFrameCallback
                         , public CameraListener
{
protected:
  GonkCameraHardware(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId, const sp<Camera>& aCamera);
  virtual ~GonkCameraHardware();
  void Init();

public:
  static  sp<GonkCameraHardware>  Connect(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId);
  void    Close();

  // derived from GonkNativeWindowNewFrameCallback
  virtual void    OnNewFrame() MOZ_OVERRIDE;

  // derived from CameraListener
  virtual void notify(int32_t aMsgType, int32_t ext1, int32_t ext2);
  virtual void postData(int32_t aMsgType, const sp<IMemory>& aDataPtr, camera_frame_metadata_t* metadata);
  virtual void postDataTimestamp(nsecs_t aTimestamp, int32_t aMsgType, const sp<IMemory>& aDataPtr);

  /**
   * The physical orientation of the camera sensor: 0, 90, 180, or 270.
   *
   * For example, suppose a device has a naturally tall screen. The
   * back-facing camera sensor is mounted in landscape. You are looking at
   * the screen. If the top side of the camera sensor is aligned with the
   * right edge of the screen in natural orientation, the value should be
   * 90. If the top side of a front-facing camera sensor is aligned with the
   * right of the screen, the value should be 270.
   *
   * RAW_SENSOR_ORIENTATION is the uncorrected orientation returned directly
   * by get_camera_info(); OFFSET_SENSOR_ORIENTATION is the offset adjusted
   * orientation.
   */
  enum {
    RAW_SENSOR_ORIENTATION,
    OFFSET_SENSOR_ORIENTATION
  };
  int      GetSensorOrientation(uint32_t aType = RAW_SENSOR_ORIENTATION);

  int      AutoFocus();
  void     CancelAutoFocus();
  int      TakePicture();
  void     CancelTakePicture();
  int      StartPreview();
  void     StopPreview();
  int      PushParameters(const CameraParameters& aParams);
  void     PullParameters(CameraParameters& aParams);
  int      StartRecording();
  int      StopRecording();
  int      SetListener(const sp<GonkCameraListener>& aListener);
  void     ReleaseRecordingFrame(const sp<IMemory>& aFrame);
  int      StoreMetaDataInBuffers(bool aEnabled);

protected:

  uint32_t                      mCameraId;
  bool                          mClosing;
  mozilla::ReentrantMonitor     mMonitor;
  uint32_t                      mNumFrames;
  sp<Camera>                    mCamera;
  mozilla::nsGonkCameraControl* mTarget;
  sp<GonkNativeWindow>          mNativeWindow;
#if GIHM_TIMING_OVERALL
  struct timespec               mStart;
  struct timespec               mAutoFocusStart;
#endif
  sp<GonkCameraListener>        mListener;
  bool                          mInitialized;
  int                           mRawSensorOrientation;
  int                           mSensorOrientation;

  bool IsInitialized()
  {
    return mInitialized;
  }

private:
  GonkCameraHardware(const GonkCameraHardware&) MOZ_DELETE;
  GonkCameraHardware& operator=(const GonkCameraHardware&) MOZ_DELETE;
};

} // namespace android

#endif // GONK_IMPL_HW_MGR_H
