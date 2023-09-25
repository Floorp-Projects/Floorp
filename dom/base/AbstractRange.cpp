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
#include "mozilla/dom/Selection.h"
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
  tmp->mSelections.Clear();
  // Unregistering of the common inclusive ancestors would by design
  // also happen when the actual implementations unlink `mStart`/`mEnd`.
  // This may introduce additional overhead which is not needed when unlinking,
  // therefore this is done here beforehand.
  if (tmp->mRegisteredClosestCommonInclusiveAncestor) {
    tmp->UnregisterClosestCommonInclusiveAncestor(
        tmp->mRegisteredClosestCommonInclusiveAncestor, true);
  }
  MOZ_DIAGNOSTIC_ASSERT(!tmp->isInList(),
                        "Shouldn't be registered now that we're unlinking");

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AbstractRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStart)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEnd)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRegisteredClosestCommonInclusiveAncestor)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void AbstractRange::MarkDescendants(const nsINode& aNode) {
  // Set NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection on
  // aNode's descendants unless aNode is already marked as a range common
  // ancestor or a descendant of one, in which case all of our descendants have
  // the bit set already.
  if (!aNode.IsMaybeSelected()) {
    // don't set the Descendant bit on |aNode| itself
    nsINode* node = aNode.GetNextNode(&aNode);
    while (node) {
      node->SetDescendantOfClosestCommonInclusiveAncestorForRangeInSelection();
      if (!node->IsClosestCommonInclusiveAncestorForRangeInSelection()) {
        node = node->GetNextNode(&aNode);
      } else {
        // optimize: skip this sub-tree since it's marked already.
        node = node->GetNextNonChildNode(&aNode);
      }
    }
  }
}

void AbstractRange::UnmarkDescendants(const nsINode& aNode) {
  // Unset NodeIsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection
  // on aNode's descendants unless aNode is a descendant of another range common
  // ancestor. Also, exclude descendants of range common ancestors (but not the
  // common ancestor itself).
  if (!aNode
           .IsDescendantOfClosestCommonInclusiveAncestorForRangeInSelection()) {
    // we know |aNode| doesn't have any bit set
    nsINode* node = aNode.GetNextNode(&aNode);
    while (node) {
      node->ClearDescendantOfClosestCommonInclusiveAncestorForRangeInSelection();
      if (!node->IsClosestCommonInclusiveAncestorForRangeInSelection()) {
        node = node->GetNextNode(&aNode);
      } else {
        // We found an ancestor of an overlapping range, skip its descendants.
        node = node->GetNextNonChildNode(&aNode);
      }
    }
  }
}

