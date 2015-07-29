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
#include "mozilla/LookAndFeel.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsDisplayList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "RestyleManager.h"
#include "nsRuleProcessorData.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"


using mozilla::layers::Layer;
using mozilla::dom::Animation;
using mozilla::dom::KeyframeEffectReadOnly;

namespace mozilla {

/* static */ bool
IsGeometricProperty(nsCSSProperty aProperty)
{
  switch (aProperty) {
    case eCSSProperty_bottom:
    case eCSSProperty_height:
    case eCSSProperty_left:
    case eCSSProperty_right:
    case eCSSProperty_top:
    case eCSSProperty_width:
      return true;
    default:
      return false;
  }
}

CommonAnimationManager::CommonAnimationManager(nsPresContext *aPresContext)
  : mPresContext(aPresContext)
  , mIsObservingRefreshDriver(false)
{
  PR_INIT_CLIST(&mElementCollections);
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
  if (!mIsObservingRefreshDriver) {
    NS_ASSERTION(aCollection->mNeedsRefreshes,
      "Added data which doesn't need refreshing?");
    // We need to observe the refresh driver.
    mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
    mIsObservingRefreshDriver = true;
  }

  PR_INSERT_BEFORE(aCollection, &mElementCollections);
}

void
CommonAnimationManager::RemoveAllElementCollections()
{
  while (!PR_CLIST_IS_EMPTY(&mElementCollections)) {
    AnimationCollection* head =
      static_cast<AnimationCollection*>(PR_LIST_HEAD(&mElementCollections));
    head->Destroy();
  }
}

void
CommonAnimationManager::MaybeStartObservingRefreshDriver()
{
  if (mIsObservingRefreshDriver || !NeedsRefresh()) {
    return;
  }

  mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
  mIsObservingRefreshDriver = true;
}

void
CommonAnimationManager::MaybeStartOrStopObservingRefreshDriver()
{
  bool needsRefresh = NeedsRefresh();
  if (needsRefresh && !mIsObservingRefreshDriver) {
    mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
  } else if (!needsRefresh && mIsObservingRefreshDriver) {
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
  mIsObservingRefreshDriver = needsRefresh;
}

bool
CommonAnimationManager::NeedsRefresh() const
{
  for (PRCList *l = PR_LIST_HEAD(&mElementCollections);
       l != &mElementCollections;
       l = PR_NEXT_LINK(l)) {
    if (static_cast<AnimationCollection*>(l)->mNeedsRefreshes) {
      return true;
    }
  }
  return false;
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
      !collection->HasAnimationOfProperty(aProperty) ||
      !collection->CanPerformOnCompositorThread(
        AnimationCollection::CanAnimate_AllowPartial)) {
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
CommonAnimationManager::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
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

  PRCList* next = PR_LIST_HEAD(&mElementCollections);
  while (next != &mElementCollections) {
    AnimationCollection* collection = static_cast<AnimationCollection*>(next);
    next = PR_NEXT_LINK(next);

    collection->EnsureStyleRuleFor(now, EnsureStyleRule_IsNotThrottled);

    dom::Element* elementToRestyle = collection->GetElementToRestyle();
    if (elementToRestyle) {
      nsRestyleHint rshint = collection->IsForTransitions()
        ? eRestyle_CSSTransitions : eRestyle_CSSAnimations;
      aTracker.AddPendingRestyle(elementToRestyle, rshint, nsChangeHint(0));
    }
  }
}

void
CommonAnimationManager::NotifyCollectionUpdated(AnimationCollection&
                                                  aCollection)
{
  MaybeStartObservingRefreshDriver();
  mPresContext->ClearLastStyleUpdateForAllAnimations();
  mPresContext->RestyleManager()->IncrementAnimationGeneration();
  aCollection.UpdateAnimationGeneration(mPresContext);
  aCollection.PostRestyleForAnimation(mPresContext);
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
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementCollections)) {
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
    mPresContext->RefreshDriver()->MostRecentRefresh(),
    EnsureStyleRule_IsNotThrottled);

  return collection->mStyleRule;
}

