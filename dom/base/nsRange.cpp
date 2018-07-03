/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the DOM Range object.
 */

#include "nscore.h"
#include "nsRange.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsError.h"
#include "nsIContentIterator.h"
#include "nsINodeList.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsTextFrame.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Likely.h"
#include "nsCSSFrameConstructor.h"
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsComputedDOMStyle.h"
#include "mozilla/dom/InspectorFontFace.h"

using namespace mozilla;
using namespace mozilla::dom;

JSObject*
nsRange::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Range_Binding::Wrap(aCx, this, aGivenProto);
}

DocGroup*
nsRange::GetDocGroup() const
{
  return mOwner ? mOwner->GetDocGroup() : nullptr;
}

/******************************************************
 * stack based utilty class for managing monitor
 ******************************************************/

static void InvalidateAllFrames(nsINode* aNode)
{
  MOZ_ASSERT(aNode, "bad arg");

  nsIFrame* frame = nullptr;
  switch (aNode->NodeType()) {
    case nsINode::TEXT_NODE:
    case nsINode::ELEMENT_NODE:
    {
      nsIContent* content = static_cast<nsIContent*>(aNode);
      frame = content->GetPrimaryFrame();
      break;
    }
    case nsINode::DOCUMENT_NODE:
    {
      nsIDocument* doc = static_cast<nsIDocument*>(aNode);
      nsIPresShell* shell = doc ? doc->GetShell() : nullptr;
      frame = shell ? shell->GetRootFrame() : nullptr;
      break;
    }
  }
  for (nsIFrame* f = frame; f; f = f->GetNextContinuation()) {
    f->InvalidateFrameSubtree();
  }
}

// Utility routine to detect if a content node is completely contained in a range
// If outNodeBefore is returned true, then the node starts before the range does.
// If outNodeAfter is returned true, then the node ends after the range does.
// Note that both of the above might be true.
// If neither are true, the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range!

// static
nsresult
nsRange::CompareNodeToRange(nsINode* aNode, nsRange* aRange,
                            bool *outNodeBefore, bool *outNodeAfter)
{
  NS_ENSURE_STATE(aNode);
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.

  if (!aRange || !aRange->IsPositioned())
    return NS_ERROR_UNEXPECTED;

  // gather up the dom point info
  int32_t nodeStart, nodeEnd;
  nsINode* parent = aNode->GetParentNode();
  if (!parent) {
    // can't make a parent/offset pair to represent start or
    // end of the root node, because it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    parent = aNode;
    nodeStart = 0;
    uint32_t childCount = aNode->GetChildCount();
    MOZ_ASSERT(childCount <= INT32_MAX,
               "There shouldn't be over INT32_MAX children");
    nodeEnd = static_cast<int32_t>(childCount);
  }
  else {
    nodeStart = parent->ComputeIndexOf(aNode);
    nodeEnd = nodeStart + 1;
    MOZ_ASSERT(nodeStart < nodeEnd, "nodeStart shouldn't be INT32_MAX");
  }

  nsINode* rangeStartContainer = aRange->GetStartContainer();
  nsINode* rangeEndContainer = aRange->GetEndContainer();
  uint32_t rangeStartOffset = aRange->StartOffset();
  uint32_t rangeEndOffset = aRange->EndOffset();

  // is RANGE(start) <= NODE(start) ?
  bool disconnected = false;
  *outNodeBefore =
    nsContentUtils::ComparePoints(rangeStartContainer,
                                  static_cast<int32_t>(rangeStartOffset),
                                  parent, nodeStart,
                                  &disconnected) > 0;
  NS_ENSURE_TRUE(!disconnected, NS_ERROR_DOM_WRONG_DOCUMENT_ERR);

  // is RANGE(end) >= NODE(end) ?
  *outNodeAfter =
    nsContentUtils::ComparePoints(rangeEndContainer,
                                  static_cast<int32_t>(rangeEndOffset),
                                  parent, nodeEnd,
                                  &disconnected) < 0;
  NS_ENSURE_TRUE(!disconnected, NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
  return NS_OK;
}

static nsINode*
GetNextRangeCommonAncestor(nsINode* aNode)
{
  while (aNode && !aNode->IsCommonAncestorForRangeInSelection()) {
    if (!aNode->IsDescendantOfCommonAncestorForRangeInSelection()) {
      return nullptr;
    }
    aNode = aNode->GetParentNode();
  }
  return aNode;
}

/**
 * A Comparator suitable for mozilla::BinarySearchIf for searching a collection
 * of nsRange* for an overlap of (mNode, mStartOffset) .. (mNode, mEndOffset).
 */
struct IsItemInRangeComparator
{
  nsINode* mNode;
  uint32_t mStartOffset;
  uint32_t mEndOffset;

  int operator()(const nsRange* const aRange) const
  {
    int32_t cmp =
      nsContentUtils::ComparePoints(
        mNode, static_cast<int32_t>(mEndOffset),
        aRange->GetStartContainer(),
        static_cast<int32_t>(aRange->StartOffset()));
    if (cmp == 1) {
      cmp =
        nsContentUtils::ComparePoints(
          mNode, static_cast<int32_t>(mStartOffset),
          aRange->GetEndContainer(),
          static_cast<int32_t>(aRange->EndOffset()));
      if (cmp == -1) {
        return 0;
      }
      return 1;
    }
    return -1;
  }
};

/* static */ bool
nsRange::IsNodeSelected(nsINode* aNode, uint32_t aStartOffset,
                        uint32_t aEndOffset)
{
  MOZ_ASSERT(aNode, "bad arg");

  nsINode* n = GetNextRangeCommonAncestor(aNode);
  NS_ASSERTION(n || !aNode->IsSelectionDescendant(),
               "orphan selection descendant");

  // Collect the selection objects for potential ranges.
  nsTHashtable<nsPtrHashKey<Selection>> ancestorSelections;
  Selection* prevSelection = nullptr;
  uint32_t maxRangeCount = 0;
  for (; n; n = GetNextRangeCommonAncestor(n->GetParentNode())) {
    LinkedList<nsRange>* ranges = n->GetExistingCommonAncestorRanges();
    if (!ranges) {
      continue;
    }
    for (nsRange* range : *ranges) {
      MOZ_ASSERT(range->IsInSelection(),
                 "Why is this range registeed with a node?");
      // Looks like that IsInSelection() assert fails sometimes...
      if (range->IsInSelection()) {
        Selection* selection = range->mSelection;
        if (prevSelection != selection) {
          prevSelection = selection;
          ancestorSelections.PutEntry(selection);
        }
        maxRangeCount = std::max(maxRangeCount, selection->RangeCount());
      }
    }
  }

  IsItemInRangeComparator comparator = { aNode, aStartOffset, aEndOffset };
  if (!ancestorSelections.IsEmpty()) {
    for (auto iter = ancestorSelections.ConstIter(); !iter.Done(); iter.Next()) {
      Selection* selection = iter.Get()->GetKey();
      // Binary search the sorted ranges in this selection.
      // (Selection::GetRangeAt returns its ranges ordered).
      size_t low = 0;
      size_t high = selection->RangeCount();

      while (high != low) {
        size_t middle = low + (high - low) / 2;

        const nsRange* const range = selection->GetRangeAt(middle);
        int result = comparator(range);
        if (result == 0) {
          if (!range->Collapsed())
            return true;

          const nsRange* middlePlus1;
          const nsRange* middleMinus1;
          // if node end > start of middle+1, result = 1
          if (middle + 1 < high &&
              (middlePlus1 = selection->GetRangeAt(middle + 1)) &&
              nsContentUtils::ComparePoints(
                aNode, static_cast<int32_t>(aEndOffset),
                middlePlus1->GetStartContainer(),
                static_cast<int32_t>(middlePlus1->StartOffset())) > 0) {
              result = 1;
          // if node start < end of middle - 1, result = -1
          } else if (middle >= 1 &&
              (middleMinus1 = selection->GetRangeAt(middle - 1)) &&
              nsContentUtils::ComparePoints(
                aNode, static_cast<int32_t>(aStartOffset),
                middleMinus1->GetEndContainer(),
                static_cast<int32_t>(middleMinus1->EndOffset())) < 0) {
            result = -1;
          } else {
            break;
          }
        }

        if (result < 0) {
          high = middle;
        } else {
          low = middle + 1;
        }
      }
    }
  }
  return false;
}

/******************************************************
 * constructor/destructor
 ******************************************************/

nsRange::~nsRange()
{
  NS_ASSERTION(!IsInSelection(), "deleting nsRange that is in use");

  // we want the side effects (releases and list removals)
  DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr);
}

nsRange::nsRange(nsINode* aNode)
  : mRoot(nullptr)
  , mRegisteredCommonAncestor(nullptr)
  , mNextStartRef(nullptr)
  , mNextEndRef(nullptr)
  , mIsPositioned(false)
  , mMaySpanAnonymousSubtrees(false)
  , mIsGenerated(false)
  , mCalledByJS(false)
{
  MOZ_ASSERT(aNode, "range isn't in a document!");
  mOwner = aNode->OwnerDoc();
}

/* static */
nsresult
nsRange::CreateRange(nsINode* aStartContainer, uint32_t aStartOffset,
                     nsINode* aEndParent, uint32_t aEndOffset,
                     nsRange** aRange)
{
  MOZ_ASSERT(aRange);
  *aRange = nullptr;

  RefPtr<nsRange> range = new nsRange(aStartContainer);
  nsresult rv = range->SetStartAndEnd(aStartContainer, aStartOffset,
                                      aEndParent, aEndOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  range.forget(aRange);
  return NS_OK;
}

/* static */
nsresult
nsRange::CreateRange(const RawRangeBoundary& aStart,
                     const RawRangeBoundary& aEnd,
                     nsRange** aRange)
{
  RefPtr<nsRange> range = new nsRange(aStart.Container());
  nsresult rv = range->SetStartAndEnd(aStart, aEnd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  range.forget(aRange);
  return NS_OK;
}

/******************************************************
 * nsISupports
 ******************************************************/

NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_ADDREF(nsRange)
NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(
  nsRange, DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr))

// QueryInterface implementation for nsRange
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsRange)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);

  // We _could_ just rely on Reset() to UnregisterCommonAncestor(),
  // but it wouldn't know we're calling it from Unlink and so would do
  // more work than it really needs to.
  if (tmp->mRegisteredCommonAncestor) {
    tmp->UnregisterCommonAncestor(tmp->mRegisteredCommonAncestor, true);
  }

  tmp->Reset();

  // This needs to be unlinked after Reset() is called, as it controls
  // the result of IsInSelection() which is used by tmp->Reset().
  MOZ_DIAGNOSTIC_ASSERT(!tmp->isInList(),
                        "Shouldn't be registered now that we're unlinking");
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelection);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStart)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEnd)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelection)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

static void MarkDescendants(nsINode* aNode)
{
  // Set NodeIsDescendantOfCommonAncestorForRangeInSelection on aNode's
  // descendants unless aNode is already marked as a range common ancestor
  // or a descendant of one, in which case all of our descendants have the
  // bit set already.
  if (!aNode->IsSelectionDescendant()) {
    // don't set the Descendant bit on |aNode| itself
    nsINode* node = aNode->GetNextNode(aNode);
    while (node) {
      node->SetDescendantOfCommonAncestorForRangeInSelection();
      if (!node->IsCommonAncestorForRangeInSelection()) {
        node = node->GetNextNode(aNode);
      } else {
        // optimize: skip this sub-tree since it's marked already.
        node = node->GetNextNonChildNode(aNode);
      }
    }
  }
}

static void UnmarkDescendants(nsINode* aNode)
{
  // Unset NodeIsDescendantOfCommonAncestorForRangeInSelection on aNode's
  // descendants unless aNode is a descendant of another range common ancestor.
  // Also, exclude descendants of range common ancestors (but not the common
  // ancestor itself).
  if (!aNode->IsDescendantOfCommonAncestorForRangeInSelection()) {
    // we know |aNode| doesn't have any bit set
    nsINode* node = aNode->GetNextNode(aNode);
    while (node) {
      node->ClearDescendantOfCommonAncestorForRangeInSelection();
      if (!node->IsCommonAncestorForRangeInSelection()) {
        node = node->GetNextNode(aNode);
      } else {
        // We found an ancestor of an overlapping range, skip its descendants.
        node = node->GetNextNonChildNode(aNode);
      }
    }
  }
}

void
nsRange::RegisterCommonAncestor(nsINode* aNode)
{
  MOZ_ASSERT(aNode, "bad arg");

  MOZ_DIAGNOSTIC_ASSERT(IsInSelection(), "registering range not in selection");

  mRegisteredCommonAncestor = aNode;

  MarkDescendants(aNode);

  UniquePtr<LinkedList<nsRange>>& ranges = aNode->GetCommonAncestorRangesPtr();
  if (!ranges) {
    ranges = MakeUnique<LinkedList<nsRange>>();
  }

  MOZ_DIAGNOSTIC_ASSERT(!isInList());
  ranges->insertBack(this);
  aNode->SetCommonAncestorForRangeInSelection();
}

