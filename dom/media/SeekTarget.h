/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SEEK_TARGET_H
#define SEEK_TARGET_H

#include "TimeUnits.h"

namespace mozilla {

enum class MediaDecoderEventVisibility : int8_t {
  Observable,
  Suppressed
};

// Stores the seek target; the time to seek to, and whether an Accurate,
// "Fast" (nearest keyframe), or "Video Only" (no audio seek) seek was
// requested.
struct SeekTarget {
  enum Type {
    Invalid,
    PrevSyncPoint,
    Accurate,
    AccurateVideoOnly,
    NextFrame,
  };
  SeekTarget()
    : mEventVisibility(MediaDecoderEventVisibility::Observable)
    , mTime(media::TimeUnit::Invalid())
    , mType(SeekTarget::Invalid)
  {
  }
  SeekTarget(int64_t aTimeUsecs,
             Type aType,
             MediaDecoderEventVisibility aEventVisibility =
               MediaDecoderEventVisibility::Observable)
    : mEventVisibility(aEventVisibility)
    , mTime(media::TimeUnit::FromMicroseconds(aTimeUsecs))
    , mType(aType)
  {
  }
  SeekTarget(const media::TimeUnit& aTime,
             Type aType,
             MediaDecoderEventVisibility aEventVisibility =
               MediaDecoderEventVisibility::Observable)
    : mEventVisibility(aEventVisibility)
    , mTime(aTime)
    , mType(aType)
  {
  }
  SeekTarget(const SeekTarget& aOther)
    : mEventVisibility(aOther.mEventVisibility)
    , mTime(aOther.mTime)
    , mType(aOther.mType)
  {
  }
  bool IsValid() const {
    return mType != SeekTarget::Invalid;
  }
  void Reset() {
    mTime = media::TimeUnit::Invalid();
    mType = SeekTarget::Invalid;
  }
  media::TimeUnit GetTime() const {
    NS_ASSERTION(mTime.IsValid(), "Invalid SeekTarget");
    return mTime;
  }
  void SetTime(const media::TimeUnit& aTime) {
    NS_ASSERTION(aTime.IsValid(), "Invalid SeekTarget destination");
    mTime = aTime;
  }
  void SetType(Type aType) {
    mType = aType;
  }
  bool IsFast() const {
    return mType == SeekTarget::Type::PrevSyncPoint;
  }
  bool IsAccurate() const {
    return mType == SeekTarget::Type::Accurate;
  }
  bool IsVideoOnly() const {
    return mType == SeekTarget::Type::AccurateVideoOnly;
  }
  bool IsNextFrame() const {
    return mType == SeekTarget::Type::NextFrame;
  }

  MediaDecoderEventVisibility mEventVisibility;

private:
  // Seek target time.
  media::TimeUnit mTime;
  // Whether we should seek "Fast", or "Accurate".
  // "Fast" seeks to the seek point preceding mTime, whereas
  // "Accurate" seeks as close as possible to mTime.
  Type mType;
};

} // namespace mozilla

#endif /* SEEK_TARGET_H */
