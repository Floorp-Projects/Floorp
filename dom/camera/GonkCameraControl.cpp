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

#include <string.h>
#include "base/basictypes.h"
#include "libcameraservice/CameraHardwareInterface.h"
#include "camera/CameraParameters.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfo.h"
#include "nsMemory.h"
#include "jsapi.h"
#include "nsThread.h"
#include "nsPrintfCString.h"
#include "DOMCameraManager.h"
#include "GonkCameraHwMgr.h"
#include "DOMCameraCapabilities.h"
#include "DOMCameraControl.h"
#include "GonkCameraControl.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace android;

static const char* getKeyText(uint32_t aKey)
{
  switch (aKey) {
    case CAMERA_PARAM_EFFECT:
      return CameraParameters::KEY_EFFECT;
    case CAMERA_PARAM_WHITEBALANCE:
      return CameraParameters::KEY_WHITE_BALANCE;
    case CAMERA_PARAM_SCENEMODE:
      return CameraParameters::KEY_SCENE_MODE;
    case CAMERA_PARAM_FLASHMODE:
      return CameraParameters::KEY_FLASH_MODE;
    case CAMERA_PARAM_FOCUSMODE:
      return CameraParameters::KEY_FOCUS_MODE;
    case CAMERA_PARAM_ZOOM:
      return CameraParameters::KEY_ZOOM;
    case CAMERA_PARAM_METERINGAREAS:
      return CameraParameters::KEY_METERING_AREAS;
    case CAMERA_PARAM_FOCUSAREAS:
      return CameraParameters::KEY_FOCUS_AREAS;
    case CAMERA_PARAM_FOCALLENGTH:
      return CameraParameters::KEY_FOCAL_LENGTH;
    case CAMERA_PARAM_FOCUSDISTANCENEAR:
      return CameraParameters::KEY_FOCUS_DISTANCES;
    case CAMERA_PARAM_FOCUSDISTANCEOPTIMUM:
      return CameraParameters::KEY_FOCUS_DISTANCES;
    case CAMERA_PARAM_FOCUSDISTANCEFAR:
      return CameraParameters::KEY_FOCUS_DISTANCES;
    case CAMERA_PARAM_EXPOSURECOMPENSATION:
      return CameraParameters::KEY_EXPOSURE_COMPENSATION;
    case CAMERA_PARAM_SUPPORTED_PREVIEWSIZES:
      return CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES;
    case CAMERA_PARAM_SUPPORTED_VIDEOSIZES:
      return CameraParameters::KEY_SUPPORTED_VIDEO_SIZES;
    case CAMERA_PARAM_SUPPORTED_PICTURESIZES:
      return CameraParameters::KEY_SUPPORTED_PICTURE_SIZES;
    case CAMERA_PARAM_SUPPORTED_PICTUREFORMATS:
      return CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS;
    case CAMERA_PARAM_SUPPORTED_WHITEBALANCES:
      return CameraParameters::KEY_SUPPORTED_WHITE_BALANCE;
    case CAMERA_PARAM_SUPPORTED_SCENEMODES:
      return CameraParameters::KEY_SUPPORTED_SCENE_MODES;
    case CAMERA_PARAM_SUPPORTED_EFFECTS:
      return CameraParameters::KEY_SUPPORTED_EFFECTS;
    case CAMERA_PARAM_SUPPORTED_FLASHMODES:
      return CameraParameters::KEY_SUPPORTED_FLASH_MODES;
    case CAMERA_PARAM_SUPPORTED_FOCUSMODES:
      return CameraParameters::KEY_SUPPORTED_FOCUS_MODES;
    case CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS:
      return CameraParameters::KEY_MAX_NUM_FOCUS_AREAS;
    case CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS:
      return CameraParameters::KEY_MAX_NUM_METERING_AREAS;
    case CAMERA_PARAM_SUPPORTED_MINEXPOSURECOMPENSATION:
      return CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION;
    case CAMERA_PARAM_SUPPORTED_MAXEXPOSURECOMPENSATION:
      return CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION;
    case CAMERA_PARAM_SUPPORTED_EXPOSURECOMPENSATIONSTEP:
      return CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP;
    case CAMERA_PARAM_SUPPORTED_ZOOM:
      return CameraParameters::KEY_ZOOM_SUPPORTED;
    case CAMERA_PARAM_SUPPORTED_ZOOMRATIOS:
      return CameraParameters::KEY_ZOOM_RATIOS;
    default:
      return nullptr;
  }
}

