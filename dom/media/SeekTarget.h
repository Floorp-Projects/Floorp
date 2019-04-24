/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SEEK_TARGET_H
#define SEEK_TARGET_H

#include "TimeUnits.h"

namespace mozilla {

enum class MediaDecoderEventVisibility : int8_t { Observable, Suppressed };

// Stores the seek target; the time to seek to, and whether an Accurate,
// "Fast" (nearest keyframe), or "Video Only" (no audio seek) seek was
// requested.
struct SeekTarget {
  enum Type {
    Invalid,
    PrevSyncPoint,
    Accurate,
    NextFrame,
  };
  SeekTarget()
      : mTime(media::TimeUnit::Invalid()),
        mType(SeekTarget::Invalid),
        mVideoOnly(false) {}
  SeekTarget(const media::TimeUnit& aTime, Type aType, bool aVideoOnly = false)
      : mTime(aTime), mType(aType), mVideoOnly(aVideoOnly) {
    MOZ_ASSERT(mTime.IsValid());
  }
  SeekTarget(const SeekTarget& aOther)
      : mTime(aOther.mTime),
        mType(aOther.mType),
        mVideoOnly(aOther.mVideoOnly) {
    MOZ_ASSERT(mTime.IsValid());
  }
  bool IsValid() const { return mType != SeekTarget::Invalid; }
  void Reset() {
    mTime = media::TimeUnit::Invalid();
    mType = SeekTarget::Invalid;
    mVideoOnly = false;
  }
  media::TimeUnit GetTime() const {
    MOZ_ASSERT(mTime.IsValid(), "Invalid SeekTarget");
    return mTime;
  }
  void SetTime(const media::TimeUnit& aTime) {
    MOZ_ASSERT(aTime.IsValid(), "Invalid SeekTarget destination");
    mTime = aTime;
  }
  void SetType(Type aType) { mType = aType; }
  void SetVideoOnly(bool aVideoOnly) { mVideoOnly = aVideoOnly; }
  bool IsFast() const { return mType == SeekTarget::Type::PrevSyncPoint; }
  bool IsAccurate() const { return mType == SeekTarget::Type::Accurate; }
  bool IsNextFrame() const { return mType == SeekTarget::Type::NextFrame; }
  bool IsVideoOnly() const { return mVideoOnly; }

 private:
  // Seek target time.
  media::TimeUnit mTime;
  // Whether we should seek "Fast", or "Accurate".
  // "Fast" seeks to the seek point preceding mTime, whereas
  // "Accurate" seeks as close as possible to mTime.
  Type mType;
  bool mVideoOnly;
};

}  // namespace mozilla

#endif /* SEEK_TARGET_H */
