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
#include "GonkCameraParameters.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
  class nsGonkCameraControl;
  class GonkCameraParameters;
}

namespace android {

class GonkCameraHardware : public GonkNativeWindowNewFrameCallback
                         , public CameraListener
{
protected:
  GonkCameraHardware(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId, const sp<Camera>& aCamera);
  virtual ~GonkCameraHardware();
  virtual nsresult Init();

public:
  static sp<GonkCameraHardware> Connect(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId);
  virtual void Close();

  // derived from GonkNativeWindowNewFrameCallback
  virtual void OnNewFrame() MOZ_OVERRIDE;

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
  virtual int      GetSensorOrientation(uint32_t aType = RAW_SENSOR_ORIENTATION);

  virtual int      AutoFocus();
  virtual int      CancelAutoFocus();
  virtual int      StartFaceDetection();
  virtual int      StopFaceDetection();
  virtual int      TakePicture();
  virtual void     CancelTakePicture();
  virtual int      StartPreview();
  virtual void     StopPreview();
  virtual int      PushParameters(const mozilla::GonkCameraParameters& aParams);
  virtual int      PushParameters(const CameraParameters& aParams);
  virtual nsresult PullParameters(mozilla::GonkCameraParameters& aParams);
  virtual void     PullParameters(CameraParameters& aParams);
  virtual int      StartRecording();
  virtual int      StopRecording();
  virtual int      SetListener(const sp<GonkCameraListener>& aListener);
  virtual void     ReleaseRecordingFrame(const sp<IMemory>& aFrame);
  virtual int      StoreMetaDataInBuffers(bool aEnabled);

protected:
  uint32_t                      mCameraId;
  bool                          mClosing;
  uint32_t                      mNumFrames;
  sp<Camera>                    mCamera;
  mozilla::nsGonkCameraControl* mTarget;
  sp<GonkNativeWindow>          mNativeWindow;
  sp<GonkCameraListener>        mListener;
  int                           mRawSensorOrientation;
  int                           mSensorOrientation;

private:
  GonkCameraHardware(const GonkCameraHardware&) MOZ_DELETE;
  GonkCameraHardware& operator=(const GonkCameraHardware&) MOZ_DELETE;
};

} // namespace android

#endif // GONK_IMPL_HW_MGR_H
