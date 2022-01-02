/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineRemoteVideoSource.h"

#include "CamerasChild.h"
#include "Layers.h"
#include "MediaManager.h"
#include "MediaTrackConstraints.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "Tracing.h"
#include "VideoFrameUtils.h"
#include "VideoUtils.h"
#include "ImageContainer.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

namespace mozilla {

extern LazyLogModule gMediaManagerLog;
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))
#define LOG_FRAME(...) \
  MOZ_LOG(gMediaManagerLog, LogLevel::Verbose, (__VA_ARGS__))

using dom::ConstrainLongRange;
using dom::MediaSourceEnum;
using dom::MediaTrackConstraints;
using dom::MediaTrackConstraintSet;
using dom::MediaTrackSettings;
using dom::VideoFacingModeEnum;

static Maybe<VideoFacingModeEnum> GetFacingMode(const nsString& aDeviceName) {
  // Set facing mode based on device name.
#if defined(ANDROID)
  // Names are generated. Example: "Camera 0, Facing back, Orientation 90"
  //
  // See media/webrtc/trunk/webrtc/modules/video_capture/android/java/src/org/
  // webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java

  if (aDeviceName.Find(u"Facing back"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::Environment);
  }
  if (aDeviceName.Find(u"Facing front"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::User);
  }
#endif  // ANDROID
#ifdef XP_MACOSX
  // Kludge to test user-facing cameras on OSX.
  if (aDeviceName.Find(u"Face"_ns) != -1) {
    return Some(VideoFacingModeEnum::User);
  }
#endif
#ifdef XP_WIN
  // The cameras' name of Surface book are "Microsoft Camera Front" and
  // "Microsoft Camera Rear" respectively.

  if (aDeviceName.Find(u"Front"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::User);
  }
  if (aDeviceName.Find(u"Rear"_ns) != kNotFound) {
    return Some(VideoFacingModeEnum::Environment);
  }
#endif  // WINDOWS

  return Nothing();
}

MediaEngineRemoteVideoSource::MediaEngineRemoteVideoSource(
    const nsAString& aDeviceName, const nsACString& aDeviceUUID,
    camera::CaptureEngine aCapEngine, bool aScary)
    : mCapEngine(aCapEngine),
      mScary(aScary),
      mMutex("MediaEngineRemoteVideoSource::mMutex"),
      mRescalingBufferPool(/* zero_initialize */ false,
                           /* max_number_of_buffers */ 1),
      mSettingsUpdatedByFrame(MakeAndAddRef<media::Refcountable<AtomicBool>>()),
      mSettings(MakeAndAddRef<media::Refcountable<MediaTrackSettings>>()),
      mFirstFramePromise(mFirstFramePromiseHolder.Ensure(__func__)),
      mDeviceName(aDeviceName),
      mDeviceUUID(aDeviceUUID) {
  LOG("%s", __PRETTY_FUNCTION__);
  mSettings->mWidth.Construct(0);
  mSettings->mHeight.Construct(0);
  mSettings->mFrameRate.Construct(0);
  if (mCapEngine == camera::CameraEngine) {
    // Only cameras can have a facing mode.
    Maybe<VideoFacingModeEnum> facingMode = GetFacingMode(mDeviceName);
    if (facingMode.isSome()) {
      NS_ConvertASCIItoUTF16 facingString(
          dom::VideoFacingModeEnumValues::GetString(*facingMode));
      mSettings->mFacingMode.Construct(facingString);
      mFacingMode.emplace(facingString);
    }
  }
}

MediaEngineRemoteVideoSource::~MediaEngineRemoteVideoSource() {
  mFirstFramePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
}

dom::MediaSourceEnum MediaEngineRemoteVideoSource::GetMediaSource() const {
  switch (mCapEngine) {
    case camera::BrowserEngine:
      return MediaSourceEnum::Browser;
    case camera::CameraEngine:
      return MediaSourceEnum::Camera;
    case camera::ScreenEngine:
      return MediaSourceEnum::Screen;
    case camera::WinEngine:
      return MediaSourceEnum::Window;
    default:
      MOZ_CRASH();
  }
}

