/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineCameraVideoSource.h"

#include <limits>

namespace mozilla {

using namespace mozilla::gfx;
using namespace mozilla::dom;

extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Debug, msg)
#define LOGFRAME(msg) MOZ_LOG(GetMediaManagerLog(), mozilla::LogLevel::Verbose, msg)

// guts for appending data to the MSG track
bool MediaEngineCameraVideoSource::AppendToTrack(SourceMediaStream* aSource,
                                                 layers::Image* aImage,
                                                 TrackID aID,
                                                 StreamTime delta)
{
  MOZ_ASSERT(aSource);

  VideoSegment segment;
  RefPtr<layers::Image> image = aImage;
  IntSize size(image ? mWidth : 0, image ? mHeight : 0);
  segment.AppendFrame(image.forget(), delta, size);

  // This is safe from any thread, and is safe if the track is Finished
  // or Destroyed.
  // This can fail if either a) we haven't added the track yet, or b)
  // we've removed or finished the track.
  return aSource->AppendToTrack(aID, &(segment));
}

// Sub-classes (B2G or desktop) should overload one of both of these two methods
// to provide capabilities
size_t
MediaEngineCameraVideoSource::NumCapabilities()
{
  return mHardcodedCapabilities.Length();
}

void
MediaEngineCameraVideoSource::GetCapability(size_t aIndex,
                                            webrtc::CaptureCapability& aOut)
{
  MOZ_ASSERT(aIndex < mHardcodedCapabilities.Length());
  aOut = mHardcodedCapabilities[aIndex];
}

uint32_t
MediaEngineCameraVideoSource::GetFitnessDistance(const webrtc::CaptureCapability& aCandidate,
                                                 const MediaTrackConstraintSet &aConstraints,
                                                 bool aAdvanced,
                                                 const nsString& aDeviceId)
{
  // Treat width|height|frameRate == 0 on capability as "can do any".
  // This allows for orthogonal capabilities that are not in discrete steps.

  uint64_t distance =
    uint64_t(FitnessDistance(aDeviceId, aConstraints.mDeviceId, aAdvanced)) +
    uint64_t(FitnessDistance(mFacingMode, aConstraints.mFacingMode, aAdvanced)) +
    uint64_t(aCandidate.width? FitnessDistance(int32_t(aCandidate.width),
                                               aConstraints.mWidth,
                                               aAdvanced) : 0) +
    uint64_t(aCandidate.height? FitnessDistance(int32_t(aCandidate.height),
                                                aConstraints.mHeight,
                                                aAdvanced) : 0) +
    uint64_t(aCandidate.maxFPS? FitnessDistance(double(aCandidate.maxFPS),
                                                aConstraints.mFrameRate,
                                                aAdvanced) : 0);
  return uint32_t(std::min(distance, uint64_t(UINT32_MAX)));
}

// Find best capability by removing inferiors. May leave >1 of equal distance

/* static */ void
MediaEngineCameraVideoSource::TrimLessFitCandidates(CapabilitySet& set) {
  uint32_t best = UINT32_MAX;
  for (auto& candidate : set) {
    if (best > candidate.mDistance) {
      best = candidate.mDistance;
    }
  }
  for (size_t i = 0; i < set.Length();) {
    if (set[i].mDistance > best) {
      set.RemoveElementAt(i);
    } else {
      ++i;
    }
  }
  MOZ_ASSERT(set.Length());
}

// GetBestFitnessDistance returns the best distance the capture device can offer
// as a whole, given an accumulated number of ConstraintSets.
// Ideal values are considered in the first ConstraintSet only.
// Plain values are treated as Ideal in the first ConstraintSet.
// Plain values are treated as Exact in subsequent ConstraintSets.
// Infinity = UINT32_MAX e.g. device cannot satisfy accumulated ConstraintSets.
// A finite result may be used to calculate this device's ranking as a choice.

