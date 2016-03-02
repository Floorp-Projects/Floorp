/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectTimingReadOnly.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/AnimationEffectTimingReadOnlyBinding.h"
#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/KeyframeEffectBinding.h"

namespace mozilla {

TimingParams::TimingParams(const dom::AnimationEffectTimingProperties& aRhs,
                           const dom::Element* aTarget)
  : mDuration(aRhs.mDuration)
  , mDelay(TimeDuration::FromMilliseconds(aRhs.mDelay))
  , mIterations(aRhs.mIterations)
  , mIterationStart(aRhs.mIterationStart)
  , mDirection(aRhs.mDirection)
  , mFill(aRhs.mFill)
{
  mFunction = AnimationUtils::ParseEasing(aTarget, aRhs.mEasing);
}

TimingParams::TimingParams(double aDuration)
{
  mDuration.SetAsUnrestrictedDouble() = aDuration;
}

template <class OptionsType>
static const dom::AnimationEffectTimingProperties&
GetTimingProperties(const OptionsType& aOptions);

template <>
/* static */ const dom::AnimationEffectTimingProperties&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
  return aOptions.GetAsKeyframeEffectOptions();
}

template <>
/* static */ const dom::AnimationEffectTimingProperties&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
  return aOptions.GetAsKeyframeAnimationOptions();
}

template <class OptionsType>
static TimingParams
TimingParamsFromOptionsUnion(
  const OptionsType& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  if (aOptions.IsUnrestrictedDouble()) {
    return TimingParams(aOptions.GetAsUnrestrictedDouble());
  } else {
    // If aTarget is a pseudo element, we pass its parent element because
    // TimingParams only needs its owner doc to parse easing and both pseudo
    // element and its parent element should have the same owner doc.
    // Bug 1246320: Avoid passing the element for parsing the timing function
    RefPtr<dom::Element> targetElement;
    if (!aTarget.IsNull()) {
      const dom::ElementOrCSSPseudoElement& target = aTarget.Value();
      MOZ_ASSERT(target.IsElement() || target.IsCSSPseudoElement(),
                 "Uninitialized target");
      if (target.IsElement()) {
        targetElement = &target.GetAsElement();
      } else {
        targetElement = target.GetAsCSSPseudoElement().ParentElement();
      }
    }
    return TimingParams(GetTimingProperties(aOptions), targetElement);
  }
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget);
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget);
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
         mIterationStart == aOther.mIterationStart &&
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