nsString MediaEngineRemoteVideoSource::GetName() const {
  AssertIsOnOwningThread();

  return mDeviceName;
}

nsCString MediaEngineRemoteVideoSource::GetUUID() const {
  AssertIsOnOwningThread();

  return mDeviceUUID;
}

nsString MediaEngineRemoteVideoSource::GetGroupId() const {
  AssertIsOnOwningThread();

  // The remote video backend doesn't implement group id. We return the device
  // name and higher layers will correlate this with the name of audio devices.
  return mDeviceName;
}

nsresult MediaEngineRemoteVideoSource::Allocate(
    const MediaTrackConstraints& aConstraints, const MediaEnginePrefs& aPrefs,
    uint64_t aWindowID, const char** aOutBadConstraint) {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kReleased);

  NormalizedConstraints constraints(aConstraints);
  webrtc::CaptureCapability newCapability;
  LOG("ChooseCapability(kFitness) for mCapability (Allocate) ++");
  if (!ChooseCapability(constraints, aPrefs, newCapability, kFitness)) {
    *aOutBadConstraint =
        MediaConstraintsHelper::FindBadConstraint(constraints, this);
    return NS_ERROR_FAILURE;
  }
  LOG("ChooseCapability(kFitness) for mCapability (Allocate) --");

  mCaptureId =
      camera::GetChildAndCall(&camera::CamerasChild::AllocateCapture,
                              mCapEngine, mDeviceUUID.get(), aWindowID);
  if (mCaptureId < 0) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mMutex);
    mState = kAllocated;
    mCapability = newCapability;
  }

  LOG("Video device %d allocated", mCaptureId);
  return NS_OK;
}

nsresult MediaEngineRemoteVideoSource::Deallocate() {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  if (mTrack) {
    mTrack->End();
  }

  {
    MutexAutoLock lock(mMutex);

    mTrack = nullptr;
    mPrincipal = PRINCIPAL_HANDLE_NONE;
    mState = kReleased;
  }

  // Stop() has stopped capture synchronously on the media thread before we get
  // here, so there are no longer any callbacks on an IPC thread accessing
  // mImageContainer or mRescalingBufferPool.
  mImageContainer = nullptr;
  mRescalingBufferPool.Release();

  LOG("Video device %d deallocated", mCaptureId);

  if (camera::GetChildAndCall(&camera::CamerasChild::ReleaseCapture, mCapEngine,
                              mCaptureId)) {
    MOZ_ASSERT_UNREACHABLE("Couldn't release allocated device");
  }
  return NS_OK;
}

void MediaEngineRemoteVideoSource::SetTrack(const RefPtr<MediaTrack>& aTrack,
                                            const PrincipalHandle& aPrincipal) {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated);
  MOZ_ASSERT(!mTrack);
  MOZ_ASSERT(aTrack);
  MOZ_ASSERT(aTrack->AsSourceTrack());

  if (!mImageContainer) {
    mImageContainer = MakeAndAddRef<layers::ImageContainer>(
        layers::ImageContainer::ASYNCHRONOUS);
  }

  {
    MutexAutoLock lock(mMutex);
    mTrack = aTrack->AsSourceTrack();
    mPrincipal = aPrincipal;
  }
}

