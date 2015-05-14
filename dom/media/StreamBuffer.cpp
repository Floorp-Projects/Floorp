/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamBuffer.h"
#include "prlog.h"
#include <algorithm>

namespace mozilla {

extern PRLogModuleInfo* gMediaStreamGraphLog;
#define STREAM_LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)

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
                                 track->GetEnd()));
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
      t = std::min(t, track->GetEnd());
    }
  }
  return t;
}

StreamTime
StreamBuffer::GetAllTracksEnd() const
{
  if (mTracksKnownTime < STREAM_TIME_MAX) {
    // A track might be added.
    return STREAM_TIME_MAX;
  }
  StreamTime t = 0;
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (!track->IsEnded()) {
      return STREAM_TIME_MAX;
    }
    t = std::max(t, track->GetEnd());
  }
  return t;
}

StreamBuffer::Track*
StreamBuffer::FindTrack(TrackID aID)
{
  if (aID == TRACK_NONE || mTracks.IsEmpty()) {
    return nullptr;
  }

  // The tracks are sorted by ID. We can use a binary search.

  uint32_t left = 0, right = mTracks.Length() - 1;
  while (left <= right) {
    uint32_t middle = (left + right) / 2;
    if (mTracks[middle]->GetID() == aID) {
      return mTracks[middle];
    }

    if (mTracks[middle]->GetID() > aID) {
      if (middle == 0) {
        break;
      }

      right = middle - 1;
    } else {
      left = middle + 1;
    }
  }

  return nullptr;
}

void
StreamBuffer::ForgetUpTo(StreamTime aTime)
{
  // Only prune if there is a reasonable chunk (50ms @ 48kHz) to forget, so we
  // don't spend too much time pruning segments.
  const StreamTime minChunkSize = 2400;
  if (aTime < mForgottenTime + minChunkSize) {
    return;
  }
  mForgottenTime = aTime;

  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->IsEnded() && track->GetEnd() <= aTime) {
      mTracks.RemoveElementAt(i);
      mTracksDirty = true;
      --i;
      continue;
    }
    StreamTime forgetTo = std::min(track->GetEnd() - 1, aTime);
    track->ForgetUpTo(forgetTo);
  }
}

}
