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

#include "GonkCameraControl.h"
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include "base/basictypes.h"
#include "camera/CameraParameters.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsThread.h"
#include <media/MediaProfiles.h>
#include "mozilla/FileUtils.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "nsAlgorithm.h"
#include <media/mediaplayer.h>
#include "nsPrintfCString.h"
#include "nsIObserverService.h"
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#include "AutoRwLock.h"
#include "GonkCameraHwMgr.h"
#include "GonkRecorderProfiles.h"
#include "CameraCommon.h"
#include "GonkCameraParameters.h"
#include "DeviceStorageFileDescriptor.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::ipc;
using namespace android;

#define RETURN_IF_NO_CAMERA_HW()                                          \
  do {                                                                    \
    if (!mCameraHw.get()) {                                               \
      DOM_CAMERA_LOGE("%s:%d : mCameraHw is null\n", __func__, __LINE__); \
      return NS_ERROR_NOT_AVAILABLE;                                      \
    }                                                                     \
  } while(0)

// Construct nsGonkCameraControl on the main thread.
nsGonkCameraControl::nsGonkCameraControl(uint32_t aCameraId)
  : CameraControlImpl(aCameraId)
  , mLastPictureSize({0, 0})
  , mLastThumbnailSize({0, 0})
  , mPreviewFps(30)
  , mResumePreviewAfterTakingPicture(false) // XXXmikeh - see bug 950102
  , mDeferConfigUpdate(0)
  , mMediaProfiles(nullptr)
  , mRecorder(nullptr)
  , mProfileManager(nullptr)
  , mRecorderProfile(nullptr)
  , mVideoFile(nullptr)
  , mReentrantMonitor("GonkCameraControl::OnTakePictureMonitor")
{
  // Constructor runs on the main thread...
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mImageContainer = LayerManager::CreateImageContainer();
}

nsresult
nsGonkCameraControl::StartImpl(const Configuration* aInitialConfig)
{
  /**
   * For initialization, we try to return the camera control to the upper
   * upper layer (i.e. the DOM) as quickly as possible. To do this, the
   * camera is initialized in the following stages:
   *
   *  0. Initialize() initializes the hardware;
   *  1. SetConfigurationInternal() does the minimal configuration
   *     required so that we can start the preview -and- report a valid
   *     configuration to the upper layer;
   *  2. OnHardwareStateChange() reports that the hardware is ready,
   *     which the upper (e.g. DOM) layer can (and does) use to return
   *     the camera control object;
   *  3. StartPreviewImpl() starts the flow of preview frames from the
   *     camera hardware.
   *
   * The intent of the above flow is to let the Main Thread do as much work
   * up-front as possible without waiting for blocking Camera Thread calls
   * to complete.
   */
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  nsresult rv = Initialize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aInitialConfig) {
    rv = SetConfigurationInternal(*aInitialConfig);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // The initial configuration failed, close up the hardware
      StopImpl();
      return rv;
    }
  }

  OnHardwareStateChange(CameraControlListener::kHardwareOpen);
  return StartPreviewImpl();
}

nsresult
nsGonkCameraControl::Initialize()
{
  mCameraHw = GonkCameraHardware::Connect(this, mCameraId);
  if (!mCameraHw.get()) {
    DOM_CAMERA_LOGE("Failed to connect to camera %d (this=%p)\n", mCameraId, this);
    return NS_ERROR_FAILURE;
  }

  DOM_CAMERA_LOGI("Initializing camera %d (this=%p, mCameraHw=%p)\n", mCameraId, this, mCameraHw.get());

  // Initialize our camera configuration database.
  PullParametersImpl();

  // Set preferred preview frame format.
  mParams.Set(CAMERA_PARAM_PREVIEWFORMAT, NS_LITERAL_STRING("yuv420sp"));
  PushParametersImpl();

  // Grab any other settings we'll need later.
  mParams.Get(CAMERA_PARAM_PICTURE_FILEFORMAT, mFileFormat);
  mParams.Get(CAMERA_PARAM_THUMBNAILSIZE, mLastThumbnailSize);

  // The emulator's camera returns -1 for these values; bump them up to 0
  int areas;
  mParams.Get(CAMERA_PARAM_SUPPORTED_MAXMETERINGAREAS, areas);
  mCurrentConfiguration.mMaxMeteringAreas = areas != -1 ? areas : 0;
  mParams.Get(CAMERA_PARAM_SUPPORTED_MAXFOCUSAREAS, areas);
  mCurrentConfiguration.mMaxFocusAreas = areas != -1 ? areas : 0;

  mParams.Get(CAMERA_PARAM_PICTURE_SIZE, mLastPictureSize);
  mParams.Get(CAMERA_PARAM_PREVIEWSIZE, mCurrentConfiguration.mPreviewSize);
  mParams.Get(CAMERA_PARAM_VIDEOSIZE, mLastRecorderSize);

  DOM_CAMERA_LOGI(" - maximum metering areas:        %u\n", mCurrentConfiguration.mMaxMeteringAreas);
  DOM_CAMERA_LOGI(" - maximum focus areas:           %u\n", mCurrentConfiguration.mMaxFocusAreas);
  DOM_CAMERA_LOGI(" - default picture size:          %u x %u\n",
    mLastPictureSize.width, mLastPictureSize.height);
  DOM_CAMERA_LOGI(" - default thumbnail size:        %u x %u\n",
    mLastThumbnailSize.width, mLastThumbnailSize.height);
  DOM_CAMERA_LOGI(" - default preview size:          %u x %u\n",
    mCurrentConfiguration.mPreviewSize.width, mCurrentConfiguration.mPreviewSize.height);
  DOM_CAMERA_LOGI(" - default video recorder size:   %u x %u\n",
    mLastRecorderSize.width, mLastRecorderSize.height);
  DOM_CAMERA_LOGI(" - default picture file format:   %s\n",
    NS_ConvertUTF16toUTF8(mFileFormat).get());

  return NS_OK;
}

