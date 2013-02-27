/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"
#include "Layers.h"
#include "ImageTypes.h"
#include "ImageContainer.h"
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
NS_IMPL_THREADSAFE_ISUPPORTS1(MediaEngineWebRTCVideoSource, nsIRunnable)

// ViEExternalRenderer Callback.
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
   unsigned char* buffer, int size, uint32_t time_stamp, int64_t render_time)
{
  if (mInSnapshotMode) {
    // Set the condition variable to false and notify Snapshot().
    PR_Lock(mSnapshotLock);
    mInSnapshotMode = false;
    PR_NotifyCondVar(mSnapshotCondVar);
    PR_Unlock(mSnapshotLock);
    return 0;
  }

  // Check for proper state.
  if (mState != kStarted) {
    LOG(("DeliverFrame: video not started"));
    return 0;
  }

  // Create a video frame and append it to the track.
  ImageFormat format = PLANAR_YCBCR;

  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(&format, 1);

  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());

  uint8_t* frame = static_cast<uint8_t*> (buffer);
  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  layers::PlanarYCbCrImage::Data data;
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
  ReentrantMonitorAutoEnter enter(mMonitor);

  // implicitly releases last image
  mImage = image.forget();

  return 0;
}

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

  ReentrantMonitorAutoEnter enter(mMonitor);
  if (mState != kStarted)
    return;

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  TrackTicks target = TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
  TrackTicks delta = target - aLastEndTime;
  LOGFRAME(("NotifyPull, desired = %ld, target = %ld, delta = %ld %s", (int64_t) aDesiredTime, 
            (int64_t) target, (int64_t) delta, image ? "" : "<null>"));
  // Don't append if we've already provided a frame that supposedly goes past the current aDesiredTime
  // Doing so means a negative delta and thus messes up handling of the graph
  if (delta > 0) {
    // NULL images are allowed
    segment.AppendFrame(image ? image.forget() : nullptr, delta, gfxIntSize(mWidth, mHeight));
    aSource->AppendToTrack(aID, &(segment));
    aLastEndTime = target;
  }
}