// nsDOMCameraControl implementation-specific constructor
nsDOMCameraControl::nsDOMCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError)
  : mDOMCapabilities(nullptr)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  /**
   * nsDOMCameraControl is a cycle-collection participant, which means it is
   * not threadsafe--so we need to bump up its reference count here to make
   * sure that it exists long enough to be initialized.
   *
   * Once it is initialized, the GetCameraResult main-thread runnable will
   * decrement it again to make sure it can be cleaned up.
   *
   * nsGonkCameraControl MUST NOT hold a strong reference to this
   * nsDOMCameraControl or memory will leak!
   */
  NS_ADDREF_THIS();
  mCameraControl = new nsGonkCameraControl(aCameraId, aCameraThread, this, onSuccess, onError);
}

// Gonk-specific CameraControl implementation.

// Initialize nsGonkCameraControl instance--runs on camera thread.
class InitGonkCameraControl : public nsRunnable
{
public:
  InitGonkCameraControl(nsGonkCameraControl* aCameraControl, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError)
    : mCameraControl(aCameraControl)
    , mDOMCameraControl(aDOMCameraControl)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  ~InitGonkCameraControl()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  NS_IMETHOD Run()
  {
    nsresult rv = mCameraControl->Init();
    return mDOMCameraControl->Result(rv, mOnSuccessCb, mOnErrorCb);
  }

  nsRefPtr<nsGonkCameraControl> mCameraControl;
  // Raw pointer to DOM-facing camera control--it must NS_ADDREF itself for us
  nsDOMCameraControl* mDOMCameraControl;
  nsCOMPtr<nsICameraGetCameraCallback> mOnSuccessCb;
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
};

// Construct nsGonkCameraControl on the main thread.
nsGonkCameraControl::nsGonkCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError)
  : CameraControlImpl(aCameraId, aCameraThread)
  , mHwHandle(0)
  , mExposureCompensationMin(0.0)
  , mExposureCompensationStep(0.0)
  , mDeferConfigUpdate(false)
  , mWidth(0)
  , mHeight(0)
  , mFormat(PREVIEW_FORMAT_UNKNOWN)
  , mDiscardedFrameCount(0)
{
  // Constructor runs on the main thread...
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mRwLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "GonkCameraControl.Parameters.Lock");

  // ...but initialization is carried out on the camera thread.
  nsCOMPtr<nsIRunnable> init = new InitGonkCameraControl(this, aDOMCameraControl, onSuccess, onError);
  mCameraThread->Dispatch(init, NS_DISPATCH_NORMAL);
}

nsresult
nsGonkCameraControl::Init()
{
  mHwHandle = GonkCameraHardware::GetHandle(this, mCameraId);
  DOM_CAMERA_LOGI("Initializing camera %d (this=%p, mHwHandle=%d)\n", mCameraId, this, mHwHandle);

  // Initialize our camera configuration database.
  PullParametersImpl();

  // Try to set preferred image format and frame rate
  DOM_CAMERA_LOGI("Camera preview formats: %s\n", mParams.get(mParams.KEY_SUPPORTED_PREVIEW_FORMATS));
  const char* const PREVIEW_FORMAT = "yuv420p";
  const char* const BAD_PREVIEW_FORMAT = "yuv420sp";
  mParams.setPreviewFormat(PREVIEW_FORMAT);
  mParams.setPreviewFrameRate(mFps);

  // Check that our settings stuck
  PullParametersImpl();
  const char* format = mParams.getPreviewFormat();
  if (strcmp(format, PREVIEW_FORMAT) == 0) {
    mFormat = PREVIEW_FORMAT_YUV420P;  /* \o/ */
  } else if (strcmp(format, BAD_PREVIEW_FORMAT) == 0) {
    mFormat = PREVIEW_FORMAT_YUV420SP;
    DOM_CAMERA_LOGA("Camera ignored our request for '%s' preview, will have to convert (from %d)\n", PREVIEW_FORMAT, mFormat);
  } else {
    mFormat = PREVIEW_FORMAT_UNKNOWN;
    DOM_CAMERA_LOGE("Camera ignored our request for '%s' preview, returned UNSUPPORTED format '%s'\n", PREVIEW_FORMAT, format);
  }

  // Check the frame rate and log if the camera ignored our setting
  uint32_t fps = mParams.getPreviewFrameRate();
  if (fps != mFps) {
    DOM_CAMERA_LOGA("We asked for %d fps but camera returned %d fps, using that", mFps, fps);
    mFps = fps;
  }

  // Grab any other settings we'll need later.
  mExposureCompensationMin = mParams.getFloat(mParams.KEY_MIN_EXPOSURE_COMPENSATION);
  mExposureCompensationStep = mParams.getFloat(mParams.KEY_EXPOSURE_COMPENSATION_STEP);
  mMaxMeteringAreas = mParams.getInt(mParams.KEY_MAX_NUM_METERING_AREAS);
  mMaxFocusAreas = mParams.getInt(mParams.KEY_MAX_NUM_FOCUS_AREAS);

  DOM_CAMERA_LOGI(" - minimum exposure compensation: %f\n", mExposureCompensationMin);
  DOM_CAMERA_LOGI(" - exposure compensation step:    %f\n", mExposureCompensationStep);
  DOM_CAMERA_LOGI(" - maximum metering areas:        %d\n", mMaxMeteringAreas);
  DOM_CAMERA_LOGI(" - maximum focus areas:           %d\n", mMaxFocusAreas);

  return mHwHandle != 0 ? NS_OK : NS_ERROR_FAILURE;
}

