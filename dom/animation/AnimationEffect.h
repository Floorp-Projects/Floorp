/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffect_h
#define mozilla_dom_AnimationEffect_h

#include "mozilla/ComputedTiming.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/Maybe.h"
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TimingParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {

struct ElementPropertyTransition;

namespace dom {

class Animation;
class KeyframeEffect;
struct ComputedEffectTiming;

class AnimationEffect : public nsISupports,
                        public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationEffect)

  AnimationEffect(nsIDocument* aDocument, const TimingParams& aTiming)
    : mDocument(aDocument)
    , mTiming(aTiming)
  {
  }

  virtual KeyframeEffect* AsKeyframeEffect() { return nullptr; }

  virtual ElementPropertyTransition* AsTransition() { return nullptr; }
  virtual const ElementPropertyTransition* AsTransition() const
  {
    return nullptr;
  }

  nsISupports* GetParentObject() const { return mDocument; }

  bool IsCurrent() const;
  bool IsInEffect() const;
  bool HasFiniteActiveDuration() const
  {
    return SpecifiedTiming().ActiveDuration() != TimeDuration::Forever();
  }

  // AnimationEffect interface
  void GetTiming(EffectTiming& aRetVal) const;
  void GetComputedTimingAsDict(ComputedEffectTiming& aRetVal) const;
  void UpdateTiming(const OptionalEffectTiming& aTiming, ErrorResult& aRv);

  const TimingParams& SpecifiedTiming() const { return mTiming; }
  void SetSpecifiedTiming(const TimingParams& aTiming);

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
  static ComputedTiming
  GetComputedTimingAt(const Nullable<TimeDuration>& aLocalTime,
                      const TimingParams& aTiming,
                      double aPlaybackRate);
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
  virtual ~AnimationEffect() = default;

  Nullable<TimeDuration> GetLocalTime() const;

protected:
  RefPtr<nsIDocument> mDocument;
  RefPtr<Animation> mAnimation;
  TimingParams mTiming;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffect_h