void
nsRange::UnregisterCommonAncestor(nsINode* aNode, bool aIsUnlinking)
{
  MOZ_ASSERT(aNode, "bad arg");
  NS_ASSERTION(aNode->IsCommonAncestorForRangeInSelection(), "wrong node");
  MOZ_DIAGNOSTIC_ASSERT(aNode == mRegisteredCommonAncestor, "wrong node");
  LinkedList<nsRange>* ranges = aNode->GetExistingCommonAncestorRanges();
  MOZ_ASSERT(ranges);

  mRegisteredCommonAncestor = nullptr;

#ifdef DEBUG
  bool found = false;
  for (nsRange* range : *ranges) {
    if (range == this) {
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found,
             "We should be in the list on our registered common ancestor");
#endif // DEBUG

  remove();

  // We don't want to waste time unmarking flags on nodes that are
  // being unlinked anyway.
  if (!aIsUnlinking && ranges->isEmpty()) {
    aNode->ClearCommonAncestorForRangeInSelection();
    UnmarkDescendants(aNode);
  }
}

/******************************************************
 * nsIMutationObserver implementation
 ******************************************************/
void
nsRange::CharacterDataChanged(nsIContent* aContent,
                              const CharacterDataChangeInfo& aInfo)
{
  MOZ_ASSERT(!mNextEndRef);
  MOZ_ASSERT(!mNextStartRef);
  MOZ_ASSERT(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* newRoot = nullptr;
  RawRangeBoundary newStart;
  RawRangeBoundary newEnd;

  if (aInfo.mDetails &&
      aInfo.mDetails->mType == CharacterDataChangeInfo::Details::eSplit) {
    // If the splitted text node is immediately before a range boundary point
    // that refers to a child index (i.e. its parent is the boundary container)
    // then we need to adjust the corresponding boundary to account for the new
    // text node that will be inserted. However, because the new sibling hasn't
    // been inserted yet, that would result in an invalid boundary. Therefore,
    // we store the new child in mNext*Ref to make sure we adjust the boundary
    // in the next ContentInserted or ContentAppended call.
    nsINode* parentNode = aContent->GetParentNode();
    if (parentNode == mEnd.Container()) {
      if (aContent == mEnd.Ref()) {
        MOZ_ASSERT(aInfo.mDetails->mNextSibling);
        mNextEndRef = aInfo.mDetails->mNextSibling;
      }
    }

    if (parentNode == mStart.Container()) {
      if (aContent == mStart.Ref()) {
        MOZ_ASSERT(aInfo.mDetails->mNextSibling);
        mNextStartRef = aInfo.mDetails->mNextSibling;
      }
    }
  }

  // If the changed node contains our start boundary and the change starts
  // before the boundary we'll need to adjust the offset.
  if (aContent == mStart.Container() && aInfo.mChangeStart < mStart.Offset()) {
    if (aInfo.mDetails) {
      // splitText(), aInfo->mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo.mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(mStart.Offset() <= aInfo.mChangeEnd + 1,
                   "mStart.Offset() is beyond the end of this node");
      int32_t newStartOffset = mStart.Offset() - aInfo.mChangeStart;
      newStart.Set(aInfo.mDetails->mNextSibling, newStartOffset);
      if (MOZ_UNLIKELY(aContent == mRoot)) {
        newRoot = IsValidBoundary(newStart.Container());
      }

      bool isCommonAncestor =
        IsInSelection() && mStart.Container() == mEnd.Container();
      if (isCommonAncestor) {
        UnregisterCommonAncestor(mStart.Container(), false);
        RegisterCommonAncestor(newStart.Container());
      }
      if (mStart.Container()->IsDescendantOfCommonAncestorForRangeInSelection()) {
        newStart.Container()->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      // If boundary is inside changed text, position it before change
      // else adjust start offset for the change in length.
      int32_t newStartOffset = mStart.Offset() <= aInfo.mChangeEnd ?
        aInfo.mChangeStart :
        mStart.Offset() + aInfo.mChangeStart - aInfo.mChangeEnd +
          aInfo.mReplaceLength;
      newStart.Set(mStart.Container(), newStartOffset);
    }
  }

  // Do the same thing for the end boundary, except for splitText of a node
  // with no parent then only switch to the new node if the start boundary
  // did so too (otherwise the range would end up with disconnected nodes).
  if (aContent == mEnd.Container() && aInfo.mChangeStart < mEnd.Offset()) {
    if (aInfo.mDetails && (aContent->GetParentNode() || newStart.Container())) {
      // splitText(), aInfo.mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo.mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(mEnd.Offset() <= aInfo.mChangeEnd + 1,
                   "mEnd.Offset() is beyond the end of this node");
      newEnd.Set(aInfo.mDetails->mNextSibling, mEnd.Offset() - aInfo.mChangeStart);

      bool isCommonAncestor =
        IsInSelection() && mStart.Container() == mEnd.Container();
      if (isCommonAncestor && !newStart.Container()) {
        // The split occurs inside the range.
        UnregisterCommonAncestor(mStart.Container(), false);
        RegisterCommonAncestor(mStart.Container()->GetParentNode());
        newEnd.Container()->SetDescendantOfCommonAncestorForRangeInSelection();
      } else if (mEnd.Container()->
                   IsDescendantOfCommonAncestorForRangeInSelection()) {
        newEnd.Container()->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      int32_t newEndOffset = mEnd.Offset() <= aInfo.mChangeEnd ?
        aInfo.mChangeStart :
        mEnd.Offset() + aInfo.mChangeStart - aInfo.mChangeEnd +
          aInfo.mReplaceLength;
      newEnd.Set(mEnd.Container(), newEndOffset);
    }
  }

  if (aInfo.mDetails &&
      aInfo.mDetails->mType == CharacterDataChangeInfo::Details::eMerge) {
    // normalize(), aInfo.mDetails->mNextSibling is the merged text node
    // that will be removed
    nsIContent* removed = aInfo.mDetails->mNextSibling;
    if (removed == mStart.Container()) {
      newStart.Set(aContent, mStart.Offset() + aInfo.mChangeStart);
      if (MOZ_UNLIKELY(removed == mRoot)) {
        newRoot = IsValidBoundary(newStart.Container());
      }
    }
    if (removed == mEnd.Container()) {
      newEnd.Set(aContent, mEnd.Offset() + aInfo.mChangeStart);
      if (MOZ_UNLIKELY(removed == mRoot)) {
        newRoot = IsValidBoundary(newEnd.Container());
      }
    }
    // When the removed text node's parent is one of our boundary nodes we may
    // need to adjust the offset to account for the removed node. However,
    // there will also be a ContentRemoved notification later so the only cases
    // we need to handle here is when the removed node is the text node after
    // the boundary.  (The m*Offset > 0 check is an optimization - a boundary
    // point before the first child is never affected by normalize().)
    nsINode* parentNode = aContent->GetParentNode();
    if (parentNode == mStart.Container() && mStart.Offset() > 0 &&
        mStart.Offset() < parentNode->GetChildCount() &&
        removed == mStart.GetChildAtOffset()) {
      newStart.Set(aContent, aInfo.mChangeStart);
    }
    if (parentNode == mEnd.Container() && mEnd.Offset() > 0 &&
        mEnd.Offset() < parentNode->GetChildCount() &&
        removed == mEnd.GetChildAtOffset()) {
      newEnd.Set(aContent, aInfo.mChangeEnd);
    }
  }

  if (newStart.IsSet() || newEnd.IsSet()) {
    if (!newStart.IsSet()) {
      newStart = mStart;
    }
    if (!newEnd.IsSet()) {
      newEnd = mEnd;
    }
    DoSetRange(newStart, newEnd,
               newRoot ? newRoot : mRoot.get(),
               !newEnd.Container()->GetParentNode() || !newStart.Container()->GetParentNode());
  }
}

void
nsRange::ContentAppended(nsIContent*  aFirstNewContent)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* container = aFirstNewContent->GetParentNode();
  MOZ_ASSERT(container);
  if (container->IsSelectionDescendant() && IsInSelection()) {
    nsINode* child = aFirstNewContent;
    while (child) {
      if (!child->IsDescendantOfCommonAncestorForRangeInSelection()) {
        MarkDescendants(child);
        child->SetDescendantOfCommonAncestorForRangeInSelection();
      }
      child = child->GetNextSibling();
    }
  }

  if (mNextStartRef || mNextEndRef) {
    // A splitText has occurred, if any mNext*Ref was set, we need to adjust
    // the range boundaries.
    if (mNextStartRef) {
      mStart.SetAfterRef(mStart.Container(), mNextStartRef);
      MOZ_ASSERT(mNextStartRef == aFirstNewContent);
      mNextStartRef = nullptr;
    }
    if (mNextEndRef) {
      mEnd.SetAfterRef(mEnd.Container(), mNextEndRef);
      MOZ_ASSERT(mNextEndRef == aFirstNewContent);
      mNextEndRef = nullptr;
    }
    DoSetRange(mStart.AsRaw(), mEnd.AsRaw(), mRoot, true);
  }
}

void
nsRange::ContentInserted(nsIContent* aChild)
{
  MOZ_ASSERT(mIsPositioned, "shouldn't be notified if not positioned");

  bool updateBoundaries = false;
  nsINode* container = aChild->GetParentNode();
  MOZ_ASSERT(container);
  RawRangeBoundary newStart(mStart);
  RawRangeBoundary newEnd(mEnd);
  MOZ_ASSERT(aChild->GetParentNode() == container);

  // Invalidate boundary offsets if a child that may have moved them was
  // inserted.
  if (container == mStart.Container()) {
    newStart.InvalidateOffset();
    updateBoundaries = true;
  }

  if (container == mEnd.Container()) {
    newEnd.InvalidateOffset();
    updateBoundaries = true;
  }

  if (container->IsSelectionDescendant() &&
      !aChild->IsDescendantOfCommonAncestorForRangeInSelection()) {
    MarkDescendants(aChild);
    aChild->SetDescendantOfCommonAncestorForRangeInSelection();
  }

  if (mNextStartRef || mNextEndRef) {
    if (mNextStartRef) {
      newStart.SetAfterRef(mStart.Container(), mNextStartRef);
      MOZ_ASSERT(mNextStartRef == aChild);
      mNextStartRef = nullptr;
    }
    if (mNextEndRef) {
      newEnd.SetAfterRef(mEnd.Container(), mNextEndRef);
      MOZ_ASSERT(mNextEndRef == aChild);
      mNextEndRef = nullptr;
    }

    updateBoundaries = true;
  }

  if (updateBoundaries) {
    DoSetRange(newStart, newEnd, mRoot);
  }
}

void
nsRange::ContentRemoved(nsIContent* aChild, nsIContent* aPreviousSibling)
{
  MOZ_ASSERT(mIsPositioned, "shouldn't be notified if not positioned");
  nsINode* container = aChild->GetParentNode();
  MOZ_ASSERT(container);

  RawRangeBoundary newStart;
  RawRangeBoundary newEnd;
  Maybe<bool> gravitateStart;
  bool gravitateEnd;

  // Adjust position if a sibling was removed...
  if (container == mStart.Container()) {
    // We're only interested if our boundary reference was removed, otherwise
    // we can just invalidate the offset.
    if (aChild == mStart.Ref()) {
      newStart.SetAfterRef(container, aPreviousSibling);
    } else {
      newStart = mStart;
      newStart.InvalidateOffset();
    }
  } else {
    gravitateStart = Some(nsContentUtils::ContentIsDescendantOf(mStart.Container(), aChild));
    if (gravitateStart.value()) {
      newStart.SetAfterRef(container, aPreviousSibling);
    }
  }

  // Do same thing for end boundry.
  if (container == mEnd.Container()) {
    if (aChild == mEnd.Ref()) {
      newEnd.SetAfterRef(container, aPreviousSibling);
    } else {
      newEnd = mEnd;
      newEnd.InvalidateOffset();
    }
  } else {
    if (mStart.Container() == mEnd.Container() && gravitateStart.isSome()) {
      gravitateEnd = gravitateStart.value();
    } else {
      gravitateEnd = nsContentUtils::ContentIsDescendantOf(mEnd.Container(), aChild);
    }
    if (gravitateEnd) {
      newEnd.SetAfterRef(container, aPreviousSibling);
    }
  }

  if (newStart.IsSet() || newEnd.IsSet()) {
    DoSetRange(newStart.IsSet() ? newStart : mStart.AsRaw(),
               newEnd.IsSet() ? newEnd : mEnd.AsRaw(), mRoot);
  }

  MOZ_ASSERT(mStart.Ref() != aChild);
  MOZ_ASSERT(mEnd.Ref() != aChild);

  if (container->IsSelectionDescendant() &&
      aChild->IsDescendantOfCommonAncestorForRangeInSelection()) {
    aChild->ClearDescendantOfCommonAncestorForRangeInSelection();
    UnmarkDescendants(aChild);
  }
}

void
nsRange::ParentChainChanged(nsIContent *aContent)
{
  NS_ASSERTION(mRoot == aContent, "Wrong ParentChainChanged notification?");
  nsINode* newRoot = IsValidBoundary(mStart.Container());
  NS_ASSERTION(newRoot, "No valid boundary or root found!");
  if (newRoot != IsValidBoundary(mEnd.Container())) {
    // Sometimes ordering involved in cycle collection can lead to our
    // start parent and/or end parent being disconnected from our root
    // without our getting a ContentRemoved notification.
    // See bug 846096 for more details.
    NS_ASSERTION(mEnd.Container()->IsInNativeAnonymousSubtree(),
                 "This special case should happen only with "
                 "native-anonymous content");
    // When that happens, bail out and set pointers to null; since we're
    // in cycle collection and unreachable it shouldn't matter.
    Reset();
    return;
  }
  // This is safe without holding a strong ref to self as long as the change
  // of mRoot is the last thing in DoSetRange.
  DoSetRange(mStart.AsRaw(), mEnd.AsRaw(), newRoot);
}

bool
nsRange::IsPointInRange(const RawRangeBoundary& aPoint, ErrorResult& aRv)
{
  uint16_t compareResult = ComparePoint(aPoint, aRv);
  // If the node isn't in the range's document, it clearly isn't in the range.
  if (aRv.ErrorCodeIs(NS_ERROR_DOM_WRONG_DOCUMENT_ERR)) {
    aRv.SuppressException();
    return false;
  }

  return compareResult == 0;
}

int16_t
nsRange::ComparePoint(const RawRangeBoundary& aPoint, ErrorResult& aRv)
{
  if (NS_WARN_IF(!aPoint.IsSet())) {
    // FYI: Shouldn't reach this case if it's called by JS.  Therefore, it's
    //      okay to warn.
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return 0;
  }

  // our range is in a good state?
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  if (!nsContentUtils::ContentIsDescendantOf(aPoint.Container(), mRoot)) {
    aRv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
    return 0;
  }

  if (aPoint.Container()->NodeType() == nsINode::DOCUMENT_TYPE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return 0;
  }

  if (aPoint.Offset() > aPoint.Container()->Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  int32_t cmp = nsContentUtils::ComparePoints(aPoint, mStart.AsRaw());
  if (cmp <= 0) {
    return cmp;
  }
  if (nsContentUtils::ComparePoints(mEnd.AsRaw(), aPoint) == -1) {
    return 1;
  }

  return 0;
}

bool
nsRange::IntersectsNode(nsINode& aNode, ErrorResult& aRv)
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return false;
  }

  // Step 3.
  nsINode* parent = aNode.GetParentNode();
  if (!parent) {
    // Steps 2 and 4.
    // |parent| is null, so |node|'s root is |node| itself.
    return GetRoot() == &aNode;
  }

  // Step 5.
  int32_t nodeIndex = parent->ComputeIndexOf(&aNode);

  // Steps 6-7.
  // Note: if disconnected is true, ComparePoints returns 1.
  bool disconnected = false;
  bool result = nsContentUtils::ComparePoints(mStart.Container(), mStart.Offset(),
                                             parent, nodeIndex + 1,
                                             &disconnected) < 0 &&
               nsContentUtils::ComparePoints(parent, nodeIndex,
                                             mEnd.Container(), mEnd.Offset(),
                                             &disconnected) < 0;

  // Step 2.
  if (disconnected) {
    result = false;
  }
  return result;
}

