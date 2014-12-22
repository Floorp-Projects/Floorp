/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingPlayerTracker.h"

#include "mozilla/dom/AnimationTimeline.h"

using namespace mozilla;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(PendingPlayerTracker, mPlayPendingSet, mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PendingPlayerTracker, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PendingPlayerTracker, Release)

void
PendingPlayerTracker::AddPlayPending(dom::AnimationPlayer& aPlayer)
{
  mPlayPendingSet.PutEntry(&aPlayer);
}

void
PendingPlayerTracker::RemovePlayPending(dom::AnimationPlayer& aPlayer)
{
  mPlayPendingSet.RemoveEntry(&aPlayer);
}

bool
PendingPlayerTracker::IsWaitingToPlay(dom::AnimationPlayer const& aPlayer) const
{
  return mPlayPendingSet.Contains(const_cast<dom::AnimationPlayer*>(&aPlayer));
}

PLDHashOperator
StartPlayerAtTime(nsRefPtrHashKey<dom::AnimationPlayer>* aKey,
                  void* aReadyTime)
{
  dom::AnimationPlayer* player = aKey->GetKey();

  // For animations that are waiting until their first frame has rendered
  // before starting, we record the moment when they finish painting
  // as the "ready time" and make any pending layer animations start at
  // that time.
  //
  // Here we fast-forward the player's timeline to the same "ready time" and
  // then tell the player to start at the timeline's current time.
  //
  // Redundant calls to FastForward with the same ready time are ignored by
  // AnimationTimeline.
  dom::AnimationTimeline* timeline = player->Timeline();
  timeline->FastForward(*static_cast<const TimeStamp*>(aReadyTime));

  player->StartNow();

  return PL_DHASH_NEXT;
}

void
PendingPlayerTracker::StartPendingPlayers(const TimeStamp& aReadyTime)
{
  mPlayPendingSet.EnumerateEntries(StartPlayerAtTime,
                                   const_cast<TimeStamp*>(&aReadyTime));
  mPlayPendingSet.Clear();
}

} // namespace mozilla
