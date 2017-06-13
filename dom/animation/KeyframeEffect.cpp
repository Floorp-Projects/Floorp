/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"

#include "mozilla/dom/KeyframeAnimationOptionsBinding.h"
  // For UnrestrictedDoubleOrKeyframeAnimationOptions
#include "mozilla/dom/AnimationEffectTiming.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "nsDOMMutationObserver.h" // For nsAutoAnimationMutationBatch

namespace mozilla {
namespace dom {

KeyframeEffect::KeyframeEffect(nsIDocument* aDocument,
                               const Maybe<OwningAnimationTarget>& aTarget,
                               const TimingParams& aTiming,
                               const KeyframeEffectParams& aOptions)
  : KeyframeEffectReadOnly(aDocument, aTarget,
                           new AnimationEffectTiming(aDocument, aTiming, this),
                           aOptions)
{
}

JSObject*
KeyframeEffect::WrapObject(JSContext* aCx,
                           JS::Handle<JSObject*> aGivenProto)
{
  return KeyframeEffectBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<KeyframeEffect>
KeyframeEffect::Constructor(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffect>(aGlobal, aTarget, aKeyframes,
                                                 aOptions, aRv);
}

/* static */ already_AddRefed<KeyframeEffect>
KeyframeEffect::Constructor(const GlobalObject& aGlobal,
                            KeyframeEffectReadOnly& aSource,
                            ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffect>(aGlobal, aSource, aRv);
}

/* static */ already_AddRefed<KeyframeEffect>
KeyframeEffect::Constructor(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffect>(aGlobal, aTarget, aKeyframes,
                                                 aOptions, aRv);
}

void
KeyframeEffect::NotifySpecifiedTimingUpdated()
{
  // Use the same document for a pseudo element and its parent element.
  // Use nullptr if we don't have mTarget, so disable the mutation batch.
  nsAutoAnimationMutationBatch mb(mTarget ? mTarget->mElement->OwnerDoc()
                                          : nullptr);

  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();

    if (mAnimation->IsRelevant()) {
      nsNodeUtils::AnimationChanged(mAnimation);
    }

    RequestRestyle(EffectCompositor::RestyleType::Layer);
  }
}

void
KeyframeEffect::SetTarget(const Nullable<ElementOrCSSPseudoElement>& aTarget)
{
  Maybe<OwningAnimationTarget> newTarget = ConvertTarget(aTarget);
  if (mTarget == newTarget) {
    // Assign the same target, skip it.
    return;
  }

  if (mTarget) {
    UnregisterTarget();
    ResetIsRunningOnCompositor();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget->mElement->OwnerDoc());
    if (mAnimation) {
      nsNodeUtils::AnimationRemoved(mAnimation);
    }
  }

  mTarget = newTarget;

  if (mTarget) {
    UpdateTargetRegistration();
    RefPtr<nsStyleContext> styleContext = GetTargetStyleContext();
    if (styleContext) {
      UpdateProperties(styleContext);
    }

    MaybeUpdateFrameForCompositor();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget->mElement->OwnerDoc());
    if (mAnimation) {
      nsNodeUtils::AnimationAdded(mAnimation);
    }
  }

  // If the new target frame is also oversized we should probably record that
  // too so we have a more complete picture of the type of frame sizes we
  // encounter, hence we reset the telemetry flag here.
  mRecordedContentTooLarge = false;
  mRecordedFrameSize = false;
}

void
KeyframeEffect::SetIterationComposite(
  const IterationCompositeOperation& aIterationComposite,
  CallerType aCallerType)
{
  // Ignore iterationComposite if the Web Animations API is not enabled,
  // then the default value 'Replace' will be used.
  if (!AnimationUtils::IsCoreAPIEnabledForCaller(aCallerType)) {
    return;
  }

  if (mEffectOptions.mIterationComposite == aIterationComposite) {
    return;
  }

  if (mAnimation && mAnimation->IsRelevant()) {
    nsNodeUtils::AnimationChanged(mAnimation);
  }

  mEffectOptions.mIterationComposite = aIterationComposite;
  RequestRestyle(EffectCompositor::RestyleType::Layer);
}

void
KeyframeEffect::SetComposite(const CompositeOperation& aComposite)
{
  if (mEffectOptions.mComposite == aComposite) {
    return;
  }

  mEffectOptions.mComposite = aComposite;

  if (mAnimation && mAnimation->IsRelevant()) {
    nsNodeUtils::AnimationChanged(mAnimation);
  }

  if (mTarget) {
    RefPtr<nsStyleContext> styleContext = GetTargetStyleContext();
    if (styleContext) {
      UpdateProperties(styleContext);
    }
  }
}

} // namespace dom
} // namespace mozilla
