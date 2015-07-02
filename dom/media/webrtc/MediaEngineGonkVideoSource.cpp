/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEngineGonkVideoSource.h"

#undef LOG_TAG
#define LOG_TAG "MediaEngineGonkVideoSource"

#include <utils/Log.h>

#include "GrallocImages.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "VideoUtils.h"
#include "ScreenOrientation.h"

#include "libyuv.h"
#include "mtransport/runnable_utils.h"
#include "GonkCameraImage.h"

namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace android;

#undef LOG
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOGFRAME(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

class MediaBufferListener : public GonkCameraSource::DirectBufferListener {
public:
  MediaBufferListener(MediaEngineGonkVideoSource* aMediaEngine)
    : mMediaEngine(aMediaEngine)
  {
  }

  status_t BufferAvailable(MediaBuffer* aBuffer)
  {
    nsresult rv = mMediaEngine->OnNewMediaBufferFrame(aBuffer);
    if (NS_SUCCEEDED(rv)) {
      return OK;
    }
    return UNKNOWN_ERROR;
  }

  ~MediaBufferListener()
  {
  }

  nsRefPtr<MediaEngineGonkVideoSource> mMediaEngine;
};

#define WEBRTC_GONK_VIDEO_SOURCE_POOL_BUFFERS 10

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
                                       StreamTime aDesiredTime)
{
  VideoSegment segment;

  MonitorAutoLock lock(mMonitor);
  // B2G does AddTrack, but holds kStarted until the hardware changes state.
  // So mState could be kReleased here. We really don't care about the state,
  // though.

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  StreamTime delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);
  LOGFRAME(("NotifyPull, desired = %ld, delta = %ld %s", (int64_t) aDesiredTime,
            (int64_t) delta, image ? "" : "<null>"));

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
    aSource->AppendToTrack(aID, &(segment));
  }
}

size_t
MediaEngineGonkVideoSource::NumCapabilities()
{
  // TODO: Stop hardcoding. Use GetRecorderProfiles+GetProfileInfo (Bug 1128550)
  //
  // The camera-selecting constraints algorithm needs a set of capabilities to
  // work on. In lieu of something better, here are some generic values based on
  // http://en.wikipedia.org/wiki/Comparison_of_Firefox_OS_devices on Jan 2015.
  // When unknown, better overdo it with choices to not block legitimate asks.
  // TODO: Match with actual hardware or add code to query hardware.

  if (mHardcodedCapabilities.IsEmpty()) {
    const struct { int width, height; } hardcodes[] = {
      { 800, 1280 },
      { 720, 1280 },
      { 600, 1024 },
      { 540, 960 },
      { 480, 854 },
      { 480, 800 },
      { 320, 480 },
      { 240, 320 }, // sole mode supported by emulator on try
    };
    const int framerates[] = { 15, 30 };

    for (auto& hardcode : hardcodes) {
      webrtc::CaptureCapability c;
      c.width = hardcode.width;
      c.height = hardcode.height;
      for (int framerate : framerates) {
        c.maxFPS = framerate;
        mHardcodedCapabilities.AppendElement(c); // portrait
      }
      c.width = hardcode.height;
      c.height = hardcode.width;
      for (int framerate : framerates) {
        c.maxFPS = framerate;
        mHardcodedCapabilities.AppendElement(c); // landscape
      }
    }
  }
  return mHardcodedCapabilities.Length();
}

