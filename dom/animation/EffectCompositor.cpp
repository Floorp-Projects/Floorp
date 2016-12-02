/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectCompositor.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectSet.h"
#include "mozilla/LayerAnimationInfo.h"
#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "mozilla/StyleAnimationValue.h"
#include "nsComputedDOMStyle.h" // nsComputedDOMStyle::GetPresShellForContent
#include "nsCSSPropertyIDSet.h"
#include "nsCSSProps.h"
#include "nsIPresShell.h"
#include "nsLayoutUtils.h"
#include "nsRuleNode.h" // For nsRuleNode::ComputePropertiesOverridingAnimation
#include "nsRuleProcessorData.h" // For ElementRuleProcessorData etc.
#include "nsTArray.h"
#include <bitset>
#include <initializer_list>

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

namespace {
enum class MatchForCompositor {
  // This animation matches and should run on the compositor if possible.
  Yes,
  // This (not currently playing) animation matches and can be run on the
  // compositor if there are other animations for this property that return
  // 'Yes'.
  IfNeeded,
  // This animation does not match or can't be run on the compositor.
  No,
  // This animation does not match or can't be run on the compositor and,
  // furthermore, its presence means we should not run any animations for this
  // property on the compositor.
  NoAndBlockThisProperty
};
}

static MatchForCompositor
IsMatchForCompositor(const KeyframeEffectReadOnly& aEffect,
                     nsCSSPropertyID aProperty,
                     const nsIFrame* aFrame)
{
  const Animation* animation = aEffect.GetAnimation();
  MOZ_ASSERT(animation);

  if (!animation->IsRelevant()) {
    return MatchForCompositor::No;
  }

  bool isPlaying = animation->IsPlaying();

  // If we are finding animations for transform, check if there are other
  // animations that should block the transform animation. e.g. geometric
  // properties' animation. This check should be done regardless of whether
  // the effect has the target property |aProperty| or not.
  AnimationPerformanceWarning::Type warningType;
  if (aProperty == eCSSProperty_transform &&
      isPlaying &&
      aEffect.ShouldBlockAsyncTransformAnimations(aFrame, warningType)) {
    EffectCompositor::SetPerformanceWarning(
      aFrame, aProperty,
      AnimationPerformanceWarning(warningType));
    return MatchForCompositor::NoAndBlockThisProperty;
  }

  if (!aEffect.HasEffectiveAnimationOfProperty(aProperty)) {
    return MatchForCompositor::No;
  }

  return isPlaying ? MatchForCompositor::Yes : MatchForCompositor::IfNeeded;
}

