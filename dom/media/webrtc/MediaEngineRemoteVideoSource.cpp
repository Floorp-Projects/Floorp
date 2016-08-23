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

extern mozilla::LogModule* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOGFRAME(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

namespace mozilla {

// These need a definition somewhere because template
// code is allowed to take their address, and they aren't
// guaranteed to have one without this.
const unsigned int MediaEngineSource::kMaxDeviceNameLength;
const unsigned int MediaEngineSource::kMaxUniqueIdLength;;

using dom::ConstrainLongRange;

NS_IMPL_ISUPPORTS0(MediaEngineRemoteVideoSource)

MediaEngineRemoteVideoSource::MediaEngineRemoteVideoSource(
  int aIndex, mozilla::camera::CaptureEngine aCapEngine,
  dom::MediaSourceEnum aMediaSource, bool aScary, const char* aMonitorName)
  : MediaEngineCameraVideoSource(aIndex, aMonitorName),
    mMediaSource(aMediaSource),
    mCapEngine(aCapEngine),
    mScary(aScary)
{
  MOZ_ASSERT(aMediaSource != dom::MediaSourceEnum::Other);
  mSettings.mWidth.Construct(0);
  mSettings.mHeight.Construct(0);
  mSettings.mFrameRate.Construct(0);
  Init();
}

void
MediaEngineRemoteVideoSource::Init()
{
  LOG((__PRETTY_FUNCTION__));
  char deviceName[kMaxDeviceNameLength];
  char uniqueId[kMaxUniqueIdLength];
  if (mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::GetCaptureDevice,
    mCapEngine, mCaptureIndex,
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
  Super::Shutdown();
  if (mState == kStarted) {
    SourceMediaStream *source;
    bool empty;

    while (1) {
      {
        MonitorAutoLock lock(mMonitor);
        empty = mSources.IsEmpty();
        if (empty) {
          MOZ_ASSERT(mPrincipalHandles.IsEmpty());
          break;
        }
        source = mSources[0];
      }
      Stop(source, kVideoTrack); // XXX change to support multiple tracks
    }
    MOZ_ASSERT(mState == kStopped);
  }

  for (auto& registered : mRegisteredHandles) {
    MOZ_ASSERT(mState == kAllocated || mState == kStopped);
    Deallocate(registered.get());
  }

  MOZ_ASSERT(mState == kReleased);
  mInitDone = false;
  return;
}

nsresult
MediaEngineRemoteVideoSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const nsACString& aOrigin,
    AllocationHandle** aOutHandle,
    const char** aOutBadConstraint)
{
  LOG((__PRETTY_FUNCTION__));
  AssertIsOnOwningThread();

  if (!mInitDone) {
    LOG(("Init not done"));
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Super::Allocate(aConstraints, aPrefs, aDeviceId, aOrigin,
                                aOutHandle, aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (mState == kStarted &&
      MOZ_LOG_TEST(GetMediaManagerLog(), mozilla::LogLevel::Debug)) {
    MonitorAutoLock lock(mMonitor);
    if (mSources.IsEmpty()) {
      MOZ_ASSERT(mPrincipalHandles.IsEmpty());
      LOG(("Video device %d reallocated", mCaptureIndex));
    } else {
      LOG(("Video device %d allocated shared", mCaptureIndex));
    }
  }
  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Deallocate(AllocationHandle* aHandle)
{
  LOG((__PRETTY_FUNCTION__));
  AssertIsOnOwningThread();

  Super::Deallocate(aHandle);

  if (!mRegisteredHandles.Length()) {
    if (mState != kStopped && mState != kAllocated) {
      return NS_ERROR_FAILURE;
    }
    mozilla::camera::GetChildAndCall(
      &mozilla::camera::CamerasChild::ReleaseCaptureDevice,
      mCapEngine, mCaptureIndex);
    mState = kReleased;
    LOG(("Video device %d deallocated", mCaptureIndex));
  } else {
    LOG(("Video device %d deallocated but still in use", mCaptureIndex));
  }
  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Start(SourceMediaStream* aStream, TrackID aID,
                                    const PrincipalHandle& aPrincipalHandle)
{
  LOG((__PRETTY_FUNCTION__));
  AssertIsOnOwningThread();
  if (!mInitDone || !aStream) {
    LOG(("No stream or init not done"));
    return NS_ERROR_FAILURE;
  }

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
    mPrincipalHandles.AppendElement(aPrincipalHandle);
    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
  }

  aStream->AddTrack(aID, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);

  if (mState == kStarted) {
    return NS_OK;
  }
  mImageContainer =
    layers::LayerManager::CreateImageContainer(layers::ImageContainer::ASYNCHRONOUS);

  mState = kStarted;
  mTrackID = aID;

  if (mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::StartCapture,
    mCapEngine, mCaptureIndex, mCapability, this)) {
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
  AssertIsOnOwningThread();
  {
    MonitorAutoLock lock(mMonitor);

    // Drop any cached image so we don't start with a stale image on next
    // usage
    mImage = nullptr;

    size_t i = mSources.IndexOf(aSource);
    if (i == mSources.NoIndex) {
      // Already stopped - this is allowed
      return NS_OK;
    }

    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
    mSources.RemoveElementAt(i);
    mPrincipalHandles.RemoveElementAt(i);

    aSource->EndTrack(aID);

    if (!mSources.IsEmpty()) {
      return NS_OK;
    }
    if (mState != kStarted) {
      return NS_ERROR_FAILURE;
    }

    mState = kStopped;
  }

  mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::StopCapture,
    mCapEngine, mCaptureIndex);

  return NS_OK;
}

nsresult
MediaEngineRemoteVideoSource::Restart(AllocationHandle* aHandle,
                                      const dom::MediaTrackConstraints& aConstraints,
                                      const MediaEnginePrefs& aPrefs,
                                      const nsString& aDeviceId,
                                      const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();
  if (!mInitDone) {
    LOG(("Init not done"));
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(aHandle);
  NormalizedConstraints constraints(aConstraints);
  return ReevaluateAllocation(aHandle, &constraints, aPrefs, aDeviceId,
                              aOutBadConstraint);
}

nsresult
MediaEngineRemoteVideoSource::UpdateSingleSource(
    const AllocationHandle* aHandle,
    const NormalizedConstraints& aNetConstraints,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  if (!ChooseCapability(aNetConstraints, aPrefs, aDeviceId)) {
    *aOutBadConstraint = FindBadConstraint(aNetConstraints, *this, aDeviceId);
    return NS_ERROR_FAILURE;
  }

  switch (mState) {
    case kReleased:
      MOZ_ASSERT(aHandle);
      if (camera::GetChildAndCall(&camera::CamerasChild::AllocateCaptureDevice,
                                  mCapEngine, GetUUID().get(),
                                  kMaxUniqueIdLength, mCaptureIndex,
                                  aHandle->mOrigin)) {
        return NS_ERROR_FAILURE;
      }
      mState = kAllocated;
      SetLastCapability(mCapability);
      LOG(("Video device %d allocated for %s", mCaptureIndex,
           aHandle->mOrigin.get()));
      break;

    case kStarted:
      if (mCapability != mLastCapability) {
        camera::GetChildAndCall(&camera::CamerasChild::StopCapture,
                                mCapEngine, mCaptureIndex);
        if (camera::GetChildAndCall(&camera::CamerasChild::StartCapture,
                                    mCapEngine, mCaptureIndex, mCapability,
                                    this)) {
          LOG(("StartCapture failed"));
          return NS_ERROR_FAILURE;
        }
        SetLastCapability(mCapability);
      }
      break;

    default:
      LOG(("Video device %d %s in ignored state %d", mCaptureIndex,
             (aHandle? aHandle->mOrigin.get() : ""), mState));
      break;
  }
  return NS_OK;
}

void
MediaEngineRemoteVideoSource::SetLastCapability(
    const webrtc::CaptureCapability& aCapability)
{
  mLastCapability = mCapability;

  webrtc::CaptureCapability cap = aCapability;
  RefPtr<MediaEngineRemoteVideoSource> that = this;

  NS_DispatchToMainThread(media::NewRunnableFrom([this, that, cap]() mutable {
    mSettings.mWidth.Value() = cap.width;
    mSettings.mHeight.Value() = cap.height;
    mSettings.mFrameRate.Value() = cap.maxFPS;
    return NS_OK;
  }));
}

void
MediaEngineRemoteVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                         SourceMediaStream* aSource,
                                         TrackID aID, StreamTime aDesiredTime,
                                         const PrincipalHandle& aPrincipalHandle)
{
  VideoSegment segment;

  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted) {
    return;
  }

  StreamTime delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);

  if (delta > 0) {
    // nullptr images are allowed
    AppendToTrack(aSource, mImage, aID, delta, aPrincipalHandle);
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
                                           size_t size,
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

  if ((size_t) (mWidth*mHeight + 2*(((mWidth+1)/2)*((mHeight+1)/2))) != size) {
    MOZ_ASSERT(false, "Wrong size frame in DeliverFrame!");
    return 0;
  }

  // Create a video frame and append it to the track.
  RefPtr<layers::PlanarYCbCrImage> image = mImageContainer->CreatePlanarYCbCrImage();

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

  if (!image->CopyData(data)) {
    MOZ_ASSERT(false);
    return 0;
  }

#ifdef DEBUG
  static uint32_t frame_num = 0;
  LOGFRAME(("frame %d (%dx%d); timestamp %u, ntp_time %" PRIu64 ", render_time %" PRIu64,
            frame_num++, mWidth, mHeight, time_stamp, ntp_time, render_time));
#endif

  // we don't touch anything in 'this' until here (except for snapshot,
  // which has it's own lock)
  MonitorAutoLock lock(mMonitor);

  // implicitly releases last image
  mImage = image.forget();

  // We'll push the frame into the MSG on the next NotifyPull. This will avoid
  // swamping the MSG with frames should it be taking longer than normal to run
  // an iteration.

  return 0;
}

size_t
MediaEngineRemoteVideoSource::NumCapabilities() const
{
  mHardcodedCapabilities.Clear();
  int num = mozilla::camera::GetChildAndCall(
      &mozilla::camera::CamerasChild::NumberOfCapabilities,
      mCapEngine,
      GetUUID().get());
  if (num < 1) {
    // The default for devices that don't return discrete capabilities: treat
    // them as supporting all capabilities orthogonally. E.g. screensharing.
    // CaptureCapability defaults key values to 0, which means accept any value.
    mHardcodedCapabilities.AppendElement(webrtc::CaptureCapability());
    num = mHardcodedCapabilities.Length(); // 1
  }
  return num;
}

bool
MediaEngineRemoteVideoSource::ChooseCapability(
    const NormalizedConstraints &aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId)
{
  AssertIsOnOwningThread();

  switch(mMediaSource) {
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Window:
    case dom::MediaSourceEnum::Application: {
      FlattenedConstraints c(aConstraints);
      // The actual resolution to constrain around is not easy to find ahead of
      // time (and may in fact change over time), so as a hack, we push ideal
      // and max constraints down to desktop_capture_impl.cc and finish the
      // algorithm there.
      mCapability.width = (c.mWidth.mIdeal.valueOr(0) & 0xffff) << 16 |
                          (c.mWidth.mMax & 0xffff);
      mCapability.height = (c.mHeight.mIdeal.valueOr(0) & 0xffff) << 16 |
                           (c.mHeight.mMax & 0xffff);
      mCapability.maxFPS = c.mFrameRate.Clamp(c.mFrameRate.mIdeal.valueOr(aPrefs.mFPS));
      return true;
    }
    default:
      return MediaEngineCameraVideoSource::ChooseCapability(aConstraints, aPrefs, aDeviceId);
  }

}

void
MediaEngineRemoteVideoSource::GetCapability(size_t aIndex,
                                            webrtc::CaptureCapability& aOut) const
{
  if (!mHardcodedCapabilities.IsEmpty()) {
    MediaEngineCameraVideoSource::GetCapability(aIndex, aOut);
  }
  mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::GetCaptureCapability,
    mCapEngine,
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

  if (mozilla::camera::GetChildAndCall(
    &mozilla::camera::CamerasChild::GetCaptureDevice,
    mCapEngine, aIndex,
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
