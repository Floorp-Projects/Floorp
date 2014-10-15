/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEngineGonkVideoSource.h"

#define LOG_TAG "MediaEngineGonkVideoSource"

#include <utils/Log.h>

#include "GrallocImages.h"
#include "VideoUtils.h"
#include "ScreenOrientation.h"

#include "libyuv.h"
#include "mtransport/runnable_utils.h"

namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::gfx;

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#define LOGFRAME(msg) PR_LOG(GetMediaManagerLog(), 6, msg)
#else
#define LOG(msg)
#define LOGFRAME(msg)
#endif

// We are subclassed from CameraControlListener, which implements a
// threadsafe reference-count for us.
NS_IMPL_QUERY_INTERFACE(MediaEngineGonkVideoSource, nsISupports)
NS_IMPL_ADDREF_INHERITED(MediaEngineGonkVideoSource, CameraControlListener)
NS_IMPL_RELEASE_INHERITED(MediaEngineGonkVideoSource, CameraControlListener)

// Called if the graph thinks it's running out of buffered video; repeat
// the last frame for whatever minimum period it think it needs. Note that
// this means that no *real* frame can be inserted during this period.
void
MediaEngineGonkVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                       SourceMediaStream* aSource,
                                       TrackID aID,
                                       StreamTime aDesiredTime,
                                       TrackTicks& aLastEndTime)
{
  VideoSegment segment;

  MonitorAutoLock lock(mMonitor);
  // B2G does AddTrack, but holds kStarted until the hardware changes state.
  // So mState could be kReleased here. We really don't care about the state,
  // though.

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  TrackTicks target = aSource->TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
  TrackTicks delta = target - aLastEndTime;
  LOGFRAME(("NotifyPull, desired = %ld, target = %ld, delta = %ld %s", (int64_t) aDesiredTime,
            (int64_t) target, (int64_t) delta, image ? "" : "<null>"));

  // Bug 846188 We may want to limit incoming frames to the requested frame rate
  // mFps - if you want 30FPS, and the camera gives you 60FPS, this could
  // cause issues.
  // We may want to signal if the actual frame rate is below mMinFPS -
  // cameras often don't return the requested frame rate especially in low
  // light; we should consider surfacing this so that we can switch to a
  // lower resolution (which may up the frame rate)

  // Don't append if we've already provided a frame that supposedly goes past the current aDesiredTime
  // Doing so means a negative delta and thus messes up handling of the graph
  if (delta > 0) {
    // nullptr images are allowed
    IntSize size(image ? mWidth : 0, image ? mHeight : 0);
    segment.AppendFrame(image.forget(), delta, size);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    if (aSource->AppendToTrack(aID, &(segment))) {
      aLastEndTime = target;
    }
  }
}

void
MediaEngineGonkVideoSource::ChooseCapability(const VideoTrackConstraintsN& aConstraints,
                                             const MediaEnginePrefs& aPrefs)
{
  return GuessCapability(aConstraints, aPrefs);
}

nsresult
MediaEngineGonkVideoSource::Allocate(const VideoTrackConstraintsN& aConstraints,
                                     const MediaEnginePrefs& aPrefs)
{
  LOG((__FUNCTION__));

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  if (mState == kReleased && mInitDone) {
    ChooseCapability(aConstraints, aPrefs);
    NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                         &MediaEngineGonkVideoSource::AllocImpl));
    mCallbackMonitor.Wait();
    if (mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::Deallocate()
{
  LOG((__FUNCTION__));
  if (mSources.IsEmpty()) {

    ReentrantMonitorAutoEnter sync(mCallbackMonitor);

    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }

    // We do not register success callback here

    NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                         &MediaEngineGonkVideoSource::DeallocImpl));
    mCallbackMonitor.Wait();
    if (mState != kReleased) {
      return NS_ERROR_FAILURE;
    }

    mState = kReleased;
    LOG(("Video device %d deallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d deallocated but still in use", mCaptureIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  LOG((__FUNCTION__));
  if (!mInitDone || !aStream) {
    return NS_ERROR_FAILURE;
  }

  mSources.AppendElement(aStream);

  aStream->AddTrack(aID, USECS_PER_S, 0, new VideoSegment());
  aStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);

  if (mState == kStarted) {
    return NS_OK;
  }
  mImageContainer = layers::LayerManager::CreateImageContainer();

  NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                       &MediaEngineGonkVideoSource::StartImpl,
                                       mCapability));
  mCallbackMonitor.Wait();
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::Stop(SourceMediaStream* aSource, TrackID aID)
{
  LOG((__FUNCTION__));
  if (!mSources.RemoveElement(aSource)) {
    // Already stopped - this is allowed
    return NS_OK;
  }
  if (!mSources.IsEmpty()) {
    return NS_OK;
  }

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);

  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mState = kStopped;
    aSource->EndTrack(aID);
    // Drop any cached image so we don't start with a stale image on next
    // usage
    mImage = nullptr;
  }

  NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                       &MediaEngineGonkVideoSource::StopImpl));

  return NS_OK;
}

