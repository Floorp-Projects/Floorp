/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ElementAnimationData.h"
#include "mozilla/AnimationCollection.h"
#include "mozilla/EffectSet.h"
#include "mozilla/dom/CSSTransition.h"
#include "mozilla/dom/CSSAnimation.h"

namespace mozilla {

void ElementAnimationData::Traverse(nsCycleCollectionTraversalCallback& cb) {
  mElementData.Traverse(cb);
  mBeforeData.Traverse(cb);
  mAfterData.Traverse(cb);
  mMarkerData.Traverse(cb);
}

void ElementAnimationData::ClearAllAnimationCollections() {
  for (auto* data : {&mElementData, &mBeforeData, &mAfterData, &mMarkerData}) {
    data->mAnimations = nullptr;
    data->mTransitions = nullptr;
  }
}

ElementAnimationData::PerElementOrPseudoData::PerElementOrPseudoData() =
    default;
ElementAnimationData::PerElementOrPseudoData::~PerElementOrPseudoData() =
    default;

void ElementAnimationData::PerElementOrPseudoData::Traverse(
    nsCycleCollectionTraversalCallback& cb) {
  // We only care about mEffectSet. The animation collections are managed by the
  // pres context and go away when presentation of the document goes away.
  if (mEffectSet) {
    mEffectSet->Traverse(cb);
  }
}

EffectSet& ElementAnimationData::PerElementOrPseudoData::DoEnsureEffectSet() {
  MOZ_ASSERT(!mEffectSet);
  mEffectSet = MakeUnique<EffectSet>();
  return *mEffectSet;
}

CSSTransitionCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureTransitions(
    dom::Element& aOwner, PseudoStyleType aType) {
  MOZ_ASSERT(!mTransitions);
  mTransitions = MakeUnique<CSSTransitionCollection>(aOwner, aType);
  return *mTransitions;
}

CSSAnimationCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureAnimations(
    dom::Element& aOwner, PseudoStyleType aType) {
  MOZ_ASSERT(!mAnimations);
  mAnimations = MakeUnique<CSSAnimationCollection>(aOwner, aType);
  return *mAnimations;
}

void ElementAnimationData::PerElementOrPseudoData::DoClearEffectSet() {
  MOZ_ASSERT(mEffectSet);
  mEffectSet = nullptr;
}

void ElementAnimationData::PerElementOrPseudoData::DoClearTransitions() {
  MOZ_ASSERT(mTransitions);
  mTransitions = nullptr;
}

void ElementAnimationData::PerElementOrPseudoData::DoClearAnimations() {
  MOZ_ASSERT(mAnimations);
  mAnimations = nullptr;
}

}  // namespace mozilla
