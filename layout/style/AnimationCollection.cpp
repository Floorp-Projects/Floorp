/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationCollection.h"

#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "nsAnimationManager.h" // For dom::CSSAnimation
#include "nsPresContext.h"
#include "nsTransitionManager.h" // For dom::CSSTransition

namespace mozilla {

template <class AnimationType>
/* static */ void
AnimationCollection<AnimationType>::PropertyDtor(void* aObject,
                                                 nsIAtom* aPropertyName,
                                                 void* aPropertyValue,
                                                 void* aData)
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

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::GetAnimationCollection(
  dom::Element *aElement,
  CSSPseudoElementType aPseudoType)
{
  if (!aElement->MayHaveAnimations()) {
    // Early return for the most common case.
    return nullptr;
  }

  nsIAtom* propName = GetPropertyAtomForPseudoType(aPseudoType);
  if (!propName) {
    return nullptr;
  }

  return
    static_cast<AnimationCollection<AnimationType>*>(aElement->
                                                     GetProperty(propName));
}

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::GetAnimationCollection(
  const nsIFrame* aFrame)
{
  Maybe<NonOwningAnimationTarget> pseudoElement =
    EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (!pseudoElement) {
    return nullptr;
  }

  if (!pseudoElement->mElement->MayHaveAnimations()) {
    return nullptr;
  }

  return GetAnimationCollection(pseudoElement->mElement,
                                pseudoElement->mPseudoType);
}

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::GetOrCreateAnimationCollection(
  dom::Element* aElement,
  CSSPseudoElementType aPseudoType,
  bool* aCreatedCollection)
{
  MOZ_ASSERT(aCreatedCollection);
  *aCreatedCollection = false;

  nsIAtom* propName = GetPropertyAtomForPseudoType(aPseudoType);
  MOZ_ASSERT(propName, "Should only try to create animations for one of the"
             " recognized pseudo types");

  auto collection = static_cast<AnimationCollection<AnimationType>*>(
                      aElement->GetProperty(propName));
  if (!collection) {
    // FIXME: Consider arena-allocating?
    collection = new AnimationCollection<AnimationType>(aElement, propName);
    nsresult rv =
      aElement->SetProperty(propName, collection,
                            &AnimationCollection<AnimationType>::PropertyDtor,
                            false);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      // The collection must be destroyed via PropertyDtor, otherwise
      // mCalledPropertyDtor assertion is triggered in destructor.
      AnimationCollection<AnimationType>::PropertyDtor(aElement, propName,
                                                       collection, nullptr);
      return nullptr;
    }

    *aCreatedCollection = true;
    aElement->SetMayHaveAnimations();
  }

  return collection;
}

template <class AnimationType>
/* static */ nsString
AnimationCollection<AnimationType>::PseudoTypeAsString(
  CSSPseudoElementType aPseudoType)
{
  switch (aPseudoType) {
    case CSSPseudoElementType::before:
      return NS_LITERAL_STRING("::before");
    case CSSPseudoElementType::after:
      return NS_LITERAL_STRING("::after");
    default:
      MOZ_ASSERT(aPseudoType == CSSPseudoElementType::NotPseudo,
                 "Unexpected pseudo type");
      return EmptyString();
  }
}

template <class AnimationType>
void
AnimationCollection<AnimationType>::UpdateCheckGeneration(
  nsPresContext* aPresContext)
{
  if (aPresContext->RestyleManager()->IsServo()) {
    // stylo: ServoRestyleManager does not support animations yet.
    return;
  }
  mCheckGeneration =
    aPresContext->RestyleManager()->AsGecko()->GetAnimationGeneration();
}

template<class AnimationType>
/*static*/ nsIAtom*
AnimationCollection<AnimationType>::GetPropertyAtomForPseudoType(
  CSSPseudoElementType aPseudoType)
{
  nsIAtom* propName = nullptr;

  if (aPseudoType == CSSPseudoElementType::NotPseudo) {
    propName = TraitsType::ElementPropertyAtom();
  } else if (aPseudoType == CSSPseudoElementType::before) {
    propName = TraitsType::BeforePropertyAtom();
  } else if (aPseudoType == CSSPseudoElementType::after) {
    propName = TraitsType::AfterPropertyAtom();
  }

  return propName;
}

// Explicit class instantiations

template class AnimationCollection<dom::CSSAnimation>;
template class AnimationCollection<dom::CSSTransition>;

} // namespace mozilla
