/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationCommon.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"

#include "ActiveLayerTracker.h"
#include "gfxPlatform.h"
#include "nsCSSPropertySet.h"
#include "nsCSSValue.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMMutationObserver.h"
#include "nsStyleContext.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "FrameLayerBuilder.h"
#include "nsDisplayList.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "RestyleManager.h"
#include "nsRuleProcessorData.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"

using mozilla::dom::Animation;
using mozilla::dom::KeyframeEffectReadOnly;

namespace mozilla {

CommonAnimationManager::CommonAnimationManager(nsPresContext *aPresContext)
  : mPresContext(aPresContext)
{
}

CommonAnimationManager::~CommonAnimationManager()
{
  MOZ_ASSERT(!mPresContext, "Disconnect should have been called");
}

void
CommonAnimationManager::Disconnect()
{
  // Content nodes might outlive the transition or animation manager.
  RemoveAllElementCollections();

  mPresContext = nullptr;
}

void
CommonAnimationManager::AddElementCollection(AnimationCollection* aCollection)
{
  mElementCollections.insertBack(aCollection);
}

void
CommonAnimationManager::RemoveAllElementCollections()
{
 while (AnimationCollection* head = mElementCollections.getFirst()) {
   head->Destroy(); // Note: this removes 'head' from mElementCollections.
 }
}

AnimationCollection*
CommonAnimationManager::GetAnimationCollection(dom::Element *aElement,
                                               nsCSSPseudoElements::Type
                                                 aPseudoType,
                                               bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && mElementCollections.isEmpty()) {
    // Early return for the most common case.
    return nullptr;
  }

  nsIAtom *propName;
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    propName = GetAnimationsAtom();
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_before) {
    propName = GetAnimationsBeforeAtom();
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_after) {
    propName = GetAnimationsAfterAtom();
  } else {
    NS_ASSERTION(!aCreateIfNeeded,
                 "should never try to create transitions for pseudo "
                 "other than :before or :after");
    return nullptr;
  }
  AnimationCollection* collection =
    static_cast<AnimationCollection*>(aElement->GetProperty(propName));
  if (!collection && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    collection = new AnimationCollection(aElement, propName, this);
    nsresult rv =
      aElement->SetProperty(propName, collection,
                            &AnimationCollection::PropertyDtor, false);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      // The collection must be destroyed via PropertyDtor, otherwise
      // mCalledPropertyDtor assertion is triggered in destructor.
      AnimationCollection::PropertyDtor(aElement, propName, collection, nullptr);
      return nullptr;
    }
    if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
      aElement->SetMayHaveAnimations();
    }

    AddElementCollection(collection);
  }

  return collection;
}

AnimationCollection*
CommonAnimationManager::GetAnimationCollection(const nsIFrame* aFrame)
{
  Maybe<Pair<dom::Element*, nsCSSPseudoElements::Type>> pseudoElement =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (!pseudoElement) {
    return nullptr;
  }

  if (pseudoElement->second() ==
        nsCSSPseudoElements::ePseudo_NotPseudoElement &&
      !pseudoElement->first()->MayHaveAnimations()) {
    return nullptr;
  }

  return GetAnimationCollection(pseudoElement->first(),
                                pseudoElement->second(),
                                false /* aCreateIfNeeded */);
}

nsRestyleHint
CommonAnimationManager::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

nsRestyleHint
CommonAnimationManager::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

bool
CommonAnimationManager::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

nsRestyleHint
CommonAnimationManager::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
CommonAnimationManager::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ void
CommonAnimationManager::RulesMatching(ElementRuleProcessorData* aData)
{
  MOZ_ASSERT(aData->mPresContext == mPresContext,
             "pres context mismatch");
  nsIStyleRule *rule =
    GetAnimationRule(aData->mElement,
                     nsCSSPseudoElements::ePseudo_NotPseudoElement);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
    aData->mRuleWalker->CurrentNode()->SetIsAnimationRule();
  }
}

/* virtual */ void
CommonAnimationManager::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  MOZ_ASSERT(aData->mPresContext == mPresContext,
             "pres context mismatch");
  if (aData->mPseudoType != nsCSSPseudoElements::ePseudo_before &&
      aData->mPseudoType != nsCSSPseudoElements::ePseudo_after) {
    return;
  }

  // FIXME: Do we really want to be the only thing keeping a
  // pseudo-element alive?  I *think* the non-animation restyle should
  // handle that, but should add a test.
  nsIStyleRule *rule = GetAnimationRule(aData->mElement, aData->mPseudoType);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
    aData->mRuleWalker->CurrentNode()->SetIsAnimationRule();
  }
}

/* virtual */ void
CommonAnimationManager::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
CommonAnimationManager::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

