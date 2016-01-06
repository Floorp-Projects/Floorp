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
#include "AnimationCommon.h" // For AnimationCollection
#include "nsAnimationManager.h"
#include "nsComputedDOMStyle.h" // nsComputedDOMStyle::GetPresShellForContent
#include "nsCSSPropertySet.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsRuleNode.h" // For nsRuleNode::ComputePropertiesOverridingAnimation
#include "nsTArray.h"
#include "nsTransitionManager.h"

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

/* static */ void
EffectCompositor::MaybeUpdateCascadeResults(Element* aElement,
                                            nsCSSPseudoElements::Type
                                              aPseudoType,
                                            nsStyleContext* aStyleContext)
{
  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects || !effects->CascadeNeedsUpdate()) {
    return;
  }

  UpdateCascadeResults(*effects, aElement, aPseudoType, aStyleContext);

  MOZ_ASSERT(!effects->CascadeNeedsUpdate(), "Failed to update cascade state");
}

namespace {
  class EffectCompositeOrderComparator {
  public:
    bool Equals(const KeyframeEffectReadOnly* a,
                const KeyframeEffectReadOnly* b) const
    {
      return a == b;
    }

    bool LessThan(const KeyframeEffectReadOnly* a,
                  const KeyframeEffectReadOnly* b) const
    {
      MOZ_ASSERT(a->GetAnimation() && b->GetAnimation());
      MOZ_ASSERT(
        Equals(a, b) ||
        a->GetAnimation()->HasLowerCompositeOrderThan(*b->GetAnimation()) !=
          b->GetAnimation()->HasLowerCompositeOrderThan(*a->GetAnimation()));
      return a->GetAnimation()->HasLowerCompositeOrderThan(*b->GetAnimation());
    }
  };
}

/* static */ void
EffectCompositor::UpdateCascadeResults(Element* aElement,
                                       nsCSSPseudoElements::Type aPseudoType,
                                       nsStyleContext* aStyleContext)
{
  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects) {
    return;
  }

  UpdateCascadeResults(*effects, aElement, aPseudoType, aStyleContext);
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

/* static */ void
EffectCompositor::UpdateCascadeResults(EffectSet& aEffectSet,
                                       Element* aElement,
                                       nsCSSPseudoElements::Type aPseudoType,
                                       nsStyleContext* aStyleContext)
{
  MOZ_ASSERT(EffectSet::GetEffectSet(aElement, aPseudoType) == &aEffectSet,
             "Effect set should correspond to the specified (pseudo-)element");
  if (aEffectSet.IsEmpty()) {
    aEffectSet.MarkCascadeUpdated();
    return;
  }

  // Get a list of effects sorted by composite order.
  nsTArray<KeyframeEffectReadOnly*> sortedEffectList;
  for (KeyframeEffectReadOnly* effect : aEffectSet) {
    sortedEffectList.AppendElement(effect);
  }
  sortedEffectList.Sort(EffectCompositeOrderComparator());

  // Get properties that override the *animations* level of the cascade.
  //
  // We only do this for properties that we can animate on the compositor
  // since we will apply other properties on the main thread where the usual
  // cascade applies.
  nsCSSPropertySet overriddenProperties;
  if (aStyleContext) {
    GetOverriddenProperties(aStyleContext, overriddenProperties);
  }

  bool changed = false;
  nsCSSPropertySet animatedProperties;

  // Iterate from highest to lowest composite order.
  for (KeyframeEffectReadOnly* effect : Reversed(sortedEffectList)) {
    MOZ_ASSERT(effect->GetAnimation(),
               "Effects on a target element should have an Animation");
    bool inEffect = effect->IsInEffect();
    for (AnimationProperty& prop : effect->Properties()) {

      bool winsInCascade = !animatedProperties.HasProperty(prop.mProperty) &&
                           inEffect;

      // If this property wins in the cascade, add it to the set of animated
      // properties. We need to do this even if the property is overridden
      // (in which case we set winsInCascade to false below) since we don't
      // want to fire transitions on these properties.
      if (winsInCascade) {
        animatedProperties.AddProperty(prop.mProperty);
      }

      // For effects that will be applied to the animations level of the
      // cascade, we need to check that the property isn't being set by
      // something with higher priority in the cascade.
      //
      // We only do this, however, for properties that can be animated on
      // the compositor. For properties animated on the main thread the usual
      // cascade ensures these animations will be correctly overridden.
      if (winsInCascade &&
          !effect->GetAnimation()->AppliesToTransitionsLevel() &&
          overriddenProperties.HasProperty(prop.mProperty)) {
        winsInCascade = false;
      }

      if (winsInCascade != prop.mWinsInCascade) {
        changed = true;
      }
      prop.mWinsInCascade = winsInCascade;
    }
  }

  aEffectSet.MarkCascadeUpdated();

  // If there is any change in the cascade result, update animations on
  // layers with the winning animations.
  nsPresContext* presContext = GetPresContext(aElement);
  if (changed && presContext) {
    // We currently unconditionally update both animations and transitions
    // even if we could, for example, get away with only updating animations.
    // This is a temporary measure until we unify all animation style updating
    // under EffectCompositor.
    AnimationCollection* animations =
      presContext->AnimationManager()->GetAnimationCollection(aElement,
                                                              aPseudoType,
                                                              false);
                                                             /* don't create */
    if (animations) {
      animations->RequestRestyle(AnimationCollection::RestyleType::Layer);
    }

    AnimationCollection* transitions =
      presContext->TransitionManager()->GetAnimationCollection(aElement,
                                                               aPseudoType,
                                                               false);
                                                             /* don't create */
    if (transitions) {
      transitions->RequestRestyle(AnimationCollection::RestyleType::Layer);
    }
  }
}

/* static */ nsPresContext*
EffectCompositor::GetPresContext(Element* aElement)
{
  MOZ_ASSERT(aElement);
  nsIPresShell* shell = nsComputedDOMStyle::GetPresShellForContent(aElement);
  if (!shell) {
    return nullptr;
  }
  return shell->GetPresContext();
}

} // namespace mozilla
