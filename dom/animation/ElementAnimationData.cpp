/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ElementAnimationData.h"
#include "mozilla/AnimationCollection.h"
#include "mozilla/TimelineCollection.h"
#include "mozilla/EffectSet.h"
#include "mozilla/dom/CSSTransition.h"
#include "mozilla/dom/CSSAnimation.h"
#include "mozilla/dom/ScrollTimeline.h"
#include "mozilla/dom/ViewTimeline.h"

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
    data->mScrollTimelines = nullptr;
    data->mViewTimelines = nullptr;
    data->mProgressTimelineScheduler = nullptr;
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

ScrollTimelineCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureScrollTimelines(
    dom::Element& aOwner, PseudoStyleType aType) {
  MOZ_ASSERT(!mScrollTimelines);
  mScrollTimelines = MakeUnique<ScrollTimelineCollection>(aOwner, aType);
  return *mScrollTimelines;
}

ViewTimelineCollection&
ElementAnimationData::PerElementOrPseudoData::DoEnsureViewTimelines(
    dom::Element& aOwner, PseudoStyleType aType) {
  MOZ_ASSERT(!mViewTimelines);
  mViewTimelines = MakeUnique<ViewTimelineCollection>(aOwner, aType);
  return *mViewTimelines;
}

dom::ProgressTimelineScheduler&
ElementAnimationData::PerElementOrPseudoData::DoEnsureProgressTimelineScheduler(
    dom::Element& aOwner, PseudoStyleType aType) {
  MOZ_ASSERT(!mProgressTimelineScheduler);
  mProgressTimelineScheduler = MakeUnique<dom::ProgressTimelineScheduler>();
  return *mProgressTimelineScheduler;
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

void ElementAnimationData::PerElementOrPseudoData::DoClearScrollTimelines() {
  MOZ_ASSERT(mScrollTimelines);
  mScrollTimelines = nullptr;
}

void ElementAnimationData::PerElementOrPseudoData::DoClearViewTimelines() {
  MOZ_ASSERT(mViewTimelines);
  mViewTimelines = nullptr;
}

void ElementAnimationData::PerElementOrPseudoData::
    DoClearProgressTimelineScheduler() {
  MOZ_ASSERT(mProgressTimelineScheduler);
  mProgressTimelineScheduler = nullptr;
}

}  // namespace mozilla
