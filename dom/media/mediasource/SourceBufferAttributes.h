/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SourceBufferAttributes_h_
#define mozilla_SourceBufferAttributes_h_

#include "TimeUnits.h"
#include "mozilla/dom/SourceBufferBinding.h"
#include "mozilla/Maybe.h"

namespace mozilla {

class SourceBufferAttributes {
public:

  // Current state as per Segment Parser Loop Algorithm
  // http://w3c.github.io/media-source/index.html#sourcebuffer-segment-parser-loop
  enum class AppendState
  {
    WAITING_FOR_SEGMENT,
    PARSING_INIT_SEGMENT,
    PARSING_MEDIA_SEGMENT,
  };

  explicit SourceBufferAttributes(bool aGenerateTimestamp)
  : mGenerateTimestamps(aGenerateTimestamp)
  , mAppendWindowStart(0)
  , mAppendWindowEnd(PositiveInfinity<double>())
  , mAppendMode(dom::SourceBufferAppendMode::Segments)
  , mApparentTimestampOffset(0)
  , mAppendState(AppendState::WAITING_FOR_SEGMENT)
  {}

  SourceBufferAttributes(const SourceBufferAttributes& aOther) = default;

  double GetAppendWindowStart() const
  {
    return mAppendWindowStart;
  }

  double GetAppendWindowEnd() const
  {
    return mAppendWindowEnd;
  }

  void SetAppendWindowStart(double aWindowStart)
  {
    mAppendWindowStart = aWindowStart;
  }

  void SetAppendWindowEnd(double aWindowEnd)
  {
    mAppendWindowEnd = aWindowEnd;
  }

  double GetApparentTimestampOffset() const
  {
    return mApparentTimestampOffset;
  }

  void SetApparentTimestampOffset(double aTimestampOffset)
  {
    mApparentTimestampOffset = aTimestampOffset;
    mTimestampOffset = media::TimeUnit::FromSeconds(aTimestampOffset);
  }

  media::TimeUnit GetTimestampOffset() const
  {
    return mTimestampOffset;
  }

  void SetTimestampOffset(const media::TimeUnit& aTimestampOffset)
  {
    mTimestampOffset = aTimestampOffset;
    mApparentTimestampOffset = aTimestampOffset.ToSeconds();
  }

  dom::SourceBufferAppendMode GetAppendMode() const
  {
    return mAppendMode;
  }

  void SetAppendMode(dom::SourceBufferAppendMode aAppendMode)
  {
    mAppendMode = aAppendMode;
  }

  void SetGroupStartTimestamp(const media::TimeUnit& aGroupStartTimestamp)
  {
    mGroupStartTimestamp = Some(aGroupStartTimestamp);
  }

  media::TimeUnit GetGroupStartTimestamp() const
  {
    return mGroupStartTimestamp.ref();
  }

  bool HaveGroupStartTimestamp() const
  {
    return mGroupStartTimestamp.isSome();
  }

  void ResetGroupStartTimestamp()
  {
    mGroupStartTimestamp.reset();
  }

  void RestartGroupStartTimestamp()
  {
    mGroupStartTimestamp = Some(mGroupEndTimestamp);
  }

  media::TimeUnit GetGroupEndTimestamp() const
  {
    return mGroupEndTimestamp;
  }

  void SetGroupEndTimestamp(const media::TimeUnit& aGroupEndTimestamp)
  {
    mGroupEndTimestamp = aGroupEndTimestamp;
  }

  AppendState GetAppendState() const
  {
    return mAppendState;
  }

  void SetAppendState(AppendState aState)
  {
    mAppendState = aState;
  }

  // mGenerateTimestamp isn't mutable once the source buffer has been constructed
  bool mGenerateTimestamps;

  SourceBufferAttributes& operator=(const SourceBufferAttributes& aOther) = default;

private:
  SourceBufferAttributes() = delete;

  double mAppendWindowStart;
  double mAppendWindowEnd;
  dom::SourceBufferAppendMode mAppendMode;
  double mApparentTimestampOffset;
  media::TimeUnit mTimestampOffset;
  Maybe<media::TimeUnit> mGroupStartTimestamp;
  media::TimeUnit mGroupEndTimestamp;
  // The current append state as per https://w3c.github.io/media-source/#sourcebuffer-append-state
  AppendState mAppendState;
};

} // end namespace mozilla

#endif /* mozilla_SourceBufferAttributes_h_ */