void
nsRange::NotifySelectionListenersAfterRangeSet()
{
  if (mSelection) {
    // Our internal code should not move focus with using this instance while
    // it's calling Selection::NotifySelectionListeners() which may move focus
    // or calls selection listeners.  So, let's set mCalledByJS to false here
    // since non-*JS() methods don't set it to false.
    AutoCalledByJSRestore calledByJSRestorer(*this);
    mCalledByJS = false;
    // Be aware, this range may be modified or stop being a range for selection
    // after this call.  Additionally, the selection instance may have gone.
    RefPtr<Selection> selection = mSelection;
    selection->NotifySelectionListeners(calledByJSRestorer.SavedValue());
  }
}

/******************************************************
 * Private helper routines
 ******************************************************/

// It's important that all setting of the range start/end points
// go through this function, which will do all the right voodoo
// for content notification of range ownership.
// Calling DoSetRange with either parent argument null will collapse
// the range to have both endpoints point to the other node
void
nsRange::DoSetRange(const RawRangeBoundary& aStart,
                    const RawRangeBoundary& aEnd,
                    nsINode* aRoot, bool aNotInsertedYet)
{
  MOZ_ASSERT((aStart.IsSet() && aEnd.IsSet() && aRoot) ||
             (!aStart.IsSet() && !aEnd.IsSet() && !aRoot),
             "Set all or none");

  MOZ_ASSERT(!aRoot || aNotInsertedYet ||
             (nsContentUtils::ContentIsDescendantOf(aStart.Container(), aRoot) &&
              nsContentUtils::ContentIsDescendantOf(aEnd.Container(), aRoot) &&
              aRoot == IsValidBoundary(aStart.Container()) &&
              aRoot == IsValidBoundary(aEnd.Container())),
             "Wrong root");

  MOZ_ASSERT(!aRoot ||
             (aStart.Container()->IsContent() &&
              aEnd.Container()->IsContent() &&
              aRoot ==
               static_cast<nsIContent*>(aStart.Container())->GetBindingParent() &&
              aRoot ==
               static_cast<nsIContent*>(aEnd.Container())->GetBindingParent()) ||
             (!aRoot->GetParentNode() &&
              (aRoot->IsDocument() ||
               aRoot->IsAttr() ||
               aRoot->IsDocumentFragment() ||
                /*For backward compatibility*/
               aRoot->IsContent())),
             "Bad root");

  if (mRoot != aRoot) {
    if (mRoot) {
      mRoot->RemoveMutationObserver(this);
    }
    if (aRoot) {
      aRoot->AddMutationObserver(this);
    }
  }
  bool checkCommonAncestor =
    (mStart.Container() != aStart.Container() || mEnd.Container() != aEnd.Container()) &&
    IsInSelection() && !aNotInsertedYet;

  // GetCommonAncestor is unreliable while we're unlinking (could
  // return null if our start/end have already been unlinked), so make
  // sure to not use it here to determine our "old" current ancestor.
  mStart = aStart;
  mEnd = aEnd;

  mIsPositioned = !!mStart.Container();
  if (checkCommonAncestor) {
    nsINode* oldCommonAncestor = mRegisteredCommonAncestor;
    nsINode* newCommonAncestor = GetCommonAncestor();
    if (newCommonAncestor != oldCommonAncestor) {
      if (oldCommonAncestor) {
        UnregisterCommonAncestor(oldCommonAncestor, false);
      }
      if (newCommonAncestor) {
        RegisterCommonAncestor(newCommonAncestor);
      } else {
        NS_ASSERTION(!mIsPositioned, "unexpected disconnected nodes");
        mSelection = nullptr;
        MOZ_DIAGNOSTIC_ASSERT(!mRegisteredCommonAncestor,
                              "How can we have a registered common ancestor when we "
                              "didn't register ourselves?");
        MOZ_DIAGNOSTIC_ASSERT(!isInList(),
                              "Shouldn't be registered if we have no "
                              "mRegisteredCommonAncestor");
      }
    }
  }

  // This needs to be the last thing this function does, other than notifying
  // selection listeners. See comment in ParentChainChanged.
  mRoot = aRoot;

  // Notify any selection listeners. This has to occur last because otherwise the world
  // could be observed by a selection listener while the range was in an invalid state.
  // So we run it off of a script runner to ensure it runs after the mutation observers
  // have finished running.
  if (mSelection) {
    nsContentUtils::AddScriptRunner(NewRunnableMethod(
                                    "NotifySelectionListenersAfterRangeSet",
                                    this,
                                    &nsRange::NotifySelectionListenersAfterRangeSet));
  }
}

static int32_t
IndexOf(nsINode* aChild)
{
  nsINode* parent = aChild->GetParentNode();

  return parent ? parent->ComputeIndexOf(aChild) : -1;
}

void
nsRange::SetSelection(mozilla::dom::Selection* aSelection)
{
  if (mSelection == aSelection) {
    return;
  }

  // At least one of aSelection and mSelection must be null
  // aSelection will be null when we are removing from a selection
  // and a range can't be in more than one selection at a time,
  // thus mSelection must be null too.
  MOZ_ASSERT(!aSelection || !mSelection);

  // Extra step in case our parent failed to ensure the above
  // invariant.
  if (aSelection && mSelection) {
    mSelection->RemoveRange(*this, IgnoreErrors());
  }

  mSelection = aSelection;
  if (mSelection) {
    nsINode* commonAncestor = GetCommonAncestor();
    NS_ASSERTION(commonAncestor, "unexpected disconnected nodes");
    RegisterCommonAncestor(commonAncestor);
  } else {
    UnregisterCommonAncestor(mRegisteredCommonAncestor, false);
    MOZ_DIAGNOSTIC_ASSERT(!mRegisteredCommonAncestor,
                          "How can we have a registered common ancestor when we "
                          "just unregistered?");
    MOZ_DIAGNOSTIC_ASSERT(!isInList(),
                          "Shouldn't be registered if we have no "
                          "mRegisteredCommonAncestor after unregistering");
  }
}

nsINode*
nsRange::GetCommonAncestor() const
{
  return mIsPositioned ?
    nsContentUtils::GetCommonAncestor(mStart.Container(), mEnd.Container()) :
    nullptr;
}

void
nsRange::Reset()
{
  DoSetRange(RawRangeBoundary(), RawRangeBoundary(), nullptr);
}

/******************************************************
 * public functionality
 ******************************************************/

nsINode*
nsRange::GetStartContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mStart.Container();
}

uint32_t
nsRange::GetStartOffset(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  return mStart.Offset();
}

nsINode*
nsRange::GetEndContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mEnd.Container();
}

uint32_t
nsRange::GetEndOffset(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  return mEnd.Offset();
}

nsINode*
nsRange::GetCommonAncestorContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return nsContentUtils::GetCommonAncestor(mStart.Container(), mEnd.Container());
}

/* static */
bool
nsRange::IsValidOffset(nsINode* aNode, uint32_t aOffset)
{
  return aNode &&
         IsValidOffset(aOffset) &&
         static_cast<size_t>(aOffset) <= aNode->Length();
}

/* static */
nsINode*
nsRange::ComputeRootNode(nsINode* aNode, bool aMaySpanAnonymousSubtrees)
{
  if (!aNode) {
    return nullptr;
  }

  if (aNode->IsContent()) {
    if (aNode->NodeInfo()->NameAtom() == nsGkAtoms::documentTypeNodeName) {
      return nullptr;
    }

    nsIContent* content = aNode->AsContent();
    if (!aMaySpanAnonymousSubtrees) {
      // If the node is in a shadow tree then the ShadowRoot is the root.
      ShadowRoot* containingShadow = content->GetContainingShadow();
      if (containingShadow) {
        return containingShadow;
      }

      // If the node has a binding parent, that should be the root.
      // XXXbz maybe only for native anonymous content?
      nsINode* root = content->GetBindingParent();
      if (root) {
        return root;
      }
    }
  }

  // Elements etc. must be in document or in document fragment,
  // text nodes in document, in document fragment or in attribute.
  nsINode* root = aNode->GetUncomposedDoc();
  if (root) {
    return root;
  }

  root = aNode->SubtreeRoot();

  NS_ASSERTION(!root->IsDocument(),
               "GetUncomposedDoc should have returned a doc");

  // We allow this because of backward compatibility.
  return root;
}

/* static */
bool
nsRange::IsValidPoints(nsINode* aStartContainer, uint32_t aStartOffset,
                       nsINode* aEndContainer, uint32_t aEndOffset)
{
  // Use NS_WARN_IF() only for the cases where the arguments are unexpected.
  if (NS_WARN_IF(!aStartContainer) || NS_WARN_IF(!aEndContainer) ||
      NS_WARN_IF(!IsValidOffset(aStartContainer, aStartOffset)) ||
      NS_WARN_IF(!IsValidOffset(aEndContainer, aEndOffset))) {
    return false;
  }

  // Otherwise, don't use NS_WARN_IF() for preventing to make console messy.
  // Instead, check one by one since it is easier to catch the error reason
  // with debugger.

  if (ComputeRootNode(aStartContainer) != ComputeRootNode(aEndContainer)) {
    return false;
  }

  bool disconnected = false;
  int32_t order =
    nsContentUtils::ComparePoints(aStartContainer,
                                    static_cast<int32_t>(aStartOffset),
                                    aEndContainer,
                                    static_cast<int32_t>(aEndOffset),
                                    &disconnected);
  // FYI: disconnected should be false unless |order| is 1.
  if (order == 1 || NS_WARN_IF(disconnected)) {
    return false;
  }

  return true;
}

void
nsRange::SetStartJS(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetStart(aNode, aOffset, aErr);
}