nsresult MediaEngineRemoteVideoSource::Start() {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  MOZ_ASSERT(mTrack);

  {
    MutexAutoLock lock(mMutex);
    mState = kStarted;
  }

  mSettingsUpdatedByFrame->mValue = false;

  if (camera::GetChildAndCall(&camera::CamerasChild::StartCapture, mCapEngine,
                              mCaptureId, mCapability, this)) {
    LOG("StartCapture failed");
    MutexAutoLock lock(mMutex);
    mState = kStopped;
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaEngineRemoteVideoSource::SetLastCapability",
      [settings = mSettings, updated = mSettingsUpdatedByFrame,
       capEngine = mCapEngine, cap = mCapability]() mutable {
        switch (capEngine) {
          case camera::ScreenEngine:
          case camera::WinEngine:
            // Undo the hack where ideal and max constraints are crammed
            // together in mCapability for consumption by low-level code. We
            // don't actually know the real resolution yet, so report min(ideal,
            // max) for now.
            // TODO: This can be removed in bug 1453269.
            cap.width = std::min(cap.width >> 16, cap.width & 0xffff);
            cap.height = std::min(cap.height >> 16, cap.height & 0xffff);
            break;
          default:
            break;
        }

        if (!updated->mValue) {
          settings->mWidth.Value() = cap.width;
          settings->mHeight.Value() = cap.height;
        }
        settings->mFrameRate.Value() = cap.maxFPS;
      }));

  return NS_OK;
}

nsresult MediaEngineRemoteVideoSource::FocusOnSelectedSource() {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  int result;
  result = camera::GetChildAndCall(&camera::CamerasChild::FocusOnSelectedSource,
                                   mCapEngine, mCaptureId);
  return result == 0 ? NS_OK : NS_ERROR_FAILURE;
}

nsresult MediaEngineRemoteVideoSource::Stop() {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  if (mState == kStopped || mState == kAllocated) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kStarted);

  if (camera::GetChildAndCall(&camera::CamerasChild::StopCapture, mCapEngine,
                              mCaptureId)) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Stopping a started capture failed");
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mMutex);
    mState = kStopped;
  }

  return NS_OK;
}