// NOTE: If you need to change default value of members of AbstractRange,
//       update nsRange::Create(nsINode* aNode) and ClearForReuse() too.
AbstractRange::AbstractRange(nsINode* aNode, bool aIsDynamicRange)
    : mRegisteredClosestCommonInclusiveAncestor(nullptr),
      mIsPositioned(false),
      mIsGenerated(false),
      mCalledByJS(false),
      mIsDynamicRange(aIsDynamicRange) {
  mRefCnt.SetIsOnMainThread();
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

bool AbstractRange::IsInSelection(const Selection& aSelection) const {
  return mSelections.Contains(&aSelection);
}

void AbstractRange::RegisterSelection(Selection& aSelection) {
  if (IsInSelection(aSelection)) {
    return;
  }
  bool isFirstSelection = mSelections.IsEmpty();
  mSelections.AppendElement(&aSelection);
  if (isFirstSelection && !mRegisteredClosestCommonInclusiveAncestor) {
    nsINode* commonAncestor = GetClosestCommonInclusiveAncestor();
    MOZ_ASSERT(commonAncestor, "unexpected disconnected nodes");
    RegisterClosestCommonInclusiveAncestor(commonAncestor);
  }
}

const nsTArray<WeakPtr<Selection>>& AbstractRange::GetSelections() const {
  return mSelections;
}

void AbstractRange::UnregisterSelection(const Selection& aSelection) {
  mSelections.RemoveElement(&aSelection);
  if (mSelections.IsEmpty() && mRegisteredClosestCommonInclusiveAncestor) {
    UnregisterClosestCommonInclusiveAncestor(
        mRegisteredClosestCommonInclusiveAncestor, false);
    MOZ_DIAGNOSTIC_ASSERT(
        !mRegisteredClosestCommonInclusiveAncestor,
        "How can we have a registered common ancestor when we "
        "just unregistered?");
    MOZ_DIAGNOSTIC_ASSERT(
        !isInList(),
        "Shouldn't be registered if we have no "
        "mRegisteredClosestCommonInclusiveAncestor after unregistering");
  }
}

void AbstractRange::RegisterClosestCommonInclusiveAncestor(nsINode* aNode) {
  MOZ_ASSERT(aNode, "bad arg");

  MOZ_DIAGNOSTIC_ASSERT(IsInAnySelection(),
                        "registering range not in selection");

  mRegisteredClosestCommonInclusiveAncestor = aNode;

  MarkDescendants(*aNode);

  UniquePtr<LinkedList<AbstractRange>>& ranges =
      aNode->GetClosestCommonInclusiveAncestorRangesPtr();
  if (!ranges) {
    ranges = MakeUnique<LinkedList<AbstractRange>>();
  }

  MOZ_DIAGNOSTIC_ASSERT(!isInList());
  ranges->insertBack(this);
  aNode->SetClosestCommonInclusiveAncestorForRangeInSelection();
}

void AbstractRange::UnregisterClosestCommonInclusiveAncestor(
    nsINode* aNode, bool aIsUnlinking) {
  MOZ_ASSERT(aNode, "bad arg");
  NS_ASSERTION(aNode->IsClosestCommonInclusiveAncestorForRangeInSelection(),
               "wrong node");
  MOZ_DIAGNOSTIC_ASSERT(aNode == mRegisteredClosestCommonInclusiveAncestor,
                        "wrong node");
  LinkedList<AbstractRange>* ranges =
      aNode->GetExistingClosestCommonInclusiveAncestorRanges();
  MOZ_ASSERT(ranges);

  mRegisteredClosestCommonInclusiveAncestor = nullptr;

#ifdef DEBUG
  bool found = false;
  for (AbstractRange* range : *ranges) {
    if (range == this) {
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found,
             "We should be in the list on our registered common ancestor");
#endif  // DEBUG

  remove();

  // We don't want to waste time unmarking flags on nodes that are
  // being unlinked anyway.
  if (!aIsUnlinking && ranges->isEmpty()) {
    aNode->ClearClosestCommonInclusiveAncestorForRangeInSelection();
    UnmarkDescendants(*aNode);
  }
}

void AbstractRange::UpdateCommonAncestorIfNecessary() {
  nsINode* oldCommonAncestor = mRegisteredClosestCommonInclusiveAncestor;
  nsINode* newCommonAncestor = GetClosestCommonInclusiveAncestor();
  if (newCommonAncestor != oldCommonAncestor) {
    if (oldCommonAncestor) {
      UnregisterClosestCommonInclusiveAncestor(oldCommonAncestor, false);
    }
    if (newCommonAncestor) {
      RegisterClosestCommonInclusiveAncestor(newCommonAncestor);
    } else {
      MOZ_DIAGNOSTIC_ASSERT(!mIsPositioned, "unexpected disconnected nodes");
      mSelections.Clear();
      MOZ_DIAGNOSTIC_ASSERT(
          !mRegisteredClosestCommonInclusiveAncestor,
          "How can we have a registered common ancestor when we "
          "didn't register ourselves?");
      MOZ_DIAGNOSTIC_ASSERT(!isInList(),
                            "Shouldn't be registered if we have no "
                            "mRegisteredClosestCommonInclusiveAncestor");
    }
  }
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
