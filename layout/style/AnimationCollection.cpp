/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationCollection.h"

#include "mozilla/RestyleManager.h"
#include "nsAnimationManager.h"  // For dom::CSSAnimation
#include "nsPresContext.h"
#include "nsTransitionManager.h"  // For dom::CSSTransition

namespace mozilla {

template <class AnimationType>
/* static */ void AnimationCollection<AnimationType>::PropertyDtor(
    void* aObject, nsAtom* aPropertyName, void* aPropertyValue, void* aData) {
  AnimationCollection* collection =
      static_cast<AnimationCollection*>(aPropertyValue);
#ifdef DEBUG
  MOZ_ASSERT(!collection->mCalledPropertyDtor, "can't call dtor twice");
  collection->mCalledPropertyDtor = true;
#endif

  PostRestyleMode postRestyle = collection->mCalledDestroy
                                    ? PostRestyleMode::IfNeeded
                                    : PostRestyleMode::Never;
  {
    nsAutoAnimationMutationBatch mb(collection->mElement->OwnerDoc());

    for (size_t animIdx = collection->mAnimations.Length(); animIdx-- != 0;) {
      collection->mAnimations[animIdx]->CancelFromStyle(postRestyle);
    }
  }
  delete collection;
}

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::GetAnimationCollection(
    const dom::Element* aElement, PseudoStyleType aPseudoType) {
  if (!aElement->MayHaveAnimations()) {
    // Early return for the most common case.
    return nullptr;
  }

  nsAtom* propName = GetPropertyAtomForPseudoType(aPseudoType);
  if (!propName) {
    return nullptr;
  }

  return static_cast<AnimationCollection<AnimationType>*>(
      aElement->GetProperty(propName));
}

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::GetAnimationCollection(
    const nsIFrame* aFrame) {
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
    dom::Element* aElement, PseudoStyleType aPseudoType,
    bool* aCreatedCollection) {
  MOZ_ASSERT(aCreatedCollection);
  *aCreatedCollection = false;

  nsAtom* propName = GetPropertyAtomForPseudoType(aPseudoType);
  MOZ_ASSERT(propName,
             "Should only try to create animations for one of the"
             " recognized pseudo types");

  auto collection = static_cast<AnimationCollection<AnimationType>*>(
      aElement->GetProperty(propName));
  if (!collection) {
    // FIXME: Consider arena-allocating?
    collection = new AnimationCollection<AnimationType>(aElement, propName);
    nsresult rv = aElement->SetProperty(
        propName, collection, &AnimationCollection<AnimationType>::PropertyDtor,
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
/*static*/ nsAtom*
AnimationCollection<AnimationType>::GetPropertyAtomForPseudoType(
    PseudoStyleType aPseudoType) {
  nsAtom* propName = nullptr;

  if (aPseudoType == PseudoStyleType::NotPseudo) {
    propName = TraitsType::ElementPropertyAtom();
  } else if (aPseudoType == PseudoStyleType::before) {
    propName = TraitsType::BeforePropertyAtom();
  } else if (aPseudoType == PseudoStyleType::after) {
    propName = TraitsType::AfterPropertyAtom();
  } else if (aPseudoType == PseudoStyleType::marker) {
    propName = TraitsType::MarkerPropertyAtom();
  }

  return propName;
}

// Explicit class instantiations

template class AnimationCollection<dom::CSSAnimation>;
template class AnimationCollection<dom::CSSTransition>;

}  // namespace mozilla