nsresult MediaEngineRemoteVideoSource::Reconfigure(
    const MediaTrackConstraints& aConstraints, const MediaEnginePrefs& aPrefs,
    const char** aOutBadConstraint) {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  NormalizedConstraints constraints(aConstraints);
  webrtc::CaptureCapability newCapability;
  LOG("ChooseCapability(kFitness) for mTargetCapability (Reconfigure) ++");
  if (!ChooseCapability(constraints, aPrefs, newCapability, kFitness)) {
    *aOutBadConstraint =
        MediaConstraintsHelper::FindBadConstraint(constraints, this);
    return NS_ERROR_INVALID_ARG;
  }
  LOG("ChooseCapability(kFitness) for mTargetCapability (Reconfigure) --");

  if (mCapability == newCapability) {
    return NS_OK;
  }

  bool started = mState == kStarted;
  if (started) {
    nsresult rv = Stop();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString name;
      GetErrorName(rv, name);
      LOG("Video source %p for video device %d Reconfigure() failed "
          "unexpectedly in Stop(). rv=%s",
          this, mCaptureId, name.Data());
      return NS_ERROR_UNEXPECTED;
    }
  }

  {
    MutexAutoLock lock(mMutex);
    // Start() applies mCapability on the device.
    mCapability = newCapability;
  }

  if (started) {
    nsresult rv = Start();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString name;
      GetErrorName(rv, name);
      LOG("Video source %p for video device %d Reconfigure() failed "
          "unexpectedly in Start(). rv=%s",
          this, mCaptureId, name.Data());
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

size_t MediaEngineRemoteVideoSource::NumCapabilities() const {
  AssertIsOnOwningThread();

  if (!mCapabilities.IsEmpty()) {
    return mCapabilities.Length();
  }

  int num = camera::GetChildAndCall(&camera::CamerasChild::NumberOfCapabilities,
                                    mCapEngine, mDeviceUUID.get());
  if (num > 0) {
    mCapabilities.SetLength(num);
  } else {
    // The default for devices that don't return discrete capabilities: treat
    // them as supporting all capabilities orthogonally. E.g. screensharing.
    // CaptureCapability defaults key values to 0, which means accept any value.
    mCapabilities.AppendElement(MakeUnique<webrtc::CaptureCapability>());
    mCapabilitiesAreHardcoded = true;
  }

  return mCapabilities.Length();
}

webrtc::CaptureCapability& MediaEngineRemoteVideoSource::GetCapability(
    size_t aIndex) const {
  AssertIsOnOwningThread();
  MOZ_RELEASE_ASSERT(aIndex < mCapabilities.Length());
  if (!mCapabilities[aIndex]) {
    mCapabilities[aIndex] = MakeUnique<webrtc::CaptureCapability>();
    camera::GetChildAndCall(&camera::CamerasChild::GetCaptureCapability,
                            mCapEngine, mDeviceUUID.get(), aIndex,
                            mCapabilities[aIndex].get());
  }
  return *mCapabilities[aIndex];
}

int MediaEngineRemoteVideoSource::DeliverFrame(
    uint8_t* aBuffer, const camera::VideoFrameProperties& aProps) {
  // Cameras IPC thread - take great care with accessing members!

  Maybe<int32_t> req_max_width;
  Maybe<int32_t> req_max_height;
  Maybe<int32_t> req_ideal_width;
  Maybe<int32_t> req_ideal_height;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mState == kStarted);
    // TODO: These can be removed in bug 1453269.
    const int32_t max_width = mCapability.width & 0xffff;
    const int32_t max_height = mCapability.height & 0xffff;
    const int32_t ideal_width = (mCapability.width >> 16) & 0xffff;
    const int32_t ideal_height = (mCapability.height >> 16) & 0xffff;

    req_max_width = max_width ? Some(max_width) : Nothing();
    req_max_height = max_height ? Some(max_height) : Nothing();
    req_ideal_width = ideal_width ? Some(ideal_width) : Nothing();
    req_ideal_height = ideal_height ? Some(ideal_height) : Nothing();
  }

  // This is only used in the case of screen sharing, see bug 1453269.

  if (aProps.rotation() == 90 || aProps.rotation() == 270) {
    // This frame is rotated, so what was negotiated as width is now height,
    // and vice versa.
    std::swap(req_max_width, req_max_height);
    std::swap(req_ideal_width, req_ideal_height);
  }

  int32_t dst_max_width =
      std::min(aProps.width(), req_max_width.valueOr(aProps.width()));
  int32_t dst_max_height =
      std::min(aProps.height(), req_max_height.valueOr(aProps.height()));
  // This logic works for both camera and screen sharing case.
  // for camera case, req_ideal_width and req_ideal_height are absent.
  int32_t dst_width = req_ideal_width.valueOr(aProps.width());
  int32_t dst_height = req_ideal_height.valueOr(aProps.height());

  if (!req_ideal_width && req_ideal_height) {
    dst_width = *req_ideal_height * aProps.width() / aProps.height();
  } else if (!req_ideal_height && req_ideal_width) {
    dst_height = *req_ideal_width * aProps.height() / aProps.width();
  }
  dst_width = std::min(dst_width, dst_max_width);
  dst_height = std::min(dst_height, dst_max_height);

  // Apply scaling for screen sharing, see bug 1453269.
  switch (mCapEngine) {
    case camera::ScreenEngine:
    case camera::WinEngine: {
      // scale to average of portrait and landscape
      float scale_width = (float)dst_width / (float)aProps.width();
      float scale_height = (float)dst_height / (float)aProps.height();
      float scale = (scale_width + scale_height) / 2;
      // If both req_ideal_width & req_ideal_height are absent, scale is 1, but
      // if one is present and the other not, scale precisely to the one present
      if (!req_ideal_width) {
        scale = scale_height;
      } else if (!req_ideal_height) {
        scale = scale_width;
      }
      dst_width = int32_t(scale * (float)aProps.width());
      dst_height = int32_t(scale * (float)aProps.height());

      // if scaled rectangle exceeds max rectangle, scale to minimum of portrait
      // and landscape
      if (dst_width > dst_max_width || dst_height > dst_max_height) {
        scale_width = (float)dst_max_width / (float)dst_width;
        scale_height = (float)dst_max_height / (float)dst_height;
        scale = std::min(scale_width, scale_height);
        dst_width = int32_t(scale * dst_width);
        dst_height = int32_t(scale * dst_height);
      }
      break;
    }
    default: {
      break;
    }
  }

  // Ensure width and height are at least two. Smaller frames can lead to
  // problems with scaling and video encoding.
  dst_width = std::max(2, dst_width);
  dst_height = std::max(2, dst_height);

  rtc::Callback0<void> callback_unused;
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer =
      webrtc::WrapI420Buffer(
          aProps.width(), aProps.height(), aBuffer, aProps.yStride(),
          aBuffer + aProps.yAllocatedSize(), aProps.uStride(),
          aBuffer + aProps.yAllocatedSize() + aProps.uAllocatedSize(),
          aProps.vStride(), callback_unused);

  if ((dst_width != aProps.width() || dst_height != aProps.height()) &&
      dst_width <= aProps.width() && dst_height <= aProps.height()) {
    // Destination resolution is smaller than source buffer. We'll rescale.
    rtc::scoped_refptr<webrtc::I420Buffer> scaledBuffer =
        mRescalingBufferPool.CreateBuffer(dst_width, dst_height);
    if (!scaledBuffer) {
      MOZ_ASSERT_UNREACHABLE(
          "We might fail to allocate a buffer, but with this "
          "being a recycling pool that shouldn't happen");
      return 0;
    }
    scaledBuffer->CropAndScaleFrom(*buffer);
    buffer = scaledBuffer;
  }

  layers::PlanarYCbCrData data;
  data.mYChannel = const_cast<uint8_t*>(buffer->DataY());
  data.mYSize = gfx::IntSize(buffer->width(), buffer->height());
  data.mYStride = buffer->StrideY();
  MOZ_ASSERT(buffer->StrideU() == buffer->StrideV());
  data.mCbCrStride = buffer->StrideU();
  data.mCbChannel = const_cast<uint8_t*>(buffer->DataU());
  data.mCrChannel = const_cast<uint8_t*>(buffer->DataV());
  data.mCbCrSize =
      gfx::IntSize((buffer->width() + 1) / 2, (buffer->height() + 1) / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfx::IntSize(buffer->width(), buffer->height());
  data.mYUVColorSpace = gfx::YUVColorSpace::BT601;

  RefPtr<layers::PlanarYCbCrImage> image =
      mImageContainer->CreatePlanarYCbCrImage();
  if (!image->CopyData(data)) {
    MOZ_ASSERT_UNREACHABLE(
        "We might fail to allocate a buffer, but with this "
        "being a recycling container that shouldn't happen");
    return 0;
  }

#ifdef DEBUG
  static uint32_t frame_num = 0;
  LOG_FRAME(
      "frame %d (%dx%d)->(%dx%d); rotation %d, timeStamp %u, ntpTimeMs %" PRIu64
      ", renderTimeMs %" PRIu64,
      frame_num++, aProps.width(), aProps.height(), dst_width, dst_height,
      aProps.rotation(), aProps.timeStamp(), aProps.ntpTimeMs(),
      aProps.renderTimeMs());
#endif

  if (mImageSize.width != dst_width || mImageSize.height != dst_height) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "MediaEngineRemoteVideoSource::FrameSizeChange",
        [settings = mSettings, updated = mSettingsUpdatedByFrame,
         holder = std::move(mFirstFramePromiseHolder), dst_width,
         dst_height]() mutable {
          settings->mWidth.Value() = dst_width;
          settings->mHeight.Value() = dst_height;
          updated->mValue = true;
          // Since mImageSize was initialized to (0,0), we end up here on the
          // arrival of the first frame. We resolve the promise representing
          // arrival of first frame, after correct settings values have been
          // made available (Resolve() is idempotent if already resolved).
          holder.ResolveIfExists(true, __func__);
        }));
  }

  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mState == kStarted);
    VideoSegment segment;
    mImageSize = image->GetSize();
    segment.AppendFrame(image.forget(), mImageSize, mPrincipal);
    mTrack->AppendData(&segment);
  }

  return 0;
}