nsGonkCameraControl::~nsGonkCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mCameraHw = %p\n", __func__, __LINE__, this, mCameraHw.get());

  StopImpl();
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
}

nsresult
nsGonkCameraControl::SetConfigurationInternal(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  nsresult rv;

  switch (aConfig.mMode) {
    case kPictureMode:
      rv = SetPictureConfiguration(aConfig);
      break;

    case kVideoMode:
      rv = SetVideoConfiguration(aConfig);
      break;

    default:
      MOZ_ASSUME_UNREACHABLE("Unanticipated camera mode in SetConfigurationInternal()");
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentConfiguration.mMode = aConfig.mMode;
  mCurrentConfiguration.mRecorderProfile = aConfig.mRecorderProfile;
  if (aConfig.mMode == kVideoMode) {
    mCurrentConfiguration.mPreviewSize = mLastRecorderSize;
  }

  OnConfigurationChange();
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetConfigurationImpl(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  // Stop any currently running preview
  nsresult rv = PausePreview();
  if (NS_FAILED(rv)) {
    // warn, but plow ahead
    NS_WARNING("PausePreview() in SetConfigurationImpl() failed");
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  rv = SetConfigurationInternal(aConfig);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    StopPreviewImpl();
    return rv;
  }

  // Restart the preview
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  return StartPreviewImpl();
}

nsresult
nsGonkCameraControl::SetPictureConfiguration(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  // remove any existing recorder profile
  mRecorderProfile = nullptr;

  nsresult rv = SetPreviewSize(aConfig.mPreviewSize);
  NS_ENSURE_SUCCESS(rv, rv);

  mParams.Get(CAMERA_PARAM_PREVIEWFRAMERATE, mPreviewFps);

  DOM_CAMERA_LOGI("picture mode preview: wanted %ux%u, got %ux%u (%u fps)\n",
    aConfig.mPreviewSize.width, aConfig.mPreviewSize.height,
    mCurrentConfiguration.mPreviewSize.width, mCurrentConfiguration.mPreviewSize.height,
    mPreviewFps);

  return NS_OK;
}

nsresult
nsGonkCameraControl::SetVideoConfiguration(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  nsresult rv = SetupVideoMode(aConfig.mRecorderProfile);
  NS_ENSURE_SUCCESS(rv, rv);

  DOM_CAMERA_LOGI("video mode preview: profile '%s', got %ux%u (%u fps)\n",
    NS_ConvertUTF16toUTF8(aConfig.mRecorderProfile).get(),
    mLastRecorderSize.width, mLastRecorderSize.height,
    mPreviewFps);

  return rv;
}

// Parameter management.
nsresult
nsGonkCameraControl::PushParameters()
{
  uint32_t dcu = mDeferConfigUpdate;
  if (dcu > 0) {
    DOM_CAMERA_LOGI("Defering config update (nest level %u)\n", dcu);
    return NS_OK;
  }

  /**
   * If we're already on the camera thread, call PushParametersImpl()
   * directly, so that it executes synchronously.  Some callers
   * require this so that changes take effect immediately before
   * we can proceed.
   */
  if (NS_GetCurrentThread() != mCameraThread) {
    DOM_CAMERA_LOGT("%s:%d - dispatching to Camera Thread\n", __func__, __LINE__);
    nsCOMPtr<nsIRunnable> pushParametersTask =
      NS_NewRunnableMethod(this, &nsGonkCameraControl::PushParametersImpl);
    return mCameraThread->Dispatch(pushParametersTask, NS_DISPATCH_NORMAL);
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  return PushParametersImpl();
}

void
nsGonkCameraControl::BeginBatchParameterSet()
{
  uint32_t dcu = ++mDeferConfigUpdate;
  if (dcu == 0) {
    NS_WARNING("Overflow weirdness incrementing mDeferConfigUpdate!");
    MOZ_CRASH();
  }
  DOM_CAMERA_LOGI("Begin deferring camera configuration updates (nest level %u)\n", dcu);
}

void
nsGonkCameraControl::EndBatchParameterSet()
{
  uint32_t dcu = mDeferConfigUpdate--;
  if (dcu == 0) {
    NS_WARNING("Underflow badness decrementing mDeferConfigUpdate!");
    MOZ_CRASH();
  }
  DOM_CAMERA_LOGI("End deferring camera configuration updates (nest level %u)\n", dcu);

  if (dcu == 1) {
    PushParameters();
  }
}

template<class T> nsresult
nsGonkCameraControl::SetAndPush(uint32_t aKey, const T& aValue)
{
  nsresult rv = mParams.Set(aKey, aValue);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Camera parameter aKey=%d failed to set (0x%x)\n", aKey, rv);
    return rv;
  }
  return PushParameters();
}

// Array-of-Size parameter accessor.
nsresult
nsGonkCameraControl::Get(uint32_t aKey, nsTArray<Size>& aSizes)
{
  if (aKey == CAMERA_PARAM_SUPPORTED_VIDEOSIZES) {
    nsresult rv = mParams.Get(aKey, aSizes);
    if (aSizes.Length() != 0) {
      return rv;
    }
    DOM_CAMERA_LOGI("Camera doesn't support video independent of the preview\n");
    aKey = CAMERA_PARAM_SUPPORTED_PREVIEWSIZES;
  }

  return mParams.Get(aKey, aSizes);
}

// Array-of-doubles parameter accessor.
nsresult
nsGonkCameraControl::Get(uint32_t aKey, nsTArray<double>& aValues)
{
  return mParams.Get(aKey, aValues);
}

// Array-of-nsString parameter accessor.
nsresult
nsGonkCameraControl::Get(uint32_t aKey, nsTArray<nsString>& aValues)
{
  return mParams.Get(aKey, aValues);
}

// nsString-valued parameter accessors
nsresult
nsGonkCameraControl::Set(uint32_t aKey, const nsAString& aValue)
{
  nsresult rv = mParams.Set(aKey, aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aKey == CAMERA_PARAM_PICTURE_FILEFORMAT) {
    // Picture format -- need to keep it for the TakePicture() callback.
    mFileFormat = aValue;
  }

  return PushParameters();
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, nsAString& aRet)
{
  return mParams.Get(aKey, aRet);
}

// Double-valued parameter accessors
nsresult
nsGonkCameraControl::Set(uint32_t aKey, double aValue)
{
  return SetAndPush(aKey, aValue);
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, double& aRet)
{
  return mParams.Get(aKey, aRet);
}

// Signed-64-bit parameter accessors.
nsresult
nsGonkCameraControl::Set(uint32_t aKey, int64_t aValue)
{
  return SetAndPush(aKey, aValue);
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, int64_t& aRet)
{
  return mParams.Get(aKey, aRet);
}

// Weighted-region parameter accessors.
nsresult
nsGonkCameraControl::Set(uint32_t aKey, const nsTArray<Region>& aRegions)
{
  return SetAndPush(aKey, aRegions);
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, nsTArray<Region>& aRegions)
{
  return mParams.Get(aKey, aRegions);
}

// Singleton-size parameter accessors.
nsresult
nsGonkCameraControl::Set(uint32_t aKey, const Size& aSize)
{
  switch (aKey) {
    case CAMERA_PARAM_PICTURESIZE:
      DOM_CAMERA_LOGI("setting picture size to %ux%u\n", aSize.width, aSize.height);
      return SetPictureSize(aSize);

    case CAMERA_PARAM_THUMBNAILSIZE:
      DOM_CAMERA_LOGI("setting thumbnail size to %ux%u\n", aSize.width, aSize.height);
      return SetThumbnailSize(aSize);

    default:
      return SetAndPush(aKey, aSize);
  }
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, Size& aSize)
{
  return mParams.Get(aKey, aSize);
}

// Signed int parameter accessors.
nsresult
nsGonkCameraControl::Set(uint32_t aKey, int aValue)
{
  if (aKey == CAMERA_PARAM_PICTURE_ROTATION) {
    RETURN_IF_NO_CAMERA_HW();
    aValue = RationalizeRotation(aValue + mCameraHw->GetSensorOrientation());
  }
  return SetAndPush(aKey, aValue);
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, int& aRet)
{
  if (aKey == CAMERA_PARAM_SENSORANGLE) {
    RETURN_IF_NO_CAMERA_HW();
    aRet = mCameraHw->GetSensorOrientation();
    return NS_OK;
  }

  return mParams.Get(aKey, aRet);
}

// GPS location parameter accessors.
nsresult
nsGonkCameraControl::SetLocation(const Position& aLocation)
{
  return SetAndPush(CAMERA_PARAM_PICTURE_LOCATION, aLocation);
}

nsresult
nsGonkCameraControl::StartPreviewImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mPreviewState == CameraControlListener::kPreviewStarted) {
    DOM_CAMERA_LOGW("Camera preview already started, nothing to do\n");
    return NS_OK;
  }

  DOM_CAMERA_LOGI("Starting preview (this=%p)\n", this);

  if (mCameraHw->StartPreview() != OK) {
    DOM_CAMERA_LOGE("Failed to start camera preview\n");
    return NS_ERROR_FAILURE;
  }

  OnPreviewStateChange(CameraControlListener::kPreviewStarted);
  return NS_OK;
}

