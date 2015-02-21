/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineCameraVideoSource.h"

#include <limits>

namespace mozilla {

using namespace mozilla::gfx;
using dom::OwningLongOrConstrainLongRange;
using dom::ConstrainLongRange;
using dom::OwningDoubleOrConstrainDoubleRange;
using dom::ConstrainDoubleRange;
using dom::MediaTrackConstraintSet;

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaManagerLog();
#define LOG(msg) PR_LOG(GetMediaManagerLog(), PR_LOG_DEBUG, msg)
#define LOGFRAME(msg) PR_LOG(GetMediaManagerLog(), 6, msg)
#else
#define LOG(msg)
#define LOGFRAME(msg)
#endif

// guts for appending data to the MSG track
bool MediaEngineCameraVideoSource::AppendToTrack(SourceMediaStream* aSource,
                                                 layers::Image* aImage,
                                                 TrackID aID,
                                                 StreamTime delta)
{
  MOZ_ASSERT(aSource);

  VideoSegment segment;
  nsRefPtr<layers::Image> image = aImage;
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

// The full algorithm for all cameras. Sources that don't list capabilities
// need to fake it and hardcode some by populating mHardcodedCapabilities above.

// Fitness distance returned as integer math * 1000. Infinity = UINT32_MAX

template<class ValueType, class ConstrainRange>
/* static */ uint32_t
MediaEngineCameraVideoSource::FitnessDistance(ValueType n,
                                              const ConstrainRange& aRange)
{
  if ((aRange.mExact.WasPassed() && aRange.mExact.Value() != n) ||
      (aRange.mMin.WasPassed() && aRange.mMin.Value() > n) ||
      (aRange.mMax.WasPassed() && aRange.mMax.Value() < n)) {
    return UINT32_MAX;
  }
  if (!aRange.mIdeal.WasPassed() || n == aRange.mIdeal.Value()) {
    return 0;
  }
  return uint32_t(ValueType((std::abs(n - aRange.mIdeal.Value()) * 1000) /
                            std::max(std::abs(n), std::abs(aRange.mIdeal.Value()))));
}

// Binding code doesn't templatize well...

template<>
/* static */ uint32_t
MediaEngineCameraVideoSource::FitnessDistance(int32_t n,
    const OwningLongOrConstrainLongRange& aConstraint)
{
  if (aConstraint.IsLong()) {
    ConstrainLongRange range;
    range.mIdeal.Construct(aConstraint.GetAsLong());
    return FitnessDistance(n, range);
  } else {
    return FitnessDistance(n, aConstraint.GetAsConstrainLongRange());
  }
}

template<>
/* static */ uint32_t
MediaEngineCameraVideoSource::FitnessDistance(double n,
    const OwningDoubleOrConstrainDoubleRange& aConstraint)
{
  if (aConstraint.IsDouble()) {
    ConstrainDoubleRange range;
    range.mIdeal.Construct(aConstraint.GetAsDouble());
    return FitnessDistance(n, range);
  } else {
    return FitnessDistance(n, aConstraint.GetAsConstrainDoubleRange());
  }
}

/*static*/ uint32_t
MediaEngineCameraVideoSource::GetFitnessDistance(const webrtc::CaptureCapability& aCandidate,
                                                 const MediaTrackConstraintSet &aConstraints)
{
  uint64_t distance =
    uint64_t(FitnessDistance(int32_t(aCandidate.width), aConstraints.mWidth)) +
    uint64_t(FitnessDistance(int32_t(aCandidate.height), aConstraints.mHeight)) +
    uint64_t(FitnessDistance(double(aCandidate.maxFPS), aConstraints.mFrameRate));
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
// Infinity = UINT32_MAX e.g. device cannot satisfy accumulated ConstraintSets.
// A finite result may be used to calculate this device's ranking as a choice.

uint32_t
MediaEngineCameraVideoSource::GetBestFitnessDistance(
    const nsTArray<const MediaTrackConstraintSet*>& aConstraintSets)
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
      uint32_t distance = GetFitnessDistance(cap, *cs);
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

bool
MediaEngineCameraVideoSource::ChooseCapability(
    const dom::MediaTrackConstraints &aConstraints,
    const MediaEnginePrefs &aPrefs)
{
  LOG(("ChooseCapability: prefs: %dx%d @%d-%dfps",
       aPrefs.mWidth, aPrefs.mHeight, aPrefs.mFPS, aPrefs.mMinFPS));

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
    candidate.mDistance = GetFitnessDistance(cap, aConstraints);
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
        if (GetFitnessDistance(cap, cs) == UINT32_MAX) {
          rejects.AppendElement(candidate);
          candidateSet.RemoveElementAt(i);
        } else {
          ++i;
        }
      }
      if (!candidateSet.Length()) {
        candidateSet.MoveElementsFrom(rejects);
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
  {
    MediaTrackConstraintSet prefs;
    prefs.mWidth.SetAsLong() = aPrefs.GetWidth();
    prefs.mHeight.SetAsLong() = aPrefs.GetHeight();
    prefs.mFrameRate.SetAsDouble() = aPrefs.mMinFPS;

    for (auto& candidate : candidateSet) {
      webrtc::CaptureCapability cap;
      GetCapability(candidate.mIndex, cap);
      candidate.mDistance = GetFitnessDistance(cap, prefs);
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

  LOG(("chose cap %dx%d @%dfps codec %d raw %d",
       mCapability.width, mCapability.height, mCapability.maxFPS,
       mCapability.codecType, mCapability.rawType));
  return true;
}

void
MediaEngineCameraVideoSource::GetName(nsAString& aName)
{
  aName = mDeviceName;
}

void
MediaEngineCameraVideoSource::GetUUID(nsAString& aUUID)
{
  aUUID = mUniqueId;
}

void
MediaEngineCameraVideoSource::SetDirectListeners(bool aHasDirectListeners)
{
  LOG((__FUNCTION__));
  mHasDirectListeners = aHasDirectListeners;
}

} // namespace mozilla