uint32_t MediaEngineRemoteVideoSource::GetDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints,
    const DistanceCalculation aCalculate) const {
  if (aCalculate == kFeasibility) {
    return GetFeasibilityDistance(aCandidate, aConstraints);
  }
  return GetFitnessDistance(aCandidate, aConstraints);
}

uint32_t MediaEngineRemoteVideoSource::GetFitnessDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints) const {
  AssertIsOnOwningThread();

  // Treat width|height|frameRate == 0 on capability as "can do any".
  // This allows for orthogonal capabilities that are not in discrete steps.

  typedef MediaConstraintsHelper H;
  uint64_t distance =
      uint64_t(H::FitnessDistance(mFacingMode, aConstraints.mFacingMode)) +
      uint64_t(aCandidate.width ? H::FitnessDistance(int32_t(aCandidate.width),
                                                     aConstraints.mWidth)
                                : 0) +
      uint64_t(aCandidate.height
                   ? H::FitnessDistance(int32_t(aCandidate.height),
                                        aConstraints.mHeight)
                   : 0) +
      uint64_t(aCandidate.maxFPS ? H::FitnessDistance(double(aCandidate.maxFPS),
                                                      aConstraints.mFrameRate)
                                 : 0);
  return uint32_t(std::min(distance, uint64_t(UINT32_MAX)));
}

