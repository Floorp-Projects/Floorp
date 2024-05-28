/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CrossShadowBoundaryRange.h"
#include "nsContentUtils.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsIContentInlines.h"

namespace mozilla::dom {
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RangeBoundary& aStartBoundary,
                                 const RangeBoundary& aEndBoundary,
                                 nsRange* aOwner);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RangeBoundary& aStartBoundary,
                                 const RawRangeBoundary& aEndBoundary,
                                 nsRange* aOwner);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RawRangeBoundary& aStartBoundary,
                                 const RangeBoundary& aEndBoundary,
                                 nsRange* aOwner);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RawRangeBoundary& aStartBoundary,
                                 const RawRangeBoundary& aEndBoundary,
                                 nsRange* aOwner);

template void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsINode* aRootNode, nsRange* aOwner);
template void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary,
    nsINode* aRootNode, nsRange* aOwner);
template void CrossShadowBoundaryRange::DoSetRange(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsINode* aRootNode, nsRange* aOwner);
template void CrossShadowBoundaryRange::DoSetRange(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, nsINode* aRootNode, nsRange* aOwner);

template nsresult CrossShadowBoundaryRange::SetStartAndEnd(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary);
template nsresult CrossShadowBoundaryRange::SetStartAndEnd(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary);
template nsresult CrossShadowBoundaryRange::SetStartAndEnd(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary);
template nsresult CrossShadowBoundaryRange::SetStartAndEnd(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary);

nsTArray<RefPtr<CrossShadowBoundaryRange>>*
    CrossShadowBoundaryRange::sCachedRanges = nullptr;

NS_IMPL_CYCLE_COLLECTING_ADDREF(CrossShadowBoundaryRange)

NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_INTERRUPTABLE_LAST_RELEASE(
    CrossShadowBoundaryRange,
    DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr, nullptr),
    AbstractRange::MaybeCacheToReuse(*this))

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CrossShadowBoundaryRange)
NS_INTERFACE_MAP_END_INHERITING(CrossShadowBoundaryRange)

NS_IMPL_CYCLE_COLLECTION_CLASS(CrossShadowBoundaryRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CrossShadowBoundaryRange,
                                                StaticRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCommonAncestor)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CrossShadowBoundaryRange,
                                                  StaticRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCommonAncestor)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CrossShadowBoundaryRange,
                                               StaticRange)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

/* static */
template <typename SPT, typename SRT, typename EPT, typename ERT>
already_AddRefed<CrossShadowBoundaryRange> CrossShadowBoundaryRange::Create(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, nsRange* aOwner) {
  RefPtr<CrossShadowBoundaryRange> range;
  if (!sCachedRanges || sCachedRanges->IsEmpty()) {
    range = new CrossShadowBoundaryRange(aStartBoundary.Container(), aOwner);
  } else {
    range = sCachedRanges->PopLastElement().forget();
  }

  range->Init(aStartBoundary.Container());
  range->DoSetRange(aStartBoundary, aEndBoundary, nullptr, aOwner);
  return range.forget();
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, nsINode* aRootNode,
    nsRange* aOwner) {
  // aRootNode is useless to CrossShadowBoundaryRange because aStartBoundary
  // and aEndBoundary could have different roots.
  StaticRange::DoSetRange(aStartBoundary, aEndBoundary, nullptr);

  nsINode* startRoot = RangeUtils::ComputeRootNode(mStart.Container());
  nsINode* endRoot = RangeUtils::ComputeRootNode(mEnd.Container());

  nsINode* previousCommonAncestor = mCommonAncestor;
  if (startRoot == endRoot) {
    MOZ_ASSERT(!startRoot && !endRoot);
    MOZ_ASSERT(!aOwner);
    // This should be the case when Release() is called.
    mCommonAncestor = startRoot;
    mOwner = nullptr;
  } else {
    mCommonAncestor =
        nsContentUtils::GetClosestCommonShadowIncludingInclusiveAncestor(
            mStart.Container(), mEnd.Container());
    MOZ_ASSERT_IF(mOwner, mOwner == aOwner);
    if (!mOwner) {
      mOwner = aOwner;
    }
  }

  if (previousCommonAncestor != mCommonAncestor) {
    if (previousCommonAncestor) {
      previousCommonAncestor->RemoveMutationObserver(this);
    }
    if (mCommonAncestor) {
      mCommonAncestor->AddMutationObserver(this);
    }
  }
}
void CrossShadowBoundaryRange::ContentRemoved(nsIContent* aChild,
                                              nsIContent* aPreviousSibling) {
  // It's unclear from the spec about what should the selection be after
  // DOM mutation. See https://github.com/w3c/selection-api/issues/168
  //
  // For now, we just clear the selection if the removed node is related
  // to mStart or mEnd.
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
  MOZ_DIAGNOSTIC_ASSERT(mOwner->GetCrossShadowBoundaryRange() == this);

  RefPtr<CrossShadowBoundaryRange> kungFuDeathGrip(this);

  if (mStart.Container() == aChild || mEnd.Container() == aChild) {
    mOwner->ResetCrossShadowBoundaryRange();
    return;
  }

  if (const auto* shadowRoot = aChild->GetShadowRoot()) {
    if (mStart.Container() == shadowRoot || mEnd.Container() == shadowRoot) {
      mOwner->ResetCrossShadowBoundaryRange();
      return;
    }
  }

  if (mStart.Container()->IsShadowIncludingInclusiveDescendantOf(aChild) ||
      mEnd.Container()->IsShadowIncludingInclusiveDescendantOf(aChild)) {
    mOwner->ResetCrossShadowBoundaryRange();
    return;
  }
}
}  // namespace mozilla::dom