// Helper function to factor out the common logic from
// GetAnimationsForCompositor and HasAnimationsForCompositor.
//
// Takes an optional array to fill with eligible animations.
//
// Returns true if there are eligible animations, false otherwise.
bool
FindAnimationsForCompositor(const nsIFrame* aFrame,
                            nsCSSPropertyID aProperty,
                            nsTArray<RefPtr<dom::Animation>>* aMatches /*out*/)
{
  MOZ_ASSERT(!aMatches || aMatches->IsEmpty(),
             "Matches array, if provided, should be empty");

  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects || effects->IsEmpty()) {
    return false;
  }

  // If the property will be added to the animations level of the cascade but
  // there is an !important rule for that property in the cascade then the
  // animation will not be applied since the !important rule overrides it.
  if (effects->PropertiesWithImportantRules().HasProperty(aProperty) &&
      effects->PropertiesForAnimationsLevel().HasProperty(aProperty)) {
    return false;
  }

  if (aFrame->RefusedAsyncAnimation()) {
    return false;
  }

  if (aFrame->StyleContext()->StyleSource().IsServoComputedValues()) {
    NS_ERROR("stylo: cannot handle compositor-driven animations yet");
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

  bool foundRunningAnimations = false;
  for (KeyframeEffectReadOnly* effect : *effects) {
    MatchForCompositor matchResult =
      IsMatchForCompositor(*effect, aProperty, aFrame);

    if (matchResult == MatchForCompositor::NoAndBlockThisProperty) {
      if (aMatches) {
        aMatches->Clear();
      }
      return false;
    }

    if (matchResult == MatchForCompositor::No) {
      continue;
    }

    if (aMatches) {
      aMatches->AppendElement(effect->GetAnimation());
    }

    if (matchResult == MatchForCompositor::Yes) {
      foundRunningAnimations = true;
    }
  }

  // If all animations we added were not currently playing animations, don't
  // send them to the compositor.
  if (aMatches && !foundRunningAnimations) {
    aMatches->Clear();
  }

  MOZ_ASSERT(!foundRunningAnimations || !aMatches || !aMatches->IsEmpty(),
             "If return value is true, matches array should be non-empty");

  if (aMatches && foundRunningAnimations) {
    aMatches->Sort(AnimationPtrComparator<RefPtr<dom::Animation>>());
  }

  return foundRunningAnimations;
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

  // Ignore animations on orphaned elements.
  if (!aElement->IsInComposedDoc()) {
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
    if (mPresContext->RestyleManager()->IsServo()) {
      NS_ERROR("stylo: Servo-backed style system should not be using "
               "EffectCompositor");
      return;
    }
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
      // Skip animations on elements that have been orphaned since they
      // requested a restyle.
      if (iter.Key().mElement->IsInComposedDoc()) {
        elementsToRestyle.AppendElement(iter.Key());
      }
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
                                             nsCSSPropertyID aProperty)
{
  return FindAnimationsForCompositor(aFrame, aProperty, nullptr);
}

/* static */ nsTArray<RefPtr<dom::Animation>>
EffectCompositor::GetAnimationsForCompositor(const nsIFrame* aFrame,
                                             nsCSSPropertyID aProperty)
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
                                             nsCSSPropertyID aProperty)
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

  // Get a list of effects sorted by composite order.
  nsTArray<KeyframeEffectReadOnly*> sortedEffectList(effects->Count());
  for (KeyframeEffectReadOnly* effect : *effects) {
    sortedEffectList.AppendElement(effect);
  }
  sortedEffectList.Sort(EffectCompositeOrderComparator());

  RefPtr<AnimValuesStyleRule>& animationRule =
    effects->AnimationRule(aCascadeLevel);
  animationRule = nullptr;

  // If multiple animations affect the same property, animations with higher
  // composite order (priority) override or add or animations with lower
  // priority except properties in propertiesToSkip.
  const nsCSSPropertyIDSet& propertiesToSkip =
    aCascadeLevel == CascadeLevel::Animations
    ? effects->PropertiesForAnimationsLevel().Invert()
    : effects->PropertiesForAnimationsLevel();
  for (KeyframeEffectReadOnly* effect : sortedEffectList) {
    effect->GetAnimation()->ComposeStyle(animationRule, propertiesToSkip);
  }

  MOZ_ASSERT(effects == EffectSet::GetEffectSet(aElement, aPseudoType),
             "EffectSet should not change while composing style");

  effects->UpdateAnimationRuleRefreshTime(aCascadeLevel, aRefreshTime);
}