nsresult
MediaEngineGonkVideoSource::Allocate(const dom::MediaTrackConstraints& aConstraints,
                                     const MediaEnginePrefs& aPrefs,
                                     const nsString& aDeviceId)
{
  LOG((__FUNCTION__));

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  if (mState == kReleased && mInitDone) {
    ChooseCapability(aConstraints, aPrefs, aDeviceId);
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
  bool empty;
  {
    MonitorAutoLock lock(mMonitor);
    empty = mSources.IsEmpty();
  }
  if (empty) {

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

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
  }

  aStream->AddTrack(aID, 0, new VideoSegment());

  ReentrantMonitorAutoEnter sync(mCallbackMonitor);

  MOZ_ASSERT(mCameraControl, "mCameraControl is nullptr");
  if (mState == kStarted) {
    return NS_OK;
  } else if (!mCameraControl) {
    return NS_ERROR_FAILURE;
  }
  mTrackID = aID;
  mImageContainer = layers::LayerManager::CreateImageContainer();

  NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                       &MediaEngineGonkVideoSource::StartImpl,
                                       mCapability));
  mCallbackMonitor.Wait();
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsString> focusModes;
  mCameraControl->Get(CAMERA_PARAM_SUPPORTED_FOCUSMODES, focusModes);
  for (nsTArray<nsString>::index_type i = 0; i < focusModes.Length(); ++i) {
    if (focusModes[i].EqualsASCII("continuous-video")) {
      mCameraControl->Set(CAMERA_PARAM_FOCUSMODE, focusModes[i]);
      mCameraControl->ResumeContinuousFocus();
      break;
    }
  }

  // XXX some devices support recording camera frame only in metadata mode.
  // But GonkCameraSource requests non-metadata recording mode.
#if ANDROID_VERSION < 21
  if (NS_FAILED(InitDirectMediaBuffer())) {
    return NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::InitDirectMediaBuffer()
{
  // Check available buffer resolution.
  nsTArray<ICameraControl::Size> videoSizes;
  mCameraControl->Get(CAMERA_PARAM_SUPPORTED_VIDEOSIZES, videoSizes);
  if (!videoSizes.Length()) {
    return NS_ERROR_FAILURE;
  }

  // TODO: MediaEgnine should use supported recording frame sizes as the size
  //       range in MediaTrackConstraintSet and find the best match.
  //       Here we use the first one as the default size (largest supported size).
  android::Size videoSize;
  videoSize.width = videoSizes[0].width;
  videoSize.height = videoSizes[0].height;

  LOG(("Intial size, width: %d, height: %d", videoSize.width, videoSize.height));
  mCameraSource = GonkCameraSource::Create(mCameraControl,
                                           videoSize,
                                           MediaEngine::DEFAULT_VIDEO_FPS);

  status_t rv;
  rv = mCameraSource->AddDirectBufferListener(new MediaBufferListener(this));
  if (rv != OK) {
    return NS_ERROR_FAILURE;
  }

  rv = mCameraSource->start(nullptr);
  if (rv != OK) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
MediaEngineGonkVideoSource::Stop(SourceMediaStream* aSource, TrackID aID)
{
  LOG((__FUNCTION__));
  {
    MonitorAutoLock lock(mMonitor);

    if (!mSources.RemoveElement(aSource)) {
      // Already stopped - this is allowed
      return NS_OK;
    }
    if (!mSources.IsEmpty()) {
      return NS_OK;
    }
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
  SetName(NS_ConvertUTF8toUTF16(deviceName));
  SetUUID(deviceName.get());

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
    SourceMediaStream *source;
    bool empty;

    while (1) {
      {
        MonitorAutoLock lock(mMonitor);
        empty = mSources.IsEmpty();
        if (empty) {
          break;
        }
        source = mSources[0];
      }
      Stop(source, kVideoTrack); // XXX change to support multiple tracks
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
    mTextureClientAllocator =
      new layers::TextureClientRecycleAllocator(layers::ImageBridgeChild::GetSingleton());
    mTextureClientAllocator->SetMaxPoolSize(WEBRTC_GONK_VIDEO_SOURCE_POOL_BUFFERS);
  }
  mCallbackMonitor.Notify();
}

void
MediaEngineGonkVideoSource::DeallocImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  mCameraControl = nullptr;
  mTextureClientAllocator = nullptr;
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
  config.mPictureSize.width = aCapability.width;
  config.mPictureSize.height = aCapability.height;
  mCameraControl->Start(&config);

  hal::RegisterScreenConfigurationObserver(this);
}

void
MediaEngineGonkVideoSource::StopImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mCameraSource.get()) {
    mCameraSource->stop();
    mCameraSource = nullptr;
  }

  hal::UnregisterScreenConfigurationObserver(this);
  mCameraControl->Stop();
}