/**
* Initialization and Shutdown functions for the video source, called by the
* constructor and destructor respectively.
*/

void
MediaEngineGonkVideoSource::Init()
{
  nsAutoCString deviceName;
  ICameraControl::GetCameraName(mCaptureIndex, deviceName);
  CopyUTF8toUTF16(deviceName, mDeviceName);
  CopyUTF8toUTF16(deviceName, mUniqueId);

  mInitDone = true;
}

void
MediaEngineGonkVideoSource::Shutdown()
{
  LOG((__FUNCTION__));
  if (!mInitDone) {
    return;
  }

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);

  if (mState == kStarted) {
    while (!mSources.IsEmpty()) {
      Stop(mSources[0], kVideoTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  if (mState == kAllocated || mState == kStopped) {
    Deallocate();
  }

  mState = kReleased;
  mInitDone = false;
}

// All these functions must be run on MainThread!
void
MediaEngineGonkVideoSource::AllocImpl() {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);

  mCameraControl = ICameraControl::Create(mCaptureIndex);
  if (mCameraControl) {
    mState = kAllocated;
    // Add this as a listener for CameraControl events. We don't need
    // to explicitly remove this--destroying the CameraControl object
    // in DeallocImpl() will do that for us.
    mCameraControl->AddListener(this);
  }
  mCallbackMonitor.Notify();
}

void
MediaEngineGonkVideoSource::DeallocImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  mCameraControl = nullptr;
}

// The same algorithm from bug 840244
static int
GetRotateAmount(ScreenOrientation aScreen, int aCameraMountAngle, bool aBackCamera) {
  int screenAngle = 0;
  switch (aScreen) {
    case eScreenOrientation_PortraitPrimary:
      screenAngle = 0;
      break;
    case eScreenOrientation_PortraitSecondary:
      screenAngle = 180;
      break;
   case eScreenOrientation_LandscapePrimary:
      screenAngle = 90;
      break;
   case eScreenOrientation_LandscapeSecondary:
      screenAngle = 270;
      break;
   default:
      MOZ_ASSERT(false);
      break;
  }

  int result;

  if (aBackCamera) {
    // back camera
    result = (aCameraMountAngle - screenAngle + 360) % 360;
  } else {
    // front camera
    result = (aCameraMountAngle + screenAngle) % 360;
  }
  return result;
}

// undefine to remove on-the-fly rotation support
#define DYNAMIC_GUM_ROTATION

void
MediaEngineGonkVideoSource::Notify(const hal::ScreenConfiguration& aConfiguration) {
#ifdef DYNAMIC_GUM_ROTATION
  if (mHasDirectListeners) {
    // aka hooked to PeerConnection
    MonitorAutoLock enter(mMonitor);
    mRotation = GetRotateAmount(aConfiguration.orientation(), mCameraAngle, mBackCamera);

    LOG(("*** New orientation: %d (Camera %d Back %d MountAngle: %d)",
         mRotation, mCaptureIndex, mBackCamera, mCameraAngle));
  }
#endif

  mOrientationChanged = true;
}

void
MediaEngineGonkVideoSource::StartImpl(webrtc::CaptureCapability aCapability) {
  MOZ_ASSERT(NS_IsMainThread());

  ICameraControl::Configuration config;
  config.mMode = ICameraControl::kPictureMode;
  config.mPreviewSize.width = aCapability.width;
  config.mPreviewSize.height = aCapability.height;
  mCameraControl->Start(&config);
  mCameraControl->Set(CAMERA_PARAM_PICTURE_SIZE, config.mPreviewSize);

  hal::RegisterScreenConfigurationObserver(this);
}

void
MediaEngineGonkVideoSource::StopImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  hal::UnregisterScreenConfigurationObserver(this);
  mCameraControl->Stop();
}

void
MediaEngineGonkVideoSource::OnHardwareStateChange(HardwareState aState)
{
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  if (aState == CameraControlListener::kHardwareClosed) {
    // When the first CameraControl listener is added, it gets pushed
    // the current state of the camera--normally 'closed'. We only
    // pay attention to that state if we've progressed out of the
    // allocated state.
    if (mState != kAllocated) {
      mState = kReleased;
      mCallbackMonitor.Notify();
    }
  } else {
    // Can't read this except on MainThread (ugh)
    NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                         &MediaEngineGonkVideoSource::GetRotation));
    mState = kStarted;
    mCallbackMonitor.Notify();
  }
}


