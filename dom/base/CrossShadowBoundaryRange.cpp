/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CrossShadowBoundaryRange.h"
#include "nsContentUtils.h"
#include "nsINode.h"

namespace mozilla::dom {
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RangeBoundary& aStartBoundary,
                                 const RangeBoundary& aEndBoundary);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RangeBoundary& aStartBoundary,
                                 const RawRangeBoundary& aEndBoundary);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RawRangeBoundary& aStartBoundary,
                                 const RangeBoundary& aEndBoundary);
template already_AddRefed<CrossShadowBoundaryRange>
CrossShadowBoundaryRange::Create(const RawRangeBoundary& aStartBoundary,
                                 const RawRangeBoundary& aEndBoundary);

template void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsINode* aRootNode);
template void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary,
    nsINode* aRootNode);
template void CrossShadowBoundaryRange::DoSetRange(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsINode* aRootNode);
template void CrossShadowBoundaryRange::DoSetRange(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, nsINode* aRootNode);

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
    DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr),
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
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
  RefPtr<CrossShadowBoundaryRange> range;
  if (!sCachedRanges || sCachedRanges->IsEmpty()) {
    range = new CrossShadowBoundaryRange(aStartBoundary.Container());
  } else {
    range = sCachedRanges->PopLastElement().forget();
  }

  range->Init(aStartBoundary.Container());
  range->DoSetRange(aStartBoundary, aEndBoundary, nullptr);
  return range.forget();
}

template <typename SPT, typename SRT, typename EPT, typename ERT>
void CrossShadowBoundaryRange::DoSetRange(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, nsINode* aRootNode) {
  // aRootNode is useless to CrossShadowBoundaryRange because aStartBoundary
  // and aEndBoundary could have different roots.
  StaticRange::DoSetRange(aStartBoundary, aEndBoundary, nullptr);

  nsINode* startRoot = RangeUtils::ComputeRootNode(mStart.Container());
  nsINode* endRoot = RangeUtils::ComputeRootNode(mEnd.Container());

  if (startRoot == endRoot) {
    // This should be the case when Release() is called.
    MOZ_ASSERT(!startRoot && !endRoot);
    mCommonAncestor = startRoot;
  } else {
    mCommonAncestor =
        nsContentUtils::GetClosestCommonShadowIncludingInclusiveAncestor(
            mStart.Container(), mEnd.Container());
  }
}
}  // namespace mozilla::dom
