/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"
#include "Layers.h"
#include "ImageTypes.h"
#include "ImageContainer.h"
#include "nsMemory.h"
#include "mtransport/runnable_utils.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#define LOGFRAME(msg) PR_LOG(GetMediaManagerLog(), 6, msg)
#else
#define LOG(msg)
#define LOGFRAME(msg)
#endif

/**
 * Webrtc video source.
 */
NS_IMPL_ISUPPORTS1(MediaEngineWebRTCVideoSource, nsIRunnable)

// ViEExternalRenderer Callback.
#ifndef MOZ_B2G_CAMERA
int
MediaEngineWebRTCVideoSource::FrameSizeChange(
   unsigned int w, unsigned int h, unsigned int streams)
{
  mWidth = w;
  mHeight = h;
  LOG(("Video FrameSizeChange: %ux%u", w, h));
  return 0;
}

// ViEExternalRenderer Callback. Process every incoming frame here.
int
MediaEngineWebRTCVideoSource::DeliverFrame(
   unsigned char* buffer, int size, uint32_t time_stamp, int64_t render_time,
   void *handle)
{
  // mInSnapshotMode can only be set before the camera is turned on and
  // the renderer is started, so this amounts to a 1-shot
  if (mInSnapshotMode) {
    // Set the condition variable to false and notify Snapshot().
    MonitorAutoLock lock(mMonitor);
    mInSnapshotMode = false;
    lock.Notify();
    return 0;
  }

  // Check for proper state.
  if (mState != kStarted) {
    LOG(("DeliverFrame: video not started"));
    return 0;
  }

  MOZ_ASSERT(mWidth*mHeight*3/2 == size);
  if (mWidth*mHeight*3/2 != size) {
    return 0;
  }

  // Create a video frame and append it to the track.
  ImageFormat format = PLANAR_YCBCR;

  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(&format, 1);

  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());

  uint8_t* frame = static_cast<uint8_t*> (buffer);
  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrData data;
  data.mYChannel = frame;
  data.mYSize = gfxIntSize(mWidth, mHeight);
  data.mYStride = mWidth * lumaBpp/ 8;
  data.mCbCrStride = mWidth * chromaBpp / 8;
  data.mCbChannel = frame + mHeight * data.mYStride;
  data.mCrChannel = data.mCbChannel + mHeight * data.mCbCrStride / 2;
  data.mCbCrSize = gfxIntSize(mWidth/ 2, mHeight/ 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfxIntSize(mWidth, mHeight);
  data.mStereoMode = STEREO_MODE_MONO;

  videoImage->SetData(data);

#ifdef DEBUG
  static uint32_t frame_num = 0;
  LOGFRAME(("frame %d (%dx%d); timestamp %u, render_time %lu", frame_num++,
            mWidth, mHeight, time_stamp, render_time));
#endif

  // we don't touch anything in 'this' until here (except for snapshot,
  // which has it's own lock)
  MonitorAutoLock lock(mMonitor);

  // implicitly releases last image
  mImage = image.forget();

  return 0;
}
#endif

// Called if the graph thinks it's running out of buffered video; repeat
// the last frame for whatever minimum period it think it needs.  Note that
// this means that no *real* frame can be inserted during this period.
void
MediaEngineWebRTCVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                         SourceMediaStream *aSource,
                                         TrackID aID,
                                         StreamTime aDesiredTime,
                                         TrackTicks &aLastEndTime)
{
  VideoSegment segment;

  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted)
    return;

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  TrackTicks target = TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
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
    if (image) {
      segment.AppendFrame(image.forget(), delta, gfxIntSize(mWidth, mHeight));
    } else {
      segment.AppendFrame(nullptr, delta, gfxIntSize(0,0));
    }
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    if (aSource->AppendToTrack(aID, &(segment))) {
      aLastEndTime = target;
    }
  }
}

