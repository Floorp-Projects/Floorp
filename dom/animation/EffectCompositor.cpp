/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectCompositor.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffect.h" // For KeyframeEffectReadOnly
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectSet.h"
#include "mozilla/InitializerList.h"
#include "mozilla/LayerAnimationInfo.h"
#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "nsComputedDOMStyle.h" // nsComputedDOMStyle::GetPresShellForContent
#include "nsCSSPropertySet.h"
#include "nsCSSProps.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsRuleNode.h" // For nsRuleNode::ComputePropertiesOverridingAnimation
#include "nsRuleProcessorData.h" // For ElementRuleProcessorData etc.
#include "nsTArray.h"

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
  Maybe<NonOwningAnimationTarget> pseudoElement =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (pseudoElement) {
    EffectCompositor::MaybeUpdateCascadeResults(pseudoElement->mElement,
                                                pseudoElement->mPseudoType,
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

  // Disable async animations if we have a rendering observer that
  // depends on our content (svg masking, -moz-element etc) so that
  // it gets updated correctly.
  nsIContent* content = aFrame->GetContent();
  while (content) {
    if (content->HasRenderingObservers()) {
      EffectCompositor::SetPerformanceWarning(
        aFrame, aProperty,
        AnimationPerformanceWarning(
          AnimationPerformanceWarning::Type::HasRenderingObserver));
      return false;
    }
    content = content->GetParent();
  }

  bool foundSome = false;
  for (KeyframeEffectReadOnly* effect : *effects) {
    MOZ_ASSERT(effect && effect->GetAnimation());
    Animation* animation = effect->GetAnimation();

    if (!animation->IsPlaying()) {
      continue;
    }

    AnimationPerformanceWarning::Type warningType;
    if (aProperty == eCSSProperty_transform &&
        effect->ShouldBlockAsyncTransformAnimations(aFrame,
                                                    warningType)) {
      if (aMatches) {
        aMatches->Clear();
      }
      EffectCompositor::SetPerformanceWarning(
        aFrame, aProperty,
        AnimationPerformanceWarning(warningType));
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
                                 CSSPseudoElementType aPseudoType,
                                 RestyleType aRestyleType,
                                 CascadeLevel aCascadeLevel)
{
  if (!mPresContext) {
    // Pres context will be null after the effect compositor is disconnected.
    return;
  }

  auto& elementsToRestyle = mElementsToRestyle[aCascadeLevel];
  PseudoElementHashEntry::KeyType key = { aElement, aPseudoType };

  if (aRestyleType == RestyleType::Throttled) {
    if (!elementsToRestyle.Contains(key)) {
      elementsToRestyle.Put(key, false);
    }
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
    MOZ_ASSERT(mPresContext->RestyleManager()->IsGecko(),
               "stylo: Servo-backed style system should not be using "
               "EffectCompositor");
    mPresContext->RestyleManager()->AsGecko()->IncrementAnimationGeneration();
    EffectSet* effectSet =
      EffectSet::GetEffectSet(aElement, aPseudoType);
    if (effectSet) {
      effectSet->UpdateAnimationGeneration(mPresContext);
    }
  }
}

void
EffectCompositor::PostRestyleForAnimation(dom::Element* aElement,
                                          CSSPseudoElementType aPseudoType,
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
EffectCompositor::UpdateEffectProperties(nsStyleContext* aStyleContext,
                                         dom::Element* aElement,
                                         CSSPseudoElementType aPseudoType)
{
  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effectSet) {
    return;
  }

  // Style context change might cause CSS cascade level,
  // e.g removing !important, so we should update the cascading result.
  effectSet->MarkCascadeNeedsUpdate();

  for (KeyframeEffectReadOnly* effect : *effectSet) {
    effect->UpdateProperties(aStyleContext);
  }
}

void
EffectCompositor::MaybeUpdateAnimationRule(dom::Element* aElement,
                                           CSSPseudoElementType aPseudoType,
                                           CascadeLevel aCascadeLevel,
                                           nsStyleContext* aStyleContext)
{
  // First update cascade results since that may cause some elements to
  // be marked as needing a restyle.
  MaybeUpdateCascadeResults(aElement, aPseudoType, aStyleContext);

  auto& elementsToRestyle = mElementsToRestyle[aCascadeLevel];
  PseudoElementHashEntry::KeyType key = { aElement, aPseudoType };

  if (!mPresContext || !elementsToRestyle.Contains(key)) {
    return;
  }

  ComposeAnimationRule(aElement, aPseudoType, aCascadeLevel,
                       mPresContext->RefreshDriver()->MostRecentRefresh());

  elementsToRestyle.Remove(key);
}

nsIStyleRule*
EffectCompositor::GetAnimationRule(dom::Element* aElement,
                                   CSSPseudoElementType aPseudoType,
                                   CascadeLevel aCascadeLevel,
                                   nsStyleContext* aStyleContext)
{
  // NOTE: We need to be careful about early returns in this method where
  // we *don't* update mElementsToRestyle. When we get a call to
  // RequestRestyle that results in a call to PostRestyleForAnimation, we
  // will set a bool flag in mElementsToRestyle indicating that we've
  // called PostRestyleForAnimation so we don't need to call it again
  // until that restyle happens. During that restyle, if we arrive here
  // and *don't* update mElementsToRestyle we'll continue to skip calling
  // PostRestyleForAnimation from RequestRestyle.

  if (!mPresContext || !mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  MOZ_ASSERT(mPresContext->RestyleManager()->IsGecko(),
             "stylo: Servo-backed style system should not be using "
             "EffectCompositor");
  if (mPresContext->RestyleManager()->AsGecko()->SkipAnimationRules()) {
    // We don't need to worry about updating mElementsToRestyle in this case
    // since this is not the animation restyle we requested when we called
    // PostRestyleForAnimation (see comment at start of this method).
    return nullptr;
  }

  MaybeUpdateAnimationRule(aElement, aPseudoType, aCascadeLevel, aStyleContext);

#ifdef DEBUG
  {
    auto& elementsToRestyle = mElementsToRestyle[aCascadeLevel];
    PseudoElementHashEntry::KeyType key = { aElement, aPseudoType };
    MOZ_ASSERT(!elementsToRestyle.Contains(key),
               "Element should no longer require a restyle after its "
               "animation rule has been updated");
  }
#endif

  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effectSet) {
    return nullptr;
  }

  return effectSet->AnimationRule(aCascadeLevel);
}

/* static */ dom::Element*
EffectCompositor::GetElementToRestyle(dom::Element* aElement,
                                      CSSPseudoElementType aPseudoType)
{
  if (aPseudoType == CSSPseudoElementType::NotPseudo) {
    return aElement;
  }

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  if (!primaryFrame) {
    return nullptr;
  }
  nsIFrame* pseudoFrame;
  if (aPseudoType == CSSPseudoElementType::before) {
    pseudoFrame = nsLayoutUtils::GetBeforeFrame(primaryFrame);
  } else if (aPseudoType == CSSPseudoElementType::after) {
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
    nsTArray<PseudoElementHashEntry::KeyType> elementsToRestyle(
      elementSet.Count());
    for (auto iter = elementSet.Iter(); !iter.Done(); iter.Next()) {
      elementsToRestyle.AppendElement(iter.Key());
    }

    for (auto& pseudoElem : elementsToRestyle) {
      MaybeUpdateCascadeResults(pseudoElem.mElement,
                                pseudoElem.mPseudoType,
                                nullptr);

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
EffectCompositor::ClearIsRunningOnCompositor(const nsIFrame *aFrame,
                                             nsCSSProperty aProperty)
{
  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects) {
    return;
  }

  for (KeyframeEffectReadOnly* effect : *effects) {
    effect->SetIsRunningOnCompositor(aProperty, false);
  }
}

/* static */ void
EffectCompositor::MaybeUpdateCascadeResults(Element* aElement,
                                            CSSPseudoElementType aPseudoType,
                                            nsStyleContext* aStyleContext)
{
  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects || !effects->CascadeNeedsUpdate()) {
    return;
  }

  nsStyleContext* styleContext = aStyleContext;
  if (!styleContext) {
    dom::Element* elementToRestyle = GetElementToRestyle(aElement, aPseudoType);
    if (elementToRestyle) {
      nsIFrame* frame = elementToRestyle->GetPrimaryFrame();
      if (frame) {
        styleContext = frame->StyleContext();
      }
    }
  }
  UpdateCascadeResults(*effects, aElement, aPseudoType, styleContext);

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
                                       CSSPseudoElementType aPseudoType,
                                       nsStyleContext* aStyleContext)
{
  EffectSet* effects = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effects) {
    return;
  }

  UpdateCascadeResults(*effects, aElement, aPseudoType, aStyleContext);
}

/* static */ Maybe<NonOwningAnimationTarget>
EffectCompositor::GetAnimationElementAndPseudoForFrame(const nsIFrame* aFrame)
{
  // Always return the same object to benefit from return-value optimization.
  Maybe<NonOwningAnimationTarget> result;

  CSSPseudoElementType pseudoType =
    aFrame->StyleContext()->GetPseudoType();

  if (pseudoType != CSSPseudoElementType::NotPseudo &&
      pseudoType != CSSPseudoElementType::before &&
      pseudoType != CSSPseudoElementType::after) {
    return result;
  }

  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return result;
  }

  if (pseudoType == CSSPseudoElementType::before ||
      pseudoType == CSSPseudoElementType::after) {
    content = content->GetParent();
    if (!content) {
      return result;
    }
  }

  if (!content->IsElement()) {
    return result;
  }

  result.emplace(content->AsElement(), pseudoType);

  return result;
}

/* static */ void
EffectCompositor::ComposeAnimationRule(dom::Element* aElement,
                                       CSSPseudoElementType aPseudoType,
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

  MOZ_ASSERT(effects == EffectSet::GetEffectSet(aElement, aPseudoType),
             "EffectSet should not change while composing style");

  effects->UpdateAnimationRuleRefreshTime(aCascadeLevel, aRefreshTime);
}

/* static */ void
EffectCompositor::GetOverriddenProperties(nsStyleContext* aStyleContext,
                                          EffectSet& aEffectSet,
                                          nsCSSPropertySet&
                                            aPropertiesOverridden)
{
  AutoTArray<nsCSSProperty, LayerAnimationInfo::kRecords> propertiesToTrack;
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
                                       CSSPseudoElementType aPseudoType,
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

/* static */ void
EffectCompositor::SetPerformanceWarning(
  const nsIFrame *aFrame,
  nsCSSProperty aProperty,
  const AnimationPerformanceWarning& aWarning)
{
  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects) {
    return;
  }

  for (KeyframeEffectReadOnly* effect : *effects) {
    effect->SetPerformanceWarning(aProperty, aWarning);
  }
}

// ---------------------------------------------------------
//
// Nested class: AnimationStyleRuleProcessor
//
// ---------------------------------------------------------

NS_IMPL_ISUPPORTS(EffectCompositor::AnimationStyleRuleProcessor,
                  nsIStyleRuleProcessor)

nsRestyleHint
EffectCompositor::AnimationStyleRuleProcessor::HasStateDependentStyle(
  StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

nsRestyleHint
EffectCompositor::AnimationStyleRuleProcessor::HasStateDependentStyle(
  PseudoElementStateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

bool
EffectCompositor::AnimationStyleRuleProcessor::HasDocumentStateDependentStyle(
  StateRuleProcessorData* aData)
{
  return false;
}

nsRestyleHint
EffectCompositor::AnimationStyleRuleProcessor::HasAttributeDependentStyle(
                        AttributeRuleProcessorData* aData,
                        RestyleHintData& aRestyleHintDataResult)
{
  return nsRestyleHint(0);
}

bool
EffectCompositor::AnimationStyleRuleProcessor::MediumFeaturesChanged(
  nsPresContext* aPresContext)
{
  return false;
}

void
EffectCompositor::AnimationStyleRuleProcessor::RulesMatching(
  ElementRuleProcessorData* aData)
{
  nsIStyleRule *rule =
    mCompositor->GetAnimationRule(aData->mElement,
                                  CSSPseudoElementType::NotPseudo,
                                  mCascadeLevel,
                                  nullptr);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
    aData->mRuleWalker->CurrentNode()->SetIsAnimationRule();
  }
}

void
EffectCompositor::AnimationStyleRuleProcessor::RulesMatching(
  PseudoElementRuleProcessorData* aData)
{
  if (aData->mPseudoType != CSSPseudoElementType::before &&
      aData->mPseudoType != CSSPseudoElementType::after) {
    return;
  }

  nsIStyleRule *rule =
    mCompositor->GetAnimationRule(aData->mElement,
                                  aData->mPseudoType,
                                  mCascadeLevel,
                                  nullptr);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
    aData->mRuleWalker->CurrentNode()->SetIsAnimationRule();
  }
}

void
EffectCompositor::AnimationStyleRuleProcessor::RulesMatching(
  AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
void
EffectCompositor::AnimationStyleRuleProcessor::RulesMatching(
  XULTreeRuleProcessorData* aData)
{
}
#endif

size_t
EffectCompositor::AnimationStyleRuleProcessor::SizeOfExcludingThis(
  MallocSizeOf aMallocSizeOf) const
{
  return 0;
}

size_t
EffectCompositor::AnimationStyleRuleProcessor::SizeOfIncludingThis(
  MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace mozilla
