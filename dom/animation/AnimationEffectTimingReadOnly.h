/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffectTimingReadOnly_h
#define mozilla_dom_AnimationEffectTimingReadOnly_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

// X11 has a #define for None
#ifdef None
#undef None
#endif
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"  // for FillMode
                                                         // and PlaybackDirection

namespace mozilla {

struct TimingParams
{
  // The unitialized state of mDuration represents "auto".
  // Bug 1237173: We will replace this with Maybe<TimeDuration>.
  dom::OwningUnrestrictedDoubleOrString mDuration;
  TimeDuration mDelay;      // Initializes to zero
  double mIterations = 1.0; // Can be NaN, negative, +/-Infinity
  dom::PlaybackDirection mDirection = dom::PlaybackDirection::Normal;
  dom::FillMode mFill = dom::FillMode::Auto;

  bool operator==(const TimingParams& aOther) const;
  bool operator!=(const TimingParams& aOther) const
  {
    return !(*this == aOther);
  }
};


namespace dom {

class AnimationEffectTimingReadOnly : public nsWrapperCache
{
public:
  AnimationEffectTimingReadOnly() = default;
  explicit AnimationEffectTimingReadOnly(const TimingParams& aTiming)
    : mTiming(aTiming) { }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationEffectTimingReadOnly)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationEffectTimingReadOnly)

protected:
  virtual ~AnimationEffectTimingReadOnly() = default;

public:
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  double Delay() const { return mTiming.mDelay.ToMilliseconds(); }
  double EndDelay() const { return 0.0; }
  FillMode Fill() const { return mTiming.mFill; }
  double IterationStart() const { return 0.0; }
  double Iterations() const { return mTiming.mIterations; }
  void GetDuration(OwningUnrestrictedDoubleOrString& aRetVal) const
  {
    aRetVal = mTiming.mDuration;
  }
  PlaybackDirection Direction() const { return mTiming.mDirection; }
  void GetEasing(nsString& aRetVal) const { aRetVal.AssignLiteral("linear"); }

  const TimingParams& AsTimingParams() const { return mTiming; }
  void SetTimingParams(const TimingParams& aTiming) { mTiming = aTiming; }

protected:
  nsCOMPtr<nsISupports> mParent;
  TimingParams mTiming;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffectTimingReadOnly_h
