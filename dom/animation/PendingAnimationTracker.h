/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingAnimationTracker_h
#define mozilla_dom_PendingAnimationTracker_h

#include "mozilla/dom/Animation.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsTHashtable.h"

class nsIFrame;

namespace mozilla {

class PendingAnimationTracker final
{
public:
  explicit PendingAnimationTracker(nsIDocument* aDocument)
    : mDocument(aDocument)
  { }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PendingAnimationTracker)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(PendingAnimationTracker)

  void AddPlayPending(dom::Animation& aPlayer)
  {
    MOZ_ASSERT(!IsWaitingToPause(aPlayer),
               "Player is already waiting to pause");
    AddPending(aPlayer, mPlayPendingSet);
  }
  void RemovePlayPending(dom::Animation& aPlayer)
  {
    RemovePending(aPlayer, mPlayPendingSet);
  }
  bool IsWaitingToPlay(const dom::Animation& aPlayer) const
  {
    return IsWaiting(aPlayer, mPlayPendingSet);
  }

  void AddPausePending(dom::Animation& aPlayer)
  {
    MOZ_ASSERT(!IsWaitingToPlay(aPlayer),
               "Player is already waiting to play");
    AddPending(aPlayer, mPausePendingSet);
  }
  void RemovePausePending(dom::Animation& aPlayer)
  {
    RemovePending(aPlayer, mPausePendingSet);
  }
  bool IsWaitingToPause(const dom::Animation& aPlayer) const
  {
    return IsWaiting(aPlayer, mPausePendingSet);
  }

  void TriggerPendingAnimationsOnNextTick(const TimeStamp& aReadyTime);
  void TriggerPendingAnimationsNow();
  bool HasPendingAnimations() const {
    return mPlayPendingSet.Count() > 0 || mPausePendingSet.Count() > 0;
  }

private:
  ~PendingAnimationTracker() { }

  void EnsurePaintIsScheduled();

  typedef nsTHashtable<nsRefPtrHashKey<dom::Animation>>
    AnimationPlayerSet;

  void AddPending(dom::Animation& aPlayer,
                  AnimationPlayerSet& aSet);
  void RemovePending(dom::Animation& aPlayer,
                     AnimationPlayerSet& aSet);
  bool IsWaiting(const dom::Animation& aPlayer,
                 const AnimationPlayerSet& aSet) const;

  AnimationPlayerSet mPlayPendingSet;
  AnimationPlayerSet mPausePendingSet;
  nsCOMPtr<nsIDocument> mDocument;
};

} // namespace mozilla

#endif // mozilla_dom_PendingAnimationTracker_h