nsresult
nsGonkCameraControl::StopPreviewImpl()
{
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Stopping preview (this=%p)\n", this);

  mCameraHw->StopPreview();
  OnPreviewStateChange(CameraControlListener::kPreviewStopped);
  return NS_OK;
}

nsresult
nsGonkCameraControl::PausePreview()
{
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Pausing preview (this=%p)\n", this);

  mCameraHw->StopPreview();
  OnPreviewStateChange(CameraControlListener::kPreviewPaused);
  return NS_OK;
}

nsresult
nsGonkCameraControl::AutoFocusImpl(bool aCancelExistingCall)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();
  if (aCancelExistingCall) {
    if (mCameraHw.get()) {
      mCameraHw->CancelAutoFocus();
    }
  }

  if (mCameraHw->AutoFocus() != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetThumbnailSizeImpl(const Size& aSize)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  /**
   * We keep a copy of the specified size so that if the picture size
   * changes, we can choose a new thumbnail size close to what was asked for
   * last time.
   */
  mLastThumbnailSize = aSize;

  /**
   * If either of width or height is zero, set the other to zero as well.
   * This should disable inclusion of a thumbnail in the final picture.
   */
  if (!aSize.width || !aSize.height) {
    DOM_CAMERA_LOGW("Requested thumbnail size %ux%u, disabling thumbnail\n",
      aSize.width, aSize.height);
    Size size = { 0, 0 };
    return SetAndPush(CAMERA_PARAM_THUMBNAILSIZE, size);
  }

  /**
   * Choose the supported thumbnail size that is closest to the specified size.
   * Some drivers will fail to take a picture if the thumbnail does not have
   * the same aspect ratio as the set picture size, so we need to enforce that
   * too.
   */
  int smallestDelta = INT_MAX;
  uint32_t smallestDeltaIndex = UINT32_MAX;
  int targetArea = aSize.width * aSize.height;

  nsAutoTArray<Size, 8> supportedSizes;
  Get(CAMERA_PARAM_SUPPORTED_JPEG_THUMBNAIL_SIZES, supportedSizes);

  for (uint32_t i = 0; i < supportedSizes.Length(); ++i) {
    int area = supportedSizes[i].width * supportedSizes[i].height;
    int delta = abs(area - targetArea);

    if (area != 0
      && delta < smallestDelta
      && supportedSizes[i].width * mLastPictureSize.height /
         supportedSizes[i].height == mLastPictureSize.width
    ) {
      smallestDelta = delta;
      smallestDeltaIndex = i;
    }
  }

  if (smallestDeltaIndex == UINT32_MAX) {
    DOM_CAMERA_LOGW("Unable to find a thumbnail size close to %ux%u, disabling thumbnail\n",
      aSize.width, aSize.height);
    // If we are unable to find a thumbnail size with a suitable aspect ratio,
    // just disable the thumbnail altogether.
    Size size = { 0, 0 };
    return SetAndPush(CAMERA_PARAM_THUMBNAILSIZE, size);
  }

  Size size = supportedSizes[smallestDeltaIndex];
  DOM_CAMERA_LOGI("camera-param set thumbnail-size = %ux%u (requested %ux%u)\n",
    size.width, size.height, aSize.width, aSize.height);
  if (size.width > INT32_MAX || size.height > INT32_MAX) {
    DOM_CAMERA_LOGE("Supported thumbnail size is too big, no change\n");
    return NS_ERROR_FAILURE;
  }

  return SetAndPush(CAMERA_PARAM_THUMBNAILSIZE, size);
}

