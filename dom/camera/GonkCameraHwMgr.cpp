/*
 * Copyright (C) 2012-2015 Mozilla Foundation
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

#include "GonkCameraHwMgr.h"
#include "TestGonkCameraHardware.h"

#ifdef MOZ_WIDGET_GONK
#include <binder/IPCThreadState.h>
#include <sys/system_properties.h>
#include "GonkNativeWindow.h"
#endif

#include "base/basictypes.h"
#include "nsDebug.h"
#include "mozilla/layers/TextureClient.h"
#include "CameraPreferences.h"
#include "mozilla/RefPtr.h"
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 21
#include "GonkBufferQueueProducer.h"
#endif
#include "GonkCameraControl.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace android;

#ifndef MOZ_WIDGET_GONK
NS_IMPL_ISUPPORTS0(GonkCameraHardware);
NS_IMPL_ISUPPORTS0(android::Camera);
#endif

GonkCameraHardware::GonkCameraHardware(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId, const sp<Camera>& aCamera)
  : mCameraId(aCameraId)
  , mClosing(false)
  , mNumFrames(0)
#ifdef MOZ_WIDGET_GONK
  , mCamera(aCamera)
#endif
  , mTarget(aTarget)
  , mRawSensorOrientation(0)
  , mSensorOrientation(0)
  , mEmulated(false)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p (aTarget=%p)\n", __func__, __LINE__, (void*)this, (void*)aTarget);
}

void
GonkCameraHardware::OnRateLimitPreview(bool aLimit)
{
  ::OnRateLimitPreview(mTarget, aLimit);
}

#ifdef MOZ_WIDGET_GONK
void
GonkCameraHardware::OnNewFrame()
{
  if (mClosing) {
    return;
  }
  RefPtr<TextureClient> buffer = mNativeWindow->getCurrentBuffer();
  if (!buffer) {
    DOM_CAMERA_LOGE("received null frame");
    return;
  }
  OnNewPreviewFrame(mTarget, buffer);
}

// Android data callback
void
GonkCameraHardware::postData(int32_t aMsgType, const sp<IMemory>& aDataPtr, camera_frame_metadata_t* metadata)
{
  if (mClosing) {
    return;
  }

  switch (aMsgType) {
    case CAMERA_MSG_PREVIEW_FRAME:
      // Do nothing
      break;

    case CAMERA_MSG_COMPRESSED_IMAGE:
      if (aDataPtr != nullptr) {
        OnTakePictureComplete(mTarget, static_cast<uint8_t*>(aDataPtr->pointer()), aDataPtr->size());
      } else {
        OnTakePictureError(mTarget);
      }
      break;

    case CAMERA_MSG_PREVIEW_METADATA:
      OnFacesDetected(mTarget, metadata);
      break;

    default:
      DOM_CAMERA_LOGE("Unhandled data callback event %d\n", aMsgType);
      break;
  }
}

// Android notify callback
void
GonkCameraHardware::notify(int32_t aMsgType, int32_t ext1, int32_t ext2)
{
  if (mClosing) {
    return;
  }

  switch (aMsgType) {
    case CAMERA_MSG_FOCUS:
      OnAutoFocusComplete(mTarget, !!ext1);
      break;

#if ANDROID_VERSION >= 16
    case CAMERA_MSG_FOCUS_MOVE:
      OnAutoFocusMoving(mTarget, !!ext1);
      break;
#endif

    case CAMERA_MSG_SHUTTER:
      OnShutter(mTarget);
      break;

    case CAMERA_MSG_ERROR:
      OnSystemError(mTarget, CameraControlListener::kSystemService, ext1, ext2);
      break;

    default:
      DOM_CAMERA_LOGE("Unhandled notify callback event %d\n", aMsgType);
      break;
  }
}

void
GonkCameraHardware::postDataTimestamp(nsecs_t aTimestamp, int32_t aMsgType, const sp<IMemory>& aDataPtr)
{
  DOM_CAMERA_LOGI("%s",__func__);
  if (mClosing) {
    return;
  }

  if (mListener.get()) {
    DOM_CAMERA_LOGI("Listener registered, posting recording frame!");
    if (!mListener->postDataTimestamp(aTimestamp, aMsgType, aDataPtr)) {
      DOM_CAMERA_LOGW("Listener unable to process. Drop a recording frame.");
      mCamera->releaseRecordingFrame(aDataPtr);
    }
  } else {
    DOM_CAMERA_LOGW("No listener was set. Drop a recording frame.");
    mCamera->releaseRecordingFrame(aDataPtr);
  }
}
#endif

nsresult
GonkCameraHardware::Init()
{
  DOM_CAMERA_LOGT("%s: this=%p\n", __func__, (void* )this);

#ifdef MOZ_WIDGET_GONK
  CameraInfo info;
  int rv = Camera::getCameraInfo(mCameraId, &info);
  if (rv != 0) {
    DOM_CAMERA_LOGE("%s: failed to get CameraInfo mCameraId %d\n", __func__, mCameraId);
    return NS_ERROR_NOT_INITIALIZED;
   }

  mRawSensorOrientation = info.orientation;
  mSensorOrientation = mRawSensorOrientation;

  /**
   * Non-V4L2-based camera driver adds extra offset onto picture orientation
   * set by gecko, so we have to adjust it back.
   */
  char propname[PROP_NAME_MAX];
  char prop[PROP_VALUE_MAX];
  int offset = 0;
  snprintf(propname, sizeof(propname), "ro.moz.cam.%d.sensor_offset", mCameraId);
  if (__system_property_get(propname, prop) > 0) {
    offset = clamped(atoi(prop), 0, 270);
    mSensorOrientation += offset;
    mSensorOrientation %= 360;
  }
  DOM_CAMERA_LOGI("Sensor orientation: base=%d, offset=%d, final=%d\n", info.orientation, offset, mSensorOrientation);

  if (__system_property_get("ro.kernel.qemu", prop) > 0 && atoi(prop)) {
    DOM_CAMERA_LOGI("Using emulated camera\n");
    mEmulated = true;
  }

  // Disable shutter sound in android CameraService because gaia camera app will play it
  mCamera->sendCommand(CAMERA_CMD_ENABLE_SHUTTER_SOUND, 0, 0);

