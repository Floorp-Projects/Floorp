/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingPlayerTracker.h"

using namespace mozilla;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(PendingPlayerTracker, mPlayPendingSet)

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

} // namespace mozilla
