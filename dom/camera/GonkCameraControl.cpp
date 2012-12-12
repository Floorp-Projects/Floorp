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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include "base/basictypes.h"
#include "libcameraservice/CameraHardwareInterface.h"
#include "camera/CameraParameters.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfo.h"
#include "nsMemory.h"
#include "jsapi.h"
#include "nsThread.h"
#include <media/MediaProfiles.h>
#include "mozilla/FileUtils.h"
#include "mozilla/Services.h"
#include "nsAlgorithm.h"
#include <media/mediaplayer.h>
#include "nsPrintfCString.h"
#include "nsIObserverService.h"
#include "DOMCameraManager.h"
#include "GonkCameraHwMgr.h"
#include "DOMCameraCapabilities.h"
#include "DOMCameraControl.h"
#include "GonkRecorderProfiles.h"
#include "GonkCameraControl.h"
#include "CameraCommon.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
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
    case CAMERA_PARAM_THUMBNAILWIDTH:
      return CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH;
    case CAMERA_PARAM_THUMBNAILHEIGHT:
      return CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT;
    case CAMERA_PARAM_THUMBNAILQUALITY:
      return CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY;

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
    case CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES:
      return CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES;
    default:
      return nullptr;
  }
}

// nsDOMCameraControl implementation-specific constructor
nsDOMCameraControl::nsDOMCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId)
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
  mCameraControl = new nsGonkCameraControl(aCameraId, aCameraThread, this, onSuccess, onError, aWindowId);
}

// Gonk-specific CameraControl implementation.

// Initialize nsGonkCameraControl instance--runs on camera thread.
class InitGonkCameraControl : public nsRunnable
{
public:
  InitGonkCameraControl(nsGonkCameraControl* aCameraControl, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId)
    : mCameraControl(aCameraControl)
    , mDOMCameraControl(aDOMCameraControl)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
    , mWindowId(aWindowId)
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
    return mDOMCameraControl->Result(rv, mOnSuccessCb, mOnErrorCb, mWindowId);
  }

  nsRefPtr<nsGonkCameraControl> mCameraControl;
  // Raw pointer to DOM-facing camera control--it must NS_ADDREF itself for us
  nsDOMCameraControl* mDOMCameraControl;
  nsCOMPtr<nsICameraGetCameraCallback> mOnSuccessCb;
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
  uint64_t mWindowId;
};

// Construct nsGonkCameraControl on the main thread.
nsGonkCameraControl::nsGonkCameraControl(uint32_t aCameraId, nsIThread* aCameraThread, nsDOMCameraControl* aDOMCameraControl, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, uint64_t aWindowId)
  : CameraControlImpl(aCameraId, aCameraThread, aWindowId)
  , mHwHandle(0)
  , mExposureCompensationMin(0.0)
  , mExposureCompensationStep(0.0)
  , mDeferConfigUpdate(false)
  , mWidth(0)
  , mHeight(0)
  , mLastPictureWidth(0)
  , mLastPictureHeight(0)
  , mFormat(PREVIEW_FORMAT_UNKNOWN)
  , mFps(30)
  , mDiscardedFrameCount(0)
  , mMediaProfiles(nullptr)
  , mRecorder(nullptr)
  , mProfileManager(nullptr)
  , mRecorderProfile(nullptr)
  , mVideoFile(nullptr)
{
  // Constructor runs on the main thread...
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mRwLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "GonkCameraControl.Parameters.Lock");

  // ...but initialization is carried out on the camera thread.
  nsCOMPtr<nsIRunnable> init = new InitGonkCameraControl(this, aDOMCameraControl, onSuccess, onError, aWindowId);
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
  PushParametersImpl();

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
      aRegions.Clear();
      return;
    }
  }

  return;
}