#if ANDROID_VERSION >= 21
  sp<IGraphicBufferProducer> producer;
  sp<IGonkGraphicBufferConsumer> consumer;
  GonkBufferQueue::createBufferQueue(&producer, &consumer);
  static_cast<GonkBufferQueueProducer*>(producer.get())->setSynchronousMode(false);
  mNativeWindow = new GonkNativeWindow(consumer, GonkCameraHardware::MIN_UNDEQUEUED_BUFFERS);
  mCamera->setPreviewTarget(producer);
#elif ANDROID_VERSION >= 19
  mNativeWindow = new GonkNativeWindow(GonkCameraHardware::MIN_UNDEQUEUED_BUFFERS);
  sp<GonkBufferQueue> bq = mNativeWindow->getBufferQueue();
  bq->setSynchronousMode(false);
  mCamera->setPreviewTarget(mNativeWindow->getBufferQueue());
#elif ANDROID_VERSION >= 17
  mNativeWindow = new GonkNativeWindow(GonkCameraHardware::MIN_UNDEQUEUED_BUFFERS);
  sp<GonkBufferQueue> bq = mNativeWindow->getBufferQueue();
  bq->setSynchronousMode(false);
  mCamera->setPreviewTexture(mNativeWindow->getBufferQueue());
#else
  mNativeWindow = new GonkNativeWindow();
  mCamera->setPreviewTexture(mNativeWindow);
#endif
  mNativeWindow->setNewFrameCallback(this);
  mCamera->setListener(this);

#if ANDROID_VERSION >= 16
  rv = mCamera->sendCommand(CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG, 1, 0);
  if (rv != OK) {
    NS_WARNING("Failed to send command CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG");
  }
#endif

#endif

  return NS_OK;
}

sp<GonkCameraHardware>
GonkCameraHardware::Connect(mozilla::nsGonkCameraControl* aTarget, uint32_t aCameraId)
{
  sp<Camera> camera;

  nsCString test;
  CameraPreferences::GetPref("camera.control.test.enabled", test);

  if (!test.EqualsASCII("hardware")) {
#ifdef MOZ_WIDGET_GONK
#if ANDROID_VERSION >= 18
    camera = Camera::connect(aCameraId, /* clientPackageName */String16("gonk.camera"), Camera::USE_CALLING_UID);
#else
    camera = Camera::connect(aCameraId);
#endif
#endif

    if (camera.get() == nullptr) {
      return nullptr;
    }
  }

  sp<GonkCameraHardware> cameraHardware;
  if (test.EqualsASCII("hardware")) {
    NS_WARNING("Using test Gonk hardware layer");
    cameraHardware = new TestGonkCameraHardware(aTarget, aCameraId, camera);
  } else {
    cameraHardware = new GonkCameraHardware(aTarget, aCameraId, camera);
  }

  nsresult rv = cameraHardware->Init();
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Failed to initialize camera hardware (0x%X)\n", rv);
    cameraHardware->Close();
    return nullptr;
  }

  return cameraHardware;
}

void
GonkCameraHardware::Close()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, (void*)this);

  mClosing = true;
  if (mCamera.get()) {
    mCamera->stopPreview();
    mCamera->disconnect();
  }
  mCamera.clear();
#ifdef MOZ_WIDGET_GONK
  if (mNativeWindow.get()) {
    mNativeWindow->abandon();
  }
  mNativeWindow.clear();

  // Ensure that ICamera's destructor is actually executed
  IPCThreadState::self()->flushCommands();