void
MediaEngineGonkVideoSource::GetRotation()
{
  MOZ_ASSERT(NS_IsMainThread());
  MonitorAutoLock enter(mMonitor);

  mCameraControl->Get(CAMERA_PARAM_SENSORANGLE, mCameraAngle);
  MOZ_ASSERT(mCameraAngle == 0 || mCameraAngle == 90 || mCameraAngle == 180 ||
             mCameraAngle == 270);
  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);

  nsCString deviceName;
  ICameraControl::GetCameraName(mCaptureIndex, deviceName);
  if (deviceName.EqualsASCII("back")) {
    mBackCamera = true;
  }

  mRotation = GetRotateAmount(config.orientation(), mCameraAngle, mBackCamera);
  LOG(("*** Initial orientation: %d (Camera %d Back %d MountAngle: %d)",
       mRotation, mCaptureIndex, mBackCamera, mCameraAngle));
}

void
MediaEngineGonkVideoSource::OnUserError(UserContext aContext, nsresult aError)
{
  {
    // Scope the monitor, since there is another monitor below and we don't want
    // unexpected deadlock.
    ReentrantMonitorAutoEnter sync(mCallbackMonitor);
    mCallbackMonitor.Notify();
  }

  // A main thread runnable to send error code to all queued PhotoCallbacks.
  class TakePhotoError : public nsRunnable {
  public:
    TakePhotoError(nsTArray<nsRefPtr<PhotoCallback>>& aCallbacks,
                   nsresult aRv)
      : mRv(aRv)
    {
      mCallbacks.SwapElements(aCallbacks);
    }

    NS_IMETHOD Run()
    {
      uint32_t callbackNumbers = mCallbacks.Length();
      for (uint8_t i = 0; i < callbackNumbers; i++) {
        mCallbacks[i]->PhotoError(mRv);
      }
      // PhotoCallback needs to dereference on main thread.
      mCallbacks.Clear();
      return NS_OK;
    }

  protected:
    nsTArray<nsRefPtr<PhotoCallback>> mCallbacks;
    nsresult mRv;
  };

  if (aContext == UserContext::kInTakePicture) {
    MonitorAutoLock lock(mMonitor);
    if (mPhotoCallbacks.Length()) {
      NS_DispatchToMainThread(new TakePhotoError(mPhotoCallbacks, aError));
    }
  }
}

void
MediaEngineGonkVideoSource::OnTakePictureComplete(uint8_t* aData, uint32_t aLength, const nsAString& aMimeType)
{
  // It needs to start preview because Gonk camera will stop preview while
  // taking picture.
  mCameraControl->StartPreview();

  // Create a main thread runnable to generate a blob and call all current queued
  // PhotoCallbacks.
  class GenerateBlobRunnable : public nsRunnable {
  public:
    GenerateBlobRunnable(nsTArray<nsRefPtr<PhotoCallback>>& aCallbacks,
                         uint8_t* aData,
                         uint32_t aLength,
                         const nsAString& aMimeType)
      : mPhotoDataLength(aLength)
    {
      mCallbacks.SwapElements(aCallbacks);
      mPhotoData = (uint8_t*) moz_malloc(aLength);
      memcpy(mPhotoData, aData, mPhotoDataLength);
      mMimeType = aMimeType;
    }

    NS_IMETHOD Run()
    {
      nsRefPtr<dom::File> blob =
        dom::File::CreateMemoryFile(nullptr, mPhotoData, mPhotoDataLength, mMimeType);
      uint32_t callbackCounts = mCallbacks.Length();
      for (uint8_t i = 0; i < callbackCounts; i++) {
        nsRefPtr<dom::File> tempBlob = blob;
        mCallbacks[i]->PhotoComplete(tempBlob.forget());
      }
      // PhotoCallback needs to dereference on main thread.
      mCallbacks.Clear();
      return NS_OK;
    }

    nsTArray<nsRefPtr<PhotoCallback>> mCallbacks;
    uint8_t* mPhotoData;
    nsString mMimeType;
    uint32_t mPhotoDataLength;
  };

  // All elements in mPhotoCallbacks will be swapped in GenerateBlobRunnable
  // constructor. This captured image will be sent to all the queued
  // PhotoCallbacks in this runnable.
  MonitorAutoLock lock(mMonitor);
  if (mPhotoCallbacks.Length()) {
    NS_DispatchToMainThread(
      new GenerateBlobRunnable(mPhotoCallbacks, aData, aLength, aMimeType));
  }
}

