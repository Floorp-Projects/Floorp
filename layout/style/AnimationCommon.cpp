/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationCommon.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"

#include "ActiveLayerTracker.h"
#include "gfxPlatform.h"
#include "nsRuleData.h"
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
CommonAnimationManager::GetAnimationCollection(const nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return nullptr;
  }
  nsIAtom* animProp;
  if (aFrame->IsGeneratedContentFrame()) {
    nsIFrame* parent = aFrame->GetParent();
    if (parent->IsGeneratedContentFrame()) {
      return nullptr;
    }
    nsIAtom* name = content->NodeInfo()->NameAtom();
    if (name == nsGkAtoms::mozgeneratedcontentbefore) {
      animProp = GetAnimationsBeforeAtom();
    } else if (name == nsGkAtoms::mozgeneratedcontentafter) {
      animProp = GetAnimationsAfterAtom();
    } else {
      return nullptr;
    }
    content = content->GetParent();
    if (!content) {
      return nullptr;
    }
  } else {
    if (!content->MayHaveAnimations()) {
      return nullptr;
    }
    animProp = GetAnimationsAtom();
  }

  return static_cast<AnimationCollection*>(content->GetProperty(animProp));
}

AnimationCollection*
CommonAnimationManager::GetAnimationsForCompositor(const nsIFrame* aFrame,
                                                   nsCSSProperty aProperty)
{
  AnimationCollection* collection = GetAnimationCollection(aFrame);
  if (!collection ||
      !collection->HasCurrentAnimationOfProperty(aProperty) ||
      !collection->CanPerformOnCompositorThread(aFrame)) {
    return nullptr;
  }

  // This animation can be done on the compositor.
  return collection;
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

AnimationCollection*
CommonAnimationManager::GetAnimations(dom::Element *aElement,
                                      nsCSSPseudoElements::Type aPseudoType,
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

    collection->RequestRestyle(AnimationCollection::RestyleType::Standard);
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
    GetAnimations(aElement, aPseudoType, false);
  if (!collection) {
    return nullptr;
  }

  RestyleManager* restyleManager = mPresContext->RestyleManager();
  if (restyleManager->SkipAnimationRules()) {
    return nullptr;
  }

  collection->EnsureStyleRuleFor(
    mPresContext->RefreshDriver()->MostRecentRefresh());

  return collection->mStyleRule;
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

NS_IMPL_ISUPPORTS(AnimValuesStyleRule, nsIStyleRule)

/* virtual */ void
AnimValuesStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  nsStyleContext *contextParent = aRuleData->mStyleContext->GetParent();
  if (contextParent && contextParent->HasPseudoElementData()) {
    // Don't apply transitions or animations to things inside of
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.

    // Prevent structs from being cached on the rule node since we're inside
    // a pseudo-element, as we could determine cacheability differently
    // when walking the rule tree for a style context that is not inside
    // a pseudo-element.  Note that nsRuleNode::GetStyle##name_ and GetStyleData
    // will never look at cached structs when we're animating things inside
    // a pseduo-element, so that we don't incorrectly return a struct that
    // is only appropriate for non-pseudo-elements.
    aRuleData->mConditions.SetUncacheable();
    return;
  }

  for (uint32_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
    PropertyValuePair &cv = mPropertyValuePairs[i];
    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[cv.mProperty]))
    {
      nsCSSValue *prop = aRuleData->ValueFor(cv.mProperty);
      if (prop->GetUnit() == eCSSUnit_Null) {
#ifdef DEBUG
        bool ok =
#endif
          StyleAnimationValue::UncomputeValue(cv.mProperty, cv.mValue, *prop);
        MOZ_ASSERT(ok, "could not store computed value");
      }
    }
  }
}

/* virtual */ bool
AnimValuesStyleRule::MightMapInheritedStyleData()
{
  return mStyleBits & NS_STYLE_INHERITED_STRUCT_MASK;
}