#endif
}

GonkCameraHardware::~GonkCameraHardware()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, (void*)this);
  mCamera.clear();
#ifdef MOZ_WIDGET_GONK
  mNativeWindow.clear();
#endif
}

int
GonkCameraHardware::GetSensorOrientation(uint32_t aType)
{
  DOM_CAMERA_LOGI("%s\n", __func__);

  switch (aType) {
    case OFFSET_SENSOR_ORIENTATION:
      return mSensorOrientation;

    case RAW_SENSOR_ORIENTATION:
      return mRawSensorOrientation;

    default:
      DOM_CAMERA_LOGE("%s:%d : unknown aType=%d\n", __func__, __LINE__, aType);
      return 0;
  }
}

bool
GonkCameraHardware::IsEmulated()
{
  return mEmulated;
}

int
GonkCameraHardware::AutoFocus()
{
  DOM_CAMERA_LOGI("%s\n", __func__);
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  return mCamera->autoFocus();
}

int
GonkCameraHardware::CancelAutoFocus()
{
  DOM_CAMERA_LOGI("%s\n", __func__);
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  return mCamera->cancelAutoFocus();
}

int
GonkCameraHardware::StartFaceDetection()
{
  DOM_CAMERA_LOGI("%s\n", __func__);
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }

  int rv = INVALID_OPERATION;
#if ANDROID_VERSION >= 15
  rv = mCamera->sendCommand(CAMERA_CMD_START_FACE_DETECTION, CAMERA_FACE_DETECTION_HW, 0);
#endif
  if (rv != OK) {
    DOM_CAMERA_LOGE("Start face detection failed with status %d", rv);
  }

  return rv;
}

int
GonkCameraHardware::StopFaceDetection()
{
  DOM_CAMERA_LOGI("%s\n", __func__);
  if (mClosing) {
    return DEAD_OBJECT;
  }

  int rv = INVALID_OPERATION;
#if ANDROID_VERSION >= 15
  rv = mCamera->sendCommand(CAMERA_CMD_STOP_FACE_DETECTION, 0, 0);
#endif
  if (rv != OK) {
    DOM_CAMERA_LOGE("Stop face detection failed with status %d", rv);
  }

  return rv;
}

int
GonkCameraHardware::TakePicture()
{
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  return mCamera->takePicture(CAMERA_MSG_SHUTTER | CAMERA_MSG_COMPRESSED_IMAGE);
}

void
GonkCameraHardware::CancelTakePicture()
{
  DOM_CAMERA_LOGW("%s: android::Camera do not provide this capability\n", __func__);
}

int
GonkCameraHardware::PushParameters(const GonkCameraParameters& aParams)
{
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  const String8 s = aParams.Flatten();
  return mCamera->setParameters(s);
}

nsresult
GonkCameraHardware::PullParameters(GonkCameraParameters& aParams)
{
  if (NS_WARN_IF(mClosing)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const String8 s = mCamera->getParameters();
  return aParams.Unflatten(s);
}

#ifdef MOZ_WIDGET_GONK
int
GonkCameraHardware::PushParameters(const CameraParameters& aParams)
{
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  String8 s = aParams.flatten();
  return mCamera->setParameters(s);
}

void
GonkCameraHardware::PullParameters(CameraParameters& aParams)
{
  if (!NS_WARN_IF(mClosing)) {
    const String8 s = mCamera->getParameters();
    aParams.unflatten(s);
  }
}
#endif

int
GonkCameraHardware::StartPreview()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  return mCamera->startPreview();
}

void
GonkCameraHardware::StopPreview()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (!mClosing) {
    mCamera->stopPreview();
  }
}

int
GonkCameraHardware::StartRecording()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }

  int rv = mCamera->startRecording();
  if (rv != OK) {
    DOM_CAMERA_LOGE("mHardware->startRecording() failed with status %d", rv);
  }
  return rv;
}

int
GonkCameraHardware::StopRecording()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (mClosing) {
    return DEAD_OBJECT;
  }
  mCamera->stopRecording();
  return OK;
}

#ifdef MOZ_WIDGET_GONK
int
GonkCameraHardware::SetListener(const sp<GonkCameraListener>& aListener)
{
  mListener = aListener;
  return OK;
}

void
GonkCameraHardware::ReleaseRecordingFrame(const sp<IMemory>& aFrame)
{
  if (!NS_WARN_IF(mClosing)) {
    mCamera->releaseRecordingFrame(aFrame);
  }
}
#endif

int
GonkCameraHardware::StoreMetaDataInBuffers(bool aEnabled)
{
  if (NS_WARN_IF(mClosing)) {
    return DEAD_OBJECT;
  }
  return mCamera->storeMetaDataInBuffers(aEnabled);
}