/* static */ void
EffectCompositor::GetOverriddenProperties(nsStyleContext* aStyleContext,
                                          EffectSet& aEffectSet,
                                          nsCSSPropertyIDSet&
                                            aPropertiesOverridden)
{
  AutoTArray<nsCSSPropertyID, LayerAnimationInfo::kRecords> propertiesToTrack;
  {
    nsCSSPropertyIDSet propertiesToTrackAsSet;
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
  nsTArray<KeyframeEffectReadOnly*> sortedEffectList(aEffectSet.Count());
  for (KeyframeEffectReadOnly* effect : aEffectSet) {
    sortedEffectList.AppendElement(effect);
  }
  sortedEffectList.Sort(EffectCompositeOrderComparator());

  // Get properties that override the *animations* level of the cascade.
  //
  // We only do this for properties that we can animate on the compositor
  // since we will apply other properties on the main thread where the usual
  // cascade applies.
  nsCSSPropertyIDSet overriddenProperties;
  if (aStyleContext) {
    GetOverriddenProperties(aStyleContext, aEffectSet, overriddenProperties);
  }

  // Returns a bitset the represents which properties from
  // LayerAnimationInfo::sRecords are present in |aPropertySet|.
  auto compositorPropertiesInSet =
    [](nsCSSPropertyIDSet& aPropertySet) ->
      std::bitset<LayerAnimationInfo::kRecords> {
        std::bitset<LayerAnimationInfo::kRecords> result;
        for (size_t i = 0; i < LayerAnimationInfo::kRecords; i++) {
          if (aPropertySet.HasProperty(
                LayerAnimationInfo::sRecords[i].mProperty)) {
            result.set(i);
          }
        }
      return result;
    };

  nsCSSPropertyIDSet& propertiesWithImportantRules =
    aEffectSet.PropertiesWithImportantRules();
  nsCSSPropertyIDSet& propertiesForAnimationsLevel =
    aEffectSet.PropertiesForAnimationsLevel();

  // Record which compositor-animatable properties were originally set so we can
  // compare for changes later.
  std::bitset<LayerAnimationInfo::kRecords>
    prevCompositorPropertiesWithImportantRules =
      compositorPropertiesInSet(propertiesWithImportantRules);
  std::bitset<LayerAnimationInfo::kRecords>
    prevCompositorPropertiesForAnimationsLevel =
      compositorPropertiesInSet(propertiesForAnimationsLevel);

  propertiesWithImportantRules.Empty();
  propertiesForAnimationsLevel.Empty();

  bool hasCompositorPropertiesForTransition = false;

  for (const KeyframeEffectReadOnly* effect : sortedEffectList) {
    MOZ_ASSERT(effect->GetAnimation(),
               "Effects on a target element should have an Animation");
    CascadeLevel cascadeLevel = effect->GetAnimation()->CascadeLevel();

    for (const AnimationProperty& prop : effect->Properties()) {
      if (overriddenProperties.HasProperty(prop.mProperty)) {
        propertiesWithImportantRules.AddProperty(prop.mProperty);
      }
      if (cascadeLevel == EffectCompositor::CascadeLevel::Animations) {
        propertiesForAnimationsLevel.AddProperty(prop.mProperty);
      }

      if (nsCSSProps::PropHasFlags(prop.mProperty,
                                   CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR) &&
          cascadeLevel == EffectCompositor::CascadeLevel::Transitions) {
        hasCompositorPropertiesForTransition = true;
      }
    }
  }

  aEffectSet.MarkCascadeUpdated();

  nsPresContext* presContext = GetPresContext(aElement);
  if (!presContext) {
    return;
  }

  // If properties for compositor are newly overridden by !important rules, or
  // released from being overridden by !important rules, we need to update
  // layers for animations level because it's a trigger to send animations to
  // the compositor or pull animations back from the compositor.
  if (prevCompositorPropertiesWithImportantRules !=
        compositorPropertiesInSet(propertiesWithImportantRules)) {
    presContext->EffectCompositor()->
      RequestRestyle(aElement, aPseudoType,
                     EffectCompositor::RestyleType::Layer,
                     EffectCompositor::CascadeLevel::Animations);
  }
  // If we have transition properties for compositor and if the same propery
  // for animations level is newly added or removed, we need to update layers
  // for transitions level because composite order has been changed now.
  if (hasCompositorPropertiesForTransition &&
      prevCompositorPropertiesForAnimationsLevel !=
        compositorPropertiesInSet(propertiesForAnimationsLevel)) {
    presContext->EffectCompositor()->
      RequestRestyle(aElement, aPseudoType,
                     EffectCompositor::RestyleType::Layer,
                     EffectCompositor::CascadeLevel::Transitions);
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
  nsCSSPropertyID aProperty,
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

/* static */ StyleAnimationValue
EffectCompositor::GetBaseStyle(nsCSSPropertyID aProperty,
                               nsStyleContext* aStyleContext,
                               dom::Element& aElement)
{
  MOZ_ASSERT(aStyleContext, "Need style context to resolve the base value");
  MOZ_ASSERT(!aStyleContext->StyleSource().IsServoComputedValues(),
             "Bug 1311257: Servo backend does not support the base value yet");

  RefPtr<nsStyleContext> styleContextWithoutAnimation =
    aStyleContext->PresContext()->StyleSet()->AsGecko()->
      ResolveStyleWithoutAnimation(&aElement, aStyleContext,
                                   eRestyle_AllHintsWithAnimations);

  StyleAnimationValue baseStyle;
  DebugOnly<bool> result =
    StyleAnimationValue::ExtractComputedValue(aProperty,
                                              styleContextWithoutAnimation,
                                              baseStyle);
  MOZ_ASSERT(result, "could not extract computed value");

  return baseStyle;
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
