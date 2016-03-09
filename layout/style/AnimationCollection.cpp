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

// Explicit class instantiations

template class AnimationCollection<dom::CSSAnimation>;
template class AnimationCollection<dom::CSSTransition>;

} // namespace mozilla
