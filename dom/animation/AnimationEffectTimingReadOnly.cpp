/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectTimingReadOnly.h"

#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/AnimationEffectTimingReadOnlyBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"

namespace mozilla {

TimingParams::TimingParams(const dom::AnimationEffectTimingProperties& aRhs,
                           const dom::Element* aTarget)
  : mDuration(aRhs.mDuration)
  , mDelay(TimeDuration::FromMilliseconds(aRhs.mDelay))
  , mIterations(aRhs.mIterations)
  , mDirection(aRhs.mDirection)
  , mFill(aRhs.mFill)
{
  mFunction = AnimationUtils::ParseEasing(aTarget, aRhs.mEasing);
}

TimingParams::TimingParams(double aDuration)
{
  mDuration.SetAsUnrestrictedDouble() = aDuration;
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
  const dom::Element* aTarget)
{
  if (aOptions.IsUnrestrictedDouble()) {
    return TimingParams(aOptions.GetAsUnrestrictedDouble());
  } else {
    MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
    return TimingParams(aOptions.GetAsKeyframeEffectOptions(), aTarget);
  }
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
  const dom::Element* aTarget)
{
  if (aOptions.IsUnrestrictedDouble()) {
    return TimingParams(aOptions.GetAsUnrestrictedDouble());
  } else {
    MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
    return TimingParams(aOptions.GetAsKeyframeAnimationOptions(), aTarget);
  }
}

bool
TimingParams::operator==(const TimingParams& aOther) const
{
  bool durationEqual;
  if (mDuration.IsUnrestrictedDouble()) {
    durationEqual = aOther.mDuration.IsUnrestrictedDouble() &&
                    (mDuration.GetAsUnrestrictedDouble() ==
                     aOther.mDuration.GetAsUnrestrictedDouble());
  } else {
    // We consider all string values and uninitialized values as meaning "auto".
    // Since mDuration is either a string or uninitialized, we consider it equal
    // if aOther.mDuration is also either a string or uninitialized.
    durationEqual = !aOther.mDuration.IsUnrestrictedDouble();
  }
  return durationEqual &&
         mDelay == aOther.mDelay &&
         mIterations == aOther.mIterations &&
         mDirection == aOther.mDirection &&
         mFill == aOther.mFill &&
         mFunction == aOther.mFunction;
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffectTimingReadOnly, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationEffectTimingReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationEffectTimingReadOnly, Release)

JSObject*
AnimationEffectTimingReadOnly::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AnimationEffectTimingReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

void
AnimationEffectTimingReadOnly::GetEasing(nsString& aRetVal) const
{
  if (mTiming.mFunction.isSome()) {
    mTiming.mFunction->AppendToString(aRetVal);
  } else {
    aRetVal.AssignLiteral("linear");
  }
}

} // namespace dom
} // namespace mozilla