nsGonkCameraControl::~nsGonkCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mHwHandle = %d\n", __func__, __LINE__, this, mHwHandle);
  GonkCameraHardware::ReleaseHandle(mHwHandle);
  if (mRwLock) {
    PRRWLock* lock = mRwLock;
    mRwLock = nullptr;
    PR_DestroyRWLock(lock);
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
}

class RwAutoLockRead
{
public:
  RwAutoLockRead(PRRWLock* aRwLock)
    : mRwLock(aRwLock)
  {
    PR_RWLock_Rlock(mRwLock);
  }

  ~RwAutoLockRead()
  {
    PR_RWLock_Unlock(mRwLock);
  }

protected:
  PRRWLock* mRwLock;
};

class RwAutoLockWrite
{
public:
  RwAutoLockWrite(PRRWLock* aRwLock)
    : mRwLock(aRwLock)
  {
    PR_RWLock_Wlock(mRwLock);
  }

  ~RwAutoLockWrite()
  {
    PR_RWLock_Unlock(mRwLock);
  }

protected:
  PRRWLock* mRwLock;
};

const char*
nsGonkCameraControl::GetParameter(const char* aKey)
{
  RwAutoLockRead lock(mRwLock);
  return mParams.get(aKey);
}

const char*
nsGonkCameraControl::GetParameterConstChar(uint32_t aKey)
{
  const char* key = getKeyText(aKey);
  if (!key) {
    return nullptr;
  }

  RwAutoLockRead lock(mRwLock);
  return mParams.get(key);
}

double
nsGonkCameraControl::GetParameterDouble(uint32_t aKey)
{
  double val;
  int index = 0;
  double focusDistance[3];
  const char* s;

  const char* key = getKeyText(aKey);
  if (!key) {
    // return 1x when zooming is not supported
    return aKey == CAMERA_PARAM_ZOOM ? 1.0 : 0.0;
  }

  RwAutoLockRead lock(mRwLock);
  switch (aKey) {
    case CAMERA_PARAM_ZOOM:
      val = mParams.getInt(key);
      return val / 100;

    /**
     * The gonk camera parameters API only exposes one focus distance property
     * that contains "Near,Optimum,Far" distances, in metres, where 'Far' may
     * be 'Infinity'.
     */
    case CAMERA_PARAM_FOCUSDISTANCEFAR:
      ++index;
      // intentional fallthrough

    case CAMERA_PARAM_FOCUSDISTANCEOPTIMUM:
      ++index;
      // intentional fallthrough

    case CAMERA_PARAM_FOCUSDISTANCENEAR:
      s = mParams.get(key);
      if (sscanf(s, "%lf,%lf,%lf", &focusDistance[0], &focusDistance[1], &focusDistance[2]) == 3) {
        return focusDistance[index];
      }
      return 0.0;

    case CAMERA_PARAM_EXPOSURECOMPENSATION:
      index = mParams.getInt(key);
      if (!index) {
        // NaN indicates automatic exposure compensation
        return NAN;
      }
      val = (index - 1) * mExposureCompensationStep + mExposureCompensationMin;
      DOM_CAMERA_LOGI("index = %d --> compensation = %f\n", index, val);
      return val;

    default:
      return mParams.getFloat(key);
  }
}