/* static */ const CommonAnimationManager::LayerAnimationRecord
  CommonAnimationManager::sLayerAnimationInfo[] =
    { { eCSSProperty_transform,
        nsDisplayItem::TYPE_TRANSFORM,
        nsChangeHint_UpdateTransformLayer },
      { eCSSProperty_opacity,
        nsDisplayItem::TYPE_OPACITY,
        nsChangeHint_UpdateOpacityLayer } };

/* static */ const CommonAnimationManager::LayerAnimationRecord*
CommonAnimationManager::LayerAnimationRecordFor(nsCSSProperty aProperty)
{
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
             "unexpected property");
  const auto& info = sLayerAnimationInfo;
  for (size_t i = 0; i < ArrayLength(info); ++i) {
    if (aProperty == info[i].mProperty) {
      return &info[i];
    }
  }
  return nullptr;
}

#ifdef DEBUG
/* static */ void
CommonAnimationManager::Initialize()
{
  const auto& info = CommonAnimationManager::sLayerAnimationInfo;
  for (size_t i = 0; i < ArrayLength(info); i++) {
    auto record = info[i];
    MOZ_ASSERT(nsCSSProps::PropHasFlags(record.mProperty,
                                        CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
               "CSS property with entry in sLayerAnimationInfo does not "
               "have the CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR flag");
  }

  // Check that every property with the flag for animating on the
  // compositor has an entry in sLayerAnimationInfo.
  for (nsCSSProperty prop = nsCSSProperty(0);
       prop < eCSSProperty_COUNT;
       prop = nsCSSProperty(prop + 1)) {
    if (nsCSSProps::PropHasFlags(prop,
                                 CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR)) {
      bool found = false;
      for (size_t i = 0; i < ArrayLength(info); i++) {
        auto record = info[i];
        if (record.mProperty == prop) {
          found = true;
          break;
        }
      }
      MOZ_ASSERT(found,
                 "CSS property with the CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR "
                 "flag does not have an entry in sLayerAnimationInfo");
    }
  }
}
#endif

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
AnimationCollection::CanAnimatePropertyOnCompositor(
  const dom::Element *aElement,
  nsCSSProperty aProperty,
  CanAnimateFlags aFlags)
{
  bool shouldLog = nsLayoutUtils::IsAnimationLoggingEnabled();
  if (!gfxPlatform::OffMainThreadCompositingEnabled()) {
    if (shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Compositor disabled");
      LogAsyncAnimationFailure(message);
    }
    return false;
  }

  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
  if (IsGeometricProperty(aProperty)) {
    if (shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation of geometric property '");
      message.Append(nsCSSProps::GetStringValue(aProperty));
      message.AppendLiteral("' is disabled");
      LogAsyncAnimationFailure(message, aElement);
    }
    return false;
  }
  if (aProperty == eCSSProperty_transform) {
    if (frame->Preserves3D() ||
        frame->Preserves3DChildren()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async animation of 'preserve-3d' transforms is not supported.  See bug 779598");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    // Note that testing BackfaceIsHidden() is not a sufficient test for
    // what we need for animating backface-visibility correctly if we
    // remove the above test for Preserves3DChildren(); that would require
    // looking at backface-visibility on descendants as well.
    if (frame->StyleDisplay()->BackfaceIsHidden()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async animation of 'backface-visibility: hidden' transforms is not supported.  See bug 1186204.");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (frame->IsSVGTransformed()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async 'transform' animations of frames with SVG transforms is not supported.  See bug 779599");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (aFlags & CanAnimate_HasGeometricProperty) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Performance warning: Async animation of 'transform' not possible due to presence of geometric properties");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
  }
  bool enabled = nsLayoutUtils::AreAsyncAnimationsEnabled();
  if (!enabled && shouldLog) {
    nsCString message;
    message.AppendLiteral("Performance warning: Async animations are disabled");
    LogAsyncAnimationFailure(message);
  }
  bool propertyAllowed = (aProperty == eCSSProperty_transform) ||
                         (aProperty == eCSSProperty_opacity) ||
                         (aFlags & CanAnimate_AllowPartial);
  return enabled && propertyAllowed;
}

/* static */ bool
AnimationCollection::IsCompositorAnimationDisabledForFrame(
  nsIFrame* aFrame)
{
  void* prop = aFrame->Properties().Get(nsIFrame::RefusedAsyncAnimation());
  return bool(reinterpret_cast<intptr_t>(prop));
}

bool
AnimationCollection::CanPerformOnCompositorThread(
  CanAnimateFlags aFlags) const
{
  dom::Element* element = GetElementToRestyle();
  if (!element) {
    return false;
  }
  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(element);
  if (!frame) {
    return false;
  }

  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const Animation* anim = mAnimations[animIdx];
    if (!anim->IsPlaying()) {
      continue;
    }

    const KeyframeEffectReadOnly* effect = anim->GetEffect();
    MOZ_ASSERT(effect, "A playing animation should have an effect");

    for (size_t propIdx = 0, propEnd = effect->Properties().Length();
         propIdx != propEnd; ++propIdx) {
      if (IsGeometricProperty(effect->Properties()[propIdx].mProperty)) {
        aFlags = CanAnimateFlags(aFlags | CanAnimate_HasGeometricProperty);
        break;
      }
    }
  }

  bool existsProperty = false;
  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const Animation* anim = mAnimations[animIdx];
    if (!anim->IsPlaying()) {
      continue;
    }

    const KeyframeEffectReadOnly* effect = anim->GetEffect();
    MOZ_ASSERT(effect, "A playing animation should have an effect");

    existsProperty = existsProperty || effect->Properties().Length() > 0;

    for (size_t propIdx = 0, propEnd = effect->Properties().Length();
         propIdx != propEnd; ++propIdx) {
      const AnimationProperty& prop = effect->Properties()[propIdx];
      if (!CanAnimatePropertyOnCompositor(element,
                                          prop.mProperty,
                                          aFlags) ||
          IsCompositorAnimationDisabledForFrame(frame)) {
        return false;
      }
    }
  }

  // No properties to animate
  if (!existsProperty) {
    return false;
  }

  return true;
}

