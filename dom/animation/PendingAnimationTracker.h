/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PendingAnimationTracker_h
#define mozilla_dom_PendingAnimationTracker_h

#include "mozilla/dom/Animation.h"
#include "mozilla/TypedEnumBits.h"
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
    // We'd like to assert here that IsWaitingToPause(aAnimation) is false but
    // if |aAnimation| was tracked here as a pause-pending animation when it was
    // removed from |mDocument|, then re-attached to |mDocument|, and then
    // played again, we could end up here with IsWaitingToPause returning true.
    //
    // However, that should be harmless since all it means is that we'll call
    // Animation::TriggerOnNextTick or Animation::TriggerNow twice, both of
    // which will handle the redundant call gracefully.
    AddPending(aAnimation, mPlayPendingSet);
    mHasPlayPendingGeometricAnimations = CheckState::Indeterminate;
  }
  void RemovePlayPending(dom::Animation& aAnimation)
  {
    RemovePending(aAnimation, mPlayPendingSet);
    mHasPlayPendingGeometricAnimations = CheckState::Indeterminate;
  }
  bool IsWaitingToPlay(const dom::Animation& aAnimation) const
  {
    return IsWaiting(aAnimation, mPlayPendingSet);
  }

  void AddPausePending(dom::Animation& aAnimation)
  {
    // As with AddPausePending, we'd like to assert that
    // IsWaitingToPlay(aAnimation) is false but there are some circumstances
    // where this can be true. Fortunately adding the animation to both pending
    // sets should be harmless.
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

  /**
   * Looks amongst the set of play-pending animations, and, if there are
   * animations that affect geometric properties, notifies all play-pending
   * animations so that they can be synchronized, if needed.
   */
  void MarkAnimationsThatMightNeedSynchronization();

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

public:
  enum class CheckState {
    Indeterminate = 0,
    Absent = 1 << 0,
    AnimationsPresent = 1 << 1,
    TransitionsPresent = 1 << 2,
  };

private:
  CheckState mHasPlayPendingGeometricAnimations = CheckState::Indeterminate;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(PendingAnimationTracker::CheckState)

} // namespace mozilla

#endif // mozilla_dom_PendingAnimationTracker_h