/* virtual */ size_t
CommonAnimationManager::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mElementCollections
  //
  // The following members are not measured
  // - mPresContext, because it's non-owning

  return 0;
}

/* virtual */ size_t
CommonAnimationManager::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
CommonAnimationManager::AddStyleUpdatesTo(RestyleTracker& aTracker)
{
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();

  for (AnimationCollection* collection = mElementCollections.getFirst();
       collection; collection = collection->getNext()) {
    collection->EnsureStyleRuleFor(now);

    dom::Element* elementToRestyle = collection->GetElementToRestyle();
    if (elementToRestyle) {
      nsRestyleHint rshint = collection->IsForTransitions()
        ? eRestyle_CSSTransitions : eRestyle_CSSAnimations;
      aTracker.AddPendingRestyle(elementToRestyle, rshint, nsChangeHint(0));
    }
  }
}

/* static */ bool
CommonAnimationManager::ExtractComputedValueForTransition(
                          nsCSSProperty aProperty,
                          nsStyleContext* aStyleContext,
                          StyleAnimationValue& aComputedValue)
{
  bool result = StyleAnimationValue::ExtractComputedValue(aProperty,
                                                          aStyleContext,
                                                          aComputedValue);
  if (aProperty == eCSSProperty_visibility) {
    MOZ_ASSERT(aComputedValue.GetUnit() ==
                 StyleAnimationValue::eUnit_Enumerated,
               "unexpected unit");
    aComputedValue.SetIntValue(aComputedValue.GetIntValue(),
                               StyleAnimationValue::eUnit_Visibility);
  }
  return result;
}

void
CommonAnimationManager::FlushAnimations()
{
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  for (AnimationCollection* collection = mElementCollections.getFirst();
       collection; collection = collection->getNext()) {
    if (collection->mStyleRuleRefreshTime == now) {
      continue;
    }

    MOZ_ASSERT(collection->mElement->GetComposedDoc() ==
                 mPresContext->Document(),
               "Should not have a transition/animation collection for an "
               "element that is not part of the document tree");

    collection->RequestRestyle(EffectCompositor::RestyleType::Standard);
  }
}

nsIStyleRule*
CommonAnimationManager::GetAnimationRule(mozilla::dom::Element* aElement,
                                         nsCSSPseudoElements::Type aPseudoType)
{
  MOZ_ASSERT(
    aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
    aPseudoType == nsCSSPseudoElements::ePseudo_before ||
    aPseudoType == nsCSSPseudoElements::ePseudo_after,
    "forbidden pseudo type");

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  AnimationCollection* collection =
    GetAnimationCollection(aElement, aPseudoType, false /* aCreateIfNeeded */);
  if (!collection) {
    return nullptr;
  }

  RestyleManager* restyleManager = mPresContext->RestyleManager();
  if (restyleManager->SkipAnimationRules()) {
    return nullptr;
  }

  collection->EnsureStyleRuleFor(
    mPresContext->RefreshDriver()->MostRecentRefresh());

  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (!effectSet) {
    return nullptr;
  }

  return IsAnimationManager() ?
         effectSet->AnimationRule(EffectCompositor::CascadeLevel::Animations) :
         effectSet->AnimationRule(EffectCompositor::CascadeLevel::Transitions);
}

