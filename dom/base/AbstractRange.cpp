/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/AbstractRangeBinding.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/StaticRange.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsTArray.h"

namespace mozilla::dom {

template nsresult AbstractRange::SetStartAndEndInternal(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary,
    nsRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    nsRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, nsRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    StaticRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RangeBoundary& aStartBoundary, const RawRangeBoundary& aEndBoundary,
    StaticRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RawRangeBoundary& aStartBoundary, const RangeBoundary& aEndBoundary,
    StaticRange* aRange);
template nsresult AbstractRange::SetStartAndEndInternal(
    const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, StaticRange* aRange);
template bool AbstractRange::MaybeCacheToReuse(nsRange& aInstance);
template bool AbstractRange::MaybeCacheToReuse(StaticRange& aInstance);

bool AbstractRange::sHasShutDown = false;

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbstractRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbstractRange)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbstractRange)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AbstractRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);
  // mStart and mEnd may depend on or be depended on some other members in
  // concrete classes so that they should be unlinked in sub classes.
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStart)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEnd)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// NOTE: If you need to change default value of members of AbstractRange,
//       update nsRange::Create(nsINode* aNode) and ClearForReuse() too.
AbstractRange::AbstractRange(nsINode* aNode)
    : mIsPositioned(false), mIsGenerated(false), mCalledByJS(false) {
  Init(aNode);
}

AbstractRange::~AbstractRange() = default;

void AbstractRange::Init(nsINode* aNode) {
  MOZ_ASSERT(aNode, "range isn't in a document!");
  mOwner = aNode->OwnerDoc();
}

// static
void AbstractRange::Shutdown() {
  sHasShutDown = true;
  if (nsTArray<RefPtr<nsRange>>* cachedRanges = nsRange::sCachedRanges) {
    nsRange::sCachedRanges = nullptr;
    cachedRanges->Clear();
    delete cachedRanges;
  }
  if (nsTArray<RefPtr<StaticRange>>* cachedRanges =
          StaticRange::sCachedRanges) {
    StaticRange::sCachedRanges = nullptr;
    cachedRanges->Clear();
    delete cachedRanges;
  }
}

// static
template <class RangeType>
bool AbstractRange::MaybeCacheToReuse(RangeType& aInstance) {
  static const size_t kMaxRangeCache = 64;

  // If the instance is not used by JS and the cache is not yet full, we
  // should reuse it.  Otherwise, delete it.
  if (sHasShutDown || aInstance.GetWrapperMaybeDead() || aInstance.GetFlags() ||
      (RangeType::sCachedRanges &&
       RangeType::sCachedRanges->Length() == kMaxRangeCache)) {
    return false;
  }

  aInstance.ClearForReuse();

  if (!RangeType::sCachedRanges) {
    RangeType::sCachedRanges = new nsTArray<RefPtr<RangeType>>(16);
  }
  RangeType::sCachedRanges->AppendElement(&aInstance);
  return true;
}

nsINode* AbstractRange::GetClosestCommonInclusiveAncestor() const {
  return mIsPositioned ? nsContentUtils::GetClosestCommonInclusiveAncestor(
                             mStart.Container(), mEnd.Container())
                       : nullptr;
}

// static
template <typename SPT, typename SRT, typename EPT, typename ERT,
          typename RangeType>
nsresult AbstractRange::SetStartAndEndInternal(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, RangeType* aRange) {
  if (NS_WARN_IF(!aStartBoundary.IsSet()) ||
      NS_WARN_IF(!aEndBoundary.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  nsINode* newStartRoot =
      RangeUtils::ComputeRootNode(aStartBoundary.Container());
  if (!newStartRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aStartBoundary.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aStartBoundary.Container() == aEndBoundary.Container()) {
    if (!aEndBoundary.IsSetAndValid()) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }
    // XXX: Offsets - handle this more efficiently.
    // If the end offset is less than the start offset, this should be
    // collapsed at the end offset.
    if (*aStartBoundary.Offset(
            RangeBoundaryBase<SPT, SRT>::OffsetFilter::kValidOffsets) >
        *aEndBoundary.Offset(
            RangeBoundaryBase<EPT, ERT>::OffsetFilter::kValidOffsets)) {
      aRange->DoSetRange(aEndBoundary, aEndBoundary, newStartRoot);
    } else {
      aRange->DoSetRange(aStartBoundary, aEndBoundary, newStartRoot);
    }
    return NS_OK;
  }

  nsINode* newEndRoot = RangeUtils::ComputeRootNode(aEndBoundary.Container());
  if (!newEndRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aEndBoundary.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // If they have different root, this should be collapsed at the end point.
  if (newStartRoot != newEndRoot) {
    aRange->DoSetRange(aEndBoundary, aEndBoundary, newEndRoot);
    return NS_OK;
  }

  const Maybe<int32_t> pointOrder =
      nsContentUtils::ComparePoints(aStartBoundary, aEndBoundary);
  if (!pointOrder) {
    // Safely return a value but also detected this in debug builds.
    MOZ_ASSERT_UNREACHABLE();
    return NS_ERROR_INVALID_ARG;
  }

  // If the end point is before the start point, this should be collapsed at
  // the end point.
  if (*pointOrder == 1) {
    aRange->DoSetRange(aEndBoundary, aEndBoundary, newEndRoot);
    return NS_OK;
  }

  // Otherwise, set the range as specified.
  aRange->DoSetRange(aStartBoundary, aEndBoundary, newStartRoot);
  return NS_OK;
}

nsINode* AbstractRange::GetParentObject() const { return mOwner; }

JSObject* AbstractRange::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  MOZ_CRASH("Must be overridden");
}

void AbstractRange::ClearForReuse() {
  mOwner = nullptr;
  mStart = RangeBoundary();
  mEnd = RangeBoundary();
  mIsPositioned = false;
  mIsGenerated = false;
  mCalledByJS = false;
}

}  // namespace mozilla::dom