void
AnimationCollection::PostUpdateLayerAnimations()
{
  nsCSSPropertySet propsHandled;
  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const auto& properties = mAnimations[animIdx]->GetEffect()->Properties();
    for (size_t propIdx = properties.Length(); propIdx-- != 0; ) {
      nsCSSProperty prop = properties[propIdx].mProperty;
      if (nsCSSProps::PropHasFlags(prop,
                                   CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR) &&
          !propsHandled.HasProperty(prop)) {
        propsHandled.AddProperty(prop);
        nsChangeHint changeHint = CommonAnimationManager::
          LayerAnimationRecordFor(prop)->mChangeHint;
        dom::Element* element = GetElementToRestyle();
        if (element) {
          mManager->mPresContext->RestyleManager()->
            PostRestyleEvent(element, nsRestyleHint(0), changeHint);
        }
      }
    }
  }
}

bool
AnimationCollection::HasAnimationOfProperty(nsCSSProperty aProperty) const
{
  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const KeyframeEffectReadOnly* effect = mAnimations[animIdx]->GetEffect();
    if (effect && effect->HasAnimationOfProperty(aProperty) &&
        !effect->IsFinishedTransition()) {
      return true;
    }
  }
  return false;
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

void
AnimationCollection::NotifyAnimationUpdated()
{
  // On the next flush, force us to update the style rule
  mNeedsRefreshes = true;
  mStyleRuleRefreshTime = TimeStamp();

  mManager->NotifyCollectionUpdated(*this);
}