uint32_t MediaEngineRemoteVideoSource::GetFeasibilityDistance(
    const webrtc::CaptureCapability& aCandidate,
    const NormalizedConstraintSet& aConstraints) const {
  AssertIsOnOwningThread();

  // Treat width|height|frameRate == 0 on capability as "can do any".
  // This allows for orthogonal capabilities that are not in discrete steps.

  typedef MediaConstraintsHelper H;
  uint64_t distance =
      uint64_t(H::FitnessDistance(mFacingMode, aConstraints.mFacingMode)) +
      uint64_t(aCandidate.width
                   ? H::FeasibilityDistance(int32_t(aCandidate.width),
                                            aConstraints.mWidth)
                   : 0) +
      uint64_t(aCandidate.height
                   ? H::FeasibilityDistance(int32_t(aCandidate.height),
                                            aConstraints.mHeight)
                   : 0) +
      uint64_t(aCandidate.maxFPS
                   ? H::FeasibilityDistance(double(aCandidate.maxFPS),
                                            aConstraints.mFrameRate)
                   : 0);
  return uint32_t(std::min(distance, uint64_t(UINT32_MAX)));
}

// Find best capability by removing inferiors. May leave >1 of equal distance

/* static */
void MediaEngineRemoteVideoSource::TrimLessFitCandidates(
    nsTArray<CapabilityCandidate>& aSet) {
  uint32_t best = UINT32_MAX;
  for (auto& candidate : aSet) {
    if (best > candidate.mDistance) {
      best = candidate.mDistance;
    }
  }
  aSet.RemoveElementsBy(
      [best](const auto& set) { return set.mDistance > best; });
  MOZ_ASSERT(aSet.Length());
}

uint32_t MediaEngineRemoteVideoSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets) const {
  AssertIsOnOwningThread();

  size_t num = NumCapabilities();
  nsTArray<CapabilityCandidate> candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(CapabilityCandidate(GetCapability(i)));
  }

  bool first = true;
  for (const NormalizedConstraintSet* ns : aConstraintSets) {
    for (size_t i = 0; i < candidateSet.Length();) {
      auto& candidate = candidateSet[i];
      uint32_t distance = GetFitnessDistance(candidate.mCapability, *ns);
      if (distance == UINT32_MAX) {
        candidateSet.RemoveElementAt(i);
      } else {
        ++i;
        if (first) {
          candidate.mDistance = distance;
        }
      }
    }
    first = false;
  }
  if (!candidateSet.Length()) {
    return UINT32_MAX;
  }
  TrimLessFitCandidates(candidateSet);
  return candidateSet[0].mDistance;
}

