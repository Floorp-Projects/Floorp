/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingPlayerTracker.h"

#include "mozilla/dom/AnimationTimeline.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"

using namespace mozilla;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(PendingPlayerTracker, mPlayPendingSet, mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PendingPlayerTracker, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PendingPlayerTracker, Release)

void
PendingPlayerTracker::AddPlayPending(dom::AnimationPlayer& aPlayer)
{
  mPlayPendingSet.PutEntry(&aPlayer);

  // Schedule a paint. Otherwise animations that don't trigger a paint by
  // themselves (e.g. CSS animations with an empty keyframes rule) won't
  // start until something else paints.
  EnsurePaintIsScheduled();
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
  dom::AnimationTimeline* timeline = player->Timeline();

  // When the timeline's refresh driver is under test control, its values
  // have no correspondance to wallclock times so we shouldn't try to convert
  // aReadyTime (which is a wallclock time) to a timeline value. Instead, the
  // animation player will be started when the refresh driver is next
  // advanced since this will trigger a call to StartPendingPlayersNow.
  if (timeline->IsUnderTestControl()) {
    return PL_DHASH_NEXT;
  }

  Nullable<TimeDuration> readyTime =
    timeline->ToTimelineTime(*static_cast<const TimeStamp*>(aReadyTime));
  player->StartOnNextTick(readyTime);

  return PL_DHASH_REMOVE;
}

void
PendingPlayerTracker::StartPendingPlayersOnNextTick(const TimeStamp& aReadyTime)
{
  mPlayPendingSet.EnumerateEntries(StartPlayerAtTime,
                                   const_cast<TimeStamp*>(&aReadyTime));
}

PLDHashOperator
StartPlayerNow(nsRefPtrHashKey<dom::AnimationPlayer>* aKey, void*)
{
  aKey->GetKey()->StartNow();
  return PL_DHASH_NEXT;
}

void
PendingPlayerTracker::StartPendingPlayersNow()
{
  mPlayPendingSet.EnumerateEntries(StartPlayerNow, nullptr);
  mPlayPendingSet.Clear();
}

void
PendingPlayerTracker::EnsurePaintIsScheduled()
{
  if (!mDocument) {
    return;
  }

  nsIPresShell* presShell = mDocument->GetShell();
  if (!presShell) {
    return;
  }

  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  rootFrame->SchedulePaint();
}

} // namespace mozilla