void
nsGonkCameraControl::GetParameter(uint32_t aKey, nsTArray<CameraRegion>& aRegions)
{
  aRegions.Clear();

  const char* key = getKeyText(aKey);
  if (!key) {
    return;
  }

  RwAutoLockRead lock(mRwLock);

  const char* value = mParams.get(key);
  DOM_CAMERA_LOGI("key='%s' --> value='%s'\n", key, value);
  if (!value) {
    return;
  }

  const char* p = value;
  uint32_t count = 1;

  // count the number of regions in the string
  while ((p = strstr(p, "),("))) {
    ++count;
    p += 3;
  }

  aRegions.SetCapacity(count);
  CameraRegion* r;

  // parse all of the region sets
  uint32_t i;
  for (i = 0, p = value; p && i < count; ++i, p = strchr(p + 1, '(')) {
    r = aRegions.AppendElement();
    if (sscanf(p, "(%d,%d,%d,%d,%u)", &r->top, &r->left, &r->bottom, &r->right, &r->weight) != 5) {
      DOM_CAMERA_LOGE("%s:%d : region tuple has bad format: '%s'\n", __func__, __LINE__, p);
      goto GetParameter_error;
    }
  }

  return;

GetParameter_error:
  aRegions.Clear();
}

nsresult
nsGonkCameraControl::PushParameters()
{
  if (mDeferConfigUpdate) {
    DOM_CAMERA_LOGT("%s:%d - defering config update\n", __func__, __LINE__);
    return NS_OK;
  }

  /**
   * If we're already on the camera thread, call PushParametersImpl()
   * directly, so that it executes synchronously.  Some callers
   * require this so that changes take effect immediately before
   * we can proceed.
   */
  if (NS_IsMainThread()) {
    DOM_CAMERA_LOGT("%s:%d - dispatching to main thread\n", __func__, __LINE__);
    nsCOMPtr<nsIRunnable> pushParametersTask = NS_NewRunnableMethod(this, &nsGonkCameraControl::PushParametersImpl);
    return mCameraThread->Dispatch(pushParametersTask, NS_DISPATCH_NORMAL);
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  return PushParametersImpl();
}

void
nsGonkCameraControl::SetParameter(const char* aKey, const char* aValue)
{
  {
    RwAutoLockWrite lock(mRwLock);
    mParams.set(aKey, aValue);
  }
  PushParameters();
}

void
nsGonkCameraControl::SetParameter(uint32_t aKey, const char* aValue)
{
  const char* key = getKeyText(aKey);
  if (!key) {
    return;
  }

  {
    RwAutoLockWrite lock(mRwLock);
    mParams.set(key, aValue);
  }
  PushParameters();
}

void
nsGonkCameraControl::SetParameter(uint32_t aKey, double aValue)
{
  uint32_t index;

  const char* key = getKeyText(aKey);
  if (!key) {
    return;
  }

  {
    RwAutoLockWrite lock(mRwLock);
    if (aKey == CAMERA_PARAM_EXPOSURECOMPENSATION) {
      /**
       * Convert from real value to a Gonk index, round
       * to the nearest step; index is 1-based.
       */
      index = (aValue - mExposureCompensationMin + mExposureCompensationStep / 2) / mExposureCompensationStep + 1;
      DOM_CAMERA_LOGI("compensation = %f --> index = %d\n", aValue, index);
      mParams.set(key, index);
    } else {
      mParams.setFloat(key, aValue);
    }
  }
  PushParameters();
}

void
nsGonkCameraControl::SetParameter(uint32_t aKey, const nsTArray<CameraRegion>& aRegions)
{
  const char* key = getKeyText(aKey);
  if (!key) {
    return;
  }

  uint32_t length = aRegions.Length();

  if (!length) {
    // This tells the camera driver to revert to automatic regioning.
    mParams.set(key, "(0,0,0,0,0)");
    PushParameters();
    return;
  }

  nsCString s;

  for (uint32_t i = 0; i < length; ++i) {
    const CameraRegion* r = &aRegions[i];
    s.AppendPrintf("(%d,%d,%d,%d,%d),", r->top, r->left, r->bottom, r->right, r->weight);
  }

  // remove the trailing comma
  s.Trim(",", false, true, true);

  DOM_CAMERA_LOGI("camera region string '%s'\n", s.get());

  {
    RwAutoLockWrite lock(mRwLock);
    mParams.set(key, s.get());
  }
  PushParameters();
}

nsresult
nsGonkCameraControl::GetPreviewStreamImpl(GetPreviewStreamTask* aGetPreviewStream)
{
  SetPreviewSize(aGetPreviewStream->mSize.width, aGetPreviewStream->mSize.height);

  DOM_CAMERA_LOGI("config preview: wated %d x %d, got %d x %d (%d fps, format %d)\n", aGetPreviewStream->mSize.width, aGetPreviewStream->mSize.height, mWidth, mHeight, mFps, mFormat);

  nsCOMPtr<GetPreviewStreamResult> getPreviewStreamResult = new GetPreviewStreamResult(this, mWidth, mHeight, mFps, aGetPreviewStream->mOnSuccessCb);
  return NS_DispatchToMainThread(getPreviewStreamResult);
}

nsresult
nsGonkCameraControl::StartPreviewImpl(StartPreviewTask* aStartPreview)
{
  /**
   * If 'aStartPreview->mDOMPreview' is null, we are just restarting
   * the preview after taking a picture.  No need to monkey with the
   * currently set DOM-facing preview object.
   */
  if (aStartPreview->mDOMPreview) {
    if (mDOMPreview) {
      mDOMPreview->Stopped(true);
    }
    mDOMPreview = aStartPreview->mDOMPreview;
  } else if (!mDOMPreview) {
    return NS_ERROR_INVALID_ARG;
  }

  DOM_CAMERA_LOGI("%s: starting preview (mDOMPreview=%p)\n", __func__, mDOMPreview);
  if (GonkCameraHardware::StartPreview(mHwHandle) != OK) {
    DOM_CAMERA_LOGE("%s: failed to start preview\n", __func__);
    return NS_ERROR_FAILURE;
  }

  if (aStartPreview->mDOMPreview) {
    mDOMPreview->Started();
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::StopPreviewImpl(StopPreviewTask* aStopPreview)
{
  DOM_CAMERA_LOGI("%s: stopping preview\n", __func__);

  // StopPreview() is a synchronous call--it doesn't return
  // until the camera preview thread exits.
  GonkCameraHardware::StopPreview(mHwHandle);
  mDOMPreview->Stopped();
  mDOMPreview = nullptr;

  return NS_OK;
}

nsresult
nsGonkCameraControl::AutoFocusImpl(AutoFocusTask* aAutoFocus)
{
  nsCOMPtr<nsICameraAutoFocusCallback> cb = mAutoFocusOnSuccessCb;
  if (cb) {
    /**
     * We already have a callback, so someone has already
     * called autoFocus() -- cancel it.
     */
    mAutoFocusOnSuccessCb = nullptr;
    nsCOMPtr<nsICameraErrorCallback> ecb = mAutoFocusOnErrorCb;
    mAutoFocusOnErrorCb = nullptr;
    if (ecb) {
      nsresult rv = NS_DispatchToMainThread(new CameraErrorResult(ecb, NS_LITERAL_STRING("CANCELLED")));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    GonkCameraHardware::CancelAutoFocus(mHwHandle);
  }

  mAutoFocusOnSuccessCb = aAutoFocus->mOnSuccessCb;
  mAutoFocusOnErrorCb = aAutoFocus->mOnErrorCb;

  if (GonkCameraHardware::AutoFocus(mHwHandle) != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::TakePictureImpl(TakePictureTask* aTakePicture)
{
  nsCOMPtr<nsICameraTakePictureCallback> cb = mTakePictureOnSuccessCb;
  if (cb) {
    /**
     * We already have a callback, so someone has already
     * called TakePicture() -- cancel it.
     */
    mTakePictureOnSuccessCb = nullptr;
    nsCOMPtr<nsICameraErrorCallback> ecb = mTakePictureOnErrorCb;
    mTakePictureOnErrorCb = nullptr;
    if (ecb) {
      nsresult rv = NS_DispatchToMainThread(new CameraErrorResult(ecb, NS_LITERAL_STRING("CANCELLED")));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    GonkCameraHardware::CancelTakePicture(mHwHandle);
  }

  mTakePictureOnSuccessCb = aTakePicture->mOnSuccessCb;
  mTakePictureOnErrorCb = aTakePicture->mOnErrorCb;

  // batch-update camera configuration
  mDeferConfigUpdate = true;

  /**
   * height and width: some drivers are less friendly about getting one of
   * these set to zero, so if either is not specified, ignore both and go
   * with current or default settings.
   */
  if (aTakePicture->mSize.width && aTakePicture->mSize.height) {
    nsCString s;
    s.AppendPrintf("%dx%d", aTakePicture->mSize.width, aTakePicture->mSize.height);
    DOM_CAMERA_LOGI("setting picture size to '%s'\n", s.get());
    SetParameter(CameraParameters::KEY_PICTURE_SIZE, s.get());
  }

  // Picture format -- need to keep it for the callback.
  mFileFormat = aTakePicture->mFileFormat;
  SetParameter(CameraParameters::KEY_PICTURE_FORMAT, NS_ConvertUTF16toUTF8(mFileFormat).get());

  // Convert 'rotation' to a positive value from 0..270 degrees, in steps of 90.
  uint32_t r = static_cast<uint32_t>(aTakePicture->mRotation);
  r %= 360;
  r += 45;
  r /= 90;
  r *= 90;
  DOM_CAMERA_LOGI("setting picture rotation to %d degrees (mapped from %d)\n", r, aTakePicture->mRotation);
  SetParameter(CameraParameters::KEY_ROTATION, nsPrintfCString("%u", r).get());

  // Add any specified positional information -- don't care if these fail.
  if (!isnan(aTakePicture->mPosition.latitude)) {
    DOM_CAMERA_LOGI("setting picture latitude to %lf\n", aTakePicture->mPosition.latitude);
    SetParameter(CameraParameters::KEY_GPS_LATITUDE, nsPrintfCString("%lf", aTakePicture->mPosition.latitude).get());
  }
  if (!isnan(aTakePicture->mPosition.longitude)) {
    DOM_CAMERA_LOGI("setting picture longitude to %lf\n", aTakePicture->mPosition.longitude);
    SetParameter(CameraParameters::KEY_GPS_LONGITUDE, nsPrintfCString("%lf", aTakePicture->mPosition.longitude).get());
  }
  if (!isnan(aTakePicture->mPosition.altitude)) {
    DOM_CAMERA_LOGI("setting picture altitude to %lf\n", aTakePicture->mPosition.altitude);
    SetParameter(CameraParameters::KEY_GPS_ALTITUDE, nsPrintfCString("%lf", aTakePicture->mPosition.altitude).get());
  }
  if (!isnan(aTakePicture->mPosition.timestamp)) {
    DOM_CAMERA_LOGI("setting picture timestamp to %lf\n", aTakePicture->mPosition.timestamp);
    SetParameter(CameraParameters::KEY_GPS_TIMESTAMP, nsPrintfCString("%lf", aTakePicture->mPosition.timestamp).get());
  }

  mDeferConfigUpdate = false;
  PushParameters();

  if (GonkCameraHardware::TakePicture(mHwHandle) != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::PushParametersImpl()
{
  DOM_CAMERA_LOGI("Pushing camera parameters\n");
  RwAutoLockRead lock(mRwLock);
  if (GonkCameraHardware::PushParameters(mHwHandle, mParams) != OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsGonkCameraControl::PullParametersImpl()
{
  DOM_CAMERA_LOGI("Pulling camera parameters\n");
  RwAutoLockWrite lock(mRwLock);
  GonkCameraHardware::PullParameters(mHwHandle, mParams);
  return NS_OK;
}

nsresult
nsGonkCameraControl::StartRecordingImpl(StartRecordingTask* aStartRecording)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsGonkCameraControl::StopRecordingImpl(StopRecordingTask* aStopRecording)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsGonkCameraControl::AutoFocusComplete(bool aSuccess)
{
  /**
   * Auto focusing can change some of the camera's parameters, so
   * we need to pull a new set before sending the result to the
   * main thread.
   */
  PullParametersImpl();

  /**
   * If we make it here, regardless of the value of 'aSuccess', we
   * consider the autofocus _process_ to have succeeded.  It is up
   * to the onSuccess callback to determine how to handle the case
   * where the camera wasn't actually able to acquire focus.
   */
  nsCOMPtr<nsIRunnable> autoFocusResult = new AutoFocusResult(aSuccess, mAutoFocusOnSuccessCb);
  /**
   * Remember to set these to null so that we don't hold any extra
   * references to our document's window.
   */
  mAutoFocusOnSuccessCb = nullptr;
  mAutoFocusOnErrorCb = nullptr;
  nsresult rv = NS_DispatchToMainThread(autoFocusResult);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch autoFocus() onSuccess callback to main thread!");
  }
}

void
nsGonkCameraControl::TakePictureComplete(uint8_t* aData, uint32_t aLength)
{
  uint8_t* data = new uint8_t[aLength];

  memcpy(data, aData, aLength);

  // TODO: see bug 779144.
  nsIDOMBlob* blob = new nsDOMMemoryFile(static_cast<void*>(data), static_cast<uint64_t>(aLength), NS_LITERAL_STRING("image/jpeg"));
  nsCOMPtr<nsIRunnable> takePictureResult = new TakePictureResult(blob, mTakePictureOnSuccessCb);
  /**
   * Remember to set these to null so that we don't hold any extra
   * references to our document's window.
   */
  mTakePictureOnSuccessCb = nullptr;
  mTakePictureOnErrorCb = nullptr;
  nsresult rv = NS_DispatchToMainThread(takePictureResult);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch takePicture() onSuccess callback to main thread!");
  }
}

void
nsGonkCameraControl::SetPreviewSize(uint32_t aWidth, uint32_t aHeight)
{
  Vector<Size> previewSizes;
  uint32_t bestWidth = aWidth;
  uint32_t bestHeight = aHeight;
  uint32_t minSizeDelta = UINT32_MAX;
  uint32_t delta;
  Size size;

  mParams.getSupportedPreviewSizes(previewSizes);

  if (!aWidth && !aHeight) {
    // no size specified, take the first supported size
    size = previewSizes[0];
    bestWidth = size.width;
    bestHeight = size.height;
  } else if (aWidth && aHeight) {
    // both height and width specified, find the supported size closest to requested size
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      Size size = previewSizes[i];
      uint32_t delta = abs((long int)(size.width * size.height - aWidth * aHeight));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  } else if (!aWidth) {
    // width not specified, find closest height match
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      size = previewSizes[i];
      delta = abs((long int)(size.height - aHeight));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  } else if (!aHeight) {
    // height not specified, find closest width match
    for (uint32_t i = 0; i < previewSizes.size(); i++) {
      size = previewSizes[i];
      delta = abs((long int)(size.width - aWidth));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        bestWidth = size.width;
        bestHeight = size.height;
      }
    }
  }

  mWidth = bestWidth;
  mHeight = bestHeight;
  mParams.setPreviewSize(mWidth, mHeight);
  PushParameters();
}

// Gonk callback handlers.
namespace mozilla {

void
ReceiveImage(nsGonkCameraControl* gc, uint8_t* aData, uint32_t aLength)
{
  gc->TakePictureComplete(aData, aLength);
}

void
AutoFocusComplete(nsGonkCameraControl* gc, bool aSuccess)
{
  gc->AutoFocusComplete(aSuccess);
}

static void
GonkFrameBuilder(Image* aImage, void* aBuffer, uint32_t aWidth, uint32_t aHeight)
{
  /**
   * Cast the generic Image back to our platform-specific type and
   * populate it.
   */
  GonkIOSurfaceImage* videoImage = static_cast<GonkIOSurfaceImage*>(aImage);
  GonkIOSurfaceImage::Data data;
  data.mGraphicBuffer = static_cast<layers::GraphicBufferLocked*>(aBuffer);
  data.mPicSize = gfxIntSize(aWidth, aHeight);
  videoImage->SetData(data);
}

void
ReceiveFrame(nsGonkCameraControl* gc, layers::GraphicBufferLocked* aBuffer)
{
  if (!gc->ReceiveFrame(aBuffer, ImageFormat::GONK_IO_SURFACE, GonkFrameBuilder)) {
    aBuffer->Unlock();
  }
}

} // namespace mozilla