uint32_t
MediaEngineCameraVideoSource::GetBestFitnessDistance(
    const nsTArray<const MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId)
{
  size_t num = NumCapabilities();

  CapabilitySet candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(i);
  }

  bool first = true;
  for (const MediaTrackConstraintSet* cs : aConstraintSets) {
    for (size_t i = 0; i < candidateSet.Length();  ) {
      auto& candidate = candidateSet[i];
      webrtc::CaptureCapability cap;
      GetCapability(candidate.mIndex, cap);
      uint32_t distance = GetFitnessDistance(cap, *cs, !first, aDeviceId);
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

void
MediaEngineCameraVideoSource::LogConstraints(
    const MediaTrackConstraintSet& aConstraints, bool aAdvanced)
{
  NormalizedConstraintSet c(aConstraints, aAdvanced);
  LOG(((c.mWidth.mIdeal.WasPassed()?
        "Constraints: width: { min: %d, max: %d, ideal: %d }" :
        "Constraints: width: { min: %d, max: %d }"),
       c.mWidth.mMin, c.mWidth.mMax,
       c.mWidth.mIdeal.WasPassed()? c.mWidth.mIdeal.Value() : 0));
  LOG(((c.mHeight.mIdeal.WasPassed()?
        "             height: { min: %d, max: %d, ideal: %d }" :
        "             height: { min: %d, max: %d }"),
       c.mHeight.mMin, c.mHeight.mMax,
       c.mHeight.mIdeal.WasPassed()? c.mHeight.mIdeal.Value() : 0));
  LOG(((c.mFrameRate.mIdeal.WasPassed()?
        "             frameRate: { min: %f, max: %f, ideal: %f }" :
        "             frameRate: { min: %f, max: %f }"),
       c.mFrameRate.mMin, c.mFrameRate.mMax,
       c.mFrameRate.mIdeal.WasPassed()? c.mFrameRate.mIdeal.Value() : 0));
}

void
MediaEngineCameraVideoSource::LogCapability(const char* aHeader,
    const webrtc::CaptureCapability &aCapability, uint32_t aDistance)
{
  // RawVideoType and VideoCodecType media/webrtc/trunk/webrtc/common_types.h
  static const char* const types[] = {
    "I420",
    "YV12",
    "YUY2",
    "UYVY",
    "IYUV",
    "ARGB",
    "RGB24",
    "RGB565",
    "ARGB4444",
    "ARGB1555",
    "MJPEG",
    "NV12",
    "NV21",
    "BGRA",
    "Unknown type"
  };

  static const char* const codec[] = {
    "VP8",
    "VP9",
    "H264",
    "I420",
    "RED",
    "ULPFEC",
    "Generic codec",
    "Unknown codec"
  };

  LOG(("%s: %4u x %4u x %2u maxFps, %s, %s. Distance = %lu",
       aHeader, aCapability.width, aCapability.height, aCapability.maxFPS,
       types[std::min(std::max(uint32_t(0), uint32_t(aCapability.rawType)),
                      uint32_t(sizeof(types) / sizeof(*types) - 1))],
       codec[std::min(std::max(uint32_t(0), uint32_t(aCapability.codecType)),
                      uint32_t(sizeof(codec) / sizeof(*codec) - 1))],
       aDistance));
}

bool
MediaEngineCameraVideoSource::ChooseCapability(
    const MediaTrackConstraints &aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId)
{
  if (MOZ_LOG_TEST(GetMediaManagerLog(), LogLevel::Debug)) {
    LOG(("ChooseCapability: prefs: %dx%d @%d-%dfps",
         aPrefs.GetWidth(), aPrefs.GetHeight(),
         aPrefs.mFPS, aPrefs.mMinFPS));
    LogConstraints(aConstraints, false);
    if (aConstraints.mAdvanced.WasPassed()) {
      LOG(("Advanced array[%u]:", aConstraints.mAdvanced.Value().Length()));
      for (auto& advanced : aConstraints.mAdvanced.Value()) {
        LogConstraints(advanced, true);
      }
    }
  }

  size_t num = NumCapabilities();

  CapabilitySet candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(i);
  }

  // First, filter capabilities by required constraints (min, max, exact).

  for (size_t i = 0; i < candidateSet.Length();) {
    auto& candidate = candidateSet[i];
    webrtc::CaptureCapability cap;
    GetCapability(candidate.mIndex, cap);
    candidate.mDistance = GetFitnessDistance(cap, aConstraints, false, aDeviceId);
    LogCapability("Capability", cap, candidate.mDistance);
    if (candidate.mDistance == UINT32_MAX) {
      candidateSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }

  // Filter further with all advanced constraints (that don't overconstrain).

  if (aConstraints.mAdvanced.WasPassed()) {
    for (const MediaTrackConstraintSet &cs : aConstraints.mAdvanced.Value()) {
      CapabilitySet rejects;
      for (size_t i = 0; i < candidateSet.Length();) {
        auto& candidate = candidateSet[i];
        webrtc::CaptureCapability cap;
        GetCapability(candidate.mIndex, cap);
        if (GetFitnessDistance(cap, cs, true, aDeviceId) == UINT32_MAX) {
          rejects.AppendElement(candidate);
          candidateSet.RemoveElementAt(i);
        } else {
          ++i;
        }
      }
      if (!candidateSet.Length()) {
        candidateSet.AppendElements(Move(rejects));
      }
    }
  }
  if (!candidateSet.Length()) {
    LOG(("failed to find capability match from %d choices",num));
    return false;
  }

  // Remaining algorithm is up to the UA.

  TrimLessFitCandidates(candidateSet);

  // Any remaining multiples all have the same distance. A common case of this
  // occurs when no ideal is specified. Lean toward defaults.
  uint32_t sameDistance = candidateSet[0].mDistance;
  {
    MediaTrackConstraintSet prefs;
    prefs.mWidth.SetAsLong() = aPrefs.GetWidth();
    prefs.mHeight.SetAsLong() = aPrefs.GetHeight();
    prefs.mFrameRate.SetAsDouble() = aPrefs.mFPS;

    for (auto& candidate : candidateSet) {
      webrtc::CaptureCapability cap;
      GetCapability(candidate.mIndex, cap);
      candidate.mDistance = GetFitnessDistance(cap, prefs, false, aDeviceId);
    }
    TrimLessFitCandidates(candidateSet);
  }

  // Any remaining multiples all have the same distance, but may vary on
  // format. Some formats are more desirable for certain use like WebRTC.
  // E.g. I420 over RGB24 can remove a needless format conversion.

  bool found = false;
  for (auto& candidate : candidateSet) {
    webrtc::CaptureCapability cap;
    GetCapability(candidate.mIndex, cap);
    if (cap.rawType == webrtc::RawVideoType::kVideoI420 ||
        cap.rawType == webrtc::RawVideoType::kVideoYUY2 ||
        cap.rawType == webrtc::RawVideoType::kVideoYV12) {
      mCapability = cap;
      found = true;
      break;
    }
  }
  if (!found) {
    GetCapability(candidateSet[0].mIndex, mCapability);
  }

  LogCapability("Chosen capability", mCapability, sameDistance);
  return true;
}

void
MediaEngineCameraVideoSource::SetName(nsString aName)
{
  mDeviceName = aName;
  bool hasFacingMode = false;
  VideoFacingModeEnum facingMode = VideoFacingModeEnum::User;

  // Set facing mode based on device name.
#if defined(MOZ_B2G_CAMERA) && defined(MOZ_WIDGET_GONK)
  if (aName.EqualsLiteral("back")) {
    hasFacingMode = true;
    facingMode = VideoFacingModeEnum::Environment;
  } else if (aName.EqualsLiteral("front")) {
    hasFacingMode = true;
    facingMode = VideoFacingModeEnum::User;
  }
#endif // MOZ_B2G_CAMERA
#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
  // Names are generated. Example: "Camera 0, Facing back, Orientation 90"
  //
  // See media/webrtc/trunk/webrtc/modules/video_capture/android/java/src/org/
  // webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java

  if (aName.Find(NS_LITERAL_STRING("Facing back")) != kNotFound) {
    hasFacingMode = true;
    facingMode = VideoFacingModeEnum::Environment;
  } else if (aName.Find(NS_LITERAL_STRING("Facing front")) != kNotFound) {
    hasFacingMode = true;
    facingMode = VideoFacingModeEnum::User;
  }
#endif // ANDROID
#ifdef XP_MACOSX
  // Kludge to test user-facing cameras on OSX.
  if (aName.Find(NS_LITERAL_STRING("Face")) != -1) {
    hasFacingMode = true;
    facingMode = VideoFacingModeEnum::User;
  }
#endif
  if (hasFacingMode) {
    mFacingMode.Assign(NS_ConvertUTF8toUTF16(
        VideoFacingModeEnumValues::strings[uint32_t(facingMode)].value));
  } else {
    mFacingMode.Truncate();
  }
}

void
MediaEngineCameraVideoSource::GetName(nsAString& aName)
{
  aName = mDeviceName;
}

void
MediaEngineCameraVideoSource::SetUUID(const char* aUUID)
{
  mUniqueId.Assign(aUUID);
}

void
MediaEngineCameraVideoSource::GetUUID(nsACString& aUUID)
{
  aUUID = mUniqueId;
}

const nsCString&
MediaEngineCameraVideoSource::GetUUID()
{
  return mUniqueId;
}


void
MediaEngineCameraVideoSource::SetDirectListeners(bool aHasDirectListeners)
{
  LOG((__FUNCTION__));
  mHasDirectListeners = aHasDirectListeners;
}

} // namespace mozilla
