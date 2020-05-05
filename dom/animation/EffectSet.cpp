/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectSet.h"
#include "mozilla/dom/Element.h"  // For Element
#include "mozilla/RestyleManager.h"
#include "mozilla/LayerAnimationInfo.h"
#include "nsCSSPseudoElements.h"         // For PseudoStyleType
#include "nsCycleCollectionNoteChild.h"  // For CycleCollectionNoteChild
#include "nsPresContext.h"
#include "nsLayoutUtils.h"

namespace mozilla {

/* static */
void EffectSet::PropertyDtor(void* aObject, nsAtom* aPropertyName,
                             void* aPropertyValue, void* aData) {
  EffectSet* effectSet = static_cast<EffectSet*>(aPropertyValue);

#ifdef DEBUG
  MOZ_ASSERT(!effectSet->mCalledPropertyDtor, "Should not call dtor twice");
  effectSet->mCalledPropertyDtor = true;
#endif

  delete effectSet;
}

void EffectSet::Traverse(nsCycleCollectionTraversalCallback& aCallback) {
  for (auto iter = mEffects.Iter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.Get()->GetKey(),
                             "EffectSet::mEffects[]", aCallback.Flags());
  }
}

/* static */
EffectSet* EffectSet::GetEffectSet(const dom::Element* aElement,
                                   PseudoStyleType aPseudoType) {
  if (!aElement->MayHaveAnimations()) {
    return nullptr;
  }

  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  return static_cast<EffectSet*>(aElement->GetProperty(propName));
}

/* static */
EffectSet* EffectSet::GetOrCreateEffectSet(dom::Element* aElement,
                                           PseudoStyleType aPseudoType) {
  EffectSet* effectSet = GetEffectSet(aElement, aPseudoType);
  if (effectSet) {
    return effectSet;
  }

  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  effectSet = new EffectSet();

  nsresult rv = aElement->SetProperty(propName, effectSet,
                                      &EffectSet::PropertyDtor, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("SetProperty failed");
    // The set must be destroyed via PropertyDtor, otherwise
    // mCalledPropertyDtor assertion is triggered in destructor.
    EffectSet::PropertyDtor(aElement, propName, effectSet, nullptr);
    return nullptr;
  }

  aElement->SetMayHaveAnimations();

  return effectSet;
}

/* static */
EffectSet* EffectSet::GetEffectSetForFrame(
    const nsIFrame* aFrame, const nsCSSPropertyIDSet& aProperties) {
  MOZ_ASSERT(aFrame);

  // Transform animations are run on the primary frame (but stored on the
  // content associated with the style frame).
  const nsIFrame* frameToQuery = nullptr;
  if (aProperties.IsSubsetOf(nsCSSPropertyIDSet::TransformLikeProperties())) {
    // Make sure to return nullptr if we're looking for transform animations on
    // the inner table frame.
    if (!aFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms)) {
      return nullptr;
    }
    frameToQuery = nsLayoutUtils::GetStyleFrame(aFrame);
  } else {
    MOZ_ASSERT(
        !aProperties.Intersects(nsCSSPropertyIDSet::TransformLikeProperties()),
        "We should have only transform properties or no transform properties");
    // We don't need to explicitly return nullptr when |aFrame| is NOT the style
    // frame since there will be no effect set in that case.
    frameToQuery = aFrame;
  }

  Maybe<NonOwningAnimationTarget> target =
      EffectCompositor::GetAnimationElementAndPseudoForFrame(frameToQuery);
  if (!target) {
    return nullptr;
  }

  return GetEffectSet(target->mElement, target->mPseudoType);
}

/* static */
EffectSet* EffectSet::GetEffectSetForFrame(const nsIFrame* aFrame,
                                           DisplayItemType aDisplayItemType) {
  return EffectSet::GetEffectSetForFrame(
      aFrame, LayerAnimationInfo::GetCSSPropertiesFor(aDisplayItemType));
}

/* static */
EffectSet* EffectSet::GetEffectSetForStyleFrame(const nsIFrame* aStyleFrame) {
  Maybe<NonOwningAnimationTarget> target =
      EffectCompositor::GetAnimationElementAndPseudoForFrame(aStyleFrame);

  if (!target) {
    return nullptr;
  }

  return GetEffectSet(target->mElement, target->mPseudoType);
}

/* static */
void EffectSet::DestroyEffectSet(dom::Element* aElement,
                                 PseudoStyleType aPseudoType) {
  nsAtom* propName = GetEffectSetPropertyAtom(aPseudoType);
  EffectSet* effectSet =
      static_cast<EffectSet*>(aElement->GetProperty(propName));
  if (!effectSet) {
    return;
  }

  MOZ_ASSERT(!effectSet->IsBeingEnumerated(),
             "Should not destroy an effect set while it is being enumerated");
  effectSet = nullptr;

  aElement->RemoveProperty(propName);
}

void EffectSet::UpdateAnimationGeneration(nsPresContext* aPresContext) {
  mAnimationGeneration =
      aPresContext->RestyleManager()->GetAnimationGeneration();
}

/* static */
nsAtom** EffectSet::GetEffectSetPropertyAtoms() {
  static nsAtom* effectSetPropertyAtoms[] = {
      nsGkAtoms::animationEffectsProperty,
      nsGkAtoms::animationEffectsForBeforeProperty,
      nsGkAtoms::animationEffectsForAfterProperty,
      nsGkAtoms::animationEffectsForMarkerProperty, nullptr};

  return effectSetPropertyAtoms;
}

/* static */
nsAtom* EffectSet::GetEffectSetPropertyAtom(PseudoStyleType aPseudoType) {
  switch (aPseudoType) {
    case PseudoStyleType::NotPseudo:
      return nsGkAtoms::animationEffectsProperty;

    case PseudoStyleType::before:
      return nsGkAtoms::animationEffectsForBeforeProperty;

    case PseudoStyleType::after:
      return nsGkAtoms::animationEffectsForAfterProperty;

    case PseudoStyleType::marker:
      return nsGkAtoms::animationEffectsForMarkerProperty;

    default:
      MOZ_ASSERT_UNREACHABLE(
          "Should not try to get animation effects for "
          "a pseudo other that :before, :after or ::marker");
      return nullptr;
  }
}

void EffectSet::AddEffect(dom::KeyframeEffect& aEffect) {
  if (!mEffects.EnsureInserted(&aEffect)) {
    return;
  }

  MarkCascadeNeedsUpdate();
}

void EffectSet::RemoveEffect(dom::KeyframeEffect& aEffect) {
  if (!mEffects.EnsureRemoved(&aEffect)) {
    return;
  }

  MarkCascadeNeedsUpdate();
}

}  // namespace mozilla