static void LogCapability(const char* aHeader,
                          const webrtc::CaptureCapability& aCapability,
                          uint32_t aDistance) {
  static const char* const codec[] = {"VP8",           "VP9",          "H264",
                                      "I420",          "RED",          "ULPFEC",
                                      "Generic codec", "Unknown codec"};

  LOG("%s: %4u x %4u x %2u maxFps, %s. Distance = %" PRIu32, aHeader,
      aCapability.width, aCapability.height, aCapability.maxFPS,
      codec[std::min(std::max(uint32_t(0), uint32_t(aCapability.videoType)),
                     uint32_t(sizeof(codec) / sizeof(*codec) - 1))],
      aDistance);
}

bool MediaEngineRemoteVideoSource::ChooseCapability(
    const NormalizedConstraints& aConstraints, const MediaEnginePrefs& aPrefs,
    webrtc::CaptureCapability& aCapability,
    const DistanceCalculation aCalculate) {
  LOG("%s", __PRETTY_FUNCTION__);
  AssertIsOnOwningThread();

  if (MOZ_LOG_TEST(gMediaManagerLog, LogLevel::Debug)) {
    LOG("ChooseCapability: prefs: %dx%d @%dfps", aPrefs.GetWidth(),
        aPrefs.GetHeight(), aPrefs.mFPS);
    MediaConstraintsHelper::LogConstraints(aConstraints);
    if (!aConstraints.mAdvanced.empty()) {
      LOG("Advanced array[%zu]:", aConstraints.mAdvanced.size());
      for (auto& advanced : aConstraints.mAdvanced) {
        MediaConstraintsHelper::LogConstraints(advanced);
      }
    }
  }

  switch (mCapEngine) {
    case camera::ScreenEngine:
    case camera::WinEngine: {
      FlattenedConstraints c(aConstraints);
      // The actual resolution to constrain around is not easy to find ahead of
      // time (and may in fact change over time), so as a hack, we push ideal
      // and max constraints down to desktop_capture_impl.cc and finish the
      // algorithm there.
      // TODO: This can be removed in bug 1453269.
      aCapability.width =
          (std::min(0xffff, c.mWidth.mIdeal.valueOr(0)) & 0xffff) << 16 |
          (std::min(0xffff, c.mWidth.mMax) & 0xffff);
      aCapability.height =
          (std::min(0xffff, c.mHeight.mIdeal.valueOr(0)) & 0xffff) << 16 |
          (std::min(0xffff, c.mHeight.mMax) & 0xffff);
      aCapability.maxFPS =
          c.mFrameRate.Clamp(c.mFrameRate.mIdeal.valueOr(aPrefs.mFPS));
      return true;
    }
    default:
      break;
  }

  nsTArray<CapabilityCandidate> candidateSet;
  size_t num = NumCapabilities();
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(CapabilityCandidate(GetCapability(i)));
  }

  if (mCapabilitiesAreHardcoded && mCapEngine == camera::CameraEngine) {
    // We have a hardcoded capability, which means this camera didn't report
    // discrete capabilities. It might still allow a ranged capability, so we
    // add a couple of default candidates based on prefs and constraints.
    // The chosen candidate will be propagated to StartCapture() which will fail
    // for an invalid candidate.
    MOZ_DIAGNOSTIC_ASSERT(mCapabilities.Length() == 1);
    MOZ_DIAGNOSTIC_ASSERT(candidateSet.Length() == 1);
    candidateSet.Clear();

    FlattenedConstraints c(aConstraints);
    // Reuse the code across both the low-definition (`false`) pref and
    // the high-definition (`true`) pref.
    // If there are constraints we try to satisfy them but we default to prefs.
    // Note that since constraints are from content and can literally be
    // anything we put (rather generous) caps on them.
    for (bool isHd : {false, true}) {
      webrtc::CaptureCapability cap;
      int32_t prefWidth = aPrefs.GetWidth(isHd);
      int32_t prefHeight = aPrefs.GetHeight(isHd);

      cap.width = c.mWidth.Get(prefWidth);
      cap.width = std::max(0, std::min(cap.width, 7680));

      cap.height = c.mHeight.Get(prefHeight);
      cap.height = std::max(0, std::min(cap.height, 4320));

      cap.maxFPS = c.mFrameRate.Get(aPrefs.mFPS);
      cap.maxFPS = std::max(0, std::min(cap.maxFPS, 480));

      if (cap.width != prefWidth) {
        // Width was affected by constraints.
        // We'll adjust the height too so the aspect ratio is retained.
        cap.height = cap.width * prefHeight / prefWidth;
      } else if (cap.height != prefHeight) {
        // Height was affected by constraints but not width.
        // We'll adjust the width too so the aspect ratio is retained.
        cap.width = cap.height * prefWidth / prefHeight;
      }

      if (candidateSet.Contains(cap, CapabilityComparator())) {
        continue;
      }
      LogCapability("Hardcoded capability", cap, 0);
      candidateSet.AppendElement(cap);
    }
  }

  // First, filter capabilities by required constraints (min, max, exact).

  for (size_t i = 0; i < candidateSet.Length();) {
    auto& candidate = candidateSet[i];
    candidate.mDistance =
        GetDistance(candidate.mCapability, aConstraints, aCalculate);
    LogCapability("Capability", candidate.mCapability, candidate.mDistance);
    if (candidate.mDistance == UINT32_MAX) {
      candidateSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }

  if (candidateSet.IsEmpty()) {
    LOG("failed to find capability match from %zu choices",
        candidateSet.Length());
    return false;
  }

  // Filter further with all advanced constraints (that don't overconstrain).

  for (const auto& cs : aConstraints.mAdvanced) {
    nsTArray<CapabilityCandidate> rejects;
    for (size_t i = 0; i < candidateSet.Length();) {
      if (GetDistance(candidateSet[i].mCapability, cs, aCalculate) ==
          UINT32_MAX) {
        rejects.AppendElement(candidateSet[i]);
        candidateSet.RemoveElementAt(i);
      } else {
        ++i;
      }
    }
    if (!candidateSet.Length()) {
      candidateSet.AppendElements(std::move(rejects));
    }
  }
  MOZ_ASSERT(
      candidateSet.Length(),
      "advanced constraints filtering step can't reduce candidates to zero");

  // Remaining algorithm is up to the UA.

  TrimLessFitCandidates(candidateSet);

  // Any remaining multiples all have the same distance. A common case of this
  // occurs when no ideal is specified. Lean toward defaults.
  uint32_t sameDistance = candidateSet[0].mDistance;
  {
    MediaTrackConstraintSet prefs;
    prefs.mWidth.Construct().SetAsLong() = aPrefs.GetWidth();
    prefs.mHeight.Construct().SetAsLong() = aPrefs.GetHeight();
    prefs.mFrameRate.Construct().SetAsDouble() = aPrefs.mFPS;
    NormalizedConstraintSet normPrefs(prefs, false);

    for (auto& candidate : candidateSet) {
      candidate.mDistance =
          GetDistance(candidate.mCapability, normPrefs, aCalculate);
    }
    TrimLessFitCandidates(candidateSet);
  }

  aCapability = candidateSet[0].mCapability;

  LogCapability("Chosen capability", aCapability, sameDistance);
  return true;
}

void MediaEngineRemoteVideoSource::GetSettings(
    MediaTrackSettings& aOutSettings) const {
  aOutSettings = *mSettings;
}

}  // namespace mozilla
