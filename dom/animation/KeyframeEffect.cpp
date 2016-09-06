/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"

#include "mozilla/dom/AnimatableBinding.h"
  // For UnrestrictedDoubleOrKeyframeAnimationOptions
#include "mozilla/dom/AnimationEffectTiming.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/KeyframeUtils.h"
#include "nsDOMMutationObserver.h" // For nsAutoAnimationMutationBatch
#include "nsIScriptError.h"

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
    // We don't need to reset the mWinsInCascade member since it will be updated
    // when we later associate with a different target (and until that time this
    // flag is not used).

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
    } else if (mEffectOptions.mSpacingMode == SpacingMode::paced) {
      KeyframeUtils::ApplyDistributeSpacing(mKeyframes);
    }

    MaybeUpdateFrameForCompositor();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget->mElement->OwnerDoc());
    if (mAnimation) {
      nsNodeUtils::AnimationAdded(mAnimation);
    }
  } else if (mEffectOptions.mSpacingMode == SpacingMode::paced) {
    // New target is null, so fall back to distribute spacing.
    KeyframeUtils::ApplyDistributeSpacing(mKeyframes);
  }
}

void
KeyframeEffect::SetSpacing(JSContext* aCx,
                           const nsAString& aSpacing,
                           ErrorResult& aRv)
{
  SpacingMode spacingMode = SpacingMode::distribute;
  nsCSSPropertyID pacedProperty = eCSSProperty_UNKNOWN;
  nsAutoString invalidPacedProperty;
  KeyframeEffectParams::ParseSpacing(aSpacing,
                                     spacingMode,
                                     pacedProperty,
                                     invalidPacedProperty,
                                     aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!invalidPacedProperty.IsEmpty()) {
    const char16_t* params[] = { invalidPacedProperty.get() };
    nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aCx);
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Animation"),
                                    doc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "UnanimatablePacedProperty",
                                    params, ArrayLength(params));
  }

  if (mEffectOptions.mSpacingMode == spacingMode &&
      mEffectOptions.mPacedProperty == pacedProperty) {
    return;
  }

  mEffectOptions.mSpacingMode = spacingMode;
  mEffectOptions.mPacedProperty = pacedProperty;

  // Apply spacing. We apply distribute here. If the new spacing is paced,
  // UpdateProperties() will apply it.
  if (mEffectOptions.mSpacingMode == SpacingMode::distribute) {
    KeyframeUtils::ApplyDistributeSpacing(mKeyframes);
  }

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
