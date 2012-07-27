/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineWebRTC.h"

namespace mozilla {

/**
 * Webrtc video source.
 */
NS_IMPL_THREADSAFE_ISUPPORTS1(MediaEngineWebRTCVideoSource, nsIRunnable)

// Static variables to hold device names and UUIDs.
const unsigned int MediaEngineWebRTCVideoSource::KMaxDeviceNameLength = 128;
const unsigned int MediaEngineWebRTCVideoSource::KMaxUniqueIdLength = 256;

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
  ReentrantMonitorAutoEnter enter(mMonitor);

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
    return 0;
  }

  // Create a video frame and append it to the track.
  layers::Image::Format format = layers::Image::PLANAR_YCBCR;
  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(&format, 1);

  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());

  PRUint8* frame = static_cast<PRUint8*> (buffer);
  const PRUint8 lumaBpp = 8;
  const PRUint8 chromaBpp = 4;

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
  data.mStereoMode = layers::STEREO_MODE_MONO;

  videoImage->SetData(data);

  VideoSegment segment;
  segment.AppendFrame(image.forget(), 1, gfxIntSize(mWidth, mHeight));
  mSource->AppendToTrack(mTrackID, &(segment));
  return 0;
}

void
MediaEngineWebRTCVideoSource::GetName(nsAString& aName)
{
  char deviceName[KMaxDeviceNameLength];
  memset(deviceName, 0, KMaxDeviceNameLength);

  char uniqueId[KMaxUniqueIdLength];
  memset(uniqueId, 0, KMaxUniqueIdLength);

  if (mInitDone) {
    mViECapture->GetCaptureDevice(
      mCapIndex, deviceName, KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength
    );
    aName.Assign(NS_ConvertASCIItoUTF16(deviceName));
  }
}

void
MediaEngineWebRTCVideoSource::GetUUID(nsAString& aUUID)
{
  char deviceName[KMaxDeviceNameLength];
  memset(deviceName, 0, KMaxDeviceNameLength);

  char uniqueId[KMaxUniqueIdLength];
  memset(uniqueId, 0, KMaxUniqueIdLength);

  if (mInitDone) {
    mViECapture->GetCaptureDevice(
      mCapIndex, deviceName, KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength
    );
    aUUID.Assign(NS_ConvertASCIItoUTF16(uniqueId));
  }
}

nsresult
MediaEngineWebRTCVideoSource::Allocate()
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  char deviceName[KMaxDeviceNameLength];
  memset(deviceName, 0, KMaxDeviceNameLength);

  char uniqueId[KMaxUniqueIdLength];
  memset(uniqueId, 0, KMaxUniqueIdLength);

  mViECapture->GetCaptureDevice(
    mCapIndex, deviceName, KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength
  );

  if (mViECapture->AllocateCaptureDevice(uniqueId, KMaxUniqueIdLength, mCapIndex)) {
    return NS_ERROR_FAILURE;
  }

  if (mViECapture->StartCapture(mCapIndex) < 0) {
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

  mViECapture->StopCapture(mCapIndex);
  mViECapture->ReleaseCaptureDevice(mCapIndex);
  mState = kReleased;
  return NS_OK;
}

MediaEngineVideoOptions
MediaEngineWebRTCVideoSource::GetOptions()
{
  MediaEngineVideoOptions aOpts;
  aOpts.mWidth = mWidth;
  aOpts.mHeight = mHeight;
  aOpts.mMaxFPS = mFps;
  aOpts.codecType = kVideoCodecI420;
  return aOpts;
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
  mSource->AddTrack(aID, mFps, 0, new VideoSegment());
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  error = mViERender->AddRenderer(mCapIndex, webrtc::kVideoI420, (webrtc::ExternalRenderer*)this);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }

  error = mViERender->StartRender(mCapIndex);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }

  mState = kStarted;
  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Stop()
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }

  mSource->EndTrack(mTrackID);
  mSource->Finish();

  mViERender->StopRender(mCapIndex);
  mViERender->RemoveRenderer(mCapIndex);

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineWebRTCVideoSource::Snapshot(PRUint32 aDuration, nsIDOMFile** aFile)
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
  *aFile = nsnull;
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
  error = mViERender->AddRenderer(mCapIndex, webrtc::kVideoI420, (webrtc::ExternalRenderer*)this);
  if (error == -1) {
    return NS_ERROR_FAILURE;
  }
  error = mViERender->StartRender(mCapIndex);
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
  if (vieFile->GetCaptureDeviceSnapshot(mCapIndex, path.get()) < 0) {
    delete mSnapshotPath;
    mSnapshotPath = NULL;
    return NS_ERROR_FAILURE;
  }

  // Stop the camera.
  mViERender->StopRender(mCapIndex);
  mViERender->RemoveRenderer(mCapIndex);

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
    mViERender->StopRender(mCapIndex);
    mViERender->RemoveRenderer(mCapIndex);
    continueShutdown = true;
  }

  if (mState == kAllocated || continueShutdown) {
    mViECapture->StopCapture(mCapIndex);
    mViECapture->ReleaseCaptureDevice(mCapIndex);
    continueShutdown = false;
  }

  mViECapture->Release();
  mViERender->Release();
  mViEBase->Release();
  mState = kReleased;
  mInitDone = false;
}

}