void
MediaEngineGonkVideoSource::OnHardwareStateChange(HardwareState aState,
                                                  nsresult aReason)
{
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  switch (aState) {
    case CameraControlListener::kHardwareClosed:
    case CameraControlListener::kHardwareOpenFailed:
      mState = kReleased;
      mCallbackMonitor.Notify();
      break;
    case CameraControlListener::kHardwareOpen:
      // Can't read this except on MainThread (ugh)
      NS_DispatchToMainThread(WrapRunnable(nsRefPtr<MediaEngineGonkVideoSource>(this),
                                           &MediaEngineGonkVideoSource::GetRotation));
      mState = kStarted;
      mCallbackMonitor.Notify();
      break;
    case CameraControlListener::kHardwareUninitialized:
      // When the first CameraControl listener is added, it gets pushed
      // the current state of the camera--normally 'uninitialized'.
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unanticipated camera hardware state");
      break;
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
MediaEngineGonkVideoSource::OnTakePictureComplete(const uint8_t* aData, uint32_t aLength, const nsAString& aMimeType)
{
  // It needs to start preview because Gonk camera will stop preview while
  // taking picture.
  mCameraControl->StartPreview();

  // Create a main thread runnable to generate a blob and call all current queued
  // PhotoCallbacks.
  class GenerateBlobRunnable : public nsRunnable {
  public:
    GenerateBlobRunnable(nsTArray<nsRefPtr<PhotoCallback>>& aCallbacks,
                         const uint8_t* aData,
                         uint32_t aLength,
                         const nsAString& aMimeType)
      : mPhotoDataLength(aLength)
    {
      mCallbacks.SwapElements(aCallbacks);
      mPhotoData = (uint8_t*) malloc(aLength);
      memcpy(mPhotoData, aData, mPhotoDataLength);
      mMimeType = aMimeType;
    }

    NS_IMETHOD Run()
    {
      nsRefPtr<dom::Blob> blob =
        dom::Blob::CreateMemoryBlob(nullptr, mPhotoData, mPhotoDataLength, mMimeType);
      uint32_t callbackCounts = mCallbacks.Length();
      for (uint8_t i = 0; i < callbackCounts; i++) {
        nsRefPtr<dom::Blob> tempBlob = blob;
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
  android::sp<GraphicBuffer> graphicBuffer = nativeImage->GetGraphicBuffer();
  void *pMem = nullptr;
  // Bug 1109957 size will be wrong if width or height are odd
  uint32_t size = aWidth * aHeight * 3 / 2;
  MOZ_ASSERT(!(aWidth & 1) && !(aHeight & 1));

  graphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_MASK, &pMem);

  uint8_t* srcPtr = static_cast<uint8_t*>(pMem);
  // Create a video frame and append it to the track.
  ImageFormat format = ImageFormat::GONK_CAMERA_IMAGE;
  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(format);

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

  layers::GrallocImage* videoImage = static_cast<layers::GrallocImage*>(image.get());
  MOZ_ASSERT(mTextureClientAllocator);
  RefPtr<layers::TextureClient> textureClient
    = mTextureClientAllocator->CreateOrRecycleForDrawing(gfx::SurfaceFormat::YUV,
                                                         gfx::IntSize(dstWidth, dstHeight),
                                                         gfx::BackendType::NONE,
                                                         layers::TextureFlags::DEFAULT,
                                                         layers::ALLOC_DISALLOW_BUFFERTEXTURECLIENT);
  if (textureClient) {
    RefPtr<layers::GrallocTextureClientOGL> grallocTextureClient =
      static_cast<layers::GrallocTextureClientOGL*>(textureClient.get());

    android::sp<android::GraphicBuffer> destBuffer = grallocTextureClient->GetGraphicBuffer();

    void* destMem = nullptr;
    destBuffer->lock(android::GraphicBuffer::USAGE_SW_WRITE_OFTEN, &destMem);
    uint8_t* dstPtr = static_cast<uint8_t*>(destMem);

    int32_t yStride = destBuffer->getStride();
    // Align to 16 bytes boundary
    int32_t uvStride = ((yStride / 2) + 15) & ~0x0F;

    libyuv::ConvertToI420(srcPtr, size,
                          dstPtr, yStride,
                          dstPtr + (yStride * dstHeight + (uvStride * dstHeight / 2)), uvStride,
                          dstPtr + (yStride * dstHeight), uvStride,
                          0, 0,
                          graphicBuffer->getStride(), aHeight,
                          aWidth, aHeight,
                          static_cast<libyuv::RotationMode>(mRotation),
                          libyuv::FOURCC_NV21);
    destBuffer->unlock();

    layers::GrallocImage::GrallocData data;

    data.mPicSize = gfx::IntSize(dstWidth, dstHeight);
    data.mGraphicBuffer = textureClient;
    videoImage->SetData(data);
  } else {
    // Handle out of gralloc case.
    image = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
    layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());
    uint8_t* dstPtr = videoImage->AllocateAndGetNewBuffer(size);

    libyuv::ConvertToI420(srcPtr, size,
                          dstPtr, dstWidth,
                          dstPtr + (dstWidth * dstHeight), half_width,
                          dstPtr + (dstWidth * dstHeight * 5 / 4), half_width,
                          0, 0,
                          graphicBuffer->getStride(), aHeight,
                          aWidth, aHeight,
                          static_cast<libyuv::RotationMode>(mRotation),
                          ConvertPixelFormatToFOURCC(graphicBuffer->getPixelFormat()));

    const uint8_t lumaBpp = 8;
    const uint8_t chromaBpp = 4;

    layers::PlanarYCbCrData data;
    data.mYChannel = dstPtr;
    data.mYSize = IntSize(dstWidth, dstHeight);
    data.mYStride = dstWidth * lumaBpp / 8;
    data.mCbCrStride = dstWidth * chromaBpp / 8;
    data.mCbChannel = dstPtr + dstHeight * data.mYStride;
    data.mCrChannel = data.mCbChannel + data.mCbCrStride * (dstHeight / 2);
    data.mCbCrSize = IntSize(dstWidth / 2, dstHeight / 2);
    data.mPicX = 0;
    data.mPicY = 0;
    data.mPicSize = IntSize(dstWidth, dstHeight);
    data.mStereoMode = StereoMode::MONO;

    videoImage->SetDataNoCopy(data);
  }
  graphicBuffer->unlock();

  // Implicitly releases last preview image.
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

nsresult
MediaEngineGonkVideoSource::OnNewMediaBufferFrame(MediaBuffer* aBuffer)
{
  {
    ReentrantMonitorAutoEnter sync(mCallbackMonitor);
    if (mState == kStopped) {
      return NS_OK;
    }
  }

  MonitorAutoLock enter(mMonitor);
  if (mImage) {
    if (mImage->AsGrallocImage()) {
      // MediaEngineGonkVideoSource expects that GrallocImage is GonkCameraImage.
      // See Bug 938034.
      GonkCameraImage* cameraImage = static_cast<GonkCameraImage*>(mImage.get());
      cameraImage->SetMediaBuffer(aBuffer);
    } else {
      LOG(("mImage is non-GrallocImage"));
    }

    uint32_t len = mSources.Length();
    for (uint32_t i = 0; i < len; i++) {
      if (mSources[i]) {
        // Duration is 1 here.
        // Ideally, it should be camera timestamp here and the MSG will have
        // enough sample duration without calling NotifyPull() anymore.
        // Unfortunately, clock in gonk camera looks like is a different one
        // comparing to MSG. As result, it causes time inaccurate. (frames be
        // queued in MSG longer and longer as time going by in device like Frame)
        AppendToTrack(mSources[i], mImage, mTrackID, 1);
      }
    }
    if (mImage->AsGrallocImage()) {
      GonkCameraImage* cameraImage = static_cast<GonkCameraImage*>(mImage.get());
      // Clear MediaBuffer immediately, it prevents MediaBuffer is kept in
      // MediaStreamGraph thread.
      cameraImage->ClearMediaBuffer();
    }
  }

  return NS_OK;
}

} // namespace mozilla
