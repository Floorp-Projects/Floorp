/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamBuffer.h"
#include "prlog.h"
#include <algorithm>

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaStreamGraphLog;
#define STREAM_LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define STREAM_LOG(type, msg)
#endif

#ifdef DEBUG
void
StreamBuffer::DumpTrackInfo() const
{
  STREAM_LOG(PR_LOG_ALWAYS, ("DumpTracks: mTracksKnownTime %lld", mTracksKnownTime));
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->IsEnded()) {
      STREAM_LOG(PR_LOG_ALWAYS, ("Track[%d] %d: ended", i, track->GetID()));
    } else {
      STREAM_LOG(PR_LOG_ALWAYS, ("Track[%d] %d: %lld", i, track->GetID(),
                                 track->GetEndTimeRoundDown()));
    }
  }
}
#endif

StreamTime
StreamBuffer::GetEnd() const
{
  StreamTime t = mTracksKnownTime;
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (!track->IsEnded()) {
      t = std::min(t, track->GetEndTimeRoundDown());
    }
  }
  return t;
}

StreamBuffer::Track*
StreamBuffer::FindTrack(TrackID aID)
{
  if (aID == TRACK_NONE)
    return nullptr;
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->GetID() == aID) {
      return track;
    }
  }
  return nullptr;
}

void
StreamBuffer::ForgetUpTo(StreamTime aTime)
{
  // Round to nearest 50ms so we don't spend too much time pruning segments.
  const MediaTime roundTo = MillisecondsToMediaTime(50);
  StreamTime forget = (aTime/roundTo)*roundTo;
  if (forget <= mForgottenTime) {
    return;
  }
  mForgottenTime = forget;

  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->IsEnded() && track->GetEndTimeRoundDown() <= forget) {
      mTracks.RemoveElementAt(i);
      --i;
      continue;
    }
    TrackTicks forgetTo = std::min(track->GetEnd() - 1, track->TimeToTicksRoundDown(forget));
    track->ForgetUpTo(forgetTo);
  }
}

}
