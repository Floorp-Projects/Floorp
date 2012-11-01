/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"
#include "Layers.h"
#include "ImageTypes.h"
#include "ImageContainer.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#else
#define LOG(msg)
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

#ifdef LOG_ALL_FRAMES
  static uint32_t frame_num = 0;
  LOG(("frame %d; timestamp %u, render_time %lu", frame_num++, time_stamp, render_time));
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
                                         StreamTime aDesiredTime)
{
  VideoSegment segment;

  ReentrantMonitorAutoEnter enter(mMonitor);
  if (mState != kStarted)
    return;

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  TrackTicks target = TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
  TrackTicks delta = target - mLastEndTime;
#ifdef LOG_ALL_FRAMES
  LOG(("NotifyPull, target = %lu, delta = %lu", (uint64_t) target, (uint64_t) delta));
#endif
  // NULL images are allowed
  segment.AppendFrame(image ? image.forget() : nullptr, delta, gfxIntSize(mWidth, mHeight));
  mSource->AppendToTrack(mTrackID, &(segment));
  mLastEndTime = target;
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
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  if (!mCapabilityChosen) {
    // XXX these should come from constraints
    ChooseCapability(mWidth, mHeight, mMinFps);
  }

  if (mViECapture->AllocateCaptureDevice(mUniqueId, KMaxUniqueIdLength, mCaptureIndex)) {
    return NS_ERROR_FAILURE;
  }

  mState = kAllocated;
  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Deallocate()
{
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mViECapture->ReleaseCaptureDevice(mCaptureIndex);
  mState = kReleased;
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
  int error = 0;
  if (!mInitDone || mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  if (!aStream) {
    return NS_ERROR_FAILURE;
  }

  if (mState == kStarted) {
    return NS_OK;
  }

  mSource = aStream;
  mTrackID = aID;

  mImageContainer = layers::LayerManager::CreateImageContainer();

  mSource->AddTrack(aID, USECS_PER_S, 0, new VideoSegment());
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  mLastEndTime = 0;
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

  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Stop()
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    mState = kStopped;
    mSource->EndTrack(mTrackID);
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
  bool continueShutdown = false;

  if (!mInitDone) {
    return;
  }

  if (mState == kStarted) {
    mViERender->StopRender(mCaptureIndex);
    mViERender->RemoveRenderer(mCaptureIndex);
    continueShutdown = true;
  }

  if (mState == kAllocated || continueShutdown) {
    mViECapture->StopCapture(mCaptureIndex);
    mViECapture->ReleaseCaptureDevice(mCaptureIndex);
    continueShutdown = false;
  }

  mViECapture->Release();
  mViERender->Release();
  mViEBase->Release();
  mState = kReleased;
  mInitDone = false;
}

}
