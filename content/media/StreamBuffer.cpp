/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamBuffer.h"

namespace mozilla {

StreamTime
StreamBuffer::GetEnd() const
{
  StreamTime t = mTracksKnownTime;
  for (PRUint32 i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (!track->IsEnded()) {
      t = NS_MIN(t, track->GetEndTimeRoundDown());
    }
  }
  return t;
}

StreamBuffer::Track*
StreamBuffer::FindTrack(TrackID aID)
{
  if (aID == TRACK_NONE)
    return nsnull;
  for (PRUint32 i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->GetID() == aID) {
      return track;
    }
  }
  return nsnull;
}

void
StreamBuffer::ForgetUpTo(StreamTime aTime)
{
  // Round to nearest 50ms so we don't spend too much time pruning segments.
  const int roundTo = MillisecondsToMediaTime(50);
  StreamTime forget = (aTime/roundTo)*roundTo;
  if (forget <= mForgottenTime) {
    return;
  }
  mForgottenTime = forget;

  for (PRUint32 i = 0; i < mTracks.Length(); ++i) {
    Track* track = mTracks[i];
    if (track->IsEnded() && track->GetEndTimeRoundDown() <= forget) {
      mTracks.RemoveElementAt(i);
      --i;
      continue;
    }
    TrackTicks forgetTo = NS_MIN(track->GetEnd() - 1, track->TimeToTicksRoundDown(forget));
    track->ForgetUpTo(forgetTo);
  }
}

}
