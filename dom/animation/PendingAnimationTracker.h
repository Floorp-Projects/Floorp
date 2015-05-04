/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  void AddPlayPending(dom::Animation& aAnimation)
  {
    MOZ_ASSERT(!IsWaitingToPause(aAnimation),
               "Animation is already waiting to pause");
    AddPending(aAnimation, mPlayPendingSet);
  }
  void RemovePlayPending(dom::Animation& aAnimation)
  {
    RemovePending(aAnimation, mPlayPendingSet);
  }
  bool IsWaitingToPlay(const dom::Animation& aAnimation) const
  {
    return IsWaiting(aAnimation, mPlayPendingSet);
  }

  void AddPausePending(dom::Animation& aAnimation)
  {
    MOZ_ASSERT(!IsWaitingToPlay(aAnimation),
               "Animation is already waiting to play");
    AddPending(aAnimation, mPausePendingSet);
  }
  void RemovePausePending(dom::Animation& aAnimation)
  {
    RemovePending(aAnimation, mPausePendingSet);
  }
  bool IsWaitingToPause(const dom::Animation& aAnimation) const
  {
    return IsWaiting(aAnimation, mPausePendingSet);
  }

  void TriggerPendingAnimationsOnNextTick(const TimeStamp& aReadyTime);
  void TriggerPendingAnimationsNow();
  bool HasPendingAnimations() const {
    return mPlayPendingSet.Count() > 0 || mPausePendingSet.Count() > 0;
  }

private:
  ~PendingAnimationTracker() { }

  void EnsurePaintIsScheduled();

  typedef nsTHashtable<nsRefPtrHashKey<dom::Animation>> AnimationSet;

  void AddPending(dom::Animation& aAnimation, AnimationSet& aSet);
  void RemovePending(dom::Animation& aAnimation, AnimationSet& aSet);
  bool IsWaiting(const dom::Animation& aAnimation,
                 const AnimationSet& aSet) const;

  AnimationSet mPlayPendingSet;
  AnimationSet mPausePendingSet;
  nsCOMPtr<nsIDocument> mDocument;
};

} // namespace mozilla

#endif // mozilla_dom_PendingAnimationTracker_h