nsresult
nsGonkCameraControl::SetThumbnailSize(const Size& aSize)
{
  class SetThumbnailSize : public nsRunnable
  {
  public:
    SetThumbnailSize(nsGonkCameraControl* aCameraControl, const Size& aSize)
      : mCameraControl(aCameraControl)
      , mSize(aSize)
    {
      MOZ_COUNT_CTOR(SetThumbnailSize);
    }
    ~SetThumbnailSize() { MOZ_COUNT_DTOR(SetThumbnailSize); }

    NS_IMETHODIMP
    Run() MOZ_OVERRIDE
    {
      nsresult rv = mCameraControl->SetThumbnailSizeImpl(mSize);
      if (NS_FAILED(rv)) {
        mCameraControl->OnError(CameraControlListener::kInUnspecified,
                                CameraControlListener::kErrorSetThumbnailSizeFailed);
      }
      return NS_OK;
    }

  protected:
    nsRefPtr<nsGonkCameraControl> mCameraControl;
    Size mSize;
  };

  if (NS_GetCurrentThread() == mCameraThread) {
    return SetThumbnailSizeImpl(aSize);
  }

  return mCameraThread->Dispatch(new SetThumbnailSize(this, aSize), NS_DISPATCH_NORMAL);
}

nsresult
nsGonkCameraControl::UpdateThumbnailSize()
{
  return SetThumbnailSize(mLastThumbnailSize);
}