nsresult
MediaEngineGonkVideoSource::TakePhoto(PhotoCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  // If other callback exists, that means there is a captured picture on the way,
  // it doesn't need to TakePicture() again.
  if (!mPhotoCallbacks.Length()) {
    nsresult rv;
    if (mOrientationChanged) {
      UpdatePhotoOrientation();
    }
    rv = mCameraControl->TakePicture();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mPhotoCallbacks.AppendElement(aCallback);

  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::UpdatePhotoOrientation()
{
  MOZ_ASSERT(NS_IsMainThread());

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);

  // The rotation angle is clockwise.
  int orientation = 0;
  switch (config.orientation()) {
    case eScreenOrientation_PortraitPrimary:
      orientation = 0;
      break;
    case eScreenOrientation_PortraitSecondary:
      orientation = 180;
      break;
   case eScreenOrientation_LandscapePrimary:
      orientation = 270;
      break;
   case eScreenOrientation_LandscapeSecondary:
      orientation = 90;
      break;
  }

  // Front camera is inverse angle comparing to back camera.
  orientation = (mBackCamera ? orientation : (-orientation));

  ICameraControlParameterSetAutoEnter batch(mCameraControl);
  // It changes the orientation value in EXIF information only.
  mCameraControl->Set(CAMERA_PARAM_PICTURE_ROTATION, orientation);

  mOrientationChanged = false;

  return NS_OK;
}

uint32_t
MediaEngineGonkVideoSource::ConvertPixelFormatToFOURCC(int aFormat)
{
  switch (aFormat) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
    return libyuv::FOURCC_BGRA;
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    return libyuv::FOURCC_NV21;
  case HAL_PIXEL_FORMAT_YV12:
    return libyuv::FOURCC_YV12;
  default: {
    LOG((" xxxxx Unknown pixel format %d", aFormat));
    MOZ_ASSERT(false, "Unknown pixel format.");
    return libyuv::FOURCC_ANY;
    }
  }
}

void
MediaEngineGonkVideoSource::RotateImage(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight) {
  layers::GrallocImage *nativeImage = static_cast<layers::GrallocImage*>(aImage);
  android::sp<android::GraphicBuffer> graphicBuffer = nativeImage->GetGraphicBuffer();
  void *pMem = nullptr;
  uint32_t size = aWidth * aHeight * 3 / 2;

  graphicBuffer->lock(android::GraphicBuffer::USAGE_SW_READ_MASK, &pMem);

  uint8_t* srcPtr = static_cast<uint8_t*>(pMem);
  // Create a video frame and append it to the track.
  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());

  uint32_t dstWidth;
  uint32_t dstHeight;

  if (mRotation == 90 || mRotation == 270) {
    dstWidth = aHeight;
    dstHeight = aWidth;
  } else {
    dstWidth = aWidth;
    dstHeight = aHeight;
  }

  uint32_t half_width = dstWidth / 2;
  uint8_t* dstPtr = videoImage->AllocateAndGetNewBuffer(size);
  libyuv::ConvertToI420(srcPtr, size,
                        dstPtr, dstWidth,
                        dstPtr + (dstWidth * dstHeight), half_width,
                        dstPtr + (dstWidth * dstHeight * 5 / 4), half_width,
                        0, 0,
                        aWidth, aHeight,
                        aWidth, aHeight,
                        static_cast<libyuv::RotationMode>(mRotation),
                        ConvertPixelFormatToFOURCC(graphicBuffer->getPixelFormat()));
  graphicBuffer->unlock();

  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrData data;
  data.mYChannel = dstPtr;
  data.mYSize = IntSize(dstWidth, dstHeight);
  data.mYStride = dstWidth * lumaBpp / 8;
  data.mCbCrStride = dstWidth * chromaBpp / 8;
  data.mCbChannel = dstPtr + dstHeight * data.mYStride;
  data.mCrChannel = data.mCbChannel +( dstHeight * data.mCbCrStride / 2);
  data.mCbCrSize = IntSize(dstWidth / 2, dstHeight / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = IntSize(dstWidth, dstHeight);
  data.mStereoMode = StereoMode::MONO;

  videoImage->SetDataNoCopy(data);

  // implicitly releases last image
  mImage = image.forget();
}

bool
MediaEngineGonkVideoSource::OnNewPreviewFrame(layers::Image* aImage, uint32_t aWidth, uint32_t aHeight) {
  {
    ReentrantMonitorAutoEnter sync(mCallbackMonitor);
    if (mState == kStopped) {
      return false;
    }
  }

  MonitorAutoLock enter(mMonitor);
  // Bug XXX we'd prefer to avoid converting if mRotation == 0, but that causes problems in UpdateImage()
  RotateImage(aImage, aWidth, aHeight);
  if (mRotation != 0 && mRotation != 180) {
    uint32_t temp = aWidth;
    aWidth = aHeight;
    aHeight = temp;
  }
  if (mWidth != static_cast<int>(aWidth) || mHeight != static_cast<int>(aHeight)) {
    mWidth = aWidth;
    mHeight = aHeight;
    LOG(("Video FrameSizeChange: %ux%u", mWidth, mHeight));
  }

  return true; // return true because we're accepting the frame
}

} // namespace mozilla