void
CommonAnimationManager::ClearIsRunningOnCompositor(const nsIFrame* aFrame,
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

/*static*/ nsString
AnimationCollection::PseudoTypeAsString(nsCSSPseudoElements::Type aPseudoType)
{
  switch (aPseudoType) {
    case nsCSSPseudoElements::ePseudo_before:
      return NS_LITERAL_STRING("::before");
    case nsCSSPseudoElements::ePseudo_after:
      return NS_LITERAL_STRING("::after");
    default:
      MOZ_ASSERT(aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement,
                 "Unexpected pseudo type");
      return EmptyString();
  }
}

mozilla::dom::Element*
AnimationCollection::GetElementToRestyle() const
{
  if (IsForElement()) {
    return mElement;
  }

  nsIFrame* primaryFrame = mElement->GetPrimaryFrame();
  if (!primaryFrame) {
    return nullptr;
  }
  nsIFrame* pseudoFrame;
  if (IsForBeforePseudo()) {
    pseudoFrame = nsLayoutUtils::GetBeforeFrame(primaryFrame);
  } else if (IsForAfterPseudo()) {
    pseudoFrame = nsLayoutUtils::GetAfterFrame(primaryFrame);
  } else {
    MOZ_ASSERT(false, "unknown mElementProperty");
    return nullptr;
  }
  if (!pseudoFrame) {
    return nullptr;
  }
  return pseudoFrame->GetContent()->AsElement();
}

/*static*/ void
AnimationCollection::PropertyDtor(void *aObject, nsIAtom *aPropertyName,
                                  void *aPropertyValue, void *aData)
{
  AnimationCollection* collection =
    static_cast<AnimationCollection*>(aPropertyValue);
#ifdef DEBUG
  MOZ_ASSERT(!collection->mCalledPropertyDtor, "can't call dtor twice");
  collection->mCalledPropertyDtor = true;
#endif
  {
    nsAutoAnimationMutationBatch mb(collection->mElement->OwnerDoc());

    for (size_t animIdx = collection->mAnimations.Length(); animIdx-- != 0; ) {
      collection->mAnimations[animIdx]->CancelFromStyle();
    }
  }
  delete collection;
}

void
AnimationCollection::Tick()
{
  for (size_t animIdx = 0, animEnd = mAnimations.Length();
       animIdx != animEnd; animIdx++) {
    mAnimations[animIdx]->Tick();
  }
}

void
AnimationCollection::EnsureStyleRuleFor(TimeStamp aRefreshTime)
{
  mHasPendingAnimationRestyle = false;

  nsPresContext* presContext = mManager->PresContext();
  if (!presContext) {
    // Pres context will be null after the manager is disconnected.
    return;
  }

  if (!mStyleChanging) {
    mStyleRuleRefreshTime = aRefreshTime;
    return;
  }

  if (!mStyleRuleRefreshTime.IsNull() &&
      mStyleRuleRefreshTime == aRefreshTime) {
    // The style rule on the EffectSet may be null and valid, if we have no
    // style to apply.
    return;
  }

  // Update cascade results before updating the style rule, since the
  // cascade results can influence the style rule.
  nsStyleContext* styleContext = nullptr;
  {
    dom::Element* elementToRestyle = GetElementToRestyle();
    if (elementToRestyle) {
      nsIFrame* frame = elementToRestyle->GetPrimaryFrame();
      if (frame) {
        styleContext = frame->StyleContext();
      }
    }
  }
  EffectCompositor::MaybeUpdateCascadeResults(mElement,
                                              PseudoElementType(),
                                              styleContext);

  mStyleRuleRefreshTime = aRefreshTime;

  EffectCompositor::CascadeLevel cascadeLevel =
    IsForAnimations() ?
    EffectCompositor::CascadeLevel::Animations :
    EffectCompositor::CascadeLevel::Transitions;
  presContext->EffectCompositor()->MaybeUpdateAnimationRule(mElement,
                                                            PseudoElementType(),
                                                            cascadeLevel,
                                                            mStyleChanging);
}

void
AnimationCollection::RequestRestyle(EffectCompositor::RestyleType aRestyleType)
{
  MOZ_ASSERT(IsForElement() || IsForBeforePseudo() || IsForAfterPseudo(),
             "Unexpected mElementProperty; might restyle too much");

  nsPresContext* presContext = mManager->PresContext();
  if (!presContext) {
    // Pres context will be null after the manager is disconnected.
    return;
  }

  EffectCompositor::CascadeLevel cascadeLevel =
    IsForAnimations() ?
    EffectCompositor::CascadeLevel::Animations :
    EffectCompositor::CascadeLevel::Transitions;
  presContext->EffectCompositor()->RequestRestyle(mElement,
                                                  PseudoElementType(),
                                                  aRestyleType,
                                                  cascadeLevel);

  // Steps for RestyleType::Layer:

  if (aRestyleType == EffectCompositor::RestyleType::Layer) {
    mStyleRuleRefreshTime = TimeStamp();
    mStyleChanging = true;

    // Prompt layers to re-sync their animations.
    presContext->ClearLastStyleUpdateForAllAnimations();
    presContext->RestyleManager()->IncrementAnimationGeneration();
    EffectSet* effectSet =
      EffectSet::GetEffectSet(mElement, PseudoElementType());
    if (effectSet) {
      effectSet->UpdateAnimationGeneration(presContext);
    }
  }

  // Steps for RestyleType::Standard and above:

  // FIXME: The following arrangement now makes absolutely no sense, but
  // is also harmless and in the interests of making incremental changes
  // we leave mHasPendingAnimationRestyle here for now and remove it in a
  // subsequent patch in this series.
  if (mHasPendingAnimationRestyle) {
    return;
  }

  if (aRestyleType >= EffectCompositor::RestyleType::Standard) {
    mHasPendingAnimationRestyle = true;
  }
}

void
AnimationCollection::UpdateCheckGeneration(
  nsPresContext* aPresContext)
{
  mCheckGeneration = aPresContext->RestyleManager()->GetAnimationGeneration();
}

nsPresContext*
OwningElementRef::GetRenderedPresContext() const
{
  if (!mElement) {
    return nullptr;
  }

  nsIDocument* doc = mElement->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    return nullptr;
  }

  return shell->GetPresContext();
}

} // namespace mozilla
