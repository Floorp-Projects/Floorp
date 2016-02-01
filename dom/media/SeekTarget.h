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
// or "Fast" (nearest keyframe) seek was requested.
struct SeekTarget {
  enum Type {
    Invalid,
    PrevSyncPoint,
    Accurate
  };
  SeekTarget()
    : mTime(-1.0)
    , mType(SeekTarget::Invalid)
    , mEventVisibility(MediaDecoderEventVisibility::Observable)
  {
  }
  SeekTarget(int64_t aTimeUsecs,
             Type aType,
             MediaDecoderEventVisibility aEventVisibility =
               MediaDecoderEventVisibility::Observable)
    : mTime(aTimeUsecs)
    , mType(aType)
    , mEventVisibility(aEventVisibility)
  {
  }
  SeekTarget(const SeekTarget& aOther)
    : mTime(aOther.mTime)
    , mType(aOther.mType)
    , mEventVisibility(aOther.mEventVisibility)
  {
  }
  bool IsValid() const {
    return mType != SeekTarget::Invalid;
  }
  void Reset() {
    mTime = -1;
    mType = SeekTarget::Invalid;
  }
  // Seek target time in microseconds.
  int64_t mTime;
  // Whether we should seek "Fast", or "Accurate".
  // "Fast" seeks to the seek point preceeding mTime, whereas
  // "Accurate" seeks as close as possible to mTime.
  Type mType;
  MediaDecoderEventVisibility mEventVisibility;
};

} // namespace mozilla

#endif /* SEEK_TARGET_H */
