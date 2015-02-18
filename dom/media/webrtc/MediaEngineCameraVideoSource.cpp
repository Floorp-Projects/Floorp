/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineCameraVideoSource.h"

namespace mozilla {

using namespace mozilla::gfx;
using dom::ConstrainLongRange;
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

/* static */ bool
MediaEngineCameraVideoSource::IsWithin(int32_t n, const ConstrainLongRange& aRange) {
  return aRange.mMin <= n && n <= aRange.mMax;
}

/* static */ bool
MediaEngineCameraVideoSource::IsWithin(double n, const ConstrainDoubleRange& aRange) {
  return aRange.mMin <= n && n <= aRange.mMax;
}

/* static */ int32_t
MediaEngineCameraVideoSource::Clamp(int32_t n, const ConstrainLongRange& aRange) {
  return std::max(aRange.mMin, std::min(n, aRange.mMax));
}

/* static */ bool
MediaEngineCameraVideoSource::AreIntersecting(const ConstrainLongRange& aA, const ConstrainLongRange& aB) {
  return aA.mMax >= aB.mMin && aA.mMin <= aB.mMax;
}

/* static */ bool
MediaEngineCameraVideoSource::Intersect(ConstrainLongRange& aA, const ConstrainLongRange& aB) {
  MOZ_ASSERT(AreIntersecting(aA, aB));
  aA.mMin = std::max(aA.mMin, aB.mMin);
  aA.mMax = std::min(aA.mMax, aB.mMax);
  return true;
}

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

/*static*/
bool
MediaEngineCameraVideoSource::SatisfiesConstraintSet(const MediaTrackConstraintSet &aConstraints,
                                                     const webrtc::CaptureCapability& aCandidate) {
  if (!IsWithin(aCandidate.width, aConstraints.mWidth) ||
      !IsWithin(aCandidate.height, aConstraints.mHeight)) {
    return false;
  }
  if (!IsWithin(aCandidate.maxFPS, aConstraints.mFrameRate)) {
    return false;
  }
  return true;
}

// SatisfiesConstraintSets (plural) answers for the capture device as a whole
// whether it can satisfy an accumulated number of capabilitySets.

bool
MediaEngineCameraVideoSource::SatisfiesConstraintSets(
    const nsTArray<const MediaTrackConstraintSet*>& aConstraintSets)
{
  size_t num = NumCapabilities();

  CapabilitySet candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(i);
  }

  for (const MediaTrackConstraintSet* cs : aConstraintSets) {
    for (size_t i = 0; i < candidateSet.Length();  ) {
      webrtc::CaptureCapability cap;
      GetCapability(candidateSet[i], cap);
      if (!SatisfiesConstraintSet(*cs, cap)) {
        candidateSet.RemoveElementAt(i);
      } else {
        ++i;
      }
    }
  }
  return !!candidateSet.Length();
}

void
MediaEngineCameraVideoSource::ChooseCapability(
    const VideoTrackConstraintsN &aConstraints,
    const MediaEnginePrefs &aPrefs)
{
  LOG(("ChooseCapability: prefs: %dx%d @%d-%dfps",
       aPrefs.mWidth, aPrefs.mHeight, aPrefs.mFPS, aPrefs.mMinFPS));

  size_t num = NumCapabilities();

  CapabilitySet candidateSet;
  for (size_t i = 0; i < num; i++) {
    candidateSet.AppendElement(i);
  }

  // Pick among capabilities: First apply required constraints.

  for (size_t i = 0; i < candidateSet.Length();) {
    webrtc::CaptureCapability cap;
    GetCapability(candidateSet[i], cap);
    if (!SatisfiesConstraintSet(aConstraints.mRequired, cap)) {
      candidateSet.RemoveElementAt(i);
    } else {
      ++i;
    }
  }

  CapabilitySet tailSet;

  // Then apply advanced (formerly known as optional) constraints.

  if (aConstraints.mAdvanced.WasPassed()) {
    for (const MediaTrackConstraintSet &cs : aConstraints.mAdvanced.Value()) {
      CapabilitySet rejects;
      for (size_t i = 0; i < candidateSet.Length();) {
        webrtc::CaptureCapability cap;
        GetCapability(candidateSet[i], cap);
        if (!SatisfiesConstraintSet(cs, cap)) {
          rejects.AppendElement(candidateSet[i]);
          candidateSet.RemoveElementAt(i);
        } else {
          ++i;
        }
      }
      (candidateSet.Length()? tailSet : candidateSet).MoveElementsFrom(rejects);
    }
  }

  if (!candidateSet.Length()) {
    candidateSet.AppendElement(0);
  }

  int prefWidth = aPrefs.GetWidth();
  int prefHeight = aPrefs.GetHeight();

  // Default is closest to available capability but equal to or below;
  // otherwise closest above.  Since we handle the num=0 case above and
  // take the first entry always, we can never exit uninitialized.

  webrtc::CaptureCapability cap;
  bool higher = true;
  for (size_t i = 0; i < candidateSet.Length(); i++) {
    GetCapability(candidateSet[i], cap);
    if (higher) {
      if (i == 0 ||
          (mCapability.width > cap.width && mCapability.height > cap.height)) {
        // closer than the current choice
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
      if (cap.width <= (uint32_t) prefWidth && cap.height <= (uint32_t) prefHeight) {
        higher = false;
      }
    } else {
      if (cap.width > (uint32_t) prefWidth || cap.height > (uint32_t) prefHeight ||
          cap.maxFPS < (uint32_t) aPrefs.mMinFPS) {
        continue;
      }
      if (mCapability.width < cap.width && mCapability.height < cap.height) {
        mCapability = cap;
        // FIXME: expose expected capture delay?
      }
    }
    // Same resolution, maybe better format or FPS match
    if (mCapability.width == cap.width && mCapability.height == cap.height) {
      // FPS too low
      if (cap.maxFPS < (uint32_t) aPrefs.mMinFPS) {
        continue;
      }
      // Better match
      if (cap.maxFPS < mCapability.maxFPS) {
        mCapability = cap;
      } else if (cap.maxFPS == mCapability.maxFPS) {
        // Resolution and FPS the same, check format
        if (cap.rawType == webrtc::RawVideoType::kVideoI420
          || cap.rawType == webrtc::RawVideoType::kVideoYUY2
          || cap.rawType == webrtc::RawVideoType::kVideoYV12) {
          mCapability = cap;
        }
      }
    }
  }
  LOG(("chose cap %dx%d @%dfps codec %d raw %d",
       mCapability.width, mCapability.height, mCapability.maxFPS,
       mCapability.codecType, mCapability.rawType));
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
