/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineCameraVideoSource.h"

namespace mozilla {

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

// A special version of the algorithm for cameras that don't list capabilities.
void
MediaEngineCameraVideoSource::GuessCapability(
    const VideoTrackConstraintsN& aConstraints,
    const MediaEnginePrefs& aPrefs)
{
  LOG(("GuessCapability: prefs: %dx%d @%d-%dfps",
       aPrefs.mWidth, aPrefs.mHeight, aPrefs.mFPS, aPrefs.mMinFPS));

  // In short: compound constraint-ranges and use pref as ideal.

  ConstrainLongRange cWidth(aConstraints.mRequired.mWidth);
  ConstrainLongRange cHeight(aConstraints.mRequired.mHeight);

  if (aConstraints.mAdvanced.WasPassed()) {
    const auto& advanced = aConstraints.mAdvanced.Value();
    for (uint32_t i = 0; i < advanced.Length(); i++) {
      if (AreIntersecting(cWidth, advanced[i].mWidth) &&
          AreIntersecting(cHeight, advanced[i].mHeight)) {
        Intersect(cWidth, advanced[i].mWidth);
        Intersect(cHeight, advanced[i].mHeight);
      }
    }
  }
  // Detect Mac HD cams and give them some love in the form of a dynamic default
  // since that hardware switches between 4:3 at low res and 16:9 at higher res.
  //
  // Logic is: if we're relying on defaults in aPrefs, then
  // only use HD pref when non-HD pref is too small and HD pref isn't too big.

  bool macHD = ((!aPrefs.mWidth || !aPrefs.mHeight) &&
                mDeviceName.EqualsASCII("FaceTime HD Camera (Built-in)") &&
                (aPrefs.GetWidth() < cWidth.mMin ||
                 aPrefs.GetHeight() < cHeight.mMin) &&
                !(aPrefs.GetWidth(true) > cWidth.mMax ||
                  aPrefs.GetHeight(true) > cHeight.mMax));
  int prefWidth = aPrefs.GetWidth(macHD);
  int prefHeight = aPrefs.GetHeight(macHD);

  // Clamp width and height without distorting inherent aspect too much.

  if (IsWithin(prefWidth, cWidth) == IsWithin(prefHeight, cHeight)) {
    // If both are within, we get the default (pref) aspect.
    // If neither are within, we get the aspect of the enclosing constraint.
    // Either are presumably reasonable (presuming constraints are sane).
    mCapability.width = Clamp(prefWidth, cWidth);
    mCapability.height = Clamp(prefHeight, cHeight);
  } else {
    // But if only one clips (e.g. width), the resulting skew is undesirable:
    //       .------------.
    //       | constraint |
    //  .----+------------+----.
    //  |    |            |    |
    //  |pref|  result    |    |   prefAspect != resultAspect
    //  |    |            |    |
    //  '----+------------+----'
    //       '------------'
    //  So in this case, preserve prefAspect instead:
    //  .------------.
    //  | constraint |
    //  .------------.
    //  |pref        |             prefAspect is unchanged
    //  '------------'
    //  |            |
    //  '------------'
    if (IsWithin(prefWidth, cWidth)) {
      mCapability.height = Clamp(prefHeight, cHeight);
      mCapability.width = Clamp((mCapability.height * prefWidth) /
                                prefHeight, cWidth);
    } else {
      mCapability.width = Clamp(prefWidth, cWidth);
      mCapability.height = Clamp((mCapability.width * prefHeight) /
                                 prefWidth, cHeight);
    }
  }
  mCapability.maxFPS = MediaEngine::DEFAULT_VIDEO_FPS;
  LOG(("chose cap %dx%d @%dfps",
       mCapability.width, mCapability.height, mCapability.maxFPS));
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