#ifdef DEBUG
/* virtual */ void
AnimValuesStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.AppendLiteral("[anim values] { ");
  for (uint32_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
    const PropertyValuePair &pair = mPropertyValuePairs[i];
    str.Append(nsCSSProps::GetStringValue(pair.mProperty));
    str.AppendLiteral(": ");
    nsAutoString value;
    StyleAnimationValue::UncomputeValue(pair.mProperty, pair.mValue, value);
    AppendUTF16toUTF8(value, str);
    str.AppendLiteral("; ");
  }
  str.AppendLiteral("}\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

bool
AnimationCollection::CanPerformOnCompositorThread(const nsIFrame* aFrame) const
{
  if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animations are disabled");
      AnimationUtils::LogAsyncAnimationFailure(message);
    }
    return false;
  }

  if (aFrame->RefusedAsyncAnimation()) {
    return false;
  }

  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const Animation* anim = mAnimations[animIdx];
    if (!anim->IsPlaying()) {
      continue;
    }

    const KeyframeEffectReadOnly* effect = anim->GetEffect();
    MOZ_ASSERT(effect, "A playing animation should have an effect");

    if (effect->ShouldBlockCompositorAnimations(aFrame)) {
      return false;
    }
  }

  return true;
}

bool
AnimationCollection::HasCurrentAnimationOfProperty(nsCSSProperty
                                                     aProperty) const
{
  for (Animation* animation : mAnimations) {
    if (animation->HasCurrentEffect() &&
        animation->GetEffect()->HasAnimationOfProperty(aProperty)) {
      return true;
    }
  }
  return false;
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

  if (!mStyleChanging) {
    mStyleRuleRefreshTime = aRefreshTime;
    return;
  }

  if (!mStyleRuleRefreshTime.IsNull() &&
      mStyleRuleRefreshTime == aRefreshTime) {
    // mStyleRule may be null and valid, if we have no style to apply.
    return;
  }

  if (mManager->IsAnimationManager()) {
    // Update cascade results before updating the style rule, since the
    // cascade results can influence the style rule.
    static_cast<nsAnimationManager*>(mManager)->MaybeUpdateCascadeResults(this);
  }

  mStyleRuleRefreshTime = aRefreshTime;
  mStyleRule = nullptr;
  // We'll set mStyleChanging to true below if necessary.
  mStyleChanging = false;

  // If multiple animations specify behavior for the same property the
  // animation which occurs last in the value of animation-name wins.
  // As a result, we iterate from last animation to first and, if a
  // property has already been set, we don't leave it.
  nsCSSPropertySet properties;

  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    mAnimations[animIdx]->ComposeStyle(mStyleRule, properties, mStyleChanging);
  }
}

void
AnimationCollection::RequestRestyle(RestyleType aRestyleType)
{
  MOZ_ASSERT(IsForElement() || IsForBeforePseudo() || IsForAfterPseudo(),
             "Unexpected mElementProperty; might restyle too much");

  nsPresContext* presContext = mManager->PresContext();
  if (!presContext) {
    // Pres context will be null after the manager is disconnected.
    return;
  }

  // Steps for Restyle::Layer:

  if (aRestyleType == RestyleType::Layer) {
    mStyleRuleRefreshTime = TimeStamp();
    mStyleChanging = true;

    // Prompt layers to re-sync their animations.
    presContext->ClearLastStyleUpdateForAllAnimations();
    presContext->RestyleManager()->IncrementAnimationGeneration();
    UpdateAnimationGeneration(presContext);
  }

  // Steps for RestyleType::Standard and above:

  if (mHasPendingAnimationRestyle) {
    return;
  }

  if (aRestyleType >= RestyleType::Standard) {
    mHasPendingAnimationRestyle = true;
    PostRestyleForAnimation(presContext);
    return;
  }

  // Steps for RestyleType::Throttled:

  MOZ_ASSERT(aRestyleType == RestyleType::Throttled,
             "Should have already handled all non-throttled restyles");
  presContext->Document()->SetNeedStyleFlush();
}

void
AnimationCollection::UpdateAnimationGeneration(nsPresContext* aPresContext)
{
  mAnimationGeneration =
    aPresContext->RestyleManager()->GetAnimationGeneration();
}

void
AnimationCollection::UpdateCheckGeneration(
  nsPresContext* aPresContext)
{
  mCheckGeneration =
    aPresContext->RestyleManager()->GetAnimationGeneration();
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
