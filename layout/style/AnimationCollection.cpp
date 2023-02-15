/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationCollection.h"
#include <type_traits>

#include "mozilla/ElementAnimationData.h"
#include "mozilla/RestyleManager.h"
#include "nsDOMMutationObserver.h"      // For nsAutoAnimationMutationBatch
#include "mozilla/dom/CSSAnimation.h"   // For dom::CSSAnimation
#include "mozilla/dom/CSSTransition.h"  // For dom::CSSTransition

namespace mozilla {

template <class AnimationType>
AnimationCollection<AnimationType>::~AnimationCollection() {
  MOZ_COUNT_DTOR(AnimationCollection);

  const PostRestyleMode postRestyle =
      mCalledDestroy ? PostRestyleMode::IfNeeded : PostRestyleMode::Never;
  {
    nsAutoAnimationMutationBatch mb(mElement.OwnerDoc());

    for (size_t animIdx = mAnimations.Length(); animIdx-- != 0;) {
      mAnimations[animIdx]->CancelFromStyle(postRestyle);
    }
  }
  LinkedListElement<SelfType>::remove();
}

template <class AnimationType>
void AnimationCollection<AnimationType>::Destroy() {
  mCalledDestroy = true;
  auto* data = mElement.GetAnimationData();
  MOZ_ASSERT(data);
  if constexpr (std::is_same_v<AnimationType, dom::CSSAnimation>) {
    MOZ_ASSERT(data->GetAnimationCollection(mPseudo) == this);
    data->ClearAnimationCollectionFor(mPseudo);
  } else {
    MOZ_ASSERT(data->GetTransitionCollection(mPseudo) == this);
    data->ClearTransitionCollectionFor(mPseudo);
  }
}

template <class AnimationType>
AnimationCollection<AnimationType>* AnimationCollection<AnimationType>::Get(
    const dom::Element* aElement, const PseudoStyleType aType) {
  auto* data = aElement->GetAnimationData();
  if (!data) {
    return nullptr;
  }
  if constexpr (std::is_same_v<AnimationType, dom::CSSAnimation>) {
    return data->GetAnimationCollection(aType);
  } else {
    return data->GetTransitionCollection(aType);
  }
}

template <class AnimationType>
/* static */ AnimationCollection<AnimationType>*
AnimationCollection<AnimationType>::Get(const nsIFrame* aFrame) {
  Maybe<NonOwningAnimationTarget> target =
      EffectCompositor::GetAnimationElementAndPseudoForFrame(aFrame);
  if (!target) {
    return nullptr;
  }

  return Get(target->mElement, target->mPseudoType);
}

// Explicit class instantiations

template class AnimationCollection<dom::CSSAnimation>;
template class AnimationCollection<dom::CSSTransition>;

}  // namespace mozilla
