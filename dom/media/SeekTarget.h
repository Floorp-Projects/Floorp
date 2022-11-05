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

// Stores the seek target. It includes (1) the time to seek to (2) type of seek
// (accurate, previous keyframe, next keyframe) (3) the type of track needs to
// performs seeking.
struct SeekTarget {
  enum Type {
    Invalid,
    PrevSyncPoint,
    Accurate,
    NextFrame,
  };
  enum Track {
    All,
    AudioOnly,
    VideoOnly,
  };
  SeekTarget()
      : mTime(media::TimeUnit::Invalid()),
        mType(SeekTarget::Invalid),
        mTargetTrack(Track::All) {}
  SeekTarget(const media::TimeUnit& aTime, Type aType,
             Track aTrack = Track::All)
      : mTime(aTime), mType(aType), mTargetTrack(aTrack) {
    MOZ_ASSERT(mTime.IsValid());
  }
  SeekTarget(const SeekTarget& aOther)
      : mTime(aOther.mTime),
        mType(aOther.mType),
        mTargetTrack(aOther.mTargetTrack) {
    MOZ_ASSERT(mTime.IsValid());
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
  bool IsFast() const { return mType == SeekTarget::Type::PrevSyncPoint; }
  bool IsAccurate() const { return mType == SeekTarget::Type::Accurate; }
  bool IsNextFrame() const { return mType == SeekTarget::Type::NextFrame; }
  bool IsVideoOnly() const { return mTargetTrack == Track::VideoOnly; }
  bool IsAudioOnly() const { return mTargetTrack == Track::AudioOnly; }
  bool IsAllTracks() const { return mTargetTrack == Track::All; }
  Type GetType() const { return mType; }
  Track GetTrack() const { return mTargetTrack; }

  static const char* TrackToStr(Track aTrack) {
    switch (aTrack) {
      case Track::All:
        return "all tracks";
      case Track::AudioOnly:
        return "audio only";
      case Track::VideoOnly:
        return "video only";
      default:
        MOZ_ASSERT_UNREACHABLE("Not defined track!");
        return "none";
    }
  }

 private:
  // Seek target time.
  media::TimeUnit mTime;
  // Whether we should seek "Fast", or "Accurate".
  // "Fast" seeks to the seek point preceding mTime, whereas
  // "Accurate" seeks as close as possible to mTime.
  Type mType;
  Track mTargetTrack;
};

}  // namespace mozilla

#endif /* SEEK_TARGET_H */