void
MediaEngineWebRTCVideoSource::ChooseCapability(const MediaEnginePrefs &aPrefs)
{
#ifdef MOZ_B2G_CAMERA
  mCapability.width  = aPrefs.mWidth;
  mCapability.height = aPrefs.mHeight;
#else
  int num = mViECapture->NumberOfCapabilities(NS_ConvertUTF16toUTF8(mUniqueId).get(),
                                              KMaxUniqueIdLength);

  LOG(("ChooseCapability: prefs: %dx%d @%d-%dfps", aPrefs.mWidth, aPrefs.mHeight, aPrefs.mFPS, aPrefs.mMinFPS));

  if (num <= 0) {
    // Set to default values
    mCapability.width  = aPrefs.mWidth;
    mCapability.height = aPrefs.mHeight;
    mCapability.maxFPS = MediaEngine::DEFAULT_VIDEO_FPS;

    // Mac doesn't support capabilities.
    return;
  }

  // Default is closest to available capability but equal to or below;
  // otherwise closest above.  Since we handle the num=0 case above and
  // take the first entry always, we can never exit uninitialized.
  webrtc::CaptureCapability cap;
  bool higher = true;
  for (int i = 0; i < num; i++) {
    mViECapture->GetCaptureCapability(NS_ConvertUTF16toUTF8(mUniqueId).get(),
                                      KMaxUniqueIdLength, i, cap);
    if (higher) {
      if (i == 0 ||
          (mCapability.width > cap.width && mCapability.height > cap.height)) {
        // closer than the current choice
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
      if (cap.width <= (uint32_t) aPrefs.mWidth && cap.height <= (uint32_t) aPrefs.mHeight) {
        higher = false;
      }
    } else {
      if (cap.width > (uint32_t) aPrefs.mWidth || cap.height > (uint32_t) aPrefs.mHeight ||
          cap.maxFPS < (uint32_t) aPrefs.mMinFPS) {
        continue;
      }
      if (mCapability.width < cap.width && mCapability.height < cap.height) {
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
    }
  }
  LOG(("chose cap %dx%d @%dfps", mCapability.width, mCapability.height, mCapability.maxFPS));
#endif
}

void
MediaEngineWebRTCVideoSource::GetName(nsAString& aName)
{
  aName = mDeviceName;
}

void
MediaEngineWebRTCVideoSource::GetUUID(nsAString& aUUID)
{
  aUUID = mUniqueId;
}

nsresult
MediaEngineWebRTCVideoSource::Allocate(const MediaEnginePrefs &aPrefs)
{
  LOG((__FUNCTION__));
#ifdef MOZ_B2G_CAMERA
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  if (mState == kReleased && mInitDone) {
    ChooseCapability(aPrefs);
    NS_DispatchToMainThread(WrapRunnable(this,
                                         &MediaEngineWebRTCVideoSource::AllocImpl));
    mCallbackMonitor.Wait();
    if (mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }
  }
#else
  if (mState == kReleased && mInitDone) {
    // Note: if shared, we don't allow a later opener to affect the resolution.
    // (This may change depending on spec changes for Constraints/settings)

    ChooseCapability(aPrefs);

    if (mViECapture->AllocateCaptureDevice(NS_ConvertUTF16toUTF8(mUniqueId).get(),
                                           KMaxUniqueIdLength, mCaptureIndex)) {
      return NS_ERROR_FAILURE;
    }
    mState = kAllocated;
    LOG(("Video device %d allocated", mCaptureIndex));
  } else if (mSources.IsEmpty()) {
    LOG(("Video device %d reallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d allocated shared", mCaptureIndex));
  }
#endif

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Deallocate()
{
  LOG((__FUNCTION__));
  if (mSources.IsEmpty()) {
#ifdef MOZ_B2G_CAMERA
    ReentrantMonitorAutoEnter sync(mCallbackMonitor);
#endif
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }
#ifdef MOZ_B2G_CAMERA
    // We do not register success callback here

    NS_DispatchToMainThread(WrapRunnable(this,
                                         &MediaEngineWebRTCVideoSource::DeallocImpl));
    mCallbackMonitor.Wait();
    if (mState != kReleased) {
      return NS_ERROR_FAILURE;
    }
#elif XP_MACOSX
    // Bug 829907 - on mac, in shutdown, the mainthread stops processing
    // 'native' events, and the QTKit code uses events to the main native CFRunLoop
    // in order to provide thread safety.  In order to avoid this locking us up,
    // release the ViE capture device synchronously on MainThread (so the native
    // event isn't needed).
    // XXX Note if MainThread Dispatch()es NS_DISPATCH_SYNC to us we can deadlock.
    // XXX It might be nice to only do this if we're in shutdown...  Hard to be
    // sure when that is though.
    // Thread safety: a) we call this synchronously, and don't use ViECapture from
    // another thread anywhere else, b) ViEInputManager::DestroyCaptureDevice() grabs
    // an exclusive object lock and deletes it in a critical section, so all in all
    // this should be safe threadwise.
    NS_DispatchToMainThread(WrapRunnable(mViECapture,
                                         &webrtc::ViECapture::ReleaseCaptureDevice,
                                         mCaptureIndex),
                            NS_DISPATCH_SYNC);
#else
    mViECapture->ReleaseCaptureDevice(mCaptureIndex);
#endif
    mState = kReleased;
    LOG(("Video device %d deallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d deallocated but still in use", mCaptureIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  LOG((__FUNCTION__));
  int error = 0;
  if (!mInitDone || !aStream) {
    return NS_ERROR_FAILURE;
  }

  mSources.AppendElement(aStream);

  aStream->AddTrack(aID, USECS_PER_S, 0, new VideoSegment());
  aStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);

#ifdef MOZ_B2G_CAMERA
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
#endif

  if (mState == kStarted) {
    return NS_OK;
  }
  mImageContainer = layers::LayerManager::CreateImageContainer();

#ifdef MOZ_B2G_CAMERA
  NS_DispatchToMainThread(WrapRunnable(this,
                                       &MediaEngineWebRTCVideoSource::StartImpl,
                                       mCapability));
  mCallbackMonitor.Wait();
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
#else
  mState = kStarted;
  error = mViERender->AddRenderer(mCaptureIndex, webrtc::kVideoI420, (webrtc::ExternalRenderer*)this);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }

  error = mViERender->StartRender(mCaptureIndex);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }

  if (mViECapture->StartCapture(mCaptureIndex, mCapability) < 0) {
    return NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  LOG((__FUNCTION__));
  if (!mSources.RemoveElement(aSource)) {
    // Already stopped - this is allowed
    return NS_OK;
  }
  if (!mSources.IsEmpty()) {
    return NS_OK;
  }
#ifdef MOZ_B2G_CAMERA
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
#endif
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
#ifdef MOZ_B2G_CAMERA
  NS_DispatchToMainThread(WrapRunnable(this,
                                       &MediaEngineWebRTCVideoSource::StopImpl));
#else
  mViERender->StopRender(mCaptureIndex);
  mViERender->RemoveRenderer(mCaptureIndex);
  mViECapture->StopCapture(mCaptureIndex);
#endif

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Snapshot(uint32_t aDuration, nsIDOMFile** aFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * Initialization and Shutdown functions for the video source, called by the
 * constructor and destructor respectively.
 */

void
MediaEngineWebRTCVideoSource::Init()
{
#ifdef MOZ_B2G_CAMERA
  nsAutoCString deviceName;
  mCameraManager->GetCameraName(mCaptureIndex, deviceName);
  CopyUTF8toUTF16(deviceName, mDeviceName);
  CopyUTF8toUTF16(deviceName, mUniqueId);
#else
  // fix compile warning for these being unused. (remove once used)
  (void) mFps;
  (void) mMinFps;

  LOG((__FUNCTION__));
  if (mVideoEngine == nullptr) {
    return;
  }

  mViEBase = webrtc::ViEBase::GetInterface(mVideoEngine);
  if (mViEBase == nullptr) {
    return;
  }

  // Get interfaces for capture, render for now
  mViECapture = webrtc::ViECapture::GetInterface(mVideoEngine);
  mViERender = webrtc::ViERender::GetInterface(mVideoEngine);

  if (mViECapture == nullptr || mViERender == nullptr) {
    return;
  }

  const uint32_t KMaxDeviceNameLength = 128;
  const uint32_t KMaxUniqueIdLength = 256;
  char deviceName[KMaxDeviceNameLength];
  char uniqueId[KMaxUniqueIdLength];
  if (mViECapture->GetCaptureDevice(mCaptureIndex,
                                    deviceName, KMaxDeviceNameLength,
                                    uniqueId, KMaxUniqueIdLength)) {
    return;
  }

  CopyUTF8toUTF16(deviceName, mDeviceName);
  CopyUTF8toUTF16(uniqueId, mUniqueId);
#endif

  mInitDone = true;
}

void
MediaEngineWebRTCVideoSource::Shutdown()
{
  LOG((__FUNCTION__));
  if (!mInitDone) {
    return;
  }
#ifdef MOZ_B2G_CAMERA
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
#endif
  if (mState == kStarted) {
    while (!mSources.IsEmpty()) {
      Stop(mSources[0], kVideoTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  if (mState == kAllocated || mState == kStopped) {
    Deallocate();
  }
#ifndef MOZ_B2G_CAMERA
  mViECapture->Release();
  mViERender->Release();
  mViEBase->Release();
#endif
  mState = kReleased;
  mInitDone = false;
}

#ifdef MOZ_B2G_CAMERA

// All these functions must be run on MainThread!
void
MediaEngineWebRTCVideoSource::AllocImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  mDOMCameraControl = new nsDOMCameraControl(mCaptureIndex,
                                             mCameraThread,
                                             this,
                                             this,
                                             nsGlobalWindow::GetInnerWindowWithId(mWindowId));
  mCameraManager->Register(mDOMCameraControl);
}

void
MediaEngineWebRTCVideoSource::DeallocImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  mNativeCameraControl->ReleaseHardware(this, this);
  mNativeCameraControl = nullptr;
}

void
MediaEngineWebRTCVideoSource::StartImpl(webrtc::CaptureCapability aCapability) {
  MOZ_ASSERT(NS_IsMainThread());

  idl::CameraSize size;
  size.width = aCapability.width;
  size.height = aCapability.height;
  mNativeCameraControl->GetPreviewStream(size, this, this);
}

void
MediaEngineWebRTCVideoSource::StopImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  mNativeCameraControl->StopPreview();
  mPreviewStream = nullptr;
}

void
MediaEngineWebRTCVideoSource::SnapshotImpl() {

  MOZ_ASSERT(NS_IsMainThread());

  idl::CameraSize size;
  size.width = mCapability.width;
  size.height = mCapability.height;

  idl::CameraPosition cameraPosition;
  cameraPosition.latitude = NAN;
  cameraPosition.longitude = NAN;
  cameraPosition.altitude = NAN;
  cameraPosition.timestamp = NAN;

  mNativeCameraControl->TakePicture(size, 0, NS_LITERAL_STRING("jpeg"), cameraPosition, PR_Now() / 1000000, this, this);
}

// nsICameraGetCameraCallback
nsresult
MediaEngineWebRTCVideoSource::HandleEvent(nsISupports* /* unused */) {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  mNativeCameraControl = static_cast<nsGonkCameraControl*>(mDOMCameraControl->GetNativeCameraControl().get());
  mState = kAllocated;
  mCallbackMonitor.Notify();
  return NS_OK;
}

// nsICameraPreviewStreamCallback
nsresult
MediaEngineWebRTCVideoSource::HandleEvent(nsIDOMMediaStream* stream) {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  mPreviewStream = static_cast<DOMCameraPreview*>(stream);
  mPreviewStream->Start();
  CameraPreviewMediaStream* cameraStream = static_cast<CameraPreviewMediaStream*>(mPreviewStream->GetStream());
  cameraStream->SetFrameCallback(this);
  mState = kStarted;
  mCallbackMonitor.Notify();
  return NS_OK;
}

// nsICameraTakePictureCallback
nsresult
MediaEngineWebRTCVideoSource::HandleEvent(nsIDOMBlob* picture) {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  mLastCapture = static_cast<nsIDOMFile*>(picture);
  mCallbackMonitor.Notify();
  return NS_OK;
}

// nsICameraReleaseCallback
nsresult
MediaEngineWebRTCVideoSource::HandleEvent() {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  mState = kReleased;
  mCallbackMonitor.Notify();
  return NS_OK;
}

// nsICameraErrorCallback
nsresult
MediaEngineWebRTCVideoSource::HandleEvent(const nsAString& error) {
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter sync(mCallbackMonitor);
  mCallbackMonitor.Notify();
  return NS_OK;
}

//Except this one. This callback should called on camera preview thread.
void
MediaEngineWebRTCVideoSource::OnNewFrame(const gfxIntSize& aIntrinsicSize, layers::Image* aImage) {
  MonitorAutoLock enter(mMonitor);
  mImage = aImage;
  if (mWidth != aIntrinsicSize.width || mHeight != aIntrinsicSize.height) {
    mWidth = aIntrinsicSize.width;
    mHeight = aIntrinsicSize.height;
    LOG(("Video FrameSizeChange: %ux%u", mWidth, mHeight));
  }
}
#endif

}
