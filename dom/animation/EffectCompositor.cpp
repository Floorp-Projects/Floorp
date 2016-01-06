/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectCompositor.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffect.h" // For KeyframeEffectReadOnly
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectSet.h"
#include "mozilla/LayerAnimationInfo.h"
#include "nsCSSPropertySet.h"
#include "nsLayoutUtils.h"
#include "nsRuleNode.h" // For nsRuleNode::ComputePropertiesOverridingAnimation
#include "nsTArray.h"

using mozilla::dom::Animation;
using mozilla::dom::Element;
using mozilla::dom::KeyframeEffectReadOnly;

namespace mozilla {

// Helper function to factor out the common logic from
// GetAnimationsForCompositor and HasAnimationsForCompositor.
//
// Takes an optional array to fill with eligible animations.
//
// Returns true if there are eligible animations, false otherwise.
bool
FindAnimationsForCompositor(const nsIFrame* aFrame,
                            nsCSSProperty aProperty,
                            nsTArray<RefPtr<dom::Animation>>* aMatches /*out*/)
{
  MOZ_ASSERT(!aMatches || aMatches->IsEmpty(),
             "Matches array, if provided, should be empty");

  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects || effects->IsEmpty()) {
    return false;
  }

  if (aFrame->RefusedAsyncAnimation()) {
    return false;
  }

  if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animations are "
                            "disabled");
      AnimationUtils::LogAsyncAnimationFailure(message);
    }
    return false;
  }

  bool foundSome = false;
  for (KeyframeEffectReadOnly* effect : *effects) {
    MOZ_ASSERT(effect && effect->GetAnimation());
    Animation* animation = effect->GetAnimation();

    if (!animation->IsPlaying()) {
      continue;
    }

    if (effect->ShouldBlockCompositorAnimations(aFrame)) {
      if (aMatches) {
        aMatches->Clear();
      }
      return false;
    }

    if (!effect->HasAnimationOfProperty(aProperty)) {
      continue;
    }

    if (aMatches) {
      aMatches->AppendElement(animation);
    }
    foundSome = true;
  }

  MOZ_ASSERT(!foundSome || !aMatches || !aMatches->IsEmpty(),
             "If return value is true, matches array should be non-empty");
  return foundSome;
}

/* static */ bool
EffectCompositor::HasAnimationsForCompositor(const nsIFrame* aFrame,
                                             nsCSSProperty aProperty)
{
  return FindAnimationsForCompositor(aFrame, aProperty, nullptr);
}

/* static */ nsTArray<RefPtr<dom::Animation>>
EffectCompositor::GetAnimationsForCompositor(const nsIFrame* aFrame,
                                             nsCSSProperty aProperty)
{
  nsTArray<RefPtr<dom::Animation>> result;

#ifdef DEBUG
  bool foundSome =
#endif
    FindAnimationsForCompositor(aFrame, aProperty, &result);
  MOZ_ASSERT(!foundSome || !result.IsEmpty(),
             "If return value is true, matches array should be non-empty");

  return result;
}

/* static */ Maybe<Pair<Element*, nsCSSPseudoElements::Type>>
EffectCompositor::GetAnimationElementAndPseudoForFrame(const nsIFrame* aFrame)
{
  // Always return the same object to benefit from return-value optimization.
  Maybe<Pair<Element*, nsCSSPseudoElements::Type>> result;

  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return result;
  }

  nsCSSPseudoElements::Type pseudoType =
    nsCSSPseudoElements::ePseudo_NotPseudoElement;

  if (aFrame->IsGeneratedContentFrame()) {
    nsIFrame* parent = aFrame->GetParent();
    if (parent->IsGeneratedContentFrame()) {
      return result;
    }
    nsIAtom* name = content->NodeInfo()->NameAtom();
    if (name == nsGkAtoms::mozgeneratedcontentbefore) {
      pseudoType = nsCSSPseudoElements::ePseudo_before;
    } else if (name == nsGkAtoms::mozgeneratedcontentafter) {
      pseudoType = nsCSSPseudoElements::ePseudo_after;
    } else {
      return result;
    }
    content = content->GetParent();
    if (!content) {
      return result;
    }
  }

  if (!content->IsElement()) {
    return result;
  }

  result = Some(MakePair(content->AsElement(), pseudoType));

  return result;
}

/* static */ void
EffectCompositor::GetOverriddenProperties(nsStyleContext* aStyleContext,
                                          nsCSSPropertySet&
                                            aPropertiesOverridden)
{
  nsAutoTArray<nsCSSProperty, LayerAnimationInfo::kRecords> propertiesToTrack;
  for (const LayerAnimationInfo::Record& record :
         LayerAnimationInfo::sRecords) {
    propertiesToTrack.AppendElement(record.mProperty);
  }
  nsRuleNode::ComputePropertiesOverridingAnimation(propertiesToTrack,
                                                   aStyleContext,
                                                   aPropertiesOverridden);
}

} // namespace mozilla
