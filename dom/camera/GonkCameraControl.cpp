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
#include "GrallocImages.h"
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
      NS_WARNING("Camera hardware is not initialized");                   \
      DOM_CAMERA_LOGE("%s:%d : mCameraHw is null\n", __func__, __LINE__); \
      return NS_ERROR_NOT_INITIALIZED;                                    \
    }                                                                     \
  } while(0)

// Construct nsGonkCameraControl on the main thread.
nsGonkCameraControl::nsGonkCameraControl(uint32_t aCameraId)
  : mCameraId(aCameraId)
  , mLastThumbnailSize({0, 0})
  , mPreviewFps(30)
  , mResumePreviewAfterTakingPicture(false) // XXXmikeh - see bug 950102
  , mFlashSupported(false)
  , mLuminanceSupported(false)
  , mAutoFlashModeOverridden(false)
  , mSeparateVideoAndPreviewSizesSupported(false)
  , mDeferConfigUpdate(0)
  , mRecorder(nullptr)
  , mRecorderMonitor("GonkCameraControl::mRecorder.Monitor")
  , mVideoFile(nullptr)
  , mReentrantMonitor("GonkCameraControl::OnTakePicture.Monitor")
{
  // Constructor runs on the main thread...
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  mImageContainer = LayerManager::CreateImageContainer();
}

nsresult
nsGonkCameraControl::StartImpl(const Configuration* aInitialConfig)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  nsresult rv = StartInternal(aInitialConfig);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    OnHardwareStateChange(CameraControlListener::kHardwareOpenFailed,
                          NS_ERROR_NOT_AVAILABLE);
  }
  return rv;
}