void
nsGonkCameraControl::GetParameter(uint32_t aKey, nsTArray<CameraSize>& aSizes)
{
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
  CameraSize* s;

  // The 'value' string is in the format "w1xh1,w2xh2,w3xh3,..."
  while (p) {
    s = aSizes.AppendElement();
    if (sscanf(p, "%dx%d", &s->width, &s->height) != 2) {
      DOM_CAMERA_LOGE("%s:%d : size tuple has bad format: '%s'\n", __func__, __LINE__, p);
      aSizes.Clear();
      return;
    }
    // Look for the next record...
    p = strchr(p, ',');
    if (p) {
      // ...skip the comma too
      ++p;
    }
  }

  return;
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

void
nsGonkCameraControl::SetParameter(uint32_t aKey, int aValue)
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

nsresult
nsGonkCameraControl::GetPreviewStreamImpl(GetPreviewStreamTask* aGetPreviewStream)
{
  // stop any currently running preview
  StopPreviewInternal(true /* forced */);

  SetPreviewSize(aGetPreviewStream->mSize.width, aGetPreviewStream->mSize.height);
  DOM_CAMERA_LOGI("picture preview: wanted %d x %d, got %d x %d (%d fps, format %d)\n", aGetPreviewStream->mSize.width, aGetPreviewStream->mSize.height, mWidth, mHeight, mFps, mFormat);

  nsCOMPtr<GetPreviewStreamResult> getPreviewStreamResult = new GetPreviewStreamResult(this, mWidth, mHeight, mFps, aGetPreviewStream->mOnSuccessCb, mWindowId);
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
    StopPreviewInternal(true /* forced */);
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
nsGonkCameraControl::StopPreviewInternal(bool aForced)
{
  DOM_CAMERA_LOGI("%s: stopping preview (mDOMPreview=%p)\n", __func__, mDOMPreview);

  // StopPreview() is a synchronous call--it doesn't return
  // until the camera preview thread exits.
  if (mDOMPreview) {
    GonkCameraHardware::StopPreview(mHwHandle);
    mDOMPreview->Stopped(aForced);
    mDOMPreview = nullptr;
  }

  return NS_OK;
}

nsresult
nsGonkCameraControl::StopPreviewImpl(StopPreviewTask* aStopPreview)
{
  return StopPreviewInternal();
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
      nsresult rv = NS_DispatchToMainThread(new CameraErrorResult(ecb, NS_LITERAL_STRING("CANCELLED"), mWindowId));
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

void
nsGonkCameraControl::SetupThumbnail(uint32_t aPictureWidth, uint32_t aPictureHeight, uint32_t aPercentQuality)
{
  /**
   * Use the smallest non-0x0 thumbnail size that matches
   *  the aspect ratio of our parameters...
   */
  uint32_t smallestArea = UINT_MAX;
  uint32_t smallestIndex = UINT_MAX;
  nsAutoTArray<CameraSize, 8> thumbnailSizes;
  GetParameter(CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES, thumbnailSizes);

  for (uint32_t i = 0; i < thumbnailSizes.Length(); ++i) {
    uint32_t area = thumbnailSizes[i].width * thumbnailSizes[i].height;
    if (area != 0
      && area < smallestArea
      && thumbnailSizes[i].width * aPictureHeight / thumbnailSizes[i].height == aPictureWidth
    ) {
      smallestArea = area;
      smallestIndex = i;
    }
  }

  aPercentQuality = clamped<uint32_t>(aPercentQuality, 1, 100);
  SetParameter(CAMERA_PARAM_THUMBNAILQUALITY, static_cast<int>(aPercentQuality));

  if (smallestIndex != UINT_MAX) {
    uint32_t w = thumbnailSizes[smallestIndex].width;
    uint32_t h = thumbnailSizes[smallestIndex].height;
    DOM_CAMERA_LOGI("Using thumbnail size: %ux%u, quality: %u %%\n", w, h, aPercentQuality);
    if (w > INT_MAX || h > INT_MAX) {
      DOM_CAMERA_LOGE("Thumbnail dimension is too big, will use defaults\n");
      return;
    }
    SetParameter(CAMERA_PARAM_THUMBNAILWIDTH, static_cast<int>(w));
    SetParameter(CAMERA_PARAM_THUMBNAILHEIGHT, static_cast<int>(h));
  }
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
      nsresult rv = NS_DispatchToMainThread(new CameraErrorResult(ecb, NS_LITERAL_STRING("CANCELLED"), mWindowId));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    GonkCameraHardware::CancelTakePicture(mHwHandle);
  }

  mTakePictureOnSuccessCb = aTakePicture->mOnSuccessCb;
  mTakePictureOnErrorCb = aTakePicture->mOnErrorCb;

  // batch-update camera configuration
  mDeferConfigUpdate = true;

  if (aTakePicture->mSize.width != mLastPictureWidth || aTakePicture->mSize.height != mLastPictureHeight) {
    /**
     * height and width: some drivers are less friendly about getting one of
     * these set to zero, so if either is not specified, ignore both and go
     * with current or default settings.
     */
    if (aTakePicture->mSize.width && aTakePicture->mSize.height) {
      nsCString s;
      s.AppendPrintf("%ux%u", aTakePicture->mSize.width, aTakePicture->mSize.height);
      DOM_CAMERA_LOGI("setting picture size to '%s'\n", s.get());
      SetParameter(CameraParameters::KEY_PICTURE_SIZE, s.get());

      // Choose an appropriate thumbnail size and quality (from 1..100)
      SetupThumbnail(aTakePicture->mSize.width, aTakePicture->mSize.height, 60);
    }

    mLastPictureWidth = aTakePicture->mSize.width;
    mLastPictureHeight = aTakePicture->mSize.height;
  }

  // Picture format -- need to keep it for the callback.
  mFileFormat = aTakePicture->mFileFormat;
  SetParameter(CameraParameters::KEY_PICTURE_FORMAT, NS_ConvertUTF16toUTF8(mFileFormat).get());

  // Convert 'rotation' to a positive value from 0..270 degrees, in steps of 90.
  uint32_t r = static_cast<uint32_t>(aTakePicture->mRotation);
  r += GonkCameraHardware::GetSensorOrientation(mHwHandle);
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
  mStartRecordingOnSuccessCb = aStartRecording->mOnSuccessCb;
  mStartRecordingOnErrorCb = aStartRecording->mOnErrorCb;

  /**
   * Get the base path from device storage and append the app-specified
   * filename to it.  The filename may include a relative subpath
   * (e.g.) "DCIM/IMG_0001.jpg".
   *
   * The camera app needs to provide the file extension '.3gp' for now.
   * See bug 795202.
   */
  nsCOMPtr<nsIFile> filename = aStartRecording->mFolder;
  filename->AppendRelativePath(aStartRecording->mFilename);
  mVideoFile = new DeviceStorageFile(NS_LITERAL_STRING("videos"), filename);

  nsAutoCString nativeFilename;
  filename->GetNativePath(nativeFilename);
  DOM_CAMERA_LOGI("Video filename is '%s'\n", nativeFilename.get());

  if (!mVideoFile->IsSafePath()) {
    DOM_CAMERA_LOGE("Invalid video file name\n");
    return NS_ERROR_INVALID_ARG;
  }

  ScopedClose fd(open(nativeFilename.get(), O_RDWR | O_CREAT, 0644));
  if (fd < 0) {
    DOM_CAMERA_LOGE("Couldn't create file '%s': (%d) %s\n", nativeFilename.get(), errno, strerror(errno));
    return NS_ERROR_FAILURE;
  }

  nsresult rv = SetupRecording(fd, aStartRecording->mOptions.rotation, aStartRecording->mOptions.maxFileSizeBytes, aStartRecording->mOptions.maxVideoLengthMs);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRecorder->start() != OK) {
    DOM_CAMERA_LOGE("mRecorder->start() failed\n");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class RecordingComplete : public nsRunnable
{
public:
  RecordingComplete(DeviceStorageFile* aFile, nsACString& aType)
    : mFile(aFile)
    , mType(aType)
  { }

  ~RecordingComplete() { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsString data;
    CopyASCIItoUTF16(mType, data);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(mFile, "file-watcher-update", data.get());
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsCString mType;
};

nsresult
nsGonkCameraControl::StopRecordingImpl(StopRecordingTask* aStopRecording)
{
  mRecorder->stop();
  delete mRecorder;
  mRecorder = nullptr;

  // notify DeviceStorage that the new video file is closed and ready
  nsCString type(mRecorderProfile->GetFileMimeType());
  nsCOMPtr<nsIRunnable> recordingComplete = new RecordingComplete(mVideoFile, type);
  return NS_DispatchToMainThread(recordingComplete, NS_DISPATCH_NORMAL);
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
  nsCOMPtr<nsIRunnable> autoFocusResult = new AutoFocusResult(aSuccess, mAutoFocusOnSuccessCb, mWindowId);
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
  nsCOMPtr<nsIRunnable> takePictureResult = new TakePictureResult(blob, mTakePictureOnSuccessCb, mWindowId);
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

nsresult
nsGonkCameraControl::SetupVideoMode(const nsAString& aProfile)
{
  // read preferences for camcorder
  mMediaProfiles = MediaProfiles::getInstance();

  nsAutoCString profile = NS_ConvertUTF16toUTF8(aProfile);
  mRecorderProfile = GetGonkRecorderProfileManager().get()->Get(profile.get());
  if (!mRecorderProfile) {
    DOM_CAMERA_LOGE("Recorder profile '%s' is not supported\n", profile.get());
    return NS_ERROR_INVALID_ARG;
  }

  const GonkRecorderVideoProfile* video = mRecorderProfile->GetGonkVideoProfile();
  int width = video->GetWidth();
  int height = video->GetHeight();
  int fps = video->GetFramerate();
  if (fps == -1 || width == -1 || height == -1) {
    DOM_CAMERA_LOGE("Can't configure preview with fps=%d, width=%d, height=%d\n", fps, width, height);
    return NS_ERROR_FAILURE;
  }

  PullParametersImpl();

  // configure camera video recording parameters
  const size_t SIZE = 256;
  char buffer[SIZE];

  mParams.setPreviewSize(width, height);
  mParams.setPreviewFrameRate(fps);

  /**
   * "record-size" is probably deprecated in later ICS;
   * might need to set "video-size" instead of "record-size".
   * See bug 795332.
   */
  snprintf(buffer, SIZE, "%dx%d", width, height);
  mParams.set("record-size", buffer);

  // push the updated camera configuration immediately
  PushParameters();
  return NS_OK;
}

class GonkRecorderListener : public IMediaRecorderClient
{
public:
  GonkRecorderListener(nsGonkCameraControl* aCameraControl)
    : mCameraControl(aCameraControl)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p, aCameraControl=%p\n", __func__, __LINE__, this, mCameraControl.get());
  }

  void notify(int msg, int ext1, int ext2)
  {
    if (mCameraControl) {
      mCameraControl->HandleRecorderEvent(msg, ext1, ext2);
    }
  }

  IBinder* onAsBinder()
  {
    DOM_CAMERA_LOGE("onAsBinder() called, should NEVER get called!\n");
    return nullptr;
  }

protected:
  ~GonkRecorderListener() { }
  nsRefPtr<nsGonkCameraControl> mCameraControl;
};

void
nsGonkCameraControl::HandleRecorderEvent(int msg, int ext1, int ext2)
{
  /**
   * Refer to base/include/media/mediarecorder.h for a complete list
   * of error and info message codes.  There are duplicate values
   * within the status/error code space, as determined by code inspection:
   *
   *    +------- msg
   *    | +----- ext1
   *    | | +--- ext2
   *    V V V
   *    1           MEDIA_RECORDER_EVENT_ERROR
   *      1         MEDIA_RECORDER_ERROR_UNKNOWN
   *        [3]     ERROR_MALFORMED
   *      100       mediaplayer.h::MEDIA_ERROR_SERVER_DIED
   *        0       <always zero>
   *    2           MEDIA_RECORDER_EVENT_INFO
   *      800       MEDIA_RECORDER_INFO_MAX_DURATION_REACHED
   *        0       <always zero>
   *      801       MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED
   *        0       <always zero>
   *      1000      MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS[1b]
   *        [3]     UNKNOWN_ERROR, etc.
   *    100         MEDIA_ERROR[4]
   *      100       mediaplayer.h::MEDIA_ERROR_SERVER_DIED
   *        0       <always zero>
   *    100         MEDIA_RECORDER_TRACK_EVENT_ERROR
   *      100       MEDIA_RECORDER_TRACK_ERROR_GENERAL[1a]
   *        [3]     UNKNOWN_ERROR, etc.
   *      200       MEDIA_RECORDER_ERROR_VIDEO_NO_SYNC_FRAME[2]
   *        ?       <unknown>
   *    101         MEDIA_RECORDER_TRACK_EVENT_INFO
   *      1000      MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS[1a]
   *        [3]     UNKNOWN_ERROR, etc.
   *      N         see mediarecorder.h::media_recorder_info_type[5]
   *
   * 1. a) High 4 bits are the track number, the next 12 bits are reserved,
   *       and the final 16 bits are the actual error code (above).
   *    b) But not in this case.
   * 2. Never actually used in AOSP code?
   * 3. Specific error codes are from utils/Errors.h and/or
   *    include/media/stagefright/MediaErrors.h.
   * 4. Only in frameworks/base/media/libmedia/mediaplayer.cpp.
   * 5. These are mostly informational and we can ignore them; note that
   *    although the MEDIA_RECORDER_INFO_MAX_DURATION_REACHED and
   *    MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED values are defined in this
   *    enum, they are used with different ext1 codes.  /o\
   */
  int trackNum = -1;  // no track

  switch (msg) {
    // Recorder-related events
    case MEDIA_RECORDER_EVENT_INFO:
      switch (ext1) {
        case MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED:
          DOM_CAMERA_LOGI("recorder-event : info: maximum file size reached\n");
          OnRecorderStateChange(NS_LITERAL_STRING("FileSizeLimitReached"), ext2, trackNum);
          return;

        case MEDIA_RECORDER_INFO_MAX_DURATION_REACHED:
          DOM_CAMERA_LOGI("recorder-event : info: maximum video duration reached\n");
          OnRecorderStateChange(NS_LITERAL_STRING("VideoLengthLimitReached"), ext2, trackNum);
          return;

        case MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS:
          DOM_CAMERA_LOGI("recorder-event : info: track completed\n");
          OnRecorderStateChange(NS_LITERAL_STRING("TrackCompleted"), ext2, trackNum);
          return;
      }
      break;

    case MEDIA_RECORDER_EVENT_ERROR:
      switch (ext1) {
        case MEDIA_RECORDER_ERROR_UNKNOWN:
          DOM_CAMERA_LOGE("recorder-event : recorder-error: %d (0x%08x)\n", ext2, ext2);
          OnRecorderStateChange(NS_LITERAL_STRING("MediaRecorderFailed"), ext2, trackNum);
          return;

        case MEDIA_ERROR_SERVER_DIED:
          DOM_CAMERA_LOGE("recorder-event : recorder-error: server died\n");
          OnRecorderStateChange(NS_LITERAL_STRING("MediaServerFailed"), ext2, trackNum);
          return;
      }
      break;

    // Track-related events, see note 1(a) above.
    case MEDIA_RECORDER_TRACK_EVENT_INFO:
      trackNum = (ext1 & 0xF0000000) >> 28;
      ext1 &= 0xFFFF;
      switch (ext1) {
        case MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS:
          if (ext2 == OK) {
            DOM_CAMERA_LOGI("recorder-event : track-complete: track %d, %d (0x%08x)\n", trackNum, ext2, ext2);
            OnRecorderStateChange(NS_LITERAL_STRING("TrackCompleted"), ext2, trackNum);
            return;
          }
          DOM_CAMERA_LOGE("recorder-event : track-error: track %d, %d (0x%08x)\n", trackNum, ext2, ext2);
          OnRecorderStateChange(NS_LITERAL_STRING("TrackFailed"), ext2, trackNum);
          return;

        case MEDIA_RECORDER_TRACK_INFO_PROGRESS_IN_TIME:
          DOM_CAMERA_LOGI("recorder-event : track-info: progress in time: %d ms\n", ext2);
          return;
      }
      break;

    case MEDIA_RECORDER_TRACK_EVENT_ERROR:
      trackNum = (ext1 & 0xF0000000) >> 28;
      ext1 &= 0xFFFF;
      DOM_CAMERA_LOGE("recorder-event : track-error: track %d, %d (0x%08x)\n", trackNum, ext2, ext2);
      OnRecorderStateChange(NS_LITERAL_STRING("TrackFailed"), ext2, trackNum);
      return;
  }

  // All unhandled cases wind up here
  DOM_CAMERA_LOGW("recorder-event : unhandled: msg=%d, ext1=%d, ext2=%d\n", msg, ext1, ext2);
}

nsresult
nsGonkCameraControl::SetupRecording(int aFd, int aRotation, int64_t aMaxFileSizeBytes, int64_t aMaxVideoLengthMs)
{
  // choosing a size big enough to hold the params
  const size_t SIZE = 256;
  char buffer[SIZE];

  mRecorder = new GonkRecorder();
  CHECK_SETARG(mRecorder->init());

  nsresult rv = mRecorderProfile->ConfigureRecorder(mRecorder);
  NS_ENSURE_SUCCESS(rv, rv);

  CHECK_SETARG(mRecorder->setCameraHandle((int32_t)mHwHandle));

  DOM_CAMERA_LOGI("maxVideoLengthMs=%lld\n", aMaxVideoLengthMs);
  if (aMaxVideoLengthMs == 0) {
    aMaxVideoLengthMs = -1;
  }
  snprintf(buffer, SIZE, "max-duration=%lld", aMaxVideoLengthMs);
  CHECK_SETARG(mRecorder->setParameters(String8(buffer)));

  DOM_CAMERA_LOGI("maxFileSizeBytes=%lld\n", aMaxFileSizeBytes);
  if (aMaxFileSizeBytes == 0) {
    aMaxFileSizeBytes = -1;
  }
  snprintf(buffer, SIZE, "max-filesize=%lld", aMaxFileSizeBytes);
  CHECK_SETARG(mRecorder->setParameters(String8(buffer)));

  // adjust rotation by camera sensor offset
  int r = aRotation;
  r += GonkCameraHardware::GetSensorOrientation(mHwHandle, GonkCameraHardware::RAW_SENSOR_ORIENTATION);
  r %= 360;
  r += 45;
  r /= 90;
  r *= 90;
  if (r < 0) {
    // the video recorder only supports positive rotations
    r += 360;
  }
  DOM_CAMERA_LOGI("setting video rotation to %d degrees (mapped from %d)\n", r, aRotation);
  snprintf(buffer, SIZE, "video-param-rotation-angle-degrees=%d", r);
  CHECK_SETARG(mRecorder->setParameters(String8(buffer)));

  CHECK_SETARG(mRecorder->setListener(new GonkRecorderListener(this)));

  // recording API needs file descriptor of output file
  CHECK_SETARG(mRecorder->setOutputFile(aFd, 0, 0));
  CHECK_SETARG(mRecorder->prepare());
  return NS_OK;
}

nsresult
nsGonkCameraControl::GetPreviewStreamVideoModeImpl(GetPreviewStreamVideoModeTask* aGetPreviewStreamVideoMode)
{
  // stop any currently running preview
  StopPreviewInternal(true /* forced */);

  // setup the video mode
  nsresult rv = SetupVideoMode(aGetPreviewStreamVideoMode->mOptions.profile);
  NS_ENSURE_SUCCESS(rv, rv);
  
  const RecorderVideoProfile* video = mRecorderProfile->GetVideoProfile();
  int width = video->GetWidth();
  int height = video->GetHeight();
  int fps = video->GetFramerate();
  DOM_CAMERA_LOGI("recording preview format: %d x %d (%d fps)\n", width, height, fps);

  // create and return new preview stream object
  nsCOMPtr<GetPreviewStreamResult> getPreviewStreamResult = new GetPreviewStreamResult(this, width, height, fps, aGetPreviewStreamVideoMode->mOnSuccessCb, mWindowId);
  rv = NS_DispatchToMainThread(getPreviewStreamResult);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch GetPreviewStreamVideoMode() onSuccess callback to main thread!");
    return rv;
  }

  return NS_OK;
}

already_AddRefed<GonkRecorderProfileManager>
nsGonkCameraControl::GetGonkRecorderProfileManager()
{
  if (!mProfileManager) {
    nsTArray<CameraSize> sizes;
    nsresult rv = GetVideoSizes(sizes);
    NS_ENSURE_SUCCESS(rv, nullptr);

    mProfileManager = new GonkRecorderProfileManager(mCameraId);
    mProfileManager->SetSupportedResolutions(sizes);
  }

  nsRefPtr<GonkRecorderProfileManager> profileMgr = mProfileManager;
  return profileMgr.forget();
}

already_AddRefed<RecorderProfileManager>
nsGonkCameraControl::GetRecorderProfileManagerImpl()
{
  nsRefPtr<RecorderProfileManager> profileMgr = GetGonkRecorderProfileManager();
  return profileMgr.forget();
}

nsresult
nsGonkCameraControl::GetVideoSizes(nsTArray<CameraSize>& aVideoSizes)
{
  aVideoSizes.Clear();

  Vector<Size> sizes;
  mParams.getSupportedVideoSizes(sizes);
  if (sizes.size() == 0) {
    DOM_CAMERA_LOGI("Camera doesn't support video independent of the preview\n");
    mParams.getSupportedPreviewSizes(sizes);
  }

  if (sizes.size() == 0) {
    DOM_CAMERA_LOGW("Camera doesn't report any supported video sizes at all\n");
    return NS_OK;
  }

  for (size_t i = 0; i < sizes.size(); ++i) {
    CameraSize size;
    size.width = sizes[i].width;
    size.height = sizes[i].height;
    aVideoSizes.AppendElement(size);
  }
  return NS_OK;
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

void
OnShutter(nsGonkCameraControl* gc)
{
  gc->OnShutter();
}

void
OnClosed(nsGonkCameraControl* gc)
{
  gc->OnClosed();
}

} // namespace mozilla