nsresult
nsGonkCameraControl::SetPictureSizeImpl(const Size& aSize)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  /**
   * Some drivers are less friendly about getting one of these set to zero,
   * so if either is not specified, ignore both and go with current or
   * default settings.
   */
  if (!aSize.width || !aSize.height) {
    DOM_CAMERA_LOGW("Ignoring requested picture size of %ux%u\n", aSize.width, aSize.height);
    return NS_ERROR_INVALID_ARG;
  }

  if (aSize.width == mLastPictureSize.width && aSize.height == mLastPictureSize.height) {
    DOM_CAMERA_LOGI("Requested picture size %ux%u unchanged\n", aSize.width, aSize.height);
    return NS_OK;
  }

  /**
   * Choose the supported picture size that is closest in area to the
   * specified size. Some drivers will fail to take a picture if the
   * thumbnail size is not the same aspect ratio, so we update that
   * as well to a size closest to the last user-requested one.
   */
  int smallestDelta = INT_MAX;
  uint32_t smallestDeltaIndex = UINT32_MAX;
  int targetArea = aSize.width * aSize.height;

  nsAutoTArray<Size, 8> supportedSizes;
  Get(CAMERA_PARAM_SUPPORTED_PICTURESIZES, supportedSizes);

  for (uint32_t i = 0; i < supportedSizes.Length(); ++i) {
    int area = supportedSizes[i].width * supportedSizes[i].height;
    int delta = abs(area - targetArea);

    if (area != 0 && delta < smallestDelta) {
      smallestDelta = delta;
      smallestDeltaIndex = i;
    }
  }

  if (smallestDeltaIndex == UINT32_MAX) {
    DOM_CAMERA_LOGW("Unable to find a picture size close to %ux%u\n",
      aSize.width, aSize.height);
    return NS_ERROR_INVALID_ARG;
  }

  Size size = supportedSizes[smallestDeltaIndex];
  DOM_CAMERA_LOGI("camera-param set picture-size = %ux%u (requested %ux%u)\n",
    size.width, size.height, aSize.width, aSize.height);
  if (size.width > INT32_MAX || size.height > INT32_MAX) {
    DOM_CAMERA_LOGE("Supported picture size is too big, no change\n");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mParams.Set(CAMERA_PARAM_PICTURESIZE, size);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mLastPictureSize = size;

  // Finally, update the thumbnail size in case the picture
  // aspect ratio changed.
  return UpdateThumbnailSize();
}

int32_t
nsGonkCameraControl::RationalizeRotation(int32_t aRotation)
{
  int32_t r = aRotation;

  // The result of this operation is an angle from 0..270 degrees,
  // in steps of 90 degrees. Angles are rounded to the nearest
  // magnitude, so 45 will be rounded to 90, and -45 will be rounded
  // to -90 (not 0).
  if (r >= 0) {
    r += 45;
  } else {
    r -= 45;
  }
  r /= 90;
  r %= 4;
  r *= 90;
  if (r < 0) {
    r += 360;
  }

  return r;
}

nsresult
nsGonkCameraControl::SetPictureSize(const Size& aSize)
{
  class SetPictureSize : public nsRunnable
  {
  public:
    SetPictureSize(nsGonkCameraControl* aCameraControl, const Size& aSize)
      : mCameraControl(aCameraControl)
      , mSize(aSize)
    {
      MOZ_COUNT_CTOR(SetPictureSize);
    }
    ~SetPictureSize() { MOZ_COUNT_DTOR(SetPictureSize); }

    NS_IMETHODIMP
    Run() MOZ_OVERRIDE
    {
      nsresult rv = mCameraControl->SetPictureSizeImpl(mSize);
      if (NS_FAILED(rv)) {
        mCameraControl->OnError(CameraControlListener::kInUnspecified,
                                CameraControlListener::kErrorSetPictureSizeFailed);
      }
      return NS_OK;
    }

  protected:
    nsRefPtr<nsGonkCameraControl> mCameraControl;
    Size mSize;
  };

  if (NS_GetCurrentThread() == mCameraThread) {
    return SetPictureSizeImpl(aSize);
  }

  return mCameraThread->Dispatch(new SetPictureSize(this, aSize), NS_DISPATCH_NORMAL);
}

nsresult
nsGonkCameraControl::TakePictureImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  if (mCameraHw->TakePicture() != OK) {
    return NS_ERROR_FAILURE;
  }

  // In Gonk, taking a picture implicitly stops the preview stream,
  // so we need to reflect that here.
  OnPreviewStateChange(CameraControlListener::kPreviewPaused);
  return NS_OK;
}

nsresult
nsGonkCameraControl::PushParametersImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  DOM_CAMERA_LOGI("Pushing camera parameters\n");
  RETURN_IF_NO_CAMERA_HW();

  if (mCameraHw->PushParameters(mParams) != OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsGonkCameraControl::PullParametersImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  DOM_CAMERA_LOGI("Pulling camera parameters\n");
  RETURN_IF_NO_CAMERA_HW();

  return mCameraHw->PullParameters(mParams);
}

