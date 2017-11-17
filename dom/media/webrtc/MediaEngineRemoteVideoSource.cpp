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
#include "VideoFrameUtils.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

extern mozilla::LogModule* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOGFRAME(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

namespace mozilla {

uint64_t MediaEngineCameraVideoSource::AllocationHandle::sId = 0;

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
  mSettings->mWidth.Construct(0);
  mSettings->mHeight.Construct(0);
  mSettings->mFrameRate.Construct(0);
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
    uniqueId, kMaxUniqueIdLength, nullptr)) {
    LOG(("Error initializing RemoteVideoSource (GetCaptureDevice)"));
    return;
  }

  SetName(NS_ConvertUTF8toUTF16(deviceName));
  SetUUID(uniqueId);

  mInitDone = true;
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
          MOZ_ASSERT(mPrincipalHandles.IsEmpty());
          MOZ_ASSERT(mTargetCapabilities.IsEmpty());
          MOZ_ASSERT(mHandleIds.IsEmpty());
          MOZ_ASSERT(mImages.IsEmpty());
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
  Super::Shutdown();
  mInitDone = false;
}

nsresult
MediaEngineRemoteVideoSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    AllocationHandle** aOutHandle,
    const char** aOutBadConstraint)
{
  LOG((__PRETTY_FUNCTION__));
  AssertIsOnOwningThread();

  if (!mInitDone) {
    LOG(("Init not done"));
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Super::Allocate(aConstraints, aPrefs, aDeviceId, aPrincipalInfo,
                                aOutHandle, aOutBadConstraint);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (mState == kStarted &&
      MOZ_LOG_TEST(GetMediaManagerLog(), mozilla::LogLevel::Debug)) {
    MonitorAutoLock lock(mMonitor);
    if (mSources.IsEmpty()) {
      MOZ_ASSERT(mPrincipalHandles.IsEmpty());
      MOZ_ASSERT(mTargetCapabilities.IsEmpty());
      MOZ_ASSERT(mHandleIds.IsEmpty());
      MOZ_ASSERT(mImages.IsEmpty());
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

  mImageContainer =
    layers::LayerManager::CreateImageContainer(layers::ImageContainer::ASYNCHRONOUS);

  {
    MonitorAutoLock lock(mMonitor);
    mSources.AppendElement(aStream);
    mPrincipalHandles.AppendElement(aPrincipalHandle);
    mTargetCapabilities.AppendElement(mTargetCapability);
    mHandleIds.AppendElement(mHandleId);
    mImages.AppendElement(mImageContainer->CreatePlanarYCbCrImage());

    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
    MOZ_ASSERT(mSources.Length() == mTargetCapabilities.Length());
    MOZ_ASSERT(mSources.Length() == mHandleIds.Length());
    MOZ_ASSERT(mSources.Length() == mImages.Length());
  }

  aStream->AddTrack(aID, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);

  if (mState == kStarted) {
    return NS_OK;
  }

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
    // usage.  Also, gfx gets very upset if these are held until this object
    // is gc'd in final-cc during shutdown (bug 1374164)
    mImage = nullptr;
    // we drop mImageContainer only in MediaEngineCaptureVideoSource::Shutdown()

    size_t i = mSources.IndexOf(aSource);
    if (i == mSources.NoIndex) {
      // Already stopped - this is allowed
      return NS_OK;
    }

    MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
    MOZ_ASSERT(mSources.Length() == mTargetCapabilities.Length());
    MOZ_ASSERT(mSources.Length() == mHandleIds.Length());
    MOZ_ASSERT(mSources.Length() == mImages.Length());
    mSources.RemoveElementAt(i);
    mPrincipalHandles.RemoveElementAt(i);
    mTargetCapabilities.RemoveElementAt(i);
    mHandleIds.RemoveElementAt(i);
    mImages.RemoveElementAt(i);

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
    const NormalizedConstraints& aNewConstraint,
    const MediaEnginePrefs& aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  switch (mState) {
    case kReleased:
      MOZ_ASSERT(aHandle);
      mHandleId = aHandle->mId;
      if (!ChooseCapability(aNetConstraints, aPrefs, aDeviceId, mCapability, kFitness)) {
        *aOutBadConstraint = FindBadConstraint(aNetConstraints, *this, aDeviceId);
        return NS_ERROR_FAILURE;
      }
      mTargetCapability = mCapability;

      if (camera::GetChildAndCall(&camera::CamerasChild::AllocateCaptureDevice,
                                  mCapEngine, GetUUID().get(),
                                  kMaxUniqueIdLength, mCaptureIndex,
                                  aHandle->mPrincipalInfo)) {
        return NS_ERROR_FAILURE;
      }
      mState = kAllocated;
      SetLastCapability(mCapability);
      LOG(("Video device %d allocated", mCaptureIndex));
      break;

    case kStarted:
      {
        size_t index = mHandleIds.NoIndex;
        if (aHandle) {
          mHandleId = aHandle->mId;
          index = mHandleIds.IndexOf(mHandleId);
        }

        if (!ChooseCapability(aNewConstraint, aPrefs, aDeviceId, mTargetCapability,
                              kFitness)) {
          *aOutBadConstraint = FindBadConstraint(aNewConstraint, *this, aDeviceId);
          return NS_ERROR_FAILURE;
        }

        if (index != mHandleIds.NoIndex) {
          MonitorAutoLock lock(mMonitor);
          mTargetCapabilities[index] = mTargetCapability;
          MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
          MOZ_ASSERT(mSources.Length() == mTargetCapabilities.Length());
          MOZ_ASSERT(mSources.Length() == mHandleIds.Length());
          MOZ_ASSERT(mSources.Length() == mImages.Length());
        }

        if (!ChooseCapability(aNetConstraints, aPrefs, aDeviceId, mCapability,
                              kFeasibility)) {
          *aOutBadConstraint = FindBadConstraint(aNetConstraints, *this, aDeviceId);
          return NS_ERROR_FAILURE;
        }

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
      }

    default:
      LOG(("Video device %d in ignored state %d", mCaptureIndex, mState));
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
  switch (mMediaSource) {
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Window:
    case dom::MediaSourceEnum::Application:
      // Undo the hack where ideal and max constraints are crammed together
      // in mCapability for consumption by low-level code. We don't actually
      // know the real resolution yet, so report min(ideal, max) for now.
      cap.width = std::min(cap.width >> 16, cap.width & 0xffff);
      cap.height = std::min(cap.height >> 16, cap.height & 0xffff);
      break;

    default:
      break;
  }
  auto settings = mSettings;

  NS_DispatchToMainThread(media::NewRunnableFrom([settings, cap]() mutable {
    settings->mWidth.Value() = cap.width;
    settings->mHeight.Value() = cap.height;
    settings->mFrameRate.Value() = cap.maxFPS;
    return NS_OK;
  }));
}

void
MediaEngineRemoteVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                         SourceMediaStream* aSource,
                                         TrackID aID, StreamTime aDesiredTime,
                                         const PrincipalHandle& aPrincipalHandle)
{
  StreamTime delta = 0;
  size_t i;
  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted) {
    return;
  }

  i = mSources.IndexOf(aSource);
  if (i == mSources.NoIndex) {
    return;
  }

  delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);

  if (delta > 0) {
    AppendToTrack(aSource, mImages[i], aID, delta, aPrincipalHandle);
  }
}

void
MediaEngineRemoteVideoSource::FrameSizeChange(unsigned int w, unsigned int h)
{
  if ((mWidth < 0) || (mHeight < 0) ||
      (w !=  (unsigned int) mWidth) || (h != (unsigned int) mHeight)) {
    LOG(("MediaEngineRemoteVideoSource Video FrameSizeChange: %ux%u was %ux%u", w, h, mWidth, mHeight));
    mWidth = w;
    mHeight = h;

    auto settings = mSettings;
    NS_DispatchToMainThread(media::NewRunnableFrom([settings, w, h]() mutable {
      settings->mWidth.Value() = w;
      settings->mHeight.Value() = h;
      return NS_OK;
    }));
  }
}

int
MediaEngineRemoteVideoSource::DeliverFrame(uint8_t* aBuffer,
                                    const camera::VideoFrameProperties& aProps)
{
  MonitorAutoLock lock(mMonitor);
  // Check for proper state.
  if (mState != kStarted || !mImageContainer) {
    LOG(("DeliverFrame: video not started"));
    return 0;
  }

  // Update the dimensions
  FrameSizeChange(aProps.width(), aProps.height());

  MOZ_ASSERT(mSources.Length() == mPrincipalHandles.Length());
  MOZ_ASSERT(mSources.Length() == mTargetCapabilities.Length());
  MOZ_ASSERT(mSources.Length() == mHandleIds.Length());
  MOZ_ASSERT(mSources.Length() == mImages.Length());

  for (uint32_t i = 0; i < mTargetCapabilities.Length(); i++ ) {
    int32_t req_max_width = mTargetCapabilities[i].width & 0xffff;
    int32_t req_max_height = mTargetCapabilities[i].height & 0xffff;
    int32_t req_ideal_width = (mTargetCapabilities[i].width >> 16) & 0xffff;
    int32_t req_ideal_height = (mTargetCapabilities[i].height >> 16) & 0xffff;

    int32_t dest_max_width = std::min(req_max_width, mWidth);
    int32_t dest_max_height = std::min(req_max_height, mHeight);
    // This logic works for both camera and screen sharing case.
    // for camera case, req_ideal_width and req_ideal_height is 0.
    // The following snippet will set dst_width to dest_max_width and dst_height to dest_max_height
    int32_t dst_width = std::min(req_ideal_width > 0 ? req_ideal_width : mWidth, dest_max_width);
    int32_t dst_height = std::min(req_ideal_height > 0 ? req_ideal_height : mHeight, dest_max_height);

    int dst_stride_y = dst_width;
    int dst_stride_uv = (dst_width + 1) / 2;

    camera::VideoFrameProperties properties;
    uint8_t* frame;
    bool needReScale = !((dst_width == mWidth && dst_height == mHeight) ||
                         (dst_width > mWidth || dst_height > mHeight));

    if (!needReScale) {
      dst_width = mWidth;
      dst_height = mHeight;
      frame = aBuffer;
    } else {
      rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer;
      i420Buffer = webrtc::I420Buffer::Create(mWidth, mHeight, mWidth,
                                              (mWidth + 1) / 2, (mWidth + 1) / 2);

      const int conversionResult = webrtc::ConvertToI420(webrtc::kI420,
                                                         aBuffer,
                                                         0, 0,  // No cropping
                                                         mWidth, mHeight,
                                                         mWidth * mHeight * 3 / 2,
                                                         webrtc::kVideoRotation_0,
                                                         i420Buffer.get());

      webrtc::VideoFrame captureFrame(i420Buffer, 0, 0, webrtc::kVideoRotation_0);
      if (conversionResult < 0) {
        return 0;
      }

      rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer;
      scaledBuffer = webrtc::I420Buffer::Create(dst_width, dst_height, dst_stride_y,
                                                dst_stride_uv, dst_stride_uv);

      scaledBuffer->CropAndScaleFrom(*captureFrame.video_frame_buffer().get());
      webrtc::VideoFrame scaledFrame(scaledBuffer, 0, 0, webrtc::kVideoRotation_0);

      VideoFrameUtils::InitFrameBufferProperties(scaledFrame, properties);
      frame = new unsigned char[properties.bufferSize()];

      if (!frame) {
        return 0;
      }

      VideoFrameUtils::CopyVideoFrameBuffers(frame,
                                             properties.bufferSize(), scaledFrame);
    }

    // Create a video frame and append it to the track.
    RefPtr<layers::PlanarYCbCrImage> image = mImageContainer->CreatePlanarYCbCrImage();

    const uint8_t lumaBpp = 8;
    const uint8_t chromaBpp = 4;

    layers::PlanarYCbCrData data;

    // Take lots of care to round up!
    data.mYChannel = frame;
    data.mYSize = IntSize(dst_width, dst_height);
    data.mYStride = (dst_width * lumaBpp + 7) / 8;
    data.mCbCrStride = (dst_width * chromaBpp + 7) / 8;
    data.mCbChannel = frame + dst_height * data.mYStride;
    data.mCrChannel = data.mCbChannel + ((dst_height + 1) / 2) * data.mCbCrStride;
    data.mCbCrSize = IntSize((dst_width + 1) / 2, (dst_height + 1) / 2);
    data.mPicX = 0;
    data.mPicY = 0;
    data.mPicSize = IntSize(dst_width, dst_height);
    data.mStereoMode = StereoMode::MONO;

    if (!image->CopyData(data)) {
      MOZ_ASSERT(false);
      return 0;
    }

    if (needReScale && frame) {
      delete frame;
      frame = nullptr;
    }

#ifdef DEBUG
    static uint32_t frame_num = 0;
    LOGFRAME(("frame %d (%dx%d); timeStamp %u, ntpTimeMs %" PRIu64 ", renderTimeMs %" PRIu64,
              frame_num++, mWidth, mHeight,
              aProps.timeStamp(), aProps.ntpTimeMs(), aProps.renderTimeMs()));
#endif

    // implicitly releases last image
    mImages[i] = image.forget();
  }

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
    const nsString& aDeviceId,
    webrtc::CaptureCapability& aCapability,
    const DistanceCalculation aCalculate)
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
      aCapability.width =
        (c.mWidth.mIdeal.valueOr(0) & 0xffff) << 16 | (c.mWidth.mMax & 0xffff);
      aCapability.height =
        (c.mHeight.mIdeal.valueOr(0) & 0xffff) << 16 | (c.mHeight.mMax & 0xffff);
      aCapability.maxFPS =
        c.mFrameRate.Clamp(c.mFrameRate.mIdeal.valueOr(aPrefs.mFPS));
      return true;
    }
    default:
      return MediaEngineCameraVideoSource::ChooseCapability(aConstraints, aPrefs, aDeviceId, aCapability, aCalculate);
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
    uniqueId, sizeof(uniqueId), nullptr)) {
    return;
  }

  SetName(NS_ConvertUTF8toUTF16(deviceName));
#ifdef DEBUG
  MOZ_ASSERT(GetUUID().Equals(uniqueId));
#endif
}

}
