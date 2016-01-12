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
#include "mozilla/InitializerList.h"
#include "mozilla/LayerAnimationInfo.h"
#include "nsComputedDOMStyle.h" // nsComputedDOMStyle::GetPresShellForContent
#include "nsCSSPropertySet.h"
#include "nsCSSProps.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsRuleNode.h" // For nsRuleNode::ComputePropertiesOverridingAnimation
#include "nsTArray.h"
#include "RestyleManager.h"

using mozilla::dom::Animation;
using mozilla::dom::Element;
using mozilla::dom::KeyframeEffectReadOnly;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_CLASS(EffectCompositor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EffectCompositor)
  for (auto& elementSet : tmp->mElementsToRestyle) {
    elementSet.Clear();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EffectCompositor)
  for (auto& elementSet : tmp->mElementsToRestyle) {
    for (auto iter = elementSet.Iter(); !iter.Done(); iter.Next()) {
      CycleCollectionNoteChild(cb, iter.Key().mElement,
                               "EffectCompositor::mElementsToRestyle[]",
                               cb.Flags());
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(EffectCompositor, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(EffectCompositor, Release)

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

  // The animation cascade will almost always be up-to-date by this point
  // but there are some cases such as when we are restoring the refresh driver
  // from test control after seeking where it might not be the case.
  //
  // Those cases are probably not important but just to be safe, let's make
  // sure the cascade is up to date since if it *is* up to date, this is
  // basically a no-op.
  Maybe<Pair<dom::Element*, nsCSSPseudoElements::Type>> pseudoElement =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (pseudoElement) {
    EffectCompositor::MaybeUpdateCascadeResults(pseudoElement->first(),
                                                pseudoElement->second(),
                                                aFrame->StyleContext());
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

void
EffectCompositor::RequestRestyle(dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType,
                                 RestyleType aRestyleType,
                                 CascadeLevel aCascadeLevel)
{
  if (!mPresContext) {
    // Pres context will be null after the effect compositor is disconnected.
    return;
  }

  auto& elementsToRestyle = mElementsToRestyle[aCascadeLevel];
  PseudoElementHashKey key = { aElement, aPseudoType };

  if (aRestyleType == RestyleType::Throttled &&
      !elementsToRestyle.Contains(key)) {
    elementsToRestyle.Put(key, false);
    mPresContext->Document()->SetNeedStyleFlush();
  } else {
    // Get() returns 0 if the element is not found. It will also return
    // false if the element is found but does not have a pending restyle.
    bool hasPendingRestyle = elementsToRestyle.Get(key);
    if (!hasPendingRestyle) {
      PostRestyleForAnimation(aElement, aPseudoType, aCascadeLevel);
    }
    elementsToRestyle.Put(key, true);
  }

  if (aRestyleType == RestyleType::Layer) {
    // Prompt layers to re-sync their animations.
    mPresContext->RestyleManager()->IncrementAnimationGeneration();
    EffectSet* effectSet =
      EffectSet::GetEffectSet(aElement, aPseudoType);
    if (effectSet) {
      effectSet->UpdateAnimationGeneration(mPresContext);
    }
  }
}

void
EffectCompositor::PostRestyleForAnimation(dom::Element* aElement,
                                          nsCSSPseudoElements::Type aPseudoType,
                                          CascadeLevel aCascadeLevel)
{
  if (!mPresContext) {
    return;
  }

  dom::Element* element = GetElementToRestyle(aElement, aPseudoType);
  if (!element) {
    return;
  }

  nsRestyleHint hint = aCascadeLevel == CascadeLevel::Transitions ?
                                        eRestyle_CSSTransitions :
                                        eRestyle_CSSAnimations;
  mPresContext->PresShell()->RestyleForAnimation(element, hint);
}

void
EffectCompositor::PostRestyleForThrottledAnimations()
{
  for (size_t i = 0; i < kCascadeLevelCount; i++) {
    CascadeLevel cascadeLevel = CascadeLevel(i);
    auto& elementSet = mElementsToRestyle[cascadeLevel];

    for (auto iter = elementSet.Iter(); !iter.Done(); iter.Next()) {
      bool& postedRestyle = iter.Data();
      if (postedRestyle) {
        continue;
      }

      PostRestyleForAnimation(iter.Key().mElement,
                              iter.Key().mPseudoType,
                              cascadeLevel);
      postedRestyle = true;
    }
  }
}

void
EffectCompositor::MaybeUpdateAnimationRule(dom::Element* aElement,
                                           nsCSSPseudoElements::Type
                                             aPseudoType,
                                           CascadeLevel aCascadeLevel)
{
  // First update cascade results since that may cause some elements to
  // be marked as needing a restyle.
  MaybeUpdateCascadeResults(aElement, aPseudoType);

  auto& elementsToRestyle = mElementsToRestyle[aCascadeLevel];
  PseudoElementHashKey key = { aElement, aPseudoType };

  if (!mPresContext || !elementsToRestyle.Contains(key)) {
    return;
  }

  ComposeAnimationRule(aElement, aPseudoType, aCascadeLevel,
                       mPresContext->RefreshDriver()->MostRecentRefresh());

  elementsToRestyle.Remove(key);
}

nsIStyleRule*
EffectCompositor::GetAnimationRule(dom::Element* aElement,
                                   nsCSSPseudoElements::Type aPseudoType,
                                   CascadeLevel aCascadeLevel)
{
  if (!mPresContext || !mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effectSet) {
    return nullptr;
  }

  if (mPresContext->RestyleManager()->SkipAnimationRules()) {
    return nullptr;
  }

  MaybeUpdateAnimationRule(aElement, aPseudoType, aCascadeLevel);

  return effectSet->AnimationRule(aCascadeLevel);
}

/* static */ dom::Element*
EffectCompositor::GetElementToRestyle(dom::Element* aElement,
                                      nsCSSPseudoElements::Type aPseudoType)
{
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    return aElement;
  }

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  if (!primaryFrame) {
    return nullptr;
  }
  nsIFrame* pseudoFrame;
  if (aPseudoType == nsCSSPseudoElements::ePseudo_before) {
    pseudoFrame = nsLayoutUtils::GetBeforeFrame(primaryFrame);
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_after) {
    pseudoFrame = nsLayoutUtils::GetAfterFrame(primaryFrame);
  } else {
    NS_NOTREACHED("Should not try to get the element to restyle for a pseudo "
                  "other that :before or :after");
    return nullptr;
  }
  if (!pseudoFrame) {
    return nullptr;
  }
  return pseudoFrame->GetContent()->AsElement();
}

bool
EffectCompositor::HasPendingStyleUpdates() const
{
  for (auto& elementSet : mElementsToRestyle) {
    if (elementSet.Count()) {
      return true;
    }
  }

  return false;
}

bool
EffectCompositor::HasThrottledStyleUpdates() const
{
  for (auto& elementSet : mElementsToRestyle) {
    for (auto iter = elementSet.ConstIter(); !iter.Done(); iter.Next()) {
      if (!iter.Data()) {
        return true;
      }
    }
  }

  return false;
}

void
EffectCompositor::AddStyleUpdatesTo(RestyleTracker& aTracker)
{
  if (!mPresContext) {
    return;
  }

  for (size_t i = 0; i < kCascadeLevelCount; i++) {
    CascadeLevel cascadeLevel = CascadeLevel(i);
    auto& elementSet = mElementsToRestyle[cascadeLevel];

    // Copy the list of elements to restyle to a separate array that we can
    // iterate over. This is because we need to call MaybeUpdateCascadeResults
    // on each element, but doing that can mutate elementSet. In this case
    // it will only mutate the bool value associated with each element in the
    // set but even doing that will cause assertions in PLDHashTable to fail
    // if we are iterating over the hashtable at the same time.
    nsTArray<PseudoElementHashKey> elementsToRestyle(elementSet.Count());
    for (auto iter = elementSet.Iter(); !iter.Done(); iter.Next()) {
      elementsToRestyle.AppendElement(iter.Key());
    }

    for (auto& pseudoElem : elementsToRestyle) {
      MaybeUpdateCascadeResults(pseudoElem.mElement, pseudoElem.mPseudoType);

      ComposeAnimationRule(pseudoElem.mElement,
                           pseudoElem.mPseudoType,
                           cascadeLevel,
                           mPresContext->RefreshDriver()->MostRecentRefresh());

      dom::Element* elementToRestyle =
        GetElementToRestyle(pseudoElem.mElement, pseudoElem.mPseudoType);
      if (elementToRestyle) {
        nsRestyleHint rshint = cascadeLevel == CascadeLevel::Transitions ?
                               eRestyle_CSSTransitions :
                               eRestyle_CSSAnimations;
        aTracker.AddPendingRestyle(elementToRestyle, rshint, nsChangeHint(0));
      }
    }

    elementSet.Clear();
    // Note: mElement pointers in elementsToRestyle might now dangle
  }
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

/* static */ void
EffectCompositor::MaybeUpdateCascadeResults(Element* aElement,
                                            nsCSSPseudoElements::Type
                                              aPseudoType)
{
  nsStyleContext* styleContext = nullptr;
  {
    dom::Element* elementToRestyle = GetElementToRestyle(aElement, aPseudoType);
    if (elementToRestyle) {
      nsIFrame* frame = elementToRestyle->GetPrimaryFrame();
      if (frame) {
        styleContext = frame->StyleContext();
      }
    }
  }

  MaybeUpdateCascadeResults(aElement, aPseudoType, styleContext);
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
EffectCompositor::ComposeAnimationRule(dom::Element* aElement,
                                       nsCSSPseudoElements::Type aPseudoType,
                                       CascadeLevel aCascadeLevel,
                                       TimeStamp aRefreshTime)
{
  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects) {
    return;
  }

  // The caller is responsible for calling MaybeUpdateCascadeResults first.
  MOZ_ASSERT(!effects->CascadeNeedsUpdate(),
             "Animation cascade out of date when composing animation rule");

  // Get a list of effects for the current level sorted by composite order.
  nsTArray<KeyframeEffectReadOnly*> sortedEffectList;
  for (KeyframeEffectReadOnly* effect : *effects) {
    MOZ_ASSERT(effect->GetAnimation());
    if (effect->GetAnimation()->CascadeLevel() == aCascadeLevel) {
      sortedEffectList.AppendElement(effect);
    }
  }
  sortedEffectList.Sort(EffectCompositeOrderComparator());

  RefPtr<AnimValuesStyleRule>& animationRule =
    effects->AnimationRule(aCascadeLevel);
  animationRule = nullptr;

  // If multiple animations specify behavior for the same property the
  // animation with the *highest* composite order wins.
  // As a result, we iterate from last animation to first and, if a
  // property has already been set, we don't change it.
  nsCSSPropertySet properties;

  for (KeyframeEffectReadOnly* effect : Reversed(sortedEffectList)) {
    effect->GetAnimation()->ComposeStyle(animationRule, properties);
  }

  effects->UpdateAnimationRuleRefreshTime(aCascadeLevel, aRefreshTime);
}

/* static */ void
EffectCompositor::GetOverriddenProperties(nsStyleContext* aStyleContext,
                                          EffectSet& aEffectSet,
                                          nsCSSPropertySet&
                                            aPropertiesOverridden)
{
  nsAutoTArray<nsCSSProperty, LayerAnimationInfo::kRecords> propertiesToTrack;
  {
    nsCSSPropertySet propertiesToTrackAsSet;
    for (KeyframeEffectReadOnly* effect : aEffectSet) {
      for (const AnimationProperty& property : effect->Properties()) {
        if (nsCSSProps::PropHasFlags(property.mProperty,
                                     CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR) &&
            !propertiesToTrackAsSet.HasProperty(property.mProperty)) {
          propertiesToTrackAsSet.AddProperty(property.mProperty);
          propertiesToTrack.AppendElement(property.mProperty);
        }
      }
      // Skip iterating over the rest of the effects if we've already
      // found all the compositor-animatable properties.
      if (propertiesToTrack.Length() == LayerAnimationInfo::kRecords) {
        break;
      }
    }
  }

  if (propertiesToTrack.IsEmpty()) {
    return;
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
    GetOverriddenProperties(aStyleContext, aEffectSet, overriddenProperties);
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
          effect->GetAnimation()->CascadeLevel() == CascadeLevel::Animations &&
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
    // Update both transitions and animations. We could detect *which* levels
    // actually changed and only update them, but that's probably unnecessary.
    for (auto level : { CascadeLevel::Animations,
                        CascadeLevel::Transitions }) {
      presContext->EffectCompositor()->RequestRestyle(aElement,
                                                      aPseudoType,
                                                      RestyleType::Layer,
                                                      level);
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