nsresult
nsGonkCameraControl::StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                        const StartRecordingOptions* aOptions)
{
  NS_ENSURE_TRUE(mRecorderProfile, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_FALSE(mRecorder, NS_ERROR_FAILURE);

  /**
   * Get the base path from device storage and append the app-specified
   * filename to it.  The filename may include a relative subpath
   * (e.g.) "DCIM/IMG_0001.jpg".
   *
   * The camera app needs to provide the file extension '.3gp' for now.
   * See bug 795202.
   */
  NS_ENSURE_TRUE(aFileDescriptor, NS_ERROR_FAILURE);
  nsAutoString fullPath;
  mVideoFile = aFileDescriptor->mDSFile;
  mVideoFile->GetFullPath(fullPath);
  DOM_CAMERA_LOGI("Video filename is '%s'\n",
                  NS_LossyConvertUTF16toASCII(fullPath).get());

  if (!mVideoFile->IsSafePath()) {
    DOM_CAMERA_LOGE("Invalid video file name\n");
    return NS_ERROR_INVALID_ARG;
  }

  // SetupRecording creates a dup of the file descriptor, so we need to
  // close the file descriptor when we leave this function. Also note, that
  // since we're already off the main thread, we don't need to dispatch this.
  // We just let the CloseFileRunnable destructor do the work.
  nsRefPtr<CloseFileRunnable> closer;
  if (aFileDescriptor->mFileDescriptor.IsValid()) {
    closer = new CloseFileRunnable(aFileDescriptor->mFileDescriptor);
  }
  nsresult rv;
  int fd = aFileDescriptor->mFileDescriptor.PlatformHandle();
  if (aOptions) {
    rv = SetupRecording(fd, aOptions->rotation, aOptions->maxFileSizeBytes,
                        aOptions->maxVideoLengthMs);
  } else {
    rv = SetupRecording(fd, 0, 0, 0);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRecorder->start() != OK) {
    DOM_CAMERA_LOGE("mRecorder->start() failed\n");
    // important: we MUST destroy the recorder if start() fails!
    mRecorder = nullptr;
    return NS_ERROR_FAILURE;
  }

  OnRecorderStateChange(CameraControlListener::kRecorderStarted);
  return NS_OK;
}

nsresult
nsGonkCameraControl::StopRecordingImpl()
{
  class RecordingComplete : public nsRunnable
  {
  public:
    RecordingComplete(DeviceStorageFile* aFile)
      : mFile(aFile)
    { }

    ~RecordingComplete() { }

    NS_IMETHODIMP
    Run()
    {
      MOZ_ASSERT(NS_IsMainThread());

      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      obs->NotifyObservers(mFile, "file-watcher-notify", NS_LITERAL_STRING("modified").get());
      return NS_OK;
    }

  private:
    nsRefPtr<DeviceStorageFile> mFile;
  };

  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  // nothing to do if we have no mRecorder
  NS_ENSURE_TRUE(mRecorder, NS_OK);

  mRecorder->stop();
  mRecorder = nullptr;
  OnRecorderStateChange(CameraControlListener::kRecorderStopped);

  // notify DeviceStorage that the new video file is closed and ready
  return NS_DispatchToMainThread(new RecordingComplete(mVideoFile), NS_DISPATCH_NORMAL);
}

void
nsGonkCameraControl::OnAutoFocusComplete(bool aSuccess)
{
  class AutoFocusComplete : public nsRunnable
  {
  public:
    AutoFocusComplete(nsGonkCameraControl* aCameraControl, bool aSuccess)
      : mCameraControl(aCameraControl)
      , mSuccess(aSuccess)
    { }

    NS_IMETHODIMP
    Run() MOZ_OVERRIDE
    {
      mCameraControl->OnAutoFocusComplete(mSuccess);
      return NS_OK;
    }

  protected:
    nsRefPtr<nsGonkCameraControl> mCameraControl;
    bool mSuccess;
  };

  if (NS_GetCurrentThread() == mCameraThread) {
    /**
     * Auto focusing can change some of the camera's parameters, so
     * we need to pull a new set before notifying any clients.
     */
    PullParametersImpl();
    CameraControlImpl::OnAutoFocusComplete(aSuccess);
    return;
  }

  /**
   * Because the callback needs to call PullParametersImpl(),
   * we need to dispatch this callback through the Camera Thread.
   */
  mCameraThread->Dispatch(new AutoFocusComplete(this, aSuccess), NS_DISPATCH_NORMAL);
}

void
nsGonkCameraControl::OnTakePictureComplete(uint8_t* aData, uint32_t aLength)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  uint8_t* data = new uint8_t[aLength];

  memcpy(data, aData, aLength);

  nsString s(NS_LITERAL_STRING("image/"));
  s.Append(mFileFormat);
  DOM_CAMERA_LOGI("Got picture, type '%s', %u bytes\n", NS_ConvertUTF16toUTF8(s).get(), aLength);
  OnTakePictureComplete(data, aLength, s);

  if (mResumePreviewAfterTakingPicture) {
    nsresult rv = StartPreview();
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to restart camera preview (%x)\n", rv);
      OnPreviewStateChange(CameraControlListener::kPreviewStopped);
    }
  }

  DOM_CAMERA_LOGI("nsGonkCameraControl::OnTakePictureComplete() done\n");
}