nsresult
nsGonkCameraControl::StartInternal(const Configuration* aInitialConfig)
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
  nsresult rv = Initialize();
  switch (rv) {
    case NS_ERROR_ALREADY_INITIALIZED:
    case NS_OK:
      break;
    
    default:
      return rv;
  }

  if (aInitialConfig) {
    rv = SetConfigurationInternal(*aInitialConfig);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // The initial configuration failed, close up the hardware
      StopInternal();
      return rv;
    }
  }

  OnHardwareStateChange(CameraControlListener::kHardwareOpen, NS_OK);

  if (aInitialConfig) {
    rv = StartPreviewInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    OnConfigurationChange();
    OnPreviewStateChange(CameraControlListener::kPreviewStarted);
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::Initialize()
{
  if (mCameraHw.get()) {
    DOM_CAMERA_LOGI("Camera %d already connected (this=%p)\n", mCameraId, this);
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mCameraHw = GonkCameraHardware::Connect(this, mCameraId);
  if (!mCameraHw.get()) {
    DOM_CAMERA_LOGE("Failed to connect to camera %d (this=%p)\n", mCameraId, this);
    return NS_ERROR_NOT_INITIALIZED;
  }

  DOM_CAMERA_LOGI("Initializing camera %d (this=%p, mCameraHw=%p)\n", mCameraId, this, mCameraHw.get());
  mCurrentConfiguration.mRecorderProfile.Truncate();

  // Initialize our camera configuration database.
  PullParametersImpl();

  // Set preferred preview frame format.
  mParams.Set(CAMERA_PARAM_PREVIEWFORMAT, NS_LITERAL_STRING("yuv420sp"));
  // Turn off any normal pictures returned by the HDR scene mode
  mParams.Set(CAMERA_PARAM_SCENEMODE_HDR_RETURNNORMALPICTURE, false);
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

  mParams.Get(CAMERA_PARAM_PICTURE_SIZE, mCurrentConfiguration.mPictureSize);
  mParams.Get(CAMERA_PARAM_PREVIEWSIZE, mCurrentConfiguration.mPreviewSize);

  nsString luminance; // check for support
  mParams.Get(CAMERA_PARAM_LUMINANCE, luminance);
  mLuminanceSupported = !luminance.IsEmpty();

  nsString flashMode;
  mParams.Get(CAMERA_PARAM_FLASHMODE, flashMode);
  mFlashSupported = !flashMode.IsEmpty();

  double quality; // informational only
  mParams.Get(CAMERA_PARAM_PICTURE_QUALITY, quality);

  DOM_CAMERA_LOGI(" - maximum metering areas:        %u\n", mCurrentConfiguration.mMaxMeteringAreas);
  DOM_CAMERA_LOGI(" - maximum focus areas:           %u\n", mCurrentConfiguration.mMaxFocusAreas);
  DOM_CAMERA_LOGI(" - default picture size:          %u x %u\n",
    mCurrentConfiguration.mPictureSize.width, mCurrentConfiguration.mPictureSize.height);
  DOM_CAMERA_LOGI(" - default picture file format:   %s\n",
    NS_ConvertUTF16toUTF8(mFileFormat).get());
  DOM_CAMERA_LOGI(" - default picture quality:       %f\n", quality);
  DOM_CAMERA_LOGI(" - default thumbnail size:        %u x %u\n",
    mLastThumbnailSize.width, mLastThumbnailSize.height);
  DOM_CAMERA_LOGI(" - default preview size:          %u x %u\n",
    mCurrentConfiguration.mPreviewSize.width, mCurrentConfiguration.mPreviewSize.height);
  DOM_CAMERA_LOGI(" - luminance reporting:           %ssupported\n",
    mLuminanceSupported ? "" : "NOT ");
  if (mFlashSupported) {
    DOM_CAMERA_LOGI(" - flash:                         supported, default mode '%s'\n",
      NS_ConvertUTF16toUTF8(flashMode).get());
  } else {
    DOM_CAMERA_LOGI(" - flash:                         NOT supported\n");
  }

  nsAutoTArray<Size, 16> sizes;
  mParams.Get(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, sizes);
  if (sizes.Length() > 0) {
    mSeparateVideoAndPreviewSizesSupported = true;
    DOM_CAMERA_LOGI(" - support for separate preview and video sizes\n");
    mParams.Get(CAMERA_PARAM_VIDEOSIZE, mLastRecorderSize);
    DOM_CAMERA_LOGI(" - default video recorder size:   %u x %u\n",
      mLastRecorderSize.width, mLastRecorderSize.height);

    Size preferred;
    mParams.Get(CAMERA_PARAM_PREFERRED_PREVIEWSIZE_FOR_VIDEO, preferred);
    DOM_CAMERA_LOGI(" - preferred video preview size:  %u x %u\n",
      preferred.width, preferred.height);
  } else {
    mLastRecorderSize = mCurrentConfiguration.mPreviewSize;
  }

  nsAutoTArray<nsString, 8> modes;
  mParams.Get(CAMERA_PARAM_SUPPORTED_METERINGMODES, modes);
  if (!modes.IsEmpty()) {
    nsString mode;
    const char* kCenterWeighted = "center-weighted";

    mParams.Get(CAMERA_PARAM_METERINGMODE, mode);
    if (!mode.EqualsASCII(kCenterWeighted)) {
      nsTArray<nsString>::index_type i = modes.Length();
      while (i > 0) {
        --i;
        if (modes[i].EqualsASCII(kCenterWeighted)) {
          mParams.Set(CAMERA_PARAM_METERINGMODE, modes[i]);
          PushParametersImpl();
          break;
        }
      }
    }

    mParams.Get(CAMERA_PARAM_METERINGMODE, mode);
    DOM_CAMERA_LOGI(" - metering mode:                 '%s'\n",
      NS_ConvertUTF16toUTF8(mode).get());
  }
      
  return NS_OK;
}

nsGonkCameraControl::~nsGonkCameraControl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mCameraHw = %p\n", __func__, __LINE__, this, mCameraHw.get());

  StopImpl();
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
}

nsresult
nsGonkCameraControl::ValidateConfiguration(const Configuration& aConfig, Configuration& aValidatedConfig)
{
  nsAutoTArray<Size, 16> supportedSizes;
  Get(CAMERA_PARAM_SUPPORTED_PICTURESIZES, supportedSizes);

  nsresult rv = GetSupportedSize(aConfig.mPictureSize, supportedSizes,
                                 aValidatedConfig.mPictureSize);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("Unable to find a picture size close to %ux%u\n",
      aConfig.mPictureSize.width, aConfig.mPictureSize.height);
    return NS_ERROR_INVALID_ARG;
  }

  rv = LoadRecorderProfiles();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString profileName = aConfig.mRecorderProfile;
  if (profileName.IsEmpty()) {
    profileName.AssignASCII("default");
  }

  RecorderProfile* profile;
  if (!mRecorderProfiles.Get(profileName, &profile)) {
    DOM_CAMERA_LOGE("Recorder profile '%s' is not supported\n",
      NS_ConvertUTF16toUTF8(aConfig.mRecorderProfile).get());
    return NS_ERROR_INVALID_ARG;
  }

  aValidatedConfig.mMode = aConfig.mMode;
  aValidatedConfig.mPreviewSize = aConfig.mPreviewSize;
  aValidatedConfig.mRecorderProfile = profile->GetName();
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetConfigurationInternal(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  // Ensure sanity of all provided parameters and determine defaults if
  // none are provided when given a new configuration
  Configuration config;
  nsresult rv = ValidateConfiguration(aConfig, config);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  {
    ICameraControlParameterSetAutoEnter set(this);

    switch (config.mMode) {
      case kPictureMode:
        rv = SetPictureConfiguration(config);
        break;

      case kVideoMode:
        rv = SetVideoConfiguration(config);
        break;

      default:
        MOZ_ASSERT_UNREACHABLE("Unanticipated camera mode in SetConfigurationInternal()");
        rv = NS_ERROR_FAILURE;
        break;
    }

    DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = Set(CAMERA_PARAM_RECORDINGHINT, config.mMode == kVideoMode);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to set recording hint (0x%x)\n", rv);
    }

    mCurrentConfiguration.mMode = config.mMode;
    mCurrentConfiguration.mRecorderProfile = config.mRecorderProfile;
    
    if (config.mMode == kPictureMode) {
      mCurrentConfiguration.mPictureSize = config.mPictureSize;
    } else /* if config.mMode == kVideoMode */ {
      // The following is best-effort; we don't currently support taking
      // pictures while in video mode, but we should at least return
      // sane values to OnConfigurationChange() handlers...
      SetPictureSizeImpl(config.mPictureSize);
    }
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetConfigurationImpl(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  if (aConfig.mMode == kUnspecifiedMode) {
    DOM_CAMERA_LOGW("Can't set camera mode to 'unspecified', ignoring\n");
    return NS_ERROR_INVALID_ARG;
  }

  // Stop any currently running preview
  nsresult rv = PausePreview();
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("PausePreview() in SetConfigurationImpl() failed (0x%x)\n", rv);
    if (rv == NS_ERROR_NOT_INITIALIZED) {
      // If there no hardware available, nothing else we try will work,
      // so bail out here.
      return rv;
    }
  }

  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  rv = SetConfigurationInternal(aConfig);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    StopPreviewImpl();
    return rv;
  }

  // Restart the preview
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  rv = StartPreviewInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    StopPreviewImpl();
    return rv;
  }

  // OnConfigurationChange() indicates the success case of this operation.
  // It must not be fired until all intermediate steps, including starting
  // the preview, have completed successfully.
  OnConfigurationChange();
  OnPreviewStateChange(CameraControlListener::kPreviewStarted);
  return NS_OK;
}