/* static */ void
AnimationCollection::LogAsyncAnimationFailure(nsCString& aMessage,
                                                     const nsIContent* aContent)
{
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->NodeInfo()->NameAtom()));

    nsIAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.Append('\'');
    }
    aMessage.Append(']');
  }
  aMessage.Append('\n');
  printf_stderr("%s", aMessage.get());
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
    nsAutoAnimationMutationBatch mb(collection->mElement);

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
AnimationCollection::EnsureStyleRuleFor(TimeStamp aRefreshTime,
                                        EnsureStyleRuleFlags aFlags)
{
  if (!mNeedsRefreshes) {
    mStyleRuleRefreshTime = aRefreshTime;
    return;
  }

  if (!mStyleRuleRefreshTime.IsNull() &&
      mStyleRuleRefreshTime == aRefreshTime) {
    // mStyleRule may be null and valid, if we have no style to apply.
    return;
  }

  // If we're performing animations on the compositor thread, then we can skip
  // most of the work in this method. But even if we are throttled, then we
  // have to do the work if an animation is ending in order to get correct end
  // of animation behavior (the styles of the animation disappear, or the fill
  // mode behavior). CanThrottle returns false for any finishing animations
  // so we can force style recalculation in that case.
  if (aFlags == EnsureStyleRule_IsThrottled) {
    for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
      if (!mAnimations[animIdx]->CanThrottle()) {
        aFlags = EnsureStyleRule_IsNotThrottled;
        break;
      }
    }
  }

  if (aFlags == EnsureStyleRule_IsThrottled) {
    return;
  }

  if (mManager->IsAnimationManager()) {
    // Update cascade results before updating the style rule, since the
    // cascade results can influence the style rule.
    static_cast<nsAnimationManager*>(mManager)->MaybeUpdateCascadeResults(this);
  }

  mStyleRuleRefreshTime = aRefreshTime;
  mStyleRule = nullptr;
  // We'll set mNeedsRefreshes to true below in all cases where we need them.
  mNeedsRefreshes = false;

  // If multiple animations specify behavior for the same property the
  // animation which occurs last in the value of animation-name wins.
  // As a result, we iterate from last animation to first and, if a
  // property has already been set, we don't leave it.
  nsCSSPropertySet properties;

  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    mAnimations[animIdx]->ComposeStyle(mStyleRule, properties, mNeedsRefreshes);
  }

  mManager->MaybeStartObservingRefreshDriver();
}

bool
AnimationCollection::CanThrottleTransformChanges(TimeStamp aTime)
{
  if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    return false;
  }

  // If we know that the animation cannot cause overflow,
  // we can just disable flushes for this animation.

  // If we don't show scrollbars, we don't care about overflow.
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_ShowHideScrollbars) == 0) {
    return true;
  }

  // If this animation can cause overflow, we can throttle some of the ticks.
  if (!mStyleRuleRefreshTime.IsNull() &&
      (aTime - mStyleRuleRefreshTime) < TimeDuration::FromMilliseconds(200)) {
    return true;
  }

  dom::Element* element = GetElementToRestyle();
  if (!element) {
    return false;
  }

  // If the nearest scrollable ancestor has overflow:hidden,
  // we don't care about overflow.
  nsIScrollableFrame* scrollable = nsLayoutUtils::GetNearestScrollableFrame(
                                     nsLayoutUtils::GetStyleFrame(element));
  if (!scrollable) {
    return true;
  }

  ScrollbarStyles ss = scrollable->GetScrollbarStyles();
  if (ss.mVertical == NS_STYLE_OVERFLOW_HIDDEN &&
      ss.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
      scrollable->GetLogicalScrollPosition() == nsPoint(0, 0)) {
    return true;
  }

  return false;
}

bool
AnimationCollection::CanThrottleAnimation(TimeStamp aTime)
{
  dom::Element* element = GetElementToRestyle();
  if (!element) {
    return false;
  }
  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(element);
  if (!frame) {
    return false;
  }


  const auto& info = CommonAnimationManager::sLayerAnimationInfo;
  for (size_t i = 0; i < ArrayLength(info); i++) {
    auto record = info[i];
    if (!HasAnimationOfProperty(record.mProperty)) {
      continue;
    }

    Layer* layer = FrameLayerBuilder::GetDedicatedLayer(
                     frame, record.mLayerType);
    if (!layer || mAnimationGeneration > layer->GetAnimationGeneration()) {
      return false;
    }

    if (record.mProperty == eCSSProperty_transform &&
        !CanThrottleTransformChanges(aTime)) {
      return false;
    }
  }

  return true;
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

bool
AnimationCollection::HasCurrentAnimations() const
{
  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    if (mAnimations[animIdx]->HasCurrentEffect()) {
      return true;
    }
  }

  return false;
}

bool
AnimationCollection::HasCurrentAnimationsForProperties(
                              const nsCSSProperty* aProperties,
                              size_t aPropertyCount) const
{
  for (size_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const Animation& anim = *mAnimations[animIdx];
    const KeyframeEffectReadOnly* effect = anim.GetEffect();
    if (effect &&
        effect->IsCurrent(anim) &&
        effect->HasAnimationOfProperties(aProperties, aPropertyCount)) {
      return true;
    }
  }

  return false;
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
