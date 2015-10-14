/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineRemoteVideoSource.h"

#include "mozilla/RefPtr.h"
#include "VideoUtils.h"
#include "nsIPrefService.h"
#include "MediaTrackConstraints.h"
#include "CamerasChild.h"

extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOGFRAME(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

namespace mozilla {

using dom::ConstrainLongRange;

NS_IMPL_ISUPPORTS0(MediaEngineRemoteVideoSource)

MediaEngineRemoteVideoSource::MediaEngineRemoteVideoSource(
  int aIndex, mozilla::camera::CaptureEngine aCapEngine,
  dom::MediaSourceEnum aMediaSource, const char* aMonitorName)
  : MediaEngineCameraVideoSource(aIndex, aMonitorName),
    mMediaSource(aMediaSource),
    mCapEngine(aCapEngine)
{
  MOZ_ASSERT(aMediaSource != dom::MediaSourceEnum::Other);
  Init();
}

void
MediaEngineRemoteVideoSource::Init()
{
  LOG((__PRETTY_FUNCTION__));
  char deviceName[kMaxDeviceNameLength];
  char uniqueId[kMaxUniqueIdLength];
  if (mozilla::camera::GetCaptureDevice(mCapEngine,
                                        mCaptureIndex,
                                        deviceName, kMaxDeviceNameLength,
                                        uniqueId, kMaxUniqueIdLength)) {
    LOG(("Error initializing RemoteVideoSource (GetCaptureDevice)"));
    return;
  }

  SetName(NS_ConvertUTF8toUTF16(deviceName));
  SetUUID(uniqueId);

  mInitDone = true;

  return;
}

void
MediaEngineRemoteVideoSource::Shutdown()
{
  LOG((__PRETTY_FUNCTION__));
  if (!mInitDone) {
    return;
  }
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

  mozilla::camera::Shutdown();

  mState = kReleased;
  mInitDone = false;
  return;
}

nsresult
MediaEngineRemoteVideoSource::Allocate(const dom::MediaTrackConstraints& aConstraints,
                                       const MediaEnginePrefs& aPrefs,
                                       const nsString& aDeviceId)
{
  LOG((__PRETTY_FUNCTION__));

  if (!mInitDone) {
    LOG(("Init not done"));
    return NS_ERROR_FAILURE;
  }

  if (mState == kReleased) {
    // Note: if shared, we don't allow a later opener to affect the resolution.
    // (This may change depending on spec changes for Constraints/settings)

    if (!ChooseCapability(aConstraints, aPrefs, aDeviceId)) {
      return NS_ERROR_UNEXPECTED;
    }

    if (mozilla::camera::AllocateCaptureDevice(mCapEngine,
                                               GetUUID().get(),
                                               kMaxUniqueIdLength, mCaptureIndex)) {
      return NS_ERROR_FAILURE;
    }
    mState = kAllocated;
    LOG(("Video device %d allocated", mCaptureIndex));
  } else if (MOZ_LOG_TEST(GetMediaManagerLog(), mozilla::LogLevel::Debug)) {
    MonitorAutoLock lock(mMonitor);
    if (mSources.IsEmpty()) {
      LOG(("Video device %d reallocated", mCaptureIndex));
    } else {
      LOG(("Video device %d allocated shared", mCaptureIndex));
    }
  }

  ++mNrAllocations;

  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Deallocate()
{
  LOG((__PRETTY_FUNCTION__));

  --mNrAllocations;
  MOZ_ASSERT(mNrAllocations >= 0, "Double-deallocations are prohibited");

  if (mNrAllocations == 0) {
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }
    mozilla::camera::ReleaseCaptureDevice(mCapEngine, mCaptureIndex);
    mState = kReleased;
    LOG(("Video device %d deallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d deallocated but still in use", mCaptureIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  LOG((__PRETTY_FUNCTION__));
  if (!mInitDone || !aStream) {
    LOG(("No stream or init not done"));
    return NS_ERROR_FAILURE;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
  }

  aStream->AddTrack(aID, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);

  if (mState == kStarted) {
    return NS_OK;
  }
  mImageContainer = layers::LayerManager::CreateImageContainer();

  mState = kStarted;
  mTrackID = aID;

  if (mozilla::camera::StartCapture(mCapEngine,
                                    mCaptureIndex, mCapability, this)) {
    LOG(("StartCapture failed"));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Stop(mozilla::SourceMediaStream* aSource,
                                   mozilla::TrackID aID)
{
  LOG((__PRETTY_FUNCTION__));
  {
    MonitorAutoLock lock(mMonitor);

    if (!mSources.RemoveElement(aSource)) {
      // Already stopped - this is allowed
      return NS_OK;
    }

    aSource->EndTrack(aID);

    if (!mSources.IsEmpty()) {
      return NS_OK;
    }
    if (mState != kStarted) {
      return NS_ERROR_FAILURE;
    }

    mState = kStopped;
    // Drop any cached image so we don't start with a stale image on next
    // usage
    mImage = nullptr;
  }

  mozilla::camera::StopCapture(mCapEngine, mCaptureIndex);

  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Restart(const dom::MediaTrackConstraints& aConstraints,
                                      const MediaEnginePrefs& aPrefs,
                                      const nsString& aDeviceId)
{
  if (!mInitDone) {
    LOG(("Init not done"));
    return NS_ERROR_FAILURE;
  }
  if (!ChooseCapability(aConstraints, aPrefs, aDeviceId)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (mState != kStarted) {
    return NS_OK;
  }

  mozilla::camera::StopCapture(mCapEngine, mCaptureIndex);
  if (mozilla::camera::StartCapture(mCapEngine,
                                    mCaptureIndex, mCapability, this)) {
    LOG(("StartCapture failed"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
MediaEngineRemoteVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                         SourceMediaStream* aSource,
                                         TrackID aID, StreamTime aDesiredTime)
{
  VideoSegment segment;

  MonitorAutoLock lock(mMonitor);
  StreamTime delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);

  if (delta > 0) {
    // nullptr images are allowed
    AppendToTrack(aSource, mImage, aID, delta);
  }
}

int
MediaEngineRemoteVideoSource::FrameSizeChange(unsigned int w, unsigned int h,
                                              unsigned int streams)
{
  mWidth = w;
  mHeight = h;
  LOG(("MediaEngineRemoteVideoSource Video FrameSizeChange: %ux%u", w, h));
  return 0;
}

int
MediaEngineRemoteVideoSource::DeliverFrame(unsigned char* buffer,
                                           int size,
                                           uint32_t time_stamp,
                                           int64_t ntp_time,
                                           int64_t render_time,
                                           void *handle)
{
  // Check for proper state.
  if (mState != kStarted) {
    LOG(("DeliverFrame: video not started"));
    return 0;
  }

  if (mWidth*mHeight + 2*(((mWidth+1)/2)*((mHeight+1)/2)) != size) {
    MOZ_ASSERT(false, "Wrong size frame in DeliverFrame!");
    return 0;
  }

  // Create a video frame and append it to the track.
  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  layers::PlanarYCbCrImage* videoImage = static_cast<layers::PlanarYCbCrImage*>(image.get());

  uint8_t* frame = static_cast<uint8_t*> (buffer);
  const uint8_t lumaBpp = 8;
  const uint8_t chromaBpp = 4;

  // Take lots of care to round up!
  layers::PlanarYCbCrData data;
  data.mYChannel = frame;
  data.mYSize = IntSize(mWidth, mHeight);
  data.mYStride = (mWidth * lumaBpp + 7)/ 8;
  data.mCbCrStride = (mWidth * chromaBpp + 7) / 8;
  data.mCbChannel = frame + mHeight * data.mYStride;
  data.mCrChannel = data.mCbChannel + ((mHeight+1)/2) * data.mCbCrStride;
  data.mCbCrSize = IntSize((mWidth+1)/ 2, (mHeight+1)/ 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = IntSize(mWidth, mHeight);
  data.mStereoMode = StereoMode::MONO;

  videoImage->SetData(data);

#ifdef DEBUG
  static uint32_t frame_num = 0;
  LOGFRAME(("frame %d (%dx%d); timestamp %u, ntp_time %lu, render_time %lu", frame_num++,
            mWidth, mHeight, time_stamp, ntp_time, render_time));
#endif

  // we don't touch anything in 'this' until here (except for snapshot,
  // which has it's own lock)
  MonitorAutoLock lock(mMonitor);

  // implicitly releases last image
  mImage = image.forget();

  // Push the frame into the MSG with a minimal duration.  This will likely
  // mean we'll still get NotifyPull calls which will then return the same
  // frame again with a longer duration.  However, this means we won't
  // fail to get the frame in and drop frames.

  // XXX The timestamp for the frame should be based on the Capture time,
  // not the MSG time, and MSG should never, ever block on a (realtime)
  // video frame (or even really for streaming - audio yes, video probably no).
  // Note that MediaPipeline currently ignores the timestamps from MSG
  uint32_t len = mSources.Length();
  for (uint32_t i = 0; i < len; i++) {
    if (mSources[i]) {
      AppendToTrack(mSources[i], mImage, mTrackID, 1); // shortest possible duration
    }
  }

  return 0;
}

size_t
MediaEngineRemoteVideoSource::NumCapabilities()
{
  int num = mozilla::camera::NumberOfCapabilities(mCapEngine, GetUUID().get());
  if (num > 0) {
    return num;
  }

  switch(mMediaSource) {
  case dom::MediaSourceEnum::Camera:
#ifdef XP_MACOSX
    // Mac doesn't support capabilities.
    //
    // Hardcode generic desktop capabilities modeled on OSX camera.
    // Note: Values are empirically picked to be OSX friendly, as on OSX, values
    // other than these cause the source to not produce.

    if (mHardcodedCapabilities.IsEmpty()) {
      for (int i = 0; i < 9; i++) {
        webrtc::CaptureCapability c;
        c.width = 1920 - i*128;
        c.height = 1080 - i*72;
        c.maxFPS = 30;
        mHardcodedCapabilities.AppendElement(c);
      }
      for (int i = 0; i < 16; i++) {
        webrtc::CaptureCapability c;
        c.width = 640 - i*40;
        c.height = 480 - i*30;
        c.maxFPS = 30;
        mHardcodedCapabilities.AppendElement(c);
      }
    }
    break;
#endif
  default:
    webrtc::CaptureCapability c;
    // The default for devices that don't return discrete capabilities: treat
    // them as supporting all capabilities orthogonally. E.g. screensharing.
    c.width = 0; // 0 = accept any value
    c.height = 0;
    c.maxFPS = 0;
    mHardcodedCapabilities.AppendElement(c);
    break;
  }

  return mHardcodedCapabilities.Length();
}

bool
MediaEngineRemoteVideoSource::ChooseCapability(const MediaTrackConstraints &aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId)
{

  switch(mMediaSource) {
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Window:
    case dom::MediaSourceEnum::Application: {
      FlattenedConstraints c(aConstraints);
      mCapability.width = c.mWidth.Clamp(c.mWidth.mIdeal.WasPassed() ?
        c.mWidth.mIdeal.Value() : aPrefs.mWidth);
      mCapability.height = c.mHeight.Clamp(c.mHeight.mIdeal.WasPassed() ?
        c.mHeight.mIdeal.Value() : aPrefs.mHeight);
      mCapability.maxFPS = c.mFrameRate.Clamp(c.mFrameRate.mIdeal.WasPassed() ?
        c.mFrameRate.mIdeal.Value() : aPrefs.mFPS);
      return true;
    }
    default:
      return MediaEngineCameraVideoSource::ChooseCapability(aConstraints, aPrefs, aDeviceId);
  }

}

void
MediaEngineRemoteVideoSource::GetCapability(size_t aIndex,
                                            webrtc::CaptureCapability& aOut)
{
  if (!mHardcodedCapabilities.IsEmpty()) {
    MediaEngineCameraVideoSource::GetCapability(aIndex, aOut);
  }
  mozilla::camera::GetCaptureCapability(mCapEngine,
                                        GetUUID().get(),
                                        aIndex,
                                        aOut);
}

void MediaEngineRemoteVideoSource::Refresh(int aIndex) {
  // NOTE: mCaptureIndex might have changed when allocated!
  // Use aIndex to update information, but don't change mCaptureIndex!!
  // Caller looked up this source by uniqueId, so it shouldn't change
  char deviceName[kMaxDeviceNameLength];
  char uniqueId[kMaxUniqueIdLength];

  if (mozilla::camera::GetCaptureDevice(mCapEngine,
                                        aIndex,
                                        deviceName, sizeof(deviceName),
                                        uniqueId, sizeof(uniqueId))) {
    return;
  }

  SetName(NS_ConvertUTF8toUTF16(deviceName));
#ifdef DEBUG
  MOZ_ASSERT(GetUUID().Equals(uniqueId));
#endif
}

}