nsresult
nsGonkCameraControl::MaybeAdjustVideoSize()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  MOZ_ASSERT(mSeparateVideoAndPreviewSizesSupported);

  const Size& preview = mCurrentConfiguration.mPreviewSize;

  // Some camera drivers will ignore our preview size if it's larger
  // than the currently set video recording size, so in picture mode, we
  // give preview size priority, and bump up the video size just in case.
  // This is done on a best-effort basis.

  if (preview.width <= mLastRecorderSize.width &&
      preview.height <= mLastRecorderSize.height) {
    DOM_CAMERA_LOGI("Video size %ux%u is suitable for preview size %ux%u\n",
      mLastRecorderSize.width, mLastRecorderSize.height,
      preview.width, preview.height);
    return NS_OK;
  }

  nsTArray<Size> sizes;
  nsresult rv = Get(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, sizes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const uint32_t previewArea = preview.width * preview.height;
  uint32_t bestDelta = UINT32_MAX;
  bool foundBest = false;
  SizeIndex best;

  for (SizeIndex i = 0; i < sizes.Length(); ++i) {
    const Size& s = sizes[i];
    if (s.width < preview.width || s.height < preview.height) {
      continue;
    }

    const uint32_t area = s.width * s.height;
    const uint32_t delta = area - previewArea;
    if (delta < bestDelta) {
      bestDelta = delta;
      best = i;
      foundBest = true;
    }
  }

  if (!foundBest) {
    // If no candidate was found, the driver will be fine with a video size
    // smaller than the chosen preview size.
    DOM_CAMERA_LOGI("No video size candidate for preview size %ux%u (0x%x)\n",
      preview.width, preview.height, rv);
    return NS_OK;
  }

  DOM_CAMERA_LOGI("Adjusting video size upwards to %ux%u\n",
    sizes[best].width, sizes[best].height);
  rv = Set(CAMERA_PARAM_VIDEOSIZE, sizes[best]);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("Failed to adjust video size for preview size %ux%u (0x%x)\n",
      preview.width, preview.height, rv);
    return rv;
  }

  mLastRecorderSize = preview;
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetPictureConfiguration(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  Size max({0, 0});
  nsresult rv = SelectCaptureAndPreviewSize(aConfig.mPreviewSize,
                                            aConfig.mPictureSize, max,
                                            CAMERA_PARAM_PICTURE_SIZE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mSeparateVideoAndPreviewSizesSupported) {
    MaybeAdjustVideoSize();
  }

  rv = UpdateThumbnailSize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mParams.Get(CAMERA_PARAM_PREVIEWFRAMERATE, mPreviewFps);

  DOM_CAMERA_LOGI("picture mode preview: wanted %ux%u, got %ux%u (%u fps)\n",
                  aConfig.mPreviewSize.width, aConfig.mPreviewSize.height,
                  mCurrentConfiguration.mPreviewSize.width,
                  mCurrentConfiguration.mPreviewSize.height,
                  mPreviewFps);

  return NS_OK;
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
  if (aKey == CAMERA_PARAM_SUPPORTED_VIDEOSIZES &&
      !mSeparateVideoAndPreviewSizesSupported) {
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

  switch (aKey) {
    case CAMERA_PARAM_PICTURE_FILEFORMAT:
      // Picture format -- need to keep it for the TakePicture() callback.
      mFileFormat = aValue;
      break;

    case CAMERA_PARAM_FLASHMODE:
      // Explicit flash mode changes always win and stick.
      mAutoFlashModeOverridden = false;
      break;

    case CAMERA_PARAM_SCENEMODE:
      // Reset disabling normal pictures in HDR mode in conjunction with setting
      // scene mode because some drivers require they be changed together.
      mParams.Set(CAMERA_PARAM_SCENEMODE_HDR_RETURNNORMALPICTURE, false);
      break;
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

// Boolean parameter accessors.
nsresult
nsGonkCameraControl::Set(uint32_t aKey, bool aValue)
{
  return SetAndPush(aKey, aValue);
}

nsresult
nsGonkCameraControl::Get(uint32_t aKey, bool& aRet)
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
    case CAMERA_PARAM_PICTURE_SIZE:
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
nsGonkCameraControl::StartPreviewInternal()
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

  return NS_OK;
}

nsresult
nsGonkCameraControl::StartPreviewImpl()
{
  nsresult rv = StartPreviewInternal();
  if (NS_SUCCEEDED(rv)) {
    OnPreviewStateChange(CameraControlListener::kPreviewStarted);
  }
  return rv;
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
nsGonkCameraControl::AutoFocusImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Starting auto focus\n");

  if (mCameraHw->AutoFocus() != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::StartFaceDetectionImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Starting face detection\n");

  if (mCameraHw->StartFaceDetection() != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsGonkCameraControl::StopFaceDetectionImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Stopping face detection\n");

  if (mCameraHw->StopFaceDetection() != OK) {
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

    if (area != 0 &&
        delta < smallestDelta &&
        supportedSizes[i].width * mCurrentConfiguration.mPictureSize.height ==
          mCurrentConfiguration.mPictureSize.width * supportedSizes[i].height) {
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

android::sp<android::GonkCameraHardware>
nsGonkCameraControl::GetCameraHw()
{
  return mCameraHw;
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
        mCameraControl->OnUserError(CameraControlListener::kInSetThumbnailSize, rv);
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

  if (aSize.width == mCurrentConfiguration.mPictureSize.width &&
      aSize.height == mCurrentConfiguration.mPictureSize.height) {
    DOM_CAMERA_LOGI("Requested picture size %ux%u unchanged\n", aSize.width, aSize.height);
    return NS_OK;
  }

  nsAutoTArray<Size, 8> supportedSizes;
  Get(CAMERA_PARAM_SUPPORTED_PICTURESIZES, supportedSizes);

  Size best;
  nsresult rv = GetSupportedSize(aSize, supportedSizes, best);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGW("Unable to find a picture size close to %ux%u\n",
      aSize.width, aSize.height);
    return NS_ERROR_INVALID_ARG;
  }

  DOM_CAMERA_LOGI("camera-param set picture-size = %ux%u (requested %ux%u)\n",
    best.width, best.height, aSize.width, aSize.height);
  if (best.width > INT32_MAX || best.height > INT32_MAX) {
    DOM_CAMERA_LOGE("Supported picture size is too big, no change\n");
    return NS_ERROR_FAILURE;
  }

  rv = mParams.Set(CAMERA_PARAM_PICTURE_SIZE, best);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mCurrentConfiguration.mPictureSize = best;

  // Finally, update the thumbnail size in case the picture aspect ratio changed.
  // Some drivers will fail to take a picture if the thumbnail size is not the
  // same aspect ratio as the picture size.
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
        mCameraControl->OnUserError(CameraControlListener::kInSetPictureSize, rv);
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
nsGonkCameraControl::SetupRecordingFlash(bool aAutoEnableLowLightTorch)
{
  mAutoFlashModeOverridden = false;

  if (!aAutoEnableLowLightTorch || !mLuminanceSupported || !mFlashSupported) {
    return NS_OK;
  }

  DOM_CAMERA_LOGI("Luminance reporting and flash supported\n");

  nsresult rv = PullParametersImpl();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString luminance;
  rv = mParams.Get(CAMERA_PARAM_LUMINANCE, luminance);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we failed to get the luminance, assume it's "high"
    return NS_OK;
  }

  nsString flashMode;
  rv = mParams.Get(CAMERA_PARAM_FLASHMODE, flashMode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If we failed to get the current flash mode, swallow the error
    return NS_OK;
  }

  if (luminance.EqualsASCII("low") && flashMode.EqualsASCII("auto")) {
    DOM_CAMERA_LOGI("Low luminance detected, turning on flash\n");
    rv = SetAndPush(CAMERA_PARAM_FLASHMODE, NS_LITERAL_STRING("torch"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // If we failed to turn on the flash, swallow the error
      return NS_OK;
    }

    mAutoFlashModeOverridden = true;
  }

  return NS_OK;
}

nsresult
nsGonkCameraControl::StartRecordingImpl(DeviceStorageFileDescriptor* aFileDescriptor,
                                        const StartRecordingOptions* aOptions)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  ReentrantMonitorAutoEnter mon(mRecorderMonitor);

  NS_ENSURE_TRUE(!mCurrentConfiguration.mRecorderProfile.IsEmpty(), NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_FALSE(mRecorder, NS_ERROR_FAILURE);

  /**
   * Get the base path from device storage and append the app-specified
   * filename to it.  The filename may include a relative subpath
   * (e.g.) "DCIM/IMG_0001.jpg".
   *
   * The camera app needs to provide the file extension '.3gp' for now.
   * See bug 795202.
   */
  if (NS_WARN_IF(!aFileDescriptor)) {
    return NS_ERROR_INVALID_ARG;
  }
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
    if (NS_SUCCEEDED(rv)) {
      rv = SetupRecordingFlash(aOptions->autoEnableLowLightTorch);
    }
  } else {
    rv = SetupRecording(fd, 0, 0, 0);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mRecorder->start() != OK) {
    DOM_CAMERA_LOGE("mRecorder->start() failed\n");
    // important: we MUST destroy the recorder if start() fails!
    mRecorder = nullptr;
    // put the flash back to the 'auto' state
    if (mAutoFlashModeOverridden) {
      SetAndPush(CAMERA_PARAM_FLASHMODE, NS_LITERAL_STRING("auto"));
    }
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

  ReentrantMonitorAutoEnter mon(mRecorderMonitor);

  // nothing to do if we have no mRecorder
  if (!mRecorder) {
    return NS_OK;
  }

  mRecorder->stop();
  mRecorder = nullptr;
  OnRecorderStateChange(CameraControlListener::kRecorderStopped);

  {
    ICameraControlParameterSetAutoEnter set(this);

    if (mAutoFlashModeOverridden) {
      nsresult rv = Set(CAMERA_PARAM_FLASHMODE, NS_LITERAL_STRING("auto"));
      if (NS_FAILED(rv)) {
        DOM_CAMERA_LOGE("Failed to set flash mode (0x%x)\n", rv);
      }
    }
  }

  // notify DeviceStorage that the new video file is closed and ready
  return NS_DispatchToMainThread(new RecordingComplete(mVideoFile));
}

nsresult
nsGonkCameraControl::ResumeContinuousFocusImpl()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);
  RETURN_IF_NO_CAMERA_HW();

  DOM_CAMERA_LOGI("Resuming continuous autofocus\n");

  // see
  // http://developer.android.com/reference/android/hardware/Camera.Parameters.html#FOCUS_MODE_CONTINUOUS_PICTURE
  if (NS_WARN_IF(mCameraHw->CancelAutoFocus() != OK)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
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

bool
FeatureDetected(int32_t feature[])
{
  /**
   * For information on what constitutes a valid feature, see:
   * http://androidxref.com/4.0.4/xref/system/core/include/system/camera.h#202
   *
   * Although the comments explicitly state that undetected features are
   * indicated using the value -2000, we conservatively include anything
   * outside the explicitly valid range of [-1000, 1000] as undetected
   * as well.
   */
  const int32_t kLowerFeatureBound = -1000;
  const int32_t kUpperFeatureBound = 1000;
  return (feature[0] >= kLowerFeatureBound && feature[0] <= kUpperFeatureBound) ||
         (feature[1] >= kLowerFeatureBound && feature[1] <= kUpperFeatureBound);
}

void
nsGonkCameraControl::OnFacesDetected(camera_frame_metadata_t* aMetaData)
{
  NS_ENSURE_TRUE_VOID(aMetaData);

  nsTArray<Face> faces;
  uint32_t numFaces = aMetaData->number_of_faces;
  DOM_CAMERA_LOGI("Camera detected %d face(s)", numFaces);

  faces.SetCapacity(numFaces);

  for (uint32_t i = 0; i < numFaces; ++i) {
    Face* f = faces.AppendElement();

    f->id = aMetaData->faces[i].id;
    f->score = aMetaData->faces[i].score;
    if (f->score > 100) {
      f->score = 100;
    }
    f->bound.left = aMetaData->faces[i].rect[0];
    f->bound.top = aMetaData->faces[i].rect[1];
    f->bound.right = aMetaData->faces[i].rect[2];
    f->bound.bottom = aMetaData->faces[i].rect[3];
    DOM_CAMERA_LOGI("Camera face[%u] appended: id=%d, score=%d, bound=(%d, %d)-(%d, %d)\n",
      i, f->id, f->score, f->bound.left, f->bound.top, f->bound.right, f->bound.bottom);

    f->hasLeftEye = FeatureDetected(aMetaData->faces[i].left_eye);
    if (f->hasLeftEye) {
      f->leftEye.x = aMetaData->faces[i].left_eye[0];
      f->leftEye.y = aMetaData->faces[i].left_eye[1];
      DOM_CAMERA_LOGI("    Left eye detected at (%d, %d)\n",
        f->leftEye.x, f->leftEye.y);
    } else {
      DOM_CAMERA_LOGI("    No left eye detected\n");
    }

    f->hasRightEye = FeatureDetected(aMetaData->faces[i].right_eye);
    if (f->hasRightEye) {
      f->rightEye.x = aMetaData->faces[i].right_eye[0];
      f->rightEye.y = aMetaData->faces[i].right_eye[1];
      DOM_CAMERA_LOGI("    Right eye detected at (%d, %d)\n",
        f->rightEye.x, f->rightEye.y);
    } else {
      DOM_CAMERA_LOGI("    No right eye detected\n");
    }

    f->hasMouth = FeatureDetected(aMetaData->faces[i].mouth);
    if (f->hasMouth) {
      f->mouth.x = aMetaData->faces[i].mouth[0];
      f->mouth.y = aMetaData->faces[i].mouth[1];
      DOM_CAMERA_LOGI("    Mouth detected at (%d, %d)\n", f->mouth.x, f->mouth.y);
    } else {
      DOM_CAMERA_LOGI("    No mouth detected\n");
    }
  }

  CameraControlImpl::OnFacesDetected(faces);
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
  CameraControlImpl::OnUserError(CameraControlListener::kInTakePicture,
                                 NS_ERROR_FAILURE);
}

nsresult
nsGonkCameraControl::GetSupportedSize(const Size& aSize,
                                      const nsTArray<Size>& aSupportedSizes,
                                      Size& best)
{
  nsresult rv = NS_ERROR_INVALID_ARG;
  best = aSize;
  uint32_t minSizeDelta = UINT32_MAX;
  uint32_t delta;

  if (aSupportedSizes.IsEmpty()) {
    // no valid sizes
    return rv;
  }

  if (!aSize.width && !aSize.height) {
    // no size specified, take the first supported size
    best = aSupportedSizes[0];
    return NS_OK;
  } else if (aSize.width && aSize.height) {
    // both height and width specified, find the supported size closest to
    // the requested size, looking for an exact match first
    for (SizeIndex i = 0; i < aSupportedSizes.Length(); ++i) {
      Size size = aSupportedSizes[i];
      if (size.width == aSize.width && size.height == aSize.height) {
        best = size;
        return NS_OK;
      }
    }

    // no exact match on dimensions--look for a match closest in area
    const uint32_t targetArea = aSize.width * aSize.height;
    for (SizeIndex i = 0; i < aSupportedSizes.Length(); i++) {
      Size size = aSupportedSizes[i];
      uint32_t delta =
        abs(static_cast<long int>(size.width * size.height - targetArea));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
        rv = NS_OK;
      }
    }
  } else if (!aSize.width) {
    // width not specified, find closest height match
    for (SizeIndex i = 0; i < aSupportedSizes.Length(); i++) {
      Size size = aSupportedSizes[i];
      delta = abs(static_cast<long int>(size.height - aSize.height));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
        rv = NS_OK;
      }
    }
  } else if (!aSize.height) {
    // height not specified, find closest width match
    for (SizeIndex i = 0; i < aSupportedSizes.Length(); i++) {
      Size size = aSupportedSizes[i];
      delta = abs(static_cast<long int>(size.width - aSize.width));
      if (delta < minSizeDelta) {
        minSizeDelta = delta;
        best = size;
        rv = NS_OK;
      }
    }
  }

  return rv;
}

nsresult
nsGonkCameraControl::SelectCaptureAndPreviewSize(const Size& aPreviewSize,
                                                 const Size& aCaptureSize,
                                                 const Size& aMaxSize,
                                                 uint32_t aCaptureSizeKey)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mCameraThread);

  // At this point, we know the capture size has been validated and replaced
  // if necessary with the best matching supported value.
  DOM_CAMERA_LOGI("Select capture size %ux%u, preview size %ux%u, maximum size %ux%u\n",
                  aCaptureSize.width, aCaptureSize.height,
                  aPreviewSize.width, aPreviewSize.height,
                  aMaxSize.width, aMaxSize.height);

  nsAutoTArray<Size, 16> sizes;
  nsresult rv = Get(CAMERA_PARAM_SUPPORTED_PREVIEWSIZES, sizes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // May optionally apply a ceiling to the preview size. Any supported preview
  // size with an area larger than the maximum will be ignored regardless of
  // aspect ratio or delta to requested preview size.
  uint32_t maxArea = aMaxSize.width * aMaxSize.height;
  if (maxArea == 0) {
    maxArea = UINT32_MAX;
  }

  const uint32_t previewArea = aPreviewSize.width * aPreviewSize.height;

  // We should select a preview size with the same aspect ratio as the capture
  // size and minimize the delta with the requested preview size. If we are
  // unable to find any supported preview sizes which match the aspect ratio
  // of the capture size, we fallback to only minimizing the delta with the
  // requested preview size.

  SizeIndex bestSizeMatch = 0; // initializers to keep warnings away
  SizeIndex bestSizeMatchWithAspectRatio = 0;
  bool foundSizeMatch = false;
  bool foundSizeMatchWithAspectRatio = false;

  uint32_t bestAreaDelta = UINT32_MAX;
  uint32_t bestAreaDeltaWithAspect = UINT32_MAX;

  for (SizeIndex i = 0; i < sizes.Length(); ++i) {
    const Size& s = sizes[i];

    // preview size must be smaller or equal to the capture size
    if (aCaptureSize.width < s.width || aCaptureSize.height < s.height) {
      continue;
    }

    const uint32_t area = s.width * s.height;
    if (area > maxArea) {
      continue;
    }

    const uint32_t delta = abs(static_cast<long int>(previewArea - area));
    if (s.width * aCaptureSize.height == aCaptureSize.width * s.height) {
      if (delta == 0) {
        // exact match, including aspect ratio--we can stop now
        bestSizeMatchWithAspectRatio = i;
        foundSizeMatchWithAspectRatio = true;
        break;
      } else if (delta < bestAreaDeltaWithAspect) {
        // aspect ratio match
        bestAreaDeltaWithAspect = delta;
        bestSizeMatchWithAspectRatio = i;
        foundSizeMatchWithAspectRatio = true;
      }
    } else if (delta < bestAreaDelta) {
      bestAreaDelta = delta;
      bestSizeMatch = i;
      foundSizeMatch = true;
    }
  }

  Size previewSize;
  if (foundSizeMatchWithAspectRatio) {
    previewSize = sizes[bestSizeMatchWithAspectRatio];
  } else if (foundSizeMatch) {
    DOM_CAMERA_LOGW("Unable to match a preview size with aspect ratio of capture size %ux%u\n",
      aCaptureSize.width, aCaptureSize.height);
    previewSize = sizes[bestSizeMatch];
  } else {
    DOM_CAMERA_LOGE("Unable to find a preview size for capture size %ux%u\n",
      aCaptureSize.width, aCaptureSize.height);
    return NS_ERROR_INVALID_ARG;
  }

  DOM_CAMERA_LOGI("Setting capture size to %ux%u, preview size to %ux%u\n",
                  aCaptureSize.width, aCaptureSize.height,
                  previewSize.width, previewSize.height);

  Size oldSize;
  rv = Get(CAMERA_PARAM_PREVIEWSIZE, oldSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = Set(CAMERA_PARAM_PREVIEWSIZE, previewSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = Set(aCaptureSizeKey, aCaptureSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Set(CAMERA_PARAM_PREVIEWSIZE, oldSize); // error, try to restore the original preview size
    return rv;
  }

  mCurrentConfiguration.mPreviewSize = previewSize;
  return NS_OK;
}

nsresult
nsGonkCameraControl::SetVideoConfiguration(const Configuration& aConfig)
{
  DOM_CAMERA_LOGT("%s:%d\n", __func__, __LINE__);

  RecorderProfile* profile;
  if (!mRecorderProfiles.Get(aConfig.mRecorderProfile, &profile)) {
    DOM_CAMERA_LOGE("Recorder profile '%s' is not supported\n",
      NS_ConvertUTF16toUTF8(aConfig.mRecorderProfile).get());
    return NS_ERROR_INVALID_ARG;
  }

  const RecorderProfile::Video& video(profile->GetVideo());
  const Size& size = video.GetSize();
  const uint32_t fps = video.GetFramesPerSecond();
  if (fps == 0 || fps > INT_MAX || size.width == 0 || size.height == 0) {
    DOM_CAMERA_LOGE("Can't configure video with fps=%u, width=%u, height=%u\n",
      fps, size.width, size.height);
    return NS_ERROR_FAILURE;
  }

  PullParametersImpl();

  {
    ICameraControlParameterSetAutoEnter set(this);
    nsresult rv;

    if (mSeparateVideoAndPreviewSizesSupported) {
      // The camera supports two video streams: a low(er) resolution preview
      // stream and and a potentially high(er) resolution stream for encoding.
      Size preferred;
      rv = Get(CAMERA_PARAM_PREFERRED_PREVIEWSIZE_FOR_VIDEO, preferred);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = SelectCaptureAndPreviewSize(aConfig.mPreviewSize, size, preferred,
                                       CAMERA_PARAM_VIDEOSIZE);
      if (NS_FAILED(rv)) {
        DOM_CAMERA_LOGE("Failed to set video and preview sizes (0x%x)\n", rv);
        return rv;
      }
    } else {
      // The camera only supports a single video stream: in this case, we set
      // the preview size to be the desired video recording size, and ignore
      // the specified preview size.
      rv = Set(CAMERA_PARAM_PREVIEWSIZE, size);
      if (NS_FAILED(rv)) {
        DOM_CAMERA_LOGE("Failed to set video mode preview size (0x%x)\n", rv);
        return rv;
      }

      mCurrentConfiguration.mPreviewSize = size;
    }

    mLastRecorderSize = size;

    rv = Set(CAMERA_PARAM_PREVIEWFRAMERATE, static_cast<int>(fps));
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to set video mode frame rate (0x%x)\n", rv);
      return rv;
    }
  }

  mPreviewFps = fps;
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
nsGonkCameraControl::SetupRecording(int aFd, int aRotation,
                                    uint64_t aMaxFileSizeBytes,
                                    uint64_t aMaxVideoLengthMs)
{
  RETURN_IF_NO_CAMERA_HW();

  // choosing a size big enough to hold the params
  const size_t SIZE = 256;
  char buffer[SIZE];

  ReentrantMonitorAutoEnter mon(mRecorderMonitor);

  mRecorder = new GonkRecorder();
  CHECK_SETARG_RETURN(mRecorder->init(), NS_ERROR_FAILURE);

  nsresult rv =
    GonkRecorderProfile::ConfigureRecorder(*mRecorder, mCameraId,
                                           mCurrentConfiguration.mRecorderProfile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CHECK_SETARG_RETURN(mRecorder->setCamera(mCameraHw), NS_ERROR_FAILURE);

  DOM_CAMERA_LOGI("maxVideoLengthMs=%llu\n", aMaxVideoLengthMs);
  const uint64_t kMaxVideoLengthMs = INT64_MAX / 1000;
  if (aMaxVideoLengthMs == 0) {
    aMaxVideoLengthMs = -1;
  } else if (aMaxVideoLengthMs > kMaxVideoLengthMs) {
    // GonkRecorder parameters are internally limited to signed 64-bit values,
    // and the time length limit is converted from milliseconds to microseconds,
    // so we limit this value to prevent any unexpected overflow weirdness.
    DOM_CAMERA_LOGW("maxVideoLengthMs capped to %lld\n", kMaxVideoLengthMs);
    aMaxVideoLengthMs = kMaxVideoLengthMs;
  }
  snprintf(buffer, SIZE, "max-duration=%lld", aMaxVideoLengthMs);
  CHECK_SETARG_RETURN(mRecorder->setParameters(String8(buffer)),
                      NS_ERROR_INVALID_ARG);

  DOM_CAMERA_LOGI("maxFileSizeBytes=%llu\n", aMaxFileSizeBytes);
  if (aMaxFileSizeBytes == 0) {
    aMaxFileSizeBytes = -1;
  } else if (aMaxFileSizeBytes > INT64_MAX) {
    // GonkRecorder parameters are internally limited to signed 64-bit values
    DOM_CAMERA_LOGW("maxFileSizeBytes capped to INT64_MAX\n");
    aMaxFileSizeBytes = INT64_MAX;
  }
  snprintf(buffer, SIZE, "max-filesize=%lld", aMaxFileSizeBytes);
  CHECK_SETARG_RETURN(mRecorder->setParameters(String8(buffer)),
                      NS_ERROR_INVALID_ARG);

  // adjust rotation by camera sensor offset
  int r = aRotation;
  r += mCameraHw->GetSensorOrientation();
  r = RationalizeRotation(r);
  DOM_CAMERA_LOGI("setting video rotation to %d degrees (mapped from %d)\n", r, aRotation);
  snprintf(buffer, SIZE, "video-param-rotation-angle-degrees=%d", r);
  CHECK_SETARG_RETURN(mRecorder->setParameters(String8(buffer)),
                      NS_ERROR_INVALID_ARG);

  CHECK_SETARG_RETURN(mRecorder->setListener(new GonkRecorderListener(this)),
                      NS_ERROR_FAILURE);

  // recording API needs file descriptor of output file
  CHECK_SETARG_RETURN(mRecorder->setOutputFile(aFd, 0, 0), NS_ERROR_FAILURE);
  CHECK_SETARG_RETURN(mRecorder->prepare(), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
nsGonkCameraControl::StopInternal()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  // if we're recording, stop recording
  StopRecordingImpl();

  // stop the preview
  nsresult rv = StopPreviewImpl();

  // release the hardware handle
  if (mCameraHw.get()){
     mCameraHw->Close();
     mCameraHw.clear();
  }

  return rv;
}

nsresult
nsGonkCameraControl::StopImpl()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

  nsresult rv = StopInternal();
  if (rv != NS_ERROR_NOT_INITIALIZED) {
    rv = NS_OK;
  }
  if (NS_SUCCEEDED(rv)) {
    OnHardwareStateChange(CameraControlListener::kHardwareClosed, NS_OK);
  }
  return rv;
}

nsresult
nsGonkCameraControl::LoadRecorderProfiles()
{
  if (mRecorderProfiles.Count() == 0) {
    nsTArray<nsRefPtr<RecorderProfile>> profiles;
    nsresult rv = GonkRecorderProfile::GetAll(mCameraId, profiles);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsTArray<Size> sizes;
    rv = Get(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, sizes);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsTArray<RecorderProfile>::size_type bestIndexMatch = 0;
    int bestAreaMatch = 0;

    // Limit profiles to those video sizes supported by the camera hardware...
    for (nsTArray<RecorderProfile>::size_type i = 0; i < profiles.Length(); ++i) {
      int width = profiles[i]->GetVideo().GetSize().width;
      int height = profiles[i]->GetVideo().GetSize().height;
      if (width < 0 || height < 0) {
        DOM_CAMERA_LOGW("Ignoring weird profile '%s' with width and/or height < 0\n",
          NS_ConvertUTF16toUTF8(profiles[i]->GetName()).get());
        continue;
      }
      for (nsTArray<Size>::size_type n = 0; n < sizes.Length(); ++n) {
        if (static_cast<uint32_t>(width) == sizes[n].width &&
            static_cast<uint32_t>(height) == sizes[n].height) {
          mRecorderProfiles.Put(profiles[i]->GetName(), profiles[i]);
          int area = width * height;
          if (area > bestAreaMatch) {
            bestIndexMatch = i;
            bestAreaMatch = area;
          }
          break;
        }
      }
    }

    // Default profile is the one with the largest area.
    if (bestAreaMatch > 0) {
      nsAutoString name;
      name.AssignASCII("default");
      mRecorderProfiles.Put(name, profiles[bestIndexMatch]);
    }
  }

  return NS_OK;
}

/* static */ PLDHashOperator
nsGonkCameraControl::Enumerate(const nsAString& aProfileName,
                               RecorderProfile* aProfile,
                               void* aUserArg)
{
  nsTArray<nsString>* profiles = static_cast<nsTArray<nsString>*>(aUserArg);
  MOZ_ASSERT(profiles);
  profiles->AppendElement(aProfileName);
  return PL_DHASH_NEXT;
}

nsresult
nsGonkCameraControl::GetRecorderProfiles(nsTArray<nsString>& aProfiles)
{
  nsresult rv = LoadRecorderProfiles();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aProfiles.Clear();
  mRecorderProfiles.EnumerateRead(Enumerate, static_cast<void*>(&aProfiles));
  return NS_OK;
}

ICameraControl::RecorderProfile*
nsGonkCameraControl::GetProfileInfo(const nsAString& aProfile)
{
  RecorderProfile* profile;
  if (!mRecorderProfiles.Get(aProfile, &profile)) {
    return nullptr;
  }
  return profile;
}

void
nsGonkCameraControl::OnRateLimitPreview(bool aLimit)
{
  CameraControlImpl::OnRateLimitPreview(aLimit);
}

void
nsGonkCameraControl::OnNewPreviewFrame(layers::TextureClient* aBuffer)
{
  nsRefPtr<Image> frame = mImageContainer->CreateImage(ImageFormat::GRALLOC_PLANAR_YCBCR);

  GrallocImage* videoImage = static_cast<GrallocImage*>(frame.get());

  GrallocImage::GrallocData data;
  data.mGraphicBuffer = aBuffer;
  data.mPicSize = IntSize(mCurrentConfiguration.mPreviewSize.width,
                          mCurrentConfiguration.mPreviewSize.height);
  videoImage->SetData(data);

  OnNewPreviewFrame(frame, mCurrentConfiguration.mPreviewSize.width,
                    mCurrentConfiguration.mPreviewSize.height);
}

void
nsGonkCameraControl::OnSystemError(CameraControlListener::SystemContext aWhere,
                                   nsresult aError)
{
  if (aWhere == CameraControlListener::kSystemService) {
    StopInternal();
    OnHardwareStateChange(CameraControlListener::kHardwareClosed, NS_ERROR_FAILURE);
  }

  CameraControlImpl::OnSystemError(aWhere, aError);
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
OnAutoFocusMoving(nsGonkCameraControl* gc, bool aIsMoving)
{
  gc->OnAutoFocusMoving(aIsMoving);
}

void
OnFacesDetected(nsGonkCameraControl* gc, camera_frame_metadata_t* aMetaData)
{
  gc->OnFacesDetected(aMetaData);
}

void
OnRateLimitPreview(nsGonkCameraControl* gc, bool aLimit)
{
  gc->OnRateLimitPreview(aLimit);
}

void
OnNewPreviewFrame(nsGonkCameraControl* gc, layers::TextureClient* aBuffer)
{
  gc->OnNewPreviewFrame(aBuffer);
}

void
OnShutter(nsGonkCameraControl* gc)
{
  gc->OnShutter();
}

void
OnSystemError(nsGonkCameraControl* gc,
              CameraControlListener::SystemContext aWhere,
              int32_t aArg1, int32_t aArg2)
{
#ifdef PR_LOGGING
  DOM_CAMERA_LOGE("OnSystemError : aWhere=%d, aArg1=%d, aArg2=%d\n", aWhere, aArg1, aArg2);
#else
  unused << aArg1;
  unused << aArg2;
#endif
  gc->OnSystemError(aWhere, NS_ERROR_FAILURE);
}

} // namespace mozilla