void
nsGonkCameraControl::OnTakePictureError()
{
  CameraControlImpl::OnError(CameraControlListener::kInTakePicture,
                             CameraControlListener::kErrorApiFailed);
}

nsresult
nsGonkCameraControl::SetPreviewSize(const Size& aSize)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  nsTArray<Size> previewSizes;
  nsresult rv = mParams.Get(CAMERA_PARAM_SUPPORTED_PREVIEWSIZES, previewSizes);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("Camera failed to return any preview sizes (0x%x)\n", rv);
    return rv;
  }

  Size best = aSize;
  uint32_t minSizeDelta = UINT32_MAX;
  uint32_t delta;

  if (!aSize.width && !aSize.height) {
    // no size specified, take the first supported size
    best = previewSizes[0];
  } else if (aSize.width && aSize.height) {
    // both height and width specified, find the supported size closest to requested size
    uint32_t targetArea = aSize.width * aSize.height;
    for (uint32_t i = 0; i < previewSizes.Length(); i++) {
      Size size = previewSizes[i];
      uint32_t delta = abs((long int)(size.width * size.height - targetArea));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
      }
    }
  } else if (!aSize.width) {
    // width not specified, find closest height match
    for (uint32_t i = 0; i < previewSizes.Length(); i++) {
      Size size = previewSizes[i];
      delta = abs((long int)(size.height - aSize.height));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
      }
    }
  } else if (!aSize.height) {
    // height not specified, find closest width match
    for (uint32_t i = 0; i < previewSizes.Length(); i++) {
      Size size = previewSizes[i];
      delta = abs((long int)(size.width - aSize.width));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
      }
    }
  }

  // Some camera drivers will ignore our preview size if it's larger
  // that the currently set video recording size, so we need to set
  // both here just in case.
  mParams.Set(CAMERA_PARAM_PREVIEWSIZE, best);
  mParams.Set(CAMERA_PARAM_VIDEOSIZE, best);
  mCurrentConfiguration.mPreviewSize = best;
  return PushParameters();
}

nsresult
nsGonkCameraControl::SetupVideoMode(const nsAString& aProfile)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

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
  if (fps == -1 || width < 0 || height < 0) {
    DOM_CAMERA_LOGE("Can't configure preview with fps=%d, width=%d, height=%d\n",
      fps, width, height);
    return NS_ERROR_FAILURE;
  }

  PullParametersImpl();

  Size size;
  size.width = static_cast<uint32_t>(width);
  size.height = static_cast<uint32_t>(height);

  {
    ICameraControlParameterSetAutoEnter set(this);

    // The camera interface allows for hardware to provide two video
    //  streams, a low resolution preview and a potentially high resolution
    //  stream for encoding. For now we don't use this and set preview and video
    //  size to the same thing.
    nsresult rv = SetAndPush(CAMERA_PARAM_PREVIEWSIZE, size);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to set video mode preview size (0x%x)\n", rv);
      return rv;
    }

    rv = SetAndPush(CAMERA_PARAM_VIDEOSIZE, size);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to set video mode video size (0x%x)\n", rv);
      return rv;
    }

    rv = SetAndPush(CAMERA_PARAM_PREVIEWFRAMERATE, fps);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to set video mode frame rate (0x%x)\n", rv);
      return rv;
    }
    mPreviewFps = fps;
  }

  mLastRecorderSize = size;
  return NS_OK;
}