void
nsRange::SetStart(nsINode& aNode, uint32_t aOffset, ErrorResult& aRv)
{
 if (!nsContentUtils::LegacyIsCallerNativeCode() &&
     !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  SetStart(RawRangeBoundary(&aNode, aOffset), aRv);
}

void
nsRange::SetStart(const RawRangeBoundary& aPoint, ErrorResult& aRv)
{
  nsINode* newRoot = IsValidBoundary(aPoint.Container());
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  if (!aPoint.IsSetAndValid()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new start is after end.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(aPoint, mEnd.AsRaw()) == 1) {
    DoSetRange(aPoint, aPoint, newRoot);
    return;
  }

  DoSetRange(aPoint, mEnd.AsRaw(), mRoot);
}

void
nsRange::SetStartBeforeJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetStartBefore(aNode, aErr);
}

void
nsRange::SetStartBefore(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  // If the node is being removed from its parent, GetContainerAndOffsetBefore()
  // returns nullptr.  Then, SetStart() will throw
  // NS_ERROR_DOM_INVALID_NODE_TYPE_ERR.
  uint32_t offset = UINT32_MAX;
  nsINode* container = GetContainerAndOffsetBefore(&aNode, &offset);
  aRv = SetStart(container, offset);
}

void
nsRange::SetStartAfterJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetStartAfter(aNode, aErr);
}

void
nsRange::SetStartAfter(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  // If the node is being removed from its parent, GetContainerAndOffsetAfter()
  // returns nullptr.  Then, SetStart() will throw
  // NS_ERROR_DOM_INVALID_NODE_TYPE_ERR.
  uint32_t offset = UINT32_MAX;
  nsINode* container = GetContainerAndOffsetAfter(&aNode, &offset);
  aRv = SetStart(container, offset);
}

void
nsRange::SetEndJS(nsINode& aNode, uint32_t aOffset, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetEnd(aNode, aOffset, aErr);
}

void
nsRange::SetEnd(nsINode& aNode, uint32_t aOffset, ErrorResult& aRv)
{
 if (!nsContentUtils::LegacyIsCallerNativeCode() &&
     !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }
  AutoInvalidateSelection atEndOfBlock(this);
  SetEnd(RawRangeBoundary(&aNode, aOffset), aRv);
}

void
nsRange::SetEnd(const RawRangeBoundary& aPoint, ErrorResult& aRv)
{
  nsINode* newRoot = IsValidBoundary(aPoint.Container());
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  if (!aPoint.IsSetAndValid()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new end is before start.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(mStart.AsRaw(), aPoint) == 1) {
    DoSetRange(aPoint, aPoint, newRoot);
    return;
  }

  DoSetRange(mStart.AsRaw(), aPoint, mRoot);
}

void
nsRange::SelectNodesInContainer(nsINode* aContainer,
                                nsIContent* aStartContent,
                                nsIContent* aEndContent)
{
  MOZ_ASSERT(aContainer);
  MOZ_ASSERT(aContainer->ComputeIndexOf(aStartContent) <=
               aContainer->ComputeIndexOf(aEndContent));
  MOZ_ASSERT(aStartContent && aContainer->ComputeIndexOf(aStartContent) != -1);
  MOZ_ASSERT(aEndContent && aContainer->ComputeIndexOf(aEndContent) != -1);

  nsINode* newRoot = ComputeRootNode(aContainer, mMaySpanAnonymousSubtrees);
  MOZ_ASSERT(newRoot);
  if (!newRoot) {
    return;
  }

  RawRangeBoundary start(aContainer, aStartContent->GetPreviousSibling());
  RawRangeBoundary end(aContainer, aEndContent);
  DoSetRange(start, end, newRoot);
}

nsresult
nsRange::SetStartAndEnd(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd)
{
  if (NS_WARN_IF(!aStart.IsSet()) || NS_WARN_IF(!aEnd.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  nsINode* newStartRoot = IsValidBoundary(aStart.Container());
  if (!newStartRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aStart.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aStart.Container() == aEnd.Container()) {
    if (!aEnd.IsSetAndValid()) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }
    // XXX: Offsets - handle this more efficiently.
    // If the end offset is less than the start offset, this should be
    // collapsed at the end offset.
    if (aStart.Offset() > aEnd.Offset()) {
      DoSetRange(aEnd, aEnd, newStartRoot);
    } else {
      DoSetRange(aStart, aEnd, newStartRoot);
    }
    return NS_OK;
  }

  nsINode* newEndRoot = IsValidBoundary(aEnd.Container());
  if (!newEndRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!aEnd.IsSetAndValid()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // If they have different root, this should be collapsed at the end point.
  if (newStartRoot != newEndRoot) {
    DoSetRange(aEnd, aEnd, newEndRoot);
    return NS_OK;
  }

  // If the end point is before the start point, this should be collapsed at
  // the end point.
  if (nsContentUtils::ComparePoints(aStart, aEnd) == 1) {
    DoSetRange(aEnd, aEnd, newEndRoot);
    return NS_OK;
  }

  // Otherwise, set the range as specified.
  DoSetRange(aStart, aEnd, newStartRoot);
  return NS_OK;
}

void
nsRange::SetEndBeforeJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetEndBefore(aNode, aErr);
}

void
nsRange::SetEndBefore(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  // If the node is being removed from its parent, GetContainerAndOffsetBefore()
  // returns nullptr.  Then, SetEnd() will throw
  // NS_ERROR_DOM_INVALID_NODE_TYPE_ERR.
  uint32_t offset = UINT32_MAX;
  nsINode* container = GetContainerAndOffsetBefore(&aNode, &offset);
  aRv = SetEnd(container, offset);
}

void
nsRange::SetEndAfterJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SetEndAfter(aNode, aErr);
}

void
nsRange::SetEndAfter(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  // If the node is being removed from its parent, GetContainerAndOffsetAfter()
  // returns nullptr.  Then, SetEnd() will throw
  // NS_ERROR_DOM_INVALID_NODE_TYPE_ERR.
  uint32_t offset = UINT32_MAX;
  nsINode* container = GetContainerAndOffsetAfter(&aNode, &offset);
  aRv = SetEnd(container, offset);
}

void
nsRange::Collapse(bool aToStart)
{
  if (!mIsPositioned)
    return;

  AutoInvalidateSelection atEndOfBlock(this);
  if (aToStart) {
    DoSetRange(mStart.AsRaw(), mStart.AsRaw(), mRoot);
  } else {
    DoSetRange(mEnd.AsRaw(), mEnd.AsRaw(), mRoot);
  }
}

void
nsRange::CollapseJS(bool aToStart)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  Collapse(aToStart);
}

void
nsRange::SelectNodeJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SelectNode(aNode, aErr);
}

void
nsRange::SelectNode(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsINode* container = aNode.GetParentNode();
  nsINode* newRoot = IsValidBoundary(container);
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  int32_t index = container->ComputeIndexOf(&aNode);
  // MOZ_ASSERT(index != -1);
  // We need to compute the index here unfortunately, because, while we have
  // support for XBL, |container| may be the node's binding parent without
  // actually containing it.
  if (NS_WARN_IF(index < 0) ||
      !IsValidOffset(static_cast<uint32_t>(index)) ||
      !IsValidOffset(static_cast<uint32_t>(index) + 1)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  DoSetRange(RawRangeBoundary(container, index),
             RawRangeBoundary(container, index + 1), newRoot);
}

void
nsRange::SelectNodeContentsJS(nsINode& aNode, ErrorResult& aErr)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  SelectNodeContents(aNode, aErr);
}

void
nsRange::SelectNodeContents(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsINode* newRoot = IsValidBoundary(&aNode);
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  DoSetRange(RawRangeBoundary(&aNode, 0),
             RawRangeBoundary(&aNode, aNode.Length()), newRoot);
}

// The Subtree Content Iterator only returns subtrees that are
// completely within a given range. It doesn't return a CharacterData
// node that contains either the start or end point of the range.,
// nor does it return element nodes when nothing in the element is selected.
// We need an iterator that will also include these start/end points
// so that our methods/algorithms aren't cluttered with special
// case code that tries to include these points while iterating.
//
// The RangeSubtreeIterator class mimics the nsIContentIterator
// methods we need, so should the Content Iterator support the
// start/end points in the future, we can switchover relatively
// easy.

class MOZ_STACK_CLASS RangeSubtreeIterator
{
private:

  enum RangeSubtreeIterState { eDone=0,
                               eUseStart,
                               eUseIterator,
                               eUseEnd };

  nsCOMPtr<nsIContentIterator>  mIter;
  RangeSubtreeIterState         mIterState;

  nsCOMPtr<nsINode> mStart;
  nsCOMPtr<nsINode> mEnd;

public:

  RangeSubtreeIterator()
    : mIterState(eDone)
  {
  }
  ~RangeSubtreeIterator()
  {
  }

  nsresult Init(nsRange *aRange);
  already_AddRefed<nsINode> GetCurrentNode();
  void First();
  void Last();
  void Next();
  void Prev();

  bool IsDone()
  {
    return mIterState == eDone;
  }
};

nsresult
RangeSubtreeIterator::Init(nsRange *aRange)
{
  mIterState = eDone;
  if (aRange->Collapsed()) {
    return NS_OK;
  }

  // Grab the start point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  ErrorResult rv;
  nsCOMPtr<nsINode> node = aRange->GetStartContainer(rv);
  if (!node) return NS_ERROR_FAILURE;

  if (node->IsCharacterData() ||
      (node->IsElement() &&
       node->AsElement()->GetChildCount() == aRange->GetStartOffset(rv))) {
    mStart = node;
  }

  // Grab the end point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  node = aRange->GetEndContainer(rv);
  if (!node) return NS_ERROR_FAILURE;

  if (node->IsCharacterData() ||
      (node->IsElement() && aRange->GetEndOffset(rv) == 0)) {
    mEnd = node;
  }

  if (mStart && mStart == mEnd)
  {
    // The range starts and stops in the same CharacterData
    // node. Null out the end pointer so we only visit the
    // node once!

    mEnd = nullptr;
  }
  else
  {
    // Now create a Content Subtree Iterator to be used
    // for the subtrees between the end points!

    mIter = NS_NewContentSubtreeIterator();

    nsresult res = mIter->Init(aRange);
    if (NS_FAILED(res)) return res;

    if (mIter->IsDone())
    {
      // The subtree iterator thinks there's nothing
      // to iterate over, so just free it up so we
      // don't accidentally call into it.

      mIter = nullptr;
    }
  }

  // Initialize the iterator by calling First().
  // Note that we are ignoring the return value on purpose!

  First();

  return NS_OK;
}

already_AddRefed<nsINode>
RangeSubtreeIterator::GetCurrentNode()
{
  nsCOMPtr<nsINode> node;

  if (mIterState == eUseStart && mStart) {
    node = mStart;
  } else if (mIterState == eUseEnd && mEnd) {
    node = mEnd;
  } else if (mIterState == eUseIterator && mIter) {
    node = mIter->GetCurrentNode();
  }

  return node.forget();
}

