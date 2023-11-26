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
#include "ElementAnimationData.h"

namespace mozilla {

void EffectSet::Traverse(nsCycleCollectionTraversalCallback& aCallback) {
  for (const auto& key : mEffects) {
    CycleCollectionNoteChild(aCallback, key, "EffectSet::mEffects[]",
                             aCallback.Flags());
  }
}

/* static */
EffectSet* EffectSet::Get(const dom::Element* aElement,
                          PseudoStyleType aPseudoType) {
  if (auto* data = aElement->GetAnimationData()) {
    return data->GetEffectSetFor(aPseudoType);
  }
  return nullptr;
}

/* static */
EffectSet* EffectSet::GetOrCreate(dom::Element* aElement,
                                  PseudoStyleType aPseudoType) {
  return &aElement->EnsureAnimationData().EnsureEffectSetFor(aPseudoType);
}

/* static */
EffectSet* EffectSet::GetForFrame(const nsIFrame* aFrame,
                                  const nsCSSPropertyIDSet& aProperties) {
  MOZ_ASSERT(aFrame);

  // Transform animations are run on the primary frame (but stored on the
  // content associated with the style frame).
  const nsIFrame* frameToQuery = nullptr;
  if (aProperties.IsSubsetOf(nsCSSPropertyIDSet::TransformLikeProperties())) {
    // Make sure to return nullptr if we're looking for transform animations on
    // the inner table frame.
    if (!aFrame->SupportsCSSTransforms()) {
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

  return Get(target->mElement, target->mPseudoType);
}

/* static */
EffectSet* EffectSet::GetForFrame(const nsIFrame* aFrame,
                                  DisplayItemType aDisplayItemType) {
  return EffectSet::GetForFrame(
      aFrame, LayerAnimationInfo::GetCSSPropertiesFor(aDisplayItemType));
}

/* static */
EffectSet* EffectSet::GetForStyleFrame(const nsIFrame* aStyleFrame) {
  Maybe<NonOwningAnimationTarget> target =
      EffectCompositor::GetAnimationElementAndPseudoForFrame(aStyleFrame);

  if (!target) {
    return nullptr;
  }

  return Get(target->mElement, target->mPseudoType);
}

/* static */
EffectSet* EffectSet::GetForEffect(const dom::KeyframeEffect* aEffect) {
  NonOwningAnimationTarget target = aEffect->GetAnimationTarget();
  if (!target) {
    return nullptr;
  }

  return EffectSet::Get(target.mElement, target.mPseudoType);
}

/* static */
void EffectSet::DestroyEffectSet(dom::Element* aElement,
                                 PseudoStyleType aPseudoType) {
  if (auto* data = aElement->GetAnimationData()) {
    data->ClearEffectSetFor(aPseudoType);
  }
}

void EffectSet::UpdateAnimationGeneration(nsPresContext* aPresContext) {
  mAnimationGeneration =
      aPresContext->RestyleManager()->GetAnimationGeneration();
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