class GonkRecorderListener : public IMediaRecorderClient
{
public:
  GonkRecorderListener(nsGonkCameraControl* aCameraControl)
    : mCameraControl(aCameraControl)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p, aCameraControl=%p\n",
      __func__, __LINE__, this, mCameraControl.get());
  }

  void notify(int msg, int ext1, int ext2)
  {
    if (mCameraControl) {
      mCameraControl->OnRecorderEvent(msg, ext1, ext2);
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
nsGonkCameraControl::OnRecorderEvent(int msg, int ext1, int ext2)
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
  int trackNum = CameraControlListener::kNoTrackNumber;

  switch (msg) {
    // Recorder-related events
    case MEDIA_RECORDER_EVENT_INFO:
      switch (ext1) {
        case MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED:
          DOM_CAMERA_LOGI("recorder-event : info: maximum file size reached\n");
          OnRecorderStateChange(CameraControlListener::kFileSizeLimitReached, ext2, trackNum);
          return;

        case MEDIA_RECORDER_INFO_MAX_DURATION_REACHED:
          DOM_CAMERA_LOGI("recorder-event : info: maximum video duration reached\n");
          OnRecorderStateChange(CameraControlListener::kVideoLengthLimitReached, ext2, trackNum);
          return;

        case MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS:
          DOM_CAMERA_LOGI("recorder-event : info: track completed\n");
          OnRecorderStateChange(CameraControlListener::kTrackCompleted, ext2, trackNum);
          return;
      }
      break;

    case MEDIA_RECORDER_EVENT_ERROR:
      switch (ext1) {
        case MEDIA_RECORDER_ERROR_UNKNOWN:
          DOM_CAMERA_LOGE("recorder-event : recorder-error: %d (0x%08x)\n", ext2, ext2);
          OnRecorderStateChange(CameraControlListener::kMediaRecorderFailed, ext2, trackNum);
          return;

        case MEDIA_ERROR_SERVER_DIED:
          DOM_CAMERA_LOGE("recorder-event : recorder-error: server died\n");
          OnRecorderStateChange(CameraControlListener::kMediaServerFailed, ext2, trackNum);
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
            OnRecorderStateChange(CameraControlListener::kTrackCompleted, ext2, trackNum);
            return;
          }
          DOM_CAMERA_LOGE("recorder-event : track-error: track %d, %d (0x%08x)\n", trackNum, ext2, ext2);
          OnRecorderStateChange(CameraControlListener::kTrackFailed, ext2, trackNum);
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
      OnRecorderStateChange(CameraControlListener::kTrackFailed, ext2, trackNum);
      return;
  }

  // All unhandled cases wind up here
  DOM_CAMERA_LOGW("recorder-event : unhandled: msg=%d, ext1=%d, ext2=%d\n", msg, ext1, ext2);
}

nsresult
nsGonkCameraControl::SetupRecording(int aFd, int aRotation, int64_t aMaxFileSizeBytes, int64_t aMaxVideoLengthMs)
{
  RETURN_IF_NO_CAMERA_HW();

  // choosing a size big enough to hold the params
  const size_t SIZE = 256;
  char buffer[SIZE];

  mRecorder = new GonkRecorder();
  CHECK_SETARG(mRecorder->init());

  nsresult rv = mRecorderProfile->ConfigureRecorder(mRecorder);
  NS_ENSURE_SUCCESS(rv, rv);

  CHECK_SETARG(mRecorder->setCamera(mCameraHw));

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
  r += mCameraHw->GetSensorOrientation();
  r = RationalizeRotation(r);
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
nsGonkCameraControl::StopImpl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  // if we're recording, stop recording
  if (mRecorder) {
    DOM_CAMERA_LOGI("Stopping existing video recorder\n");
    mRecorder->stop();
    mRecorder = nullptr;
    OnRecorderStateChange(CameraControlListener::kRecorderStopped);
  }

  // stop the preview
  StopPreviewImpl();

  // release the hardware handle
  if (mCameraHw.get()){
     mCameraHw->Close();
     mCameraHw.clear();
  }

  OnHardwareStateChange(CameraControlListener::kHardwareClosed);
  return NS_OK;
}

already_AddRefed<GonkRecorderProfileManager>
nsGonkCameraControl::GetGonkRecorderProfileManager()
{
  if (!mProfileManager) {
    nsTArray<Size> sizes;
    nsresult rv = Get(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, sizes);
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

void
nsGonkCameraControl::OnNewPreviewFrame(layers::GraphicBufferLocked* aBuffer)
{
  nsRefPtr<Image> frame = mImageContainer->CreateImage(ImageFormat::GRALLOC_PLANAR_YCBCR);

  GrallocImage* videoImage = static_cast<GrallocImage*>(frame.get());

  GrallocImage::GrallocData data;
  data.mGraphicBuffer = static_cast<layers::GraphicBufferLocked*>(aBuffer);
  data.mPicSize = IntSize(mCurrentConfiguration.mPreviewSize.width,
                          mCurrentConfiguration.mPreviewSize.height);
  videoImage->SetData(data);

  OnNewPreviewFrame(frame, mCurrentConfiguration.mPreviewSize.width,
                    mCurrentConfiguration.mPreviewSize.height);
}

void
nsGonkCameraControl::OnError(CameraControlListener::CameraErrorContext aWhere,
                             CameraControlListener::CameraError aError)
{
  if (aError == CameraControlListener::kErrorServiceFailed) {
    OnPreviewStateChange(CameraControlListener::kPreviewStopped);
    OnHardwareStateChange(CameraControlListener::kHardwareClosed);
  }

  CameraControlImpl::OnError(aWhere, aError);
}

// Gonk callback handlers.
namespace mozilla {

void
OnTakePictureComplete(nsGonkCameraControl* gc, uint8_t* aData, uint32_t aLength)
{
  gc->OnTakePictureComplete(aData, aLength);
}

void
OnTakePictureError(nsGonkCameraControl* gc)
{
  gc->OnTakePictureError();
}

void
OnAutoFocusComplete(nsGonkCameraControl* gc, bool aSuccess)
{
  gc->OnAutoFocusComplete(aSuccess);
}

void
OnNewPreviewFrame(nsGonkCameraControl* gc, layers::GraphicBufferLocked* aBuffer)
{
  gc->OnNewPreviewFrame(aBuffer);
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

void
OnError(nsGonkCameraControl* gc, CameraControlListener::CameraError aError,
        int32_t aArg1, int32_t aArg2)
{
#ifdef PR_LOGGING
  DOM_CAMERA_LOGE("OnError : aError=%d, aArg1=%d, aArg2=%d\n", aError, aArg1, aArg2);
#else
  unused << aArg1;
  unused << aArg2;
#endif
  gc->OnError(CameraControlListener::kInUnspecified, aError);
}

} // namespace mozilla