void
RangeSubtreeIterator::First()
{
  if (mStart)
    mIterState = eUseStart;
  else if (mIter)
  {
    mIter->First();

    mIterState = eUseIterator;
  }
  else if (mEnd)
    mIterState = eUseEnd;
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Last()
{
  if (mEnd)
    mIterState = eUseEnd;
  else if (mIter)
  {
    mIter->Last();

    mIterState = eUseIterator;
  }
  else if (mStart)
    mIterState = eUseStart;
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Next()
{
  if (mIterState == eUseStart)
  {
    if (mIter)
    {
      mIter->First();

      mIterState = eUseIterator;
    }
    else if (mEnd)
      mIterState = eUseEnd;
    else
      mIterState = eDone;
  }
  else if (mIterState == eUseIterator)
  {
    mIter->Next();

    if (mIter->IsDone())
    {
      if (mEnd)
        mIterState = eUseEnd;
      else
        mIterState = eDone;
    }
  }
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Prev()
{
  if (mIterState == eUseEnd)
  {
    if (mIter)
    {
      mIter->Last();

      mIterState = eUseIterator;
    }
    else if (mStart)
      mIterState = eUseStart;
    else
      mIterState = eDone;
  }
  else if (mIterState == eUseIterator)
  {
    mIter->Prev();

    if (mIter->IsDone())
    {
      if (mStart)
        mIterState = eUseStart;
      else
        mIterState = eDone;
    }
  }
  else
    mIterState = eDone;
}


// CollapseRangeAfterDelete() is a utility method that is used by
// DeleteContents() and ExtractContents() to collapse the range
// in the correct place, under the range's root container (the
// range end points common container) as outlined by the Range spec:
//
// http://www.w3.org/TR/2000/REC-DOM-Level-2-Traversal-Range-20001113/ranges.html
// The assumption made by this method is that the delete or extract
// has been done already, and left the range in a state where there is
// no content between the 2 end points.

static nsresult
CollapseRangeAfterDelete(nsRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Check if range gravity took care of collapsing the range for us!
  if (aRange->Collapsed())
  {
    // aRange is collapsed so there's nothing for us to do.
    //
    // There are 2 possible scenarios here:
    //
    // 1. aRange could've been collapsed prior to the delete/extract,
    //    which would've resulted in nothing being removed, so aRange
    //    is already where it should be.
    //
    // 2. Prior to the delete/extract, aRange's start and end were in
    //    the same container which would mean everything between them
    //    was removed, causing range gravity to collapse the range.

    return NS_OK;
  }

  // aRange isn't collapsed so figure out the appropriate place to collapse!
  // First get both end points and their common ancestor.

  ErrorResult rv;
  nsCOMPtr<nsINode> commonAncestor = aRange->GetCommonAncestorContainer(rv);
  if (rv.Failed()) return rv.StealNSResult();

  nsCOMPtr<nsINode> startContainer = aRange->GetStartContainer(rv);
  if (rv.Failed()) return rv.StealNSResult();
  nsCOMPtr<nsINode> endContainer = aRange->GetEndContainer(rv);
  if (rv.Failed()) return rv.StealNSResult();

  // Collapse to one of the end points if they are already in the
  // commonAncestor. This should work ok since this method is called
  // immediately after a delete or extract that leaves no content
  // between the 2 end points!

  if (startContainer == commonAncestor) {
    aRange->Collapse(true);
    return NS_OK;
  }
  if (endContainer == commonAncestor) {
    aRange->Collapse(false);
    return NS_OK;
  }

  // End points are at differing levels. We want to collapse to the
  // point that is between the 2 subtrees that contain each point,
  // under the common ancestor.

  nsCOMPtr<nsINode> nodeToSelect(startContainer);

  while (nodeToSelect)
  {
    nsCOMPtr<nsINode> parent = nodeToSelect->GetParentNode();
    if (parent == commonAncestor)
      break; // We found the nodeToSelect!

    nodeToSelect = parent;
  }

  if (!nodeToSelect)
    return NS_ERROR_FAILURE; // This should never happen!

  aRange->SelectNode(*nodeToSelect, rv);
  if (rv.Failed()) return rv.StealNSResult();

  aRange->Collapse(false);
  return NS_OK;
}

NS_IMETHODIMP
PrependChild(nsINode* aContainer, nsINode* aChild)
{
  nsCOMPtr<nsINode> first = aContainer->GetFirstChild();
  ErrorResult rv;
  aContainer->InsertBefore(*aChild, first, rv);
  return rv.StealNSResult();
}

// Helper function for CutContents, making sure that the current node wasn't
// removed by mutation events (bug 766426)
static bool
ValidateCurrentNode(nsRange* aRange, RangeSubtreeIterator& aIter)
{
  bool before, after;
  nsCOMPtr<nsINode> node = aIter.GetCurrentNode();
  if (!node) {
    // We don't have to worry that the node was removed if it doesn't exist,
    // e.g., the iterator is done.
    return true;
  }

  nsresult res = nsRange::CompareNodeToRange(node, aRange, &before, &after);
  NS_ENSURE_SUCCESS(res, false);

  if (before || after) {
    if (node->IsCharacterData()) {
      // If we're dealing with the start/end container which is a character
      // node, pretend that the node is in the range.
      if (before && node == aRange->GetStartContainer()) {
        before = false;
      }
      if (after && node == aRange->GetEndContainer()) {
        after = false;
      }
    }
  }

  return !before && !after;
}

nsresult
nsRange::CutContents(DocumentFragment** aFragment)
{
  if (aFragment) {
    *aFragment = nullptr;
  }

  nsCOMPtr<nsIDocument> doc = mStart.Container()->OwnerDoc();

  ErrorResult res;
  nsCOMPtr<nsINode> commonAncestor = GetCommonAncestorContainer(res);
  NS_ENSURE_TRUE(!res.Failed(), res.StealNSResult());

  // If aFragment isn't null, create a temporary fragment to hold our return.
  RefPtr<DocumentFragment> retval;
  if (aFragment) {
    retval = new DocumentFragment(doc->NodeInfoManager());
  }
  nsCOMPtr<nsINode> commonCloneAncestor = retval.get();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(mRoot ? mRoot->OwnerDoc(): nullptr, nullptr);

  // Save the range end points locally to avoid interference
  // of Range gravity during our edits!

  nsCOMPtr<nsINode> startContainer = mStart.Container();
  uint32_t startOffset = mStart.Offset();
  nsCOMPtr<nsINode> endContainer = mEnd.Container();
  uint32_t endOffset = mEnd.Offset();

  if (retval) {
    // For extractContents(), abort early if there's a doctype (bug 719533).
    // This can happen only if the common ancestor is a document, in which case
    // we just need to find its doctype child and check if that's in the range.
    nsCOMPtr<nsIDocument> commonAncestorDocument = do_QueryInterface(commonAncestor);
    if (commonAncestorDocument) {
      RefPtr<DocumentType> doctype = commonAncestorDocument->GetDoctype();

      if (doctype &&
          nsContentUtils::ComparePoints(startContainer,
                                        static_cast<int32_t>(startOffset),
                                        doctype, 0) < 0 &&
          nsContentUtils::ComparePoints(doctype, 0,
                                        endContainer,
                                        static_cast<int32_t>(endOffset)) < 0) {
        return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
      }
    }
  }

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  nsresult rv = iter.Init(this);
  if (NS_FAILED(rv)) return rv;

  if (iter.IsDone())
  {
    // There's nothing for us to delete.
    rv = CollapseRangeAfterDelete(this);
    if (NS_SUCCEEDED(rv) && aFragment) {
      retval.forget(aFragment);
    }
    return rv;
  }

  iter.First();

  bool handled = false;

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsINode> nodeToResult;
    nsCOMPtr<nsINode> node = iter.GetCurrentNode();

    // Before we delete anything, advance the iterator to the next node that's
    // not a descendant of this one.  XXX It's a bit silly to iterate through
    // the descendants only to throw them out, we should use an iterator that
    // skips the descendants to begin with.

    iter.Next();
    nsCOMPtr<nsINode> nextNode = iter.GetCurrentNode();
    while (nextNode && nsContentUtils::ContentIsDescendantOf(nextNode, node)) {
      iter.Next();
      nextNode = iter.GetCurrentNode();
    }

    handled = false;

    // If it's CharacterData, make sure we might need to delete
    // part of the data, instead of removing the whole node.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    if (auto charData = CharacterData::FromNode(node)) {
      uint32_t dataLength = 0;

      if (node == startContainer) {
        if (node == endContainer) {
          // This range is completely contained within a single text node.
          // Delete or extract the data between startOffset and endOffset.

          if (endOffset > startOffset) {
            if (retval) {
              nsAutoString cutValue;
              ErrorResult err;
              charData->SubstringData(startOffset, endOffset - startOffset,
                                      cutValue, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              nsCOMPtr<nsINode> clone = node->CloneNode(false, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              clone->SetNodeValue(cutValue, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              nodeToResult = clone;
            }

            nsMutationGuard guard;
            ErrorResult err;
            charData->DeleteData(startOffset, endOffset - startOffset, err);
            if (NS_WARN_IF(err.Failed())) {
              return err.StealNSResult();
            }
            NS_ENSURE_STATE(!guard.Mutated(0) ||
                            ValidateCurrentNode(this, iter));
          }

          handled = true;
        }
        else  {
          // Delete or extract everything after startOffset.

          dataLength = charData->Length();

          if (dataLength >= startOffset) {
            if (retval) {
              nsAutoString cutValue;
              ErrorResult err;
              charData->SubstringData(startOffset, dataLength, cutValue, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              nsCOMPtr<nsINode> clone = node->CloneNode(false, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              clone->SetNodeValue(cutValue, err);
              if (NS_WARN_IF(err.Failed())) {
                return err.StealNSResult();
              }
              nodeToResult = clone;
            }

            nsMutationGuard guard;
            ErrorResult err;
            charData->DeleteData(startOffset, dataLength, err);
            if (NS_WARN_IF(err.Failed())) {
              return err.StealNSResult();
            }
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_STATE(!guard.Mutated(0) ||
                            ValidateCurrentNode(this, iter));
          }

          handled = true;
        }
      }
      else if (node == endContainer) {
        // Delete or extract everything before endOffset.
        if (retval) {
          nsAutoString cutValue;
          ErrorResult err;
          charData->SubstringData(0, endOffset, cutValue, err);
          if (NS_WARN_IF(err.Failed())) {
            return err.StealNSResult();
          }
          nsCOMPtr<nsINode> clone = node->CloneNode(false, err);
          if (NS_WARN_IF(err.Failed())) {
            return err.StealNSResult();
          }
          clone->SetNodeValue(cutValue, err);
          if (NS_WARN_IF(err.Failed())) {
            return err.StealNSResult();
          }
          nodeToResult = clone;
        }

        nsMutationGuard guard;
        ErrorResult err;
        charData->DeleteData(0, endOffset, err);
        if (NS_WARN_IF(err.Failed())) {
          return err.StealNSResult();
        }
        NS_ENSURE_STATE(!guard.Mutated(0) ||
                        ValidateCurrentNode(this, iter));
        handled = true;
      }
    }

    if (!handled && (node == endContainer || node == startContainer)) {
      if (node && node->IsElement() &&
          ((node == endContainer && endOffset == 0) ||
           (node == startContainer &&
            node->AsElement()->GetChildCount() == startOffset))) {
        if (retval) {
          ErrorResult rv;
          nodeToResult = node->CloneNode(false, rv);
          NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
        }
        handled = true;
      }
    }

    if (!handled) {
      // node was not handled above, so it must be completely contained
      // within the range. Just remove it from the tree!
      nodeToResult = node;
    }

    uint32_t parentCount = 0;
    // Set the result to document fragment if we have 'retval'.
    if (retval) {
      nsCOMPtr<nsINode> oldCommonAncestor = commonAncestor;
      if (!iter.IsDone()) {
        // Setup the parameters for the next iteration of the loop.
        NS_ENSURE_STATE(nextNode);

        // Get node's and nextNode's common parent. Do this before moving
        // nodes from original DOM to result fragment.
        commonAncestor = nsContentUtils::GetCommonAncestor(node, nextNode);
        NS_ENSURE_STATE(commonAncestor);

        nsCOMPtr<nsINode> parentCounterNode = node;
        while (parentCounterNode && parentCounterNode != commonAncestor) {
          ++parentCount;
          parentCounterNode = parentCounterNode->GetParentNode();
          NS_ENSURE_STATE(parentCounterNode);
        }
      }

      // Clone the parent hierarchy between commonAncestor and node.
      nsCOMPtr<nsINode> closestAncestor, farthestAncestor;
      rv = CloneParentsBetween(oldCommonAncestor, node,
                               getter_AddRefs(closestAncestor),
                               getter_AddRefs(farthestAncestor));
      NS_ENSURE_SUCCESS(rv, rv);

      ErrorResult res;
      if (farthestAncestor) {
        nsCOMPtr<nsINode> n = do_QueryInterface(commonCloneAncestor);
        n->AppendChild(*farthestAncestor, res);
        res.WouldReportJSException();
        if (NS_WARN_IF(res.Failed())) {
          return res.StealNSResult();
        }
      }

      nsMutationGuard guard;
      nsCOMPtr<nsINode> parent = nodeToResult->GetParentNode();
      if (closestAncestor) {
        closestAncestor->AppendChild(*nodeToResult, res);
      } else {
        commonCloneAncestor->AppendChild(*nodeToResult, res);
      }
      res.WouldReportJSException();
      if (NS_WARN_IF(res.Failed())) {
        return res.StealNSResult();
      }
      NS_ENSURE_STATE(!guard.Mutated(parent ? 2 : 1) ||
                      ValidateCurrentNode(this, iter));
    } else if (nodeToResult) {
      nsMutationGuard guard;
      nsCOMPtr<nsINode> node = nodeToResult;
      nsCOMPtr<nsINode> parent = node->GetParentNode();
      if (parent) {
        mozilla::ErrorResult error;
        parent->RemoveChild(*node, error);
        NS_ENSURE_FALSE(error.Failed(), error.StealNSResult());
      }
      NS_ENSURE_STATE(!guard.Mutated(1) ||
                      ValidateCurrentNode(this, iter));
    }

    if (!iter.IsDone() && retval) {
      // Find the equivalent of commonAncestor in the cloned tree.
      nsCOMPtr<nsINode> newCloneAncestor = nodeToResult;
      for (uint32_t i = parentCount; i; --i) {
        newCloneAncestor = newCloneAncestor->GetParentNode();
        NS_ENSURE_STATE(newCloneAncestor);
      }
      commonCloneAncestor = newCloneAncestor;
    }
  }

  rv = CollapseRangeAfterDelete(this);
  if (NS_SUCCEEDED(rv) && aFragment) {
    retval.forget(aFragment);
  }
  return rv;
}

void
nsRange::DeleteContents(ErrorResult& aRv)
{
  aRv = CutContents(nullptr);
}

already_AddRefed<DocumentFragment>
nsRange::ExtractContents(ErrorResult& rv)
{
  RefPtr<DocumentFragment> fragment;
  rv = CutContents(getter_AddRefs(fragment));
  return fragment.forget();
}

int16_t
nsRange::CompareBoundaryPoints(uint16_t aHow, nsRange& aOtherRange,
                               ErrorResult& rv)
{
  if (!mIsPositioned || !aOtherRange.IsPositioned()) {
    rv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  nsINode *ourNode, *otherNode;
  uint32_t ourOffset, otherOffset;

  switch (aHow) {
    case Range_Binding::START_TO_START:
      ourNode = mStart.Container();
      ourOffset = mStart.Offset();
      otherNode = aOtherRange.GetStartContainer();
      otherOffset = aOtherRange.StartOffset();
      break;
    case Range_Binding::START_TO_END:
      ourNode = mEnd.Container();
      ourOffset = mEnd.Offset();
      otherNode = aOtherRange.GetStartContainer();
      otherOffset = aOtherRange.StartOffset();
      break;
    case Range_Binding::END_TO_START:
      ourNode = mStart.Container();
      ourOffset = mStart.Offset();
      otherNode = aOtherRange.GetEndContainer();
      otherOffset = aOtherRange.EndOffset();
      break;
    case Range_Binding::END_TO_END:
      ourNode = mEnd.Container();
      ourOffset = mEnd.Offset();
      otherNode = aOtherRange.GetEndContainer();
      otherOffset = aOtherRange.EndOffset();
      break;
    default:
      // We were passed an illegal value
      rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return 0;
  }

  if (mRoot != aOtherRange.GetRoot()) {
    rv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
    return 0;
  }

  return nsContentUtils::ComparePoints(ourNode,
                                       static_cast<int32_t>(ourOffset),
                                       otherNode,
                                       static_cast<int32_t>(otherOffset));
}

/* static */ nsresult
nsRange::CloneParentsBetween(nsINode *aAncestor,
                             nsINode *aNode,
                             nsINode **aClosestAncestor,
                             nsINode **aFarthestAncestor)
{
  NS_ENSURE_ARG_POINTER((aAncestor && aNode && aClosestAncestor && aFarthestAncestor));

  *aClosestAncestor  = nullptr;
  *aFarthestAncestor = nullptr;

  if (aAncestor == aNode)
    return NS_OK;

  nsCOMPtr<nsINode> firstParent, lastParent;
  nsCOMPtr<nsINode> parent = aNode->GetParentNode();

  while(parent && parent != aAncestor)
  {
    ErrorResult rv;
    nsCOMPtr<nsINode> clone = parent->CloneNode(false, rv);

    if (rv.Failed()) {
      return rv.StealNSResult();
    }
    if (!clone) {
      return NS_ERROR_FAILURE;
    }

    if (! firstParent) {
      firstParent = lastParent = clone;
    } else {
      clone->AppendChild(*lastParent, rv);
      if (rv.Failed()) return rv.StealNSResult();

      lastParent = clone;
    }

    parent = parent->GetParentNode();
  }

  *aClosestAncestor  = firstParent;
  NS_IF_ADDREF(*aClosestAncestor);

  *aFarthestAncestor = lastParent;
  NS_IF_ADDREF(*aFarthestAncestor);

  return NS_OK;
}

already_AddRefed<DocumentFragment>
nsRange::CloneContents(ErrorResult& aRv)
{
  nsCOMPtr<nsINode> commonAncestor = GetCommonAncestorContainer(aRv);
  MOZ_ASSERT(!aRv.Failed(), "GetCommonAncestorContainer() shouldn't fail!");

  nsCOMPtr<nsIDocument> doc = mStart.Container()->OwnerDoc();
  NS_ASSERTION(doc, "CloneContents needs a document to continue.");
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Create a new document fragment in the context of this document,
  // which might be null


  RefPtr<DocumentFragment> clonedFrag =
    new DocumentFragment(doc->NodeInfoManager());

  nsCOMPtr<nsINode> commonCloneAncestor = clonedFrag.get();

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  aRv = iter.Init(this);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (iter.IsDone())
  {
    // There's nothing to add to the doc frag, we must be done!
    return clonedFrag.forget();
  }

  iter.First();

  // With the exception of text nodes that contain one of the range
  // end points and elements which don't have any content selected the subtree
  // iterator should only give us back subtrees that are completely contained
  // between the range's end points.
  //
  // Unfortunately these subtrees don't contain the parent hierarchy/context
  // that the Range spec requires us to return. This loop clones the
  // parent hierarchy, adds a cloned version of the subtree, to it, then
  // correctly places this new subtree into the doc fragment.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsINode> node = iter.GetCurrentNode();
    bool deepClone = !node->IsElement() ||
                       (!(node == mEnd.Container() && mEnd.Offset() == 0) &&
                        !(node == mStart.Container() &&
                          mStart.Offset() == node->AsElement()->GetChildCount()));

    // Clone the current subtree!

    nsCOMPtr<nsINode> clone = node->CloneNode(deepClone, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // If it's CharacterData, make sure we only clone what
    // is in the range.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    if (auto charData = CharacterData::FromNode(clone))
    {
      if (node == mEnd.Container()) {
        // We only need the data before mEndOffset, so get rid of any
        // data after it.

        uint32_t dataLength = charData->Length();
        if (dataLength > (uint32_t)mEnd.Offset())
        {
          charData->DeleteData(mEnd.Offset(), dataLength - mEnd.Offset(), aRv);
          if (aRv.Failed()) {
            return nullptr;
          }
        }
      }

      if (node == mStart.Container()) {
        // We don't need any data before mStartOffset, so just
        // delete it!

        if (mStart.Offset() > 0)
        {
          charData->DeleteData(0, mStart.Offset(), aRv);
          if (aRv.Failed()) {
            return nullptr;
          }
        }
      }
    }

    // Clone the parent hierarchy between commonAncestor and node.

    nsCOMPtr<nsINode> closestAncestor, farthestAncestor;

    aRv = CloneParentsBetween(commonAncestor, node,
                              getter_AddRefs(closestAncestor),
                              getter_AddRefs(farthestAncestor));

    if (aRv.Failed()) {
      return nullptr;
    }

    // Hook the parent hierarchy/context of the subtree into the clone tree.

    if (farthestAncestor)
    {
      commonCloneAncestor->AppendChild(*farthestAncestor, aRv);

      if (aRv.Failed()) {
        return nullptr;
      }
    }

    // Place the cloned subtree into the cloned doc frag tree!

    nsCOMPtr<nsINode> cloneNode = do_QueryInterface(clone);
    if (closestAncestor)
    {
      // Append the subtree under closestAncestor since it is the
      // immediate parent of the subtree.

      closestAncestor->AppendChild(*cloneNode, aRv);
    }
    else
    {
      // If we get here, there is no missing parent hierarchy between
      // commonAncestor and node, so just append clone to commonCloneAncestor.

      commonCloneAncestor->AppendChild(*cloneNode, aRv);
    }
    if (aRv.Failed()) {
      return nullptr;
    }

    // Get the next subtree to be processed. The idea here is to setup
    // the parameters for the next iteration of the loop.

    iter.Next();

    if (iter.IsDone())
      break; // We must be done!

    nsCOMPtr<nsINode> nextNode = iter.GetCurrentNode();
    if (!nextNode) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    // Get node and nextNode's common parent.
    commonAncestor = nsContentUtils::GetCommonAncestor(node, nextNode);

    if (!commonAncestor) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    // Find the equivalent of commonAncestor in the cloned tree!

    while (node && node != commonAncestor)
    {
      node = node->GetParentNode();
      if (aRv.Failed()) {
        return nullptr;
      }

      if (!node) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      cloneNode = cloneNode->GetParentNode();
      if (!cloneNode) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
    }

    commonCloneAncestor = cloneNode;
  }

  return clonedFrag.forget();
}

already_AddRefed<nsRange>
nsRange::CloneRange() const
{
  RefPtr<nsRange> range = new nsRange(mOwner);

  range->SetMaySpanAnonymousSubtrees(mMaySpanAnonymousSubtrees);

  range->DoSetRange(mStart.AsRaw(), mEnd.AsRaw(), mRoot);

  return range.forget();
}

void
nsRange::InsertNode(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  uint32_t tStartOffset = StartOffset();

  nsCOMPtr<nsINode> tStartContainer = GetStartContainer(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (&aNode == tStartContainer) {
    aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
    return;
  }

  // This is the node we'll be inserting before, and its parent
  nsCOMPtr<nsINode> referenceNode;
  nsCOMPtr<nsINode> referenceParentNode = tStartContainer;

  RefPtr<Text> startTextNode =
    tStartContainer ? tStartContainer->GetAsText() : nullptr;
  nsCOMPtr<nsINodeList> tChildList;
  if (startTextNode) {
    referenceParentNode = tStartContainer->GetParentNode();
    if (!referenceParentNode) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return;
    }

    referenceParentNode->EnsurePreInsertionValidity(aNode, tStartContainer,
                                                    aRv);
    if (aRv.Failed()) {
      return;
    }

    RefPtr<Text> secondPart = startTextNode->SplitText(tStartOffset, aRv);
    if (aRv.Failed()) {
      return;
    }

    referenceNode = do_QueryInterface(secondPart);
  } else {
    tChildList = tStartContainer->ChildNodes();

    // find the insertion point in the DOM and insert the Node
    referenceNode = tChildList->Item(tStartOffset);

    tStartContainer->EnsurePreInsertionValidity(aNode, referenceNode, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // We might need to update the end to include the new node (bug 433662).
  // Ideally we'd only do this if needed, but it's tricky to know when it's
  // needed in advance (bug 765799).
  uint32_t newOffset;

  if (referenceNode) {
    int32_t indexInParent = IndexOf(referenceNode);
    if (NS_WARN_IF(indexInParent < 0)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    newOffset = static_cast<uint32_t>(indexInParent);
  } else {
    newOffset = tChildList->Length();
  }

  if (aNode.NodeType() == nsINode::DOCUMENT_FRAGMENT_NODE) {
    newOffset += aNode.GetChildCount();
  } else {
    newOffset++;
  }

  // Now actually insert the node
  nsCOMPtr<nsINode> tResultNode;
  tResultNode = referenceParentNode->InsertBefore(aNode, referenceNode, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (Collapsed()) {
    aRv = SetEnd(referenceParentNode, newOffset);
  }
}

void
nsRange::SurroundContents(nsINode& aNewParent, ErrorResult& aRv)
{
  if (!nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(&aNewParent)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!mRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  // INVALID_STATE_ERROR: Raised if the Range partially selects a non-text
  // node.
  if (mStart.Container() != mEnd.Container()) {
    bool startIsText = mStart.Container()->IsText();
    bool endIsText = mEnd.Container()->IsText();
    nsINode* startGrandParent = mStart.Container()->GetParentNode();
    nsINode* endGrandParent = mEnd.Container()->GetParentNode();
    if (!((startIsText && endIsText &&
           startGrandParent &&
           startGrandParent == endGrandParent) ||
          (startIsText &&
           startGrandParent &&
           startGrandParent == mEnd.Container()) ||
          (endIsText &&
           endGrandParent &&
           endGrandParent == mStart.Container()))) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
  }

  // INVALID_NODE_TYPE_ERROR if aNewParent is something that can't be inserted
  // (Document, DocumentType, DocumentFragment)
  uint16_t nodeType = aNewParent.NodeType();
  if (nodeType == nsINode::DOCUMENT_NODE ||
      nodeType == nsINode::DOCUMENT_TYPE_NODE ||
      nodeType == nsINode::DOCUMENT_FRAGMENT_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  // Extract the contents within the range.

  RefPtr<DocumentFragment> docFrag = ExtractContents(aRv);

  if (aRv.Failed()) {
    return;
  }

  if (!docFrag) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Spec says we need to remove all of aNewParent's
  // children prior to insertion.

  nsCOMPtr<nsINodeList> children = aNewParent.ChildNodes();
  if (!children) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  uint32_t numChildren = children->Length();

  while (numChildren)
  {
    nsCOMPtr<nsINode> child = children->Item(--numChildren);
    if (!child) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    aNewParent.RemoveChild(*child, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Insert aNewParent at the range's start point.

  InsertNode(aNewParent, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Append the content we extracted under aNewParent.
  aNewParent.AppendChild(*docFrag, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Select aNewParent, and its contents.

  SelectNode(aNewParent, aRv);
}

void
nsRange::ToString(nsAString& aReturn, ErrorResult& aErr)
{
  // clear the string
  aReturn.Truncate();

  // If we're unpositioned, return the empty string
  if (!mIsPositioned) {
    return;
  }

#ifdef DEBUG_range
      printf("Range dump: -----------------------\n");
#endif /* DEBUG */

  // effeciency hack for simple case
  if (mStart.Container() == mEnd.Container()) {
    Text* textNode = mStart.Container() ? mStart.Container()->GetAsText() : nullptr;

    if (textNode)
    {
#ifdef DEBUG_range
      // If debug, dump it:
      textNode->List(stdout);
      printf("End Range dump: -----------------------\n");
#endif /* DEBUG */

      // grab the text
      textNode->SubstringData(mStart.Offset(), mEnd.Offset() - mStart.Offset(),
                              aReturn, aErr);
      return;
    }
  }

  /* complex case: mStart.Container() != mEnd.Container(), or mStartParent not a text
     node revisit - there are potential optimizations here and also tradeoffs.
  */

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  nsresult rv = iter->Init(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aErr.Throw(rv);
    return;
  }

  nsString tempString;

  // loop through the content iterator, which returns nodes in the range in
  // close tag order, and grab the text from any text node
  while (!iter->IsDone())
  {
    nsINode *n = iter->GetCurrentNode();

#ifdef DEBUG_range
    // If debug, dump it:
    n->List(stdout);
#endif /* DEBUG */
    Text* textNode = n->GetAsText();
    if (textNode) // if it's a text node, get the text
    {
      if (n == mStart.Container()) { // only include text past start offset
        uint32_t strLength = textNode->Length();
        textNode->SubstringData(mStart.Offset(), strLength-mStart.Offset(),
                                tempString, IgnoreErrors());
        aReturn += tempString;
      } else if (n == mEnd.Container()) { // only include text before end offset
        textNode->SubstringData(0, mEnd.Offset(), tempString, IgnoreErrors());
        aReturn += tempString;
      } else { // grab the whole kit-n-kaboodle
        textNode->GetData(tempString);
        aReturn += tempString;
      }
    }

    iter->Next();
  }

#ifdef DEBUG_range
  printf("End Range dump: -----------------------\n");
#endif /* DEBUG */
}

void
nsRange::Detach()
{
}

already_AddRefed<DocumentFragment>
nsRange::CreateContextualFragment(const nsAString& aFragment, ErrorResult& aRv)
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return nsContentUtils::CreateContextualFragment(mStart.Container(), aFragment,
                                                  false, aRv);
}

static void ExtractRectFromOffset(nsIFrame* aFrame,
                                  const int32_t aOffset, nsRect* aR,
                                  bool aFlushToOriginEdge, bool aClampToEdge)
{
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aR);

  nsPoint point;
  aFrame->GetPointFromOffset(aOffset, &point);

  // Determine if aFrame has a vertical writing mode, which will change our math
  // on the output rect.
  bool isVertical = aFrame->GetWritingMode().IsVertical();

  if (!aClampToEdge && !aR->Contains(point)) {
    // If point is outside aR, and we aren't clamping, output an empty rect
    // with origin at the point.
    if (isVertical) {
      aR->SetHeight(0);
      aR->y = point.y;
    } else {
      aR->SetWidth(0);
      aR->x = point.x;
    }
    return;
  }

  if (aClampToEdge) {
    point = aR->ClampPoint(point);
  }

  // point is within aR, and now we'll modify aR to output a rect that has point
  // on one edge. But which edge?
  if (aFlushToOriginEdge) {
    // The output rect should be flush to the edge of aR that contains the origin.
    if (isVertical) {
      aR->SetHeight(point.y - aR->y);
    } else {
      aR->SetWidth(point.x - aR->x);
    }
  } else {
    // The output rect should be flush to the edge of aR opposite the origin.
    if (isVertical) {
      aR->SetHeight(aR->YMost() - point.y);
      aR->y = point.y;
    } else {
      aR->SetWidth(aR->XMost() - point.x);
      aR->x = point.x;
    }
  }
}

static nsTextFrame*
GetTextFrameForContent(nsIContent* aContent, bool aFlushLayout)
{
  nsIDocument* doc = aContent->OwnerDoc();
  nsIPresShell* presShell = doc->GetShell();
  if (!presShell) {
    return nullptr;
  }

  const bool frameWillBeUnsuppressed =
    presShell->FrameConstructor()->EnsureFrameForTextNodeIsCreatedAfterFlush(
      static_cast<CharacterData*>(aContent));
  if (aFlushLayout) {
    doc->FlushPendingNotifications(FlushType::Layout);
  } else if (frameWillBeUnsuppressed) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame || !frame->IsTextFrame()) {
    return nullptr;
  }
  return static_cast<nsTextFrame*>(frame);
}

static nsresult GetPartialTextRect(nsLayoutUtils::RectCallback* aCallback,
                                   Sequence<nsString>* aTextList,
                                   nsIContent* aContent, int32_t aStartOffset,
                                   int32_t aEndOffset, bool aClampToEdge,
                                   bool aFlushLayout)
{
  nsTextFrame* textFrame = GetTextFrameForContent(aContent, aFlushLayout);
  if (textFrame) {
    nsIFrame* relativeTo = nsLayoutUtils::GetContainingBlockForClientRect(textFrame);
    for (nsTextFrame* f = textFrame; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
      int32_t fstart = f->GetContentOffset(), fend = f->GetContentEnd();
      if (fend <= aStartOffset || fstart >= aEndOffset)
        continue;

      // Calculate the text content offsets we'll need if text is requested.
      int32_t textContentStart = fstart;
      int32_t textContentEnd = fend;

      // overlapping with the offset we want
      f->EnsureTextRun(nsTextFrame::eInflated);
      NS_ENSURE_TRUE(f->GetTextRun(nsTextFrame::eInflated), NS_ERROR_OUT_OF_MEMORY);
      bool rtl = f->GetTextRun(nsTextFrame::eInflated)->IsRightToLeft();
      nsRect r = f->GetRectRelativeToSelf();
      if (fstart < aStartOffset) {
        // aStartOffset is within this frame
        ExtractRectFromOffset(f, aStartOffset, &r, rtl, aClampToEdge);
        textContentStart = aStartOffset;
      }
      if (fend > aEndOffset) {
        // aEndOffset is in the middle of this frame
        ExtractRectFromOffset(f, aEndOffset, &r, !rtl, aClampToEdge);
        textContentEnd = aEndOffset;
      }
      r = nsLayoutUtils::TransformFrameRectToAncestor(f, r, relativeTo);
      aCallback->AddRect(r);

      // Finally capture the text, if requested.
      if (aTextList) {
        nsIFrame::RenderedText renderedText = f->GetRenderedText(
          textContentStart,
          textContentEnd,
          nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
          nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);

        aTextList->AppendElement(renderedText.mString, fallible);
      }
    }
  }
  return NS_OK;
}

/* static */ void
nsRange::CollectClientRectsAndText(nsLayoutUtils::RectCallback* aCollector,
                                   Sequence<nsString>* aTextList,
                                   nsRange* aRange,
                                   nsINode* aStartContainer,
                                   uint32_t aStartOffset,
                                   nsINode* aEndContainer,
                                   uint32_t aEndOffset,
                                   bool aClampToEdge, bool aFlushLayout)
{
  // Currently, this method is called with start of end offset of nsRange.
  // So, they must be between 0 - INT32_MAX.
  MOZ_ASSERT(IsValidOffset(aStartOffset));
  MOZ_ASSERT(IsValidOffset(aEndOffset));

  // Hold strong pointers across the flush
  nsCOMPtr<nsINode> startContainer = aStartContainer;
  nsCOMPtr<nsINode> endContainer = aEndContainer;

  // Flush out layout so our frames are up to date.
  if (!aStartContainer->IsInComposedDoc()) {
    return;
  }

  if (aFlushLayout) {
    aStartContainer->OwnerDoc()->FlushPendingNotifications(FlushType::Layout);
    // Recheck whether we're still in the document
    if (!aStartContainer->IsInComposedDoc()) {
      return;
    }
  }

  RangeSubtreeIterator iter;

  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) return;

  if (iter.IsDone()) {
    // the range is collapsed, only continue if the cursor is in a text node
    if (aStartContainer->IsText()) {
      nsTextFrame* textFrame = GetTextFrameForContent(aStartContainer->AsText(), aFlushLayout);
      if (textFrame) {
        int32_t outOffset;
        nsIFrame* outFrame;
        textFrame->GetChildFrameContainingOffset(
                     static_cast<int32_t>(aStartOffset), false,
                     &outOffset, &outFrame);
        if (outFrame) {
           nsIFrame* relativeTo =
             nsLayoutUtils::GetContainingBlockForClientRect(outFrame);
           nsRect r = outFrame->GetRectRelativeToSelf();
           ExtractRectFromOffset(outFrame, static_cast<int32_t>(aStartOffset),
                                 &r, false, aClampToEdge);
           r.SetWidth(0);
           r = nsLayoutUtils::TransformFrameRectToAncestor(outFrame, r, relativeTo);
           aCollector->AddRect(r);
        }
      }
    }
    return;
  }

  do {
    nsCOMPtr<nsINode> node = iter.GetCurrentNode();
    iter.Next();
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    if (!content)
      continue;
    if (content->IsText()) {
       if (node == startContainer) {
         int32_t offset = startContainer == endContainer ?
           static_cast<int32_t>(aEndOffset) : content->GetText()->GetLength();
         GetPartialTextRect(aCollector, aTextList, content,
                            static_cast<int32_t>(aStartOffset), offset,
                            aClampToEdge, aFlushLayout);
         continue;
       } else if (node == endContainer) {
         GetPartialTextRect(aCollector, aTextList, content,
                            0, static_cast<int32_t>(aEndOffset),
                            aClampToEdge, aFlushLayout);
         continue;
       }
    }

    nsIFrame* frame = content->GetPrimaryFrame();
    if (frame) {
      nsLayoutUtils::GetAllInFlowRectsAndTexts(frame,
        nsLayoutUtils::GetContainingBlockForClientRect(frame), aCollector,
        aTextList,
        nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
    }
  } while (!iter.IsDone());
}

already_AddRefed<DOMRect>
nsRange::GetBoundingClientRect(bool aClampToEdge, bool aFlushLayout)
{
  RefPtr<DOMRect> rect = new DOMRect(ToSupports(this));
  if (!mStart.Container()) {
    return rect.forget();
  }

  nsLayoutUtils::RectAccumulator accumulator;
  CollectClientRectsAndText(&accumulator, nullptr, this, mStart.Container(),
    mStart.Offset(), mEnd.Container(), mEnd.Offset(), aClampToEdge, aFlushLayout);

  nsRect r = accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect :
    accumulator.mResultRect;
  rect->SetLayoutRect(r);
  return rect.forget();
}

already_AddRefed<DOMRectList>
nsRange::GetClientRects(bool aClampToEdge, bool aFlushLayout)
{
  if (!mStart.Container()) {
    return nullptr;
  }

  RefPtr<DOMRectList> rectList = new DOMRectList(this);

  nsLayoutUtils::RectListBuilder builder(rectList);

  CollectClientRectsAndText(&builder, nullptr, this, mStart.Container(),
    mStart.Offset(), mEnd.Container(), mEnd.Offset(), aClampToEdge, aFlushLayout);
  return rectList.forget();
}

void
nsRange::GetClientRectsAndTexts(
  mozilla::dom::ClientRectsAndTexts& aResult,
  ErrorResult& aErr)
{
  if (!mStart.Container()) {
    return;
  }

  aResult.mRectList = new DOMRectList(this);

  nsLayoutUtils::RectListBuilder builder(aResult.mRectList);

  CollectClientRectsAndText(&builder, &aResult.mTextList, this,
    mStart.Container(), mStart.Offset(), mEnd.Container(), mEnd.Offset(), true, true);
}

nsresult
nsRange::GetUsedFontFaces(nsTArray<nsAutoPtr<InspectorFontFace>>& aResult,
                          uint32_t aMaxRanges, bool aSkipCollapsedWhitespace)
{
  NS_ENSURE_TRUE(mStart.Container(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsINode> startContainer = do_QueryInterface(mStart.Container());
  nsCOMPtr<nsINode> endContainer = do_QueryInterface(mEnd.Container());

  // Flush out layout so our frames are up to date.
  nsIDocument* doc = mStart.Container()->OwnerDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);
  doc->FlushPendingNotifications(FlushType::Frames);

  // Recheck whether we're still in the document
  NS_ENSURE_TRUE(mStart.Container()->IsInComposedDoc(), NS_ERROR_UNEXPECTED);

  // A table to map gfxFontEntry objects to InspectorFontFace objects.
  // (We hold on to the InspectorFontFaces strongly due to the nsAutoPtrs
  // in the nsClassHashtable, until we move them out into aResult at the end
  // of the function.)
  nsLayoutUtils::UsedFontFaceTable fontFaces;

  RangeSubtreeIterator iter;
  nsresult rv = iter.Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  while (!iter.IsDone()) {
    // only collect anything if the range is not collapsed
    nsCOMPtr<nsINode> node = iter.GetCurrentNode();
    iter.Next();

    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    if (!content) {
      continue;
    }
    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame) {
      continue;
    }

    if (content->IsText()) {
       if (node == startContainer) {
         int32_t offset = startContainer == endContainer ?
           mEnd.Offset() : content->GetText()->GetLength();
         nsLayoutUtils::GetFontFacesForText(frame, mStart.Offset(), offset,
                                            true, fontFaces, aMaxRanges,
                                            aSkipCollapsedWhitespace);
         continue;
       }
       if (node == endContainer) {
         nsLayoutUtils::GetFontFacesForText(frame, 0, mEnd.Offset(),
                                            true, fontFaces, aMaxRanges,
                                            aSkipCollapsedWhitespace);
         continue;
       }
    }

    nsLayoutUtils::GetFontFacesForFrames(frame, fontFaces, aMaxRanges,
                                         aSkipCollapsedWhitespace);
  }

  // Take ownership of the InspectorFontFaces in the table and move them into
  // the aResult outparam.
  for (auto iter = fontFaces.Iter(); !iter.Done(); iter.Next()) {
    aResult.AppendElement(std::move(iter.Data()));
  }

  return NS_OK;
}

nsINode*
nsRange::GetRegisteredCommonAncestor()
{
  MOZ_ASSERT(IsInSelection(),
             "GetRegisteredCommonAncestor only valid for range in selection");
  MOZ_ASSERT(mRegisteredCommonAncestor);
  return mRegisteredCommonAncestor;
}

/* static */ bool nsRange::AutoInvalidateSelection::sIsNested;

nsRange::AutoInvalidateSelection::~AutoInvalidateSelection()
{
  if (!mCommonAncestor) {
    return;
  }
  sIsNested = false;
  ::InvalidateAllFrames(mCommonAncestor);

  // Our range might not be in a selection anymore, because one of our selection
  // listeners might have gone ahead and run script of various sorts that messed
  // with selections, ranges, etc.  But if it still is, we should check whether
  // we have a different common ancestor now, and if so invalidate its subtree
  // so it paints the selection it's in now.
  if (mRange->IsInSelection()) {
    nsINode* commonAncestor = mRange->GetRegisteredCommonAncestor();
    // XXXbz can commonAncestor really be null here?  I wouldn't think so!  If
    // it _were_, then in a debug build GetRegisteredCommonAncestor() would have
    // fatally asserted.
    if (commonAncestor && commonAncestor != mCommonAncestor) {
      ::InvalidateAllFrames(commonAncestor);
    }
  }
}

/* static */ already_AddRefed<nsRange>
nsRange::Constructor(const GlobalObject& aGlobal,
                     ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return window->GetDoc()->CreateRange(aRv);
}

static bool ExcludeIfNextToNonSelectable(nsIContent* aContent)
{
  return aContent->IsText() &&
    aContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE);
}

void
nsRange::ExcludeNonSelectableNodes(nsTArray<RefPtr<nsRange>>* aOutRanges)
{
  MOZ_ASSERT(mIsPositioned);
  MOZ_ASSERT(mEnd.Container());
  MOZ_ASSERT(mStart.Container());

  nsRange* range = this;
  RefPtr<nsRange> newRange;
  while (range) {
    nsCOMPtr<nsIContentIterator> iter = NS_NewPreContentIterator();
    nsresult rv = iter->Init(range);
    if (NS_FAILED(rv)) {
      return;
    }

    bool added = false;
    bool seenSelectable = false;
    // |firstNonSelectableContent| is the first node in a consecutive sequence
    // of non-IsSelectable nodes.  When we find a selectable node after such
    // a sequence we'll end the last nsRange, create a new one and restart
    // the outer loop.
    nsIContent* firstNonSelectableContent = nullptr;
    while (true) {
      ErrorResult err;
      nsINode* node = iter->GetCurrentNode();
      iter->Next();
      bool selectable = true;
      nsIContent* content =
        node && node->IsContent() ? node->AsContent() : nullptr;
      if (content) {
        if (firstNonSelectableContent && ExcludeIfNextToNonSelectable(content)) {
          // Ignorable whitespace next to a sequence of non-selectable nodes
          // counts as non-selectable (bug 1216001).
          selectable = false;
        }
        if (selectable) {
          nsIFrame* frame = content->GetPrimaryFrame();
          for (nsIContent* p = content; !frame && (p = p->GetParent()); ) {
            frame = p->GetPrimaryFrame();
          }
          if (frame) {
            selectable = frame->IsSelectable(nullptr);
          }
        }
      }

      if (!selectable) {
        if (!firstNonSelectableContent) {
          firstNonSelectableContent = content;
        }
        if (iter->IsDone() && seenSelectable) {
          // The tail end of the initial range is non-selectable - truncate the
          // current range before the first non-selectable node.
          range->SetEndBefore(*firstNonSelectableContent, err);
        }
      } else if (firstNonSelectableContent) {
        if (range == this && !seenSelectable) {
          // This is the initial range and all its nodes until now are
          // non-selectable so just trim them from the start.
          range->SetStartBefore(*node, err);
          if (err.Failed()) {
            return;
          }
          break; // restart the same range with a new iterator
        } else {
          // Save the end point before truncating the range.
          nsINode* endContainer = range->mEnd.Container();
          int32_t endOffset = range->mEnd.Offset();

          // Truncate the current range before the first non-selectable node.
          range->SetEndBefore(*firstNonSelectableContent, err);

          // Store it in the result (strong ref) - do this before creating
          // a new range in |newRange| below so we don't drop the last ref
          // to the range created in the previous iteration.
          if (!added && !err.Failed()) {
            aOutRanges->AppendElement(range);
          }

          // Create a new range for the remainder.
          nsINode* startContainer = node;
          int32_t startOffset = 0;
          // Don't start *inside* a node with independent selection though
          // (e.g. <input>).
          if (content && content->HasIndependentSelection()) {
            nsINode* parent = startContainer->GetParent();
            if (parent) {
              startOffset = parent->ComputeIndexOf(startContainer);
              startContainer = parent;
            }
          }
          rv = CreateRange(startContainer, startOffset, endContainer, endOffset,
                           getter_AddRefs(newRange));
          if (NS_FAILED(rv) || newRange->Collapsed()) {
            newRange = nullptr;
          }
          range = newRange;
          break; // create a new iterator for the new range, if any
        }
      } else {
        seenSelectable = true;
        if (!added) {
          added = true;
          aOutRanges->AppendElement(range);
        }
      }
      if (iter->IsDone()) {
        return;
      }
    }
  }
}

struct InnerTextAccumulator
{
  explicit InnerTextAccumulator(mozilla::dom::DOMString& aValue)
    : mString(aValue.AsAString()), mRequiredLineBreakCount(0) {}
  void FlushLineBreaks()
  {
    while (mRequiredLineBreakCount > 0) {
      // Required line breaks at the start of the text are suppressed.
      if (!mString.IsEmpty()) {
        mString.Append('\n');
      }
      --mRequiredLineBreakCount;
    }
  }
  void Append(char aCh)
  {
    Append(nsAutoString(aCh));
  }
  void Append(const nsAString& aString)
  {
    if (aString.IsEmpty()) {
      return;
    }
    FlushLineBreaks();
    mString.Append(aString);
  }
  void AddRequiredLineBreakCount(int8_t aCount)
  {
    mRequiredLineBreakCount = std::max(mRequiredLineBreakCount, aCount);
  }

  nsAString& mString;
  int8_t mRequiredLineBreakCount;
};

static bool
IsVisibleAndNotInReplacedElement(nsIFrame* aFrame)
{
  if (!aFrame || !aFrame->StyleVisibility()->IsVisible()) {
    return false;
  }
  for (nsIFrame* f = aFrame->GetParent(); f; f = f->GetParent()) {
    if (f->IsFrameOfType(nsIFrame::eReplaced) &&
        !f->GetContent()->IsHTMLElement(nsGkAtoms::button) &&
        !f->GetContent()->IsHTMLElement(nsGkAtoms::select)) {
      return false;
    }
  }
  return true;
}

static bool
ElementIsVisibleNoFlush(Element* aElement)
{
  if (!aElement) {
    return false;
  }
  RefPtr<ComputedStyle> sc =
    nsComputedDOMStyle::GetComputedStyleNoFlush(aElement, nullptr);
  return sc && sc->StyleVisibility()->IsVisible();
}

static void
AppendTransformedText(InnerTextAccumulator& aResult, nsIContent* aContainer)
{
  auto textNode = static_cast<CharacterData*>(aContainer);

  nsIFrame* frame = textNode->GetPrimaryFrame();
  if (!IsVisibleAndNotInReplacedElement(frame)) {
    return;
  }

  nsIFrame::RenderedText text =
    frame->GetRenderedText(0, aContainer->GetChildCount());
  aResult.Append(text.mString);
}

/**
 * States for tree traversal. AT_NODE means that we are about to enter
 * the current DOM node. AFTER_NODE means that we have just finished traversing
 * the children of the current DOM node and are about to apply any
 * "after processing the node's children" steps before we finish visiting
 * the node.
 */
enum TreeTraversalState {
  AT_NODE,
  AFTER_NODE
};

static int8_t
GetRequiredInnerTextLineBreakCount(nsIFrame* aFrame)
{
  if (aFrame->GetContent()->IsHTMLElement(nsGkAtoms::p)) {
    return 2;
  }
  const nsStyleDisplay* styleDisplay = aFrame->StyleDisplay();
  if (styleDisplay->IsBlockOutside(aFrame) ||
      styleDisplay->mDisplay == StyleDisplay::TableCaption) {
    return 1;
  }
  return 0;
}

static bool
IsLastCellOfRow(nsIFrame* aFrame)
{
  LayoutFrameType type = aFrame->Type();
  if (type != LayoutFrameType::TableCell &&
      type != LayoutFrameType::BCTableCell) {
    return true;
  }
  for (nsIFrame* c = aFrame; c; c = c->GetNextContinuation()) {
    if (c->GetNextSibling()) {
      return false;
    }
  }
  return true;
}

static bool
IsLastRowOfRowGroup(nsIFrame* aFrame)
{
  if (!aFrame->IsTableRowFrame()) {
    return true;
  }
  for (nsIFrame* c = aFrame; c; c = c->GetNextContinuation()) {
    if (c->GetNextSibling()) {
      return false;
    }
  }
  return true;
}

static bool
IsLastNonemptyRowGroupOfTable(nsIFrame* aFrame)
{
  if (!aFrame->IsTableRowGroupFrame()) {
    return true;
  }
  for (nsIFrame* c = aFrame; c; c = c->GetNextContinuation()) {
    for (nsIFrame* next = c->GetNextSibling(); next; next = next->GetNextSibling()) {
      if (next->PrincipalChildList().FirstChild()) {
        return false;
      }
    }
  }
  return true;
}

void
nsRange::GetInnerTextNoFlush(DOMString& aValue, ErrorResult& aError,
                             nsIContent* aContainer)
{
  InnerTextAccumulator result(aValue);

  if (aContainer->IsText()) {
    AppendTransformedText(result, aContainer);
    return;
  }

  nsIContent* currentNode = aContainer;
  TreeTraversalState currentState = AFTER_NODE;

  nsIContent* endNode = aContainer;
  TreeTraversalState endState = AFTER_NODE;

  nsIContent* firstChild = aContainer->GetFirstChild();
  if (firstChild) {
    currentNode = firstChild;
    currentState = AT_NODE;
  }

  while (currentNode != endNode || currentState != endState) {
    nsIFrame* f = currentNode->GetPrimaryFrame();
    bool isVisibleAndNotReplaced = IsVisibleAndNotInReplacedElement(f);
    if (currentState == AT_NODE) {
      bool isText = currentNode->IsText();
      if (isText && currentNode->GetParent()->IsHTMLElement(nsGkAtoms::rp) &&
          ElementIsVisibleNoFlush(currentNode->GetParent()->AsElement())) {
        nsAutoString str;
        currentNode->GetTextContent(str, aError);
        result.Append(str);
      } else if (isVisibleAndNotReplaced) {
        result.AddRequiredLineBreakCount(GetRequiredInnerTextLineBreakCount(f));
        if (isText) {
          nsIFrame::RenderedText text = f->GetRenderedText();
          result.Append(text.mString);
        }
      }
      nsIContent* child = currentNode->GetFirstChild();
      if (child) {
        currentNode = child;
        continue;
      }
      currentState = AFTER_NODE;
    }
    if (currentNode == endNode && currentState == endState) {
      break;
    }
    if (isVisibleAndNotReplaced) {
      if (currentNode->IsHTMLElement(nsGkAtoms::br)) {
        result.Append('\n');
      }
      switch (f->StyleDisplay()->mDisplay) {
      case StyleDisplay::TableCell:
        if (!IsLastCellOfRow(f)) {
          result.Append('\t');
        }
        break;
      case StyleDisplay::TableRow:
        if (!IsLastRowOfRowGroup(f) ||
            !IsLastNonemptyRowGroupOfTable(f->GetParent())) {
          result.Append('\n');
        }
        break;
      default:
        break; // Do nothing
      }
      result.AddRequiredLineBreakCount(GetRequiredInnerTextLineBreakCount(f));
    }
    nsIContent* next = currentNode->GetNextSibling();
    if (next) {
      currentNode = next;
      currentState = AT_NODE;
    } else {
      currentNode = currentNode->GetParent();
    }
  }

  // Do not flush trailing line breaks! Required breaks at the end of the text
  // are suppressed.
}
