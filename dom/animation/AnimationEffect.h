/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffect_h
#define mozilla_dom_AnimationEffect_h

#include "mozilla/ComputedTiming.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/ScrollTimeline.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TimingParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class KeyframeEffect;
struct ComputedEffectTiming;
struct EffectTiming;
struct OptionalEffectTiming;
class Document;

class AnimationEffect : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AnimationEffect)

  AnimationEffect(Document* aDocument, TimingParams&& aTiming);

  virtual KeyframeEffect* AsKeyframeEffect() { return nullptr; }

  nsISupports* GetParentObject() const;

  bool IsCurrent() const;
  bool IsInEffect() const;
  bool HasFiniteActiveDuration() const {
    return NormalizedTiming().ActiveDuration() != TimeDuration::Forever();
  }

  // AnimationEffect interface
  virtual void GetTiming(EffectTiming& aRetVal) const;
  virtual void GetComputedTimingAsDict(ComputedEffectTiming& aRetVal) const;
  virtual void UpdateTiming(const OptionalEffectTiming& aTiming,
                            ErrorResult& aRv);

  const TimingParams& SpecifiedTiming() const { return mTiming; }
  void SetSpecifiedTiming(TimingParams&& aTiming);

  const TimingParams& NormalizedTiming() const {
    MOZ_ASSERT((mAnimation && mAnimation->UsingScrollTimeline() &&
                mNormalizedTiming) ||
                   !mNormalizedTiming,
               "We do normalization only for progress-based timeline");
    return mNormalizedTiming ? *mNormalizedTiming : mTiming;
  }

  // There are 3 conditions where we have to update the normalized timing:
  // 1. mAnimation is changed, or
  // 2. the timeline of mAnimation is changed, or
  // 3. mTiming is changed.
  void UpdateNormalizedTiming();

  // This function takes as input the timing parameters of an animation and
  // returns the computed timing at the specified local time.
  //
  // The local time may be null in which case only static parameters such as the
  // active duration are calculated. All other members of the returned object
  // are given a null/initial value.
  //
  // This function returns a null mProgress member of the return value
  // if the animation should not be run
  // (because it is not currently active and is not filling at this time).
  static ComputedTiming GetComputedTimingAt(
      const Nullable<TimeDuration>& aLocalTime, const TimingParams& aTiming,
      double aPlaybackRate,
      Animation::ProgressTimelinePosition aProgressTimelinePosition);
  // Shortcut that gets the computed timing using the current local time as
  // calculated from the timeline time.
  ComputedTiming GetComputedTiming(const TimingParams* aTiming = nullptr) const;

  virtual void SetAnimation(Animation* aAnimation) = 0;
  Animation* GetAnimation() const { return mAnimation; };

  /**
   * Returns true if this effect animates one of the properties we consider
   * geometric properties, e.g. properties such as 'width' or 'margin-left'
   * that we try to synchronize with transform animations, on a valid target
   * element.
   */
  virtual bool AffectsGeometry() const = 0;

 protected:
  virtual ~AnimationEffect();

  Nullable<TimeDuration> GetLocalTime() const;

 protected:
  RefPtr<Document> mDocument;
  RefPtr<Animation> mAnimation;
  TimingParams mTiming;
  Maybe<TimingParams> mNormalizedTiming;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AnimationEffect_h
