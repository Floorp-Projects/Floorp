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
                                               CSSPseudoElementType
                                                 aPseudoType,
                                               bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && !aElement->MayHaveAnimations()) {
    // Early return for the most common case.
    return nullptr;
  }

  nsIAtom *propName;
  if (aPseudoType == CSSPseudoElementType::NotPseudo) {
    propName = GetAnimationsAtom();
  } else if (aPseudoType == CSSPseudoElementType::before) {
    propName = GetAnimationsBeforeAtom();
  } else if (aPseudoType == CSSPseudoElementType::after) {
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
    collection = new AnimationCollection(aElement, propName);
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

    aElement->SetMayHaveAnimations();

    AddElementCollection(collection);
  }

  return collection;
}

AnimationCollection*
CommonAnimationManager::GetAnimationCollection(const nsIFrame* aFrame)
{
  Maybe<Pair<dom::Element*, CSSPseudoElementType>> pseudoElement =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (!pseudoElement) {
    return nullptr;
  }

  if (!pseudoElement->first()->MayHaveAnimations()) {
    return nullptr;
  }

  return GetAnimationCollection(pseudoElement->first(),
                                pseudoElement->second(),
                                false /* aCreateIfNeeded */);
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
