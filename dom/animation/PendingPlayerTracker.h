/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingPlayerTracker_h
#define mozilla_dom_PendingPlayerTracker_h

#include "mozilla/dom/AnimationPlayer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashtable.h"

namespace mozilla {

class PendingPlayerTracker MOZ_FINAL
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PendingPlayerTracker)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(PendingPlayerTracker)

  void AddPlayPending(dom::AnimationPlayer& aPlayer);
  void RemovePlayPending(dom::AnimationPlayer& aPlayer);
  bool IsWaitingToPlay(dom::AnimationPlayer const& aPlayer) const;

private:
  ~PendingPlayerTracker() { }

  typedef nsTHashtable<nsRefPtrHashKey<dom::AnimationPlayer>>
    AnimationPlayerSet;

  AnimationPlayerSet mPlayPendingSet;
};

} // namespace mozilla

#endif // mozilla_dom_PendingPlayerTracker_h