void
MediaEngineWebRTCVideoSource::ChooseCapability(uint32_t aWidth, uint32_t aHeight, uint32_t aMinFPS)
{
  int num = mViECapture->NumberOfCapabilities(mUniqueId, KMaxUniqueIdLength);

  NS_WARN_IF_FALSE(!mCapabilityChosen,"Shouldn't select capability of a device twice");

  if (num <= 0) {
    // Set to default values
    mCapability.width  = mOpts.mWidth  = aWidth;
    mCapability.height = mOpts.mHeight = aHeight;
    mCapability.maxFPS = mOpts.mMaxFPS = DEFAULT_VIDEO_FPS;
    mOpts.codecType = kVideoCodecI420;

    // Mac doesn't support capabilities.
    mCapabilityChosen = true;
    return;
  }

  // Default is closest to available capability but equal to or below;
  // otherwise closest above.  Since we handle the num=0 case above and
  // take the first entry always, we can never exit uninitialized.
  webrtc::CaptureCapability cap;
  bool higher = true;
  for (int i = 0; i < num; i++) {
    mViECapture->GetCaptureCapability(mUniqueId, KMaxUniqueIdLength, i, cap);
    if (higher) {
      if (i == 0 ||
          (mOpts.mWidth > cap.width && mOpts.mHeight > cap.height)) {
        mOpts.mWidth = cap.width;
        mOpts.mHeight = cap.height;
        mOpts.mMaxFPS = cap.maxFPS;
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
      if (cap.width <= aWidth && cap.height <= aHeight) {
        higher = false;
      }
    } else {
      if (cap.width > aWidth || cap.height > aHeight || cap.maxFPS < aMinFPS) {
        continue;
      }
      if (mOpts.mWidth < cap.width && mOpts.mHeight < cap.height) {
        mOpts.mWidth = cap.width;
        mOpts.mHeight = cap.height;
        mOpts.mMaxFPS = cap.maxFPS;
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
    }
  }
  LOG(("chose cap %dx%d @%dfps", mOpts.mWidth, mOpts.mHeight, mOpts.mMaxFPS));
  mCapabilityChosen = true;
}

void
MediaEngineWebRTCVideoSource::GetName(nsAString& aName)
{
  // mDeviceName is UTF8
  CopyUTF8toUTF16(mDeviceName, aName);
}

void
MediaEngineWebRTCVideoSource::GetUUID(nsAString& aUUID)
{
  // mUniqueId is UTF8
  CopyUTF8toUTF16(mUniqueId, aUUID);
}

nsresult
MediaEngineWebRTCVideoSource::Allocate()
{
  LOG((__FUNCTION__));
  if (!mCapabilityChosen) {
    // XXX these should come from constraints
    ChooseCapability(mWidth, mHeight, mMinFps);
  }

  if (mState == kReleased && mInitDone) {
    if (mViECapture->AllocateCaptureDevice(mUniqueId, KMaxUniqueIdLength, mCaptureIndex)) {
      return NS_ERROR_FAILURE;
    }
    mState = kAllocated;
    LOG(("Video device %d allocated", mCaptureIndex));
  } else if (mSources.IsEmpty()) {
    LOG(("Video device %d reallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d allocated shared", mCaptureIndex));
  }

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Deallocate()
{
  LOG((__FUNCTION__));
  if (mSources.IsEmpty()) {
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }

#ifdef XP_MACOSX
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

const MediaEngineVideoOptions*
MediaEngineWebRTCVideoSource::GetOptions()
{
  if (!mCapabilityChosen) {
    ChooseCapability(mWidth, mHeight, mMinFps);
  }
  return &mOpts;
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

  if (mState == kStarted) {
    return NS_OK;
  }
  mState = kStarted;

  mImageContainer = layers::LayerManager::CreateImageContainer();

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

  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    mState = kStopped;
    aSource->EndTrack(aID);
    // Drop any cached image so we don't start with a stale image on next
    // usage
    mImage = nullptr;
  }

  mViERender->StopRender(mCaptureIndex);
  mViERender->RemoveRenderer(mCaptureIndex);
  mViECapture->StopCapture(mCaptureIndex);

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Snapshot(uint32_t aDuration, nsIDOMFile** aFile)
{
  /**
   * To get a Snapshot we do the following:
   * - Set a condition variable (mInSnapshotMode) to true
   * - Attach the external renderer and start the camera
   * - Wait for the condition variable to change to false
   *
   * Starting the camera has the effect of invoking DeliverFrame() when
   * the first frame arrives from the camera. We only need one frame for
   * GetCaptureDeviceSnapshot to work, so we immediately set the condition
   * variable to false and notify this method.
   *
   * This causes the current thread to continue (PR_CondWaitVar will return),
   * at which point we can grab a snapshot, convert it to a file and
   * return from this function after cleaning up the temporary stream object
   * and caling Stop() on the media source.
   */
  *aFile = nullptr;
  if (!mInitDone || mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mSnapshotLock = PR_NewLock();
  mSnapshotCondVar = PR_NewCondVar(mSnapshotLock);

  PR_Lock(mSnapshotLock);
  mInSnapshotMode = true;

  // Start the rendering (equivalent to calling Start(), but without a track).
  int error = 0;
  if (!mInitDone || mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  error = mViERender->AddRenderer(mCaptureIndex, webrtc::kVideoI420, (webrtc::ExternalRenderer*)this);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }
  error = mViERender->StartRender(mCaptureIndex);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }

  // Wait for the condition variable, will be set in DeliverFrame.
  // We use a while loop, because even if PR_WaitCondVar returns, it's not
  // guaranteed that the condition variable changed.
  while (mInSnapshotMode) {
    PR_WaitCondVar(mSnapshotCondVar, PR_INTERVAL_NO_TIMEOUT);
  }

  // If we get here, DeliverFrame received at least one frame.
  PR_Unlock(mSnapshotLock);
  PR_DestroyCondVar(mSnapshotCondVar);
  PR_DestroyLock(mSnapshotLock);

  webrtc::ViEFile* vieFile = webrtc::ViEFile::GetInterface(mVideoEngine);
  if (!vieFile) {
    return NS_ERROR_FAILURE;
  }

  // Create a temporary file on the main thread and put the snapshot in it.
  // See Run() in MediaEngineWebRTCVideo.h (sets mSnapshotPath).
  NS_DispatchToMainThread(this, NS_DISPATCH_SYNC);

  if (!mSnapshotPath) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF16toUTF8 path(*mSnapshotPath);
  if (vieFile->GetCaptureDeviceSnapshot(mCaptureIndex, path.get()) < 0) {
    delete mSnapshotPath;
    mSnapshotPath = NULL;
    return NS_ERROR_FAILURE;
  }

  // Stop the camera.
  mViERender->StopRender(mCaptureIndex);
  mViERender->RemoveRenderer(mCaptureIndex);

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(*mSnapshotPath, false, getter_AddRefs(file));

  delete mSnapshotPath;
  mSnapshotPath = NULL;

  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aFile = new nsDOMFileFile(file));

  return NS_OK;
}

/**
 * Initialization and Shutdown functions for the video source, called by the
 * constructor and destructor respectively.
 */

void
MediaEngineWebRTCVideoSource::Init()
{
  mDeviceName[0] = '\0'; // paranoia
  mUniqueId[0] = '\0';

  (void) mFps; // fix compile warning for this being unused. (remove once used)

  LOG((__FUNCTION__));
  if (mVideoEngine == NULL) {
    return;
  }

  mViEBase = webrtc::ViEBase::GetInterface(mVideoEngine);
  if (mViEBase == NULL) {
    return;
  }

  // Get interfaces for capture, render for now
  mViECapture = webrtc::ViECapture::GetInterface(mVideoEngine);
  mViERender = webrtc::ViERender::GetInterface(mVideoEngine);

  if (mViECapture == NULL || mViERender == NULL) {
    return;
  }

  if (mViECapture->GetCaptureDevice(mCaptureIndex,
                                    mDeviceName, sizeof(mDeviceName),
                                    mUniqueId, sizeof(mUniqueId))) {
    return;
  }

  mInitDone = true;
}

void
MediaEngineWebRTCVideoSource::Shutdown()
{
  LOG((__FUNCTION__));
  if (!mInitDone) {
    return;
  }

  if (mState == kStarted) {
    while (!mSources.IsEmpty()) {
      Stop(mSources[0], kVideoTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  if (mState == kAllocated || mState == kStopped) {
    Deallocate();
  }

  mViECapture->Release();
  mViERender->Release();
  mViEBase->Release();
  mState = kReleased;
  mInitDone = false;
}

}
