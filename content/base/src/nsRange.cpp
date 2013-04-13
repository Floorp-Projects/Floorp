/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the DOM nsIDOMRange object.
 */

#include "nscore.h"
#include "nsRange.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMDocumentType.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMText.h"
#include "nsError.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsGenericDOMDataNode.h"
#include "nsClientRect.h"
#include "nsLayoutUtils.h"
#include "nsTextFrame.h"
#include "nsFontFaceList.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Likely.h"

using namespace mozilla;

JSObject*
nsRange::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return dom::RangeBinding::Wrap(aCx, aScope, this);
}

/******************************************************
 * stack based utilty class for managing monitor
 ******************************************************/

static void InvalidateAllFrames(nsINode* aNode)
{
  NS_PRECONDITION(aNode, "bad arg");

  nsIFrame* frame = nullptr;
  switch (aNode->NodeType()) {
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::ELEMENT_NODE:
    {
      nsIContent* content = static_cast<nsIContent*>(aNode);
      frame = content->GetPrimaryFrame();
      break;
    }
    case nsIDOMNode::DOCUMENT_NODE:
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
    nodeEnd = aNode->GetChildCount();
  }
  else {
    nodeStart = parent->IndexOf(aNode);
    nodeEnd = nodeStart + 1;
  }

  nsINode* rangeStartParent = aRange->GetStartParent();
  nsINode* rangeEndParent = aRange->GetEndParent();
  int32_t rangeStartOffset = aRange->StartOffset();
  int32_t rangeEndOffset = aRange->EndOffset();

  // is RANGE(start) <= NODE(start) ?
  bool disconnected = false;
  *outNodeBefore = nsContentUtils::ComparePoints(rangeStartParent,
                                                 rangeStartOffset,
                                                 parent, nodeStart,
                                                 &disconnected) > 0;
  NS_ENSURE_TRUE(!disconnected, NS_ERROR_DOM_WRONG_DOCUMENT_ERR);

  // is RANGE(end) >= NODE(end) ?
  *outNodeAfter = nsContentUtils::ComparePoints(rangeEndParent,
                                                rangeEndOffset,
                                                parent, nodeEnd,
                                                &disconnected) < 0;
  NS_ENSURE_TRUE(!disconnected, NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
  return NS_OK;
}

struct FindSelectedRangeData
{
  nsINode*  mNode;
  nsRange* mResult;
  uint32_t  mStartOffset;
  uint32_t  mEndOffset;
};

static PLDHashOperator
FindSelectedRange(nsPtrHashKey<nsRange>* aEntry, void* userArg)
{
  nsRange* range = aEntry->GetKey();
  if (range->IsInSelection() && !range->Collapsed()) {
    FindSelectedRangeData* data = static_cast<FindSelectedRangeData*>(userArg);
    int32_t cmp = nsContentUtils::ComparePoints(data->mNode, data->mEndOffset,
                                                range->GetStartParent(),
                                                range->StartOffset());
    if (cmp == 1) {
      cmp = nsContentUtils::ComparePoints(data->mNode, data->mStartOffset,
                                          range->GetEndParent(),
                                          range->EndOffset());
      if (cmp == -1) {
        data->mResult = range;
        return PL_DHASH_STOP;
      }
    }
  }
  return PL_DHASH_NEXT;
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

/* static */ bool
nsRange::IsNodeSelected(nsINode* aNode, uint32_t aStartOffset,
                        uint32_t aEndOffset)
{
  NS_PRECONDITION(aNode, "bad arg");

  FindSelectedRangeData data = { aNode, nullptr, aStartOffset, aEndOffset };
  nsINode* n = GetNextRangeCommonAncestor(aNode);
  NS_ASSERTION(n || !aNode->IsSelectionDescendant(),
               "orphan selection descendant");
  for (; n; n = GetNextRangeCommonAncestor(n->GetParentNode())) {
    RangeHashTable* ranges =
      static_cast<RangeHashTable*>(n->GetProperty(nsGkAtoms::range));
    ranges->EnumerateEntries(FindSelectedRange, &data);
    if (data.mResult) {
      return true;
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

  // Maybe we can remove Detach() -- bug 702948.
  Telemetry::Accumulate(Telemetry::DOM_RANGE_DETACHED, mIsDetached);

  // we want the side effects (releases and list removals)
  DoSetRange(nullptr, 0, nullptr, 0, nullptr);
} 

/* static */
nsresult
nsRange::CreateRange(nsIDOMNode* aStartParent, int32_t aStartOffset,
                     nsIDOMNode* aEndParent, int32_t aEndOffset,
                     nsRange** aRange)
{
  MOZ_ASSERT(aRange);
  *aRange = nullptr;

  nsCOMPtr<nsINode> startParent = do_QueryInterface(aStartParent);
  NS_ENSURE_ARG_POINTER(startParent);

  nsRefPtr<nsRange> range = new nsRange(startParent);

  nsresult rv = range->SetStart(startParent, aStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = range->SetEnd(aEndParent, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  range.forget(aRange);
  return NS_OK;
}

/* static */
nsresult
nsRange::CreateRange(nsIDOMNode* aStartParent, int32_t aStartOffset,
                     nsIDOMNode* aEndParent, int32_t aEndOffset,
                     nsIDOMRange** aRange)
{
  nsRefPtr<nsRange> range;
  nsresult rv = nsRange::CreateRange(aStartParent, aStartOffset, aEndParent,
                                     aEndOffset, getter_AddRefs(range));
  range.forget(aRange);
  return rv;
}

/******************************************************
 * nsISupports
 ******************************************************/

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsRange)

// QueryInterface implementation for nsRange
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsRange)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMRange)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMRange)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);
  tmp->Reset();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStartParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END


static void
RangeHashTableDtor(void* aObject, nsIAtom* aPropertyName, void* aPropertyValue,
                   void* aData)
{
  nsRange::RangeHashTable* ranges =
    static_cast<nsRange::RangeHashTable*>(aPropertyValue);
  delete ranges;
}

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
  NS_PRECONDITION(aNode, "bad arg");
  NS_ASSERTION(IsInSelection(), "registering range not in selection");

  MarkDescendants(aNode);

  RangeHashTable* ranges =
    static_cast<RangeHashTable*>(aNode->GetProperty(nsGkAtoms::range));
  if (!ranges) {
    ranges = new RangeHashTable;
    ranges->Init();
    aNode->SetProperty(nsGkAtoms::range, ranges, RangeHashTableDtor, true);
  }
  ranges->PutEntry(this);
  aNode->SetCommonAncestorForRangeInSelection();
}

void
nsRange::UnregisterCommonAncestor(nsINode* aNode)
{
  NS_PRECONDITION(aNode, "bad arg");
  NS_ASSERTION(aNode->IsCommonAncestorForRangeInSelection(), "wrong node");
  RangeHashTable* ranges =
    static_cast<RangeHashTable*>(aNode->GetProperty(nsGkAtoms::range));
  NS_ASSERTION(ranges->GetEntry(this), "unknown range");

  if (ranges->Count() == 1) {
    aNode->ClearCommonAncestorForRangeInSelection();
    aNode->DeleteProperty(nsGkAtoms::range);
    UnmarkDescendants(aNode);
  } else {
    ranges->RemoveEntry(this);
  }
}

/******************************************************
 * nsIMutationObserver implementation
 ******************************************************/

void
nsRange::CharacterDataChanged(nsIDocument* aDocument,
                              nsIContent* aContent,
                              CharacterDataChangeInfo* aInfo)
{
  MOZ_ASSERT(mAssertNextInsertOrAppendIndex == -1,
             "splitText failed to notify insert/append?");
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* newRoot = nullptr;
  nsINode* newStartNode = nullptr;
  nsINode* newEndNode = nullptr;
  uint32_t newStartOffset = 0;
  uint32_t newEndOffset = 0;

  if (aInfo->mDetails &&
      aInfo->mDetails->mType == CharacterDataChangeInfo::Details::eSplit) {
    // If the splitted text node is immediately before a range boundary point
    // that refers to a child index (i.e. its parent is the boundary container)
    // then we need to increment the corresponding offset to account for the new
    // text node that will be inserted.  If so, we need to prevent the next
    // ContentInserted or ContentAppended for this range from incrementing it
    // again (when the new text node is notified).
    nsINode* parentNode = aContent->GetParentNode();
    int32_t index = -1;
    if (parentNode == mEndParent && mEndOffset > 0 &&
        (index = parentNode->IndexOf(aContent)) + 1 == mEndOffset) {
      ++mEndOffset;
      mEndOffsetWasIncremented = true;
    }
    if (parentNode == mStartParent && mStartOffset > 0 &&
        (index != -1 ? index : parentNode->IndexOf(aContent)) + 1 == mStartOffset) {
      ++mStartOffset;
      mStartOffsetWasIncremented = true;
    }
#ifdef DEBUG
    if (mStartOffsetWasIncremented || mEndOffsetWasIncremented) {
      mAssertNextInsertOrAppendIndex =
        (mStartOffsetWasIncremented ? mStartOffset : mEndOffset) - 1;
      mAssertNextInsertOrAppendNode = aInfo->mDetails->mNextSibling;
    }
#endif
  }

  // If the changed node contains our start boundary and the change starts
  // before the boundary we'll need to adjust the offset.
  if (aContent == mStartParent &&
      aInfo->mChangeStart < static_cast<uint32_t>(mStartOffset)) {
    if (aInfo->mDetails) {
      // splitText(), aInfo->mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo->mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(static_cast<uint32_t>(mStartOffset) <= aInfo->mChangeEnd + 1,
                   "mStartOffset is beyond the end of this node");
      newStartOffset = static_cast<uint32_t>(mStartOffset) - aInfo->mChangeStart;
      newStartNode = aInfo->mDetails->mNextSibling;
      if (MOZ_UNLIKELY(aContent == mRoot)) {
        newRoot = IsValidBoundary(newStartNode);
      }

      bool isCommonAncestor = IsInSelection() && mStartParent == mEndParent;
      if (isCommonAncestor) {
        UnregisterCommonAncestor(mStartParent);
        RegisterCommonAncestor(newStartNode);
      }
      if (mStartParent->IsDescendantOfCommonAncestorForRangeInSelection()) {
        newStartNode->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      // If boundary is inside changed text, position it before change
      // else adjust start offset for the change in length.
      mStartOffset = static_cast<uint32_t>(mStartOffset) <= aInfo->mChangeEnd ?
        aInfo->mChangeStart :
        mStartOffset + aInfo->mChangeStart - aInfo->mChangeEnd +
          aInfo->mReplaceLength;
    }
  }

  // Do the same thing for the end boundary, except for splitText of a node
  // with no parent then only switch to the new node if the start boundary
  // did so too (otherwise the range would end up with disconnected nodes).
  if (aContent == mEndParent &&
      aInfo->mChangeStart < static_cast<uint32_t>(mEndOffset)) {
    if (aInfo->mDetails && (aContent->GetParentNode() || newStartNode)) {
      // splitText(), aInfo->mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo->mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(static_cast<uint32_t>(mEndOffset) <= aInfo->mChangeEnd + 1,
                   "mEndOffset is beyond the end of this node");
      newEndOffset = static_cast<uint32_t>(mEndOffset) - aInfo->mChangeStart;
      newEndNode = aInfo->mDetails->mNextSibling;

      bool isCommonAncestor = IsInSelection() && mStartParent == mEndParent;
      if (isCommonAncestor && !newStartNode) {
        // The split occurs inside the range.
        UnregisterCommonAncestor(mStartParent);
        RegisterCommonAncestor(mStartParent->GetParentNode());
        newEndNode->SetDescendantOfCommonAncestorForRangeInSelection();
      } else if (mEndParent->IsDescendantOfCommonAncestorForRangeInSelection()) {
        newEndNode->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      mEndOffset = static_cast<uint32_t>(mEndOffset) <= aInfo->mChangeEnd ?
        aInfo->mChangeStart :
        mEndOffset + aInfo->mChangeStart - aInfo->mChangeEnd +
          aInfo->mReplaceLength;
    }
  }

  if (aInfo->mDetails &&
      aInfo->mDetails->mType == CharacterDataChangeInfo::Details::eMerge) {
    // normalize(), aInfo->mDetails->mNextSibling is the merged text node
    // that will be removed
    nsIContent* removed = aInfo->mDetails->mNextSibling;
    if (removed == mStartParent) {
      newStartOffset = static_cast<uint32_t>(mStartOffset) + aInfo->mChangeStart;
      newStartNode = aContent;
      if (MOZ_UNLIKELY(removed == mRoot)) {
        newRoot = IsValidBoundary(newStartNode);
      }
    }
    if (removed == mEndParent) {
      newEndOffset = static_cast<uint32_t>(mEndOffset) + aInfo->mChangeStart;
      newEndNode = aContent;
      if (MOZ_UNLIKELY(removed == mRoot)) {
        newRoot = IsValidBoundary(newEndNode);
      }
    }
    // When the removed text node's parent is one of our boundary nodes we may
    // need to adjust the offset to account for the removed node. However,
    // there will also be a ContentRemoved notification later so the only cases
    // we need to handle here is when the removed node is the text node after
    // the boundary.  (The m*Offset > 0 check is an optimization - a boundary
    // point before the first child is never affected by normalize().)
    nsINode* parentNode = aContent->GetParentNode();
    if (parentNode == mStartParent && mStartOffset > 0 &&
        mStartOffset < parentNode->GetChildCount() &&
        removed == parentNode->GetChildAt(mStartOffset)) {
      newStartNode = aContent;
      newStartOffset = aInfo->mChangeStart;
    }
    if (parentNode == mEndParent && mEndOffset > 0 &&
        mEndOffset < parentNode->GetChildCount() &&
        removed == parentNode->GetChildAt(mEndOffset)) {
      newEndNode = aContent;
      newEndOffset = aInfo->mChangeEnd;
    }
  }

  if (newStartNode || newEndNode) {
    if (!newStartNode) {
      newStartNode = mStartParent;
      newStartOffset = mStartOffset;
    }
    if (!newEndNode) {
      newEndNode = mEndParent;
      newEndOffset = mEndOffset;
    }
    DoSetRange(newStartNode, newStartOffset, newEndNode, newEndOffset,
               newRoot ? newRoot : mRoot.get(),
               !newEndNode->GetParentNode() || !newStartNode->GetParentNode());
  }
}

void
nsRange::ContentAppended(nsIDocument* aDocument,
                         nsIContent*  aContainer,
                         nsIContent*  aFirstNewContent,
                         int32_t      aNewIndexInContainer)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* container = NODE_FROM(aContainer, aDocument);
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

  if (mStartOffsetWasIncremented || mEndOffsetWasIncremented) {
    MOZ_ASSERT(mAssertNextInsertOrAppendIndex == aNewIndexInContainer);
    MOZ_ASSERT(mAssertNextInsertOrAppendNode == aFirstNewContent);
    MOZ_ASSERT(aFirstNewContent->IsNodeOfType(nsINode::eDATA_NODE));
    mStartOffsetWasIncremented = mEndOffsetWasIncremented = false;
#ifdef DEBUG
    mAssertNextInsertOrAppendIndex = -1;
    mAssertNextInsertOrAppendNode = nullptr;
#endif
  }
}

void
nsRange::ContentInserted(nsIDocument* aDocument,
                         nsIContent* aContainer,
                         nsIContent* aChild,
                         int32_t aIndexInContainer)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* container = NODE_FROM(aContainer, aDocument);

  // Adjust position if a sibling was inserted.
  if (container == mStartParent && aIndexInContainer < mStartOffset &&
      !mStartOffsetWasIncremented) {
    ++mStartOffset;
  }
  if (container == mEndParent && aIndexInContainer < mEndOffset &&
      !mEndOffsetWasIncremented) {
    ++mEndOffset;
  }
  if (container->IsSelectionDescendant() &&
      !aChild->IsDescendantOfCommonAncestorForRangeInSelection()) {
    MarkDescendants(aChild);
    aChild->SetDescendantOfCommonAncestorForRangeInSelection();
  }

  if (mStartOffsetWasIncremented || mEndOffsetWasIncremented) {
    MOZ_ASSERT(mAssertNextInsertOrAppendIndex == aIndexInContainer);
    MOZ_ASSERT(mAssertNextInsertOrAppendNode == aChild);
    MOZ_ASSERT(aChild->IsNodeOfType(nsINode::eDATA_NODE));
    mStartOffsetWasIncremented = mEndOffsetWasIncremented = false;
#ifdef DEBUG
    mAssertNextInsertOrAppendIndex = -1;
    mAssertNextInsertOrAppendNode = nullptr;
#endif
  }
}

void
nsRange::ContentRemoved(nsIDocument* aDocument,
                        nsIContent* aContainer,
                        nsIContent* aChild,
                        int32_t aIndexInContainer,
                        nsIContent* aPreviousSibling)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");
  MOZ_ASSERT(!mStartOffsetWasIncremented && !mEndOffsetWasIncremented &&
             mAssertNextInsertOrAppendIndex == -1,
             "splitText failed to notify insert/append?");

  nsINode* container = NODE_FROM(aContainer, aDocument);
  bool gravitateStart = false;
  bool gravitateEnd = false;

  // Adjust position if a sibling was removed...
  if (container == mStartParent) {
    if (aIndexInContainer < mStartOffset) {
      --mStartOffset;
    }
  }
  // ...or gravitate if an ancestor was removed.
  else if (nsContentUtils::ContentIsDescendantOf(mStartParent, aChild)) {
    gravitateStart = true;
  }

  // Do same thing for end boundry.
  if (container == mEndParent) {
    if (aIndexInContainer < mEndOffset) {
      --mEndOffset;
    }
  }
  else if (nsContentUtils::ContentIsDescendantOf(mEndParent, aChild)) {
    gravitateEnd = true;
  }

  if (gravitateStart || gravitateEnd) {
    DoSetRange(gravitateStart ? container : mStartParent.get(),
               gravitateStart ? aIndexInContainer : mStartOffset,
               gravitateEnd ? container : mEndParent.get(),
               gravitateEnd ? aIndexInContainer : mEndOffset,
               mRoot);
  }
  if (container->IsSelectionDescendant() &&
      aChild->IsDescendantOfCommonAncestorForRangeInSelection()) {
    aChild->ClearDescendantOfCommonAncestorForRangeInSelection();
    UnmarkDescendants(aChild);
  }
}

void
nsRange::ParentChainChanged(nsIContent *aContent)
{
  MOZ_ASSERT(!mStartOffsetWasIncremented && !mEndOffsetWasIncremented &&
             mAssertNextInsertOrAppendIndex == -1,
             "splitText failed to notify insert/append?");
  NS_ASSERTION(mRoot == aContent, "Wrong ParentChainChanged notification?");
  nsINode* newRoot = IsValidBoundary(mStartParent);
  NS_ASSERTION(newRoot, "No valid boundary or root found!");
  if (newRoot != IsValidBoundary(mEndParent)) {
    // Sometimes ordering involved in cycle collection can lead to our
    // start parent and/or end parent being disconnected from our root
    // without our getting a ContentRemoved notification.
    // See bug 846096 for more details.
    NS_ASSERTION(mEndParent->IsInNativeAnonymousSubtree(),
                 "This special case should happen only with "
                 "native-anonymous content");
    // When that happens, bail out and set pointers to null; since we're
    // in cycle collection and unreachable it shouldn't matter.
    Reset();
    return;
  }
  // This is safe without holding a strong ref to self as long as the change
  // of mRoot is the last thing in DoSetRange.
  DoSetRange(mStartParent, mStartOffset, mEndParent, mEndOffset, newRoot);
}

/******************************************************
 * Utilities for comparing points: API from nsIDOMRange
 ******************************************************/
NS_IMETHODIMP
nsRange::IsPointInRange(nsIDOMNode* aParent, int32_t aOffset, bool* aResult)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  if (!parent) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  *aResult = IsPointInRange(*parent, aOffset, rv);
  return rv.ErrorCode();
}

bool
nsRange::IsPointInRange(nsINode& aParent, uint32_t aOffset, ErrorResult& aRv)
{
  uint16_t compareResult = ComparePoint(aParent, aOffset, aRv);
  // If the node isn't in the range's document, it clearly isn't in the range.
  if (aRv.ErrorCode() == NS_ERROR_DOM_WRONG_DOCUMENT_ERR) {
    aRv = NS_OK;
    return false;
  }

  return compareResult == 0;
}

// returns -1 if point is before range, 0 if point is in range,
// 1 if point is after range.
NS_IMETHODIMP
nsRange::ComparePoint(nsIDOMNode* aParent, int32_t aOffset, int16_t* aResult)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);

  ErrorResult rv;
  *aResult = ComparePoint(*parent, aOffset, rv);
  return rv.ErrorCode();
}

int16_t
nsRange::ComparePoint(nsINode& aParent, uint32_t aOffset, ErrorResult& aRv)
{
  // our range is in a good state?
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  if (!nsContentUtils::ContentIsDescendantOf(&aParent, mRoot)) {
    aRv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
    return 0;
  }

  if (aParent.NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return 0;
  }

  if (aOffset > aParent.Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  int32_t cmp;
  if ((cmp = nsContentUtils::ComparePoints(&aParent, aOffset,
                                           mStartParent, mStartOffset)) <= 0) {

    return cmp;
  }
  if (nsContentUtils::ComparePoints(mEndParent, mEndOffset,
                                    &aParent, aOffset) == -1) {
    return 1;
  }

  return 0;
}

NS_IMETHODIMP
nsRange::IntersectsNode(nsIDOMNode* aNode, bool* aResult)
{
  *aResult = false;

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  // TODO: This should throw a TypeError.
  NS_ENSURE_ARG(node);

  ErrorResult rv;
  *aResult = IntersectsNode(*node, rv);
  return rv.ErrorCode();
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
  int32_t nodeIndex = parent->IndexOf(&aNode);

  // Steps 6-7.
  // Note: if disconnected is true, ComparePoints returns 1.
  bool disconnected = false;
  bool result = nsContentUtils::ComparePoints(mStartParent, mStartOffset,
                                             parent, nodeIndex + 1,
                                             &disconnected) < 0 &&
               nsContentUtils::ComparePoints(parent, nodeIndex,
                                             mEndParent, mEndOffset,
                                             &disconnected) < 0;

  // Step 2.
  if (disconnected) {
    result = false;
  }
  return result;
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
nsRange::DoSetRange(nsINode* aStartN, int32_t aStartOffset,
                    nsINode* aEndN, int32_t aEndOffset,
                    nsINode* aRoot, bool aNotInsertedYet)
{
  NS_PRECONDITION((aStartN && aEndN && aRoot) ||
                  (!aStartN && !aEndN && !aRoot),
                  "Set all or none");
  NS_PRECONDITION(!aRoot || aNotInsertedYet ||
                  (nsContentUtils::ContentIsDescendantOf(aStartN, aRoot) &&
                   nsContentUtils::ContentIsDescendantOf(aEndN, aRoot) &&
                   aRoot == IsValidBoundary(aStartN) &&
                   aRoot == IsValidBoundary(aEndN)),
                  "Wrong root");
  NS_PRECONDITION(!aRoot ||
                  (aStartN->IsNodeOfType(nsINode::eCONTENT) &&
                   aEndN->IsNodeOfType(nsINode::eCONTENT) &&
                   aRoot ==
                    static_cast<nsIContent*>(aStartN)->GetBindingParent() &&
                   aRoot ==
                    static_cast<nsIContent*>(aEndN)->GetBindingParent()) ||
                  (!aRoot->GetParentNode() &&
                   (aRoot->IsNodeOfType(nsINode::eDOCUMENT) ||
                    aRoot->IsNodeOfType(nsINode::eATTRIBUTE) ||
                    aRoot->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT) ||
                     /*For backward compatibility*/
                    aRoot->IsNodeOfType(nsINode::eCONTENT))),
                  "Bad root");

  if (mRoot != aRoot) {
    if (mRoot) {
      mRoot->RemoveMutationObserver(this);
    }
    if (aRoot) {
      aRoot->AddMutationObserver(this);
    }
  }
  bool checkCommonAncestor = (mStartParent != aStartN || mEndParent != aEndN) &&
                             IsInSelection() && !aNotInsertedYet;
  nsINode* oldCommonAncestor = checkCommonAncestor ? GetCommonAncestor() : nullptr;
  mStartParent = aStartN;
  mStartOffset = aStartOffset;
  mEndParent = aEndN;
  mEndOffset = aEndOffset;
  mIsPositioned = !!mStartParent;
  if (checkCommonAncestor) {
    nsINode* newCommonAncestor = GetCommonAncestor();
    if (newCommonAncestor != oldCommonAncestor) {
      if (oldCommonAncestor) {
        UnregisterCommonAncestor(oldCommonAncestor);
      }
      if (newCommonAncestor) {
        RegisterCommonAncestor(newCommonAncestor);
      } else {
        NS_ASSERTION(!mIsPositioned, "unexpected disconnected nodes");
        mInSelection = false;
      }
    }
  }

  // This needs to be the last thing this function does.  See comment
  // in ParentChainChanged.
  mRoot = aRoot;
}

static int32_t
IndexOf(nsINode* aChild)
{
  nsINode* parent = aChild->GetParentNode();

  return parent ? parent->IndexOf(aChild) : -1;
}

static int32_t
IndexOf(nsIDOMNode* aChildNode)
{
  // convert node to nsIContent, so that we can find the child index

  nsCOMPtr<nsINode> child = do_QueryInterface(aChildNode);
  if (!child) {
    return -1;
  }
  return IndexOf(child);
}

nsINode*
nsRange::GetCommonAncestor() const
{
  return mIsPositioned ?
    nsContentUtils::GetCommonAncestor(mStartParent, mEndParent) :
    nullptr;
}

void
nsRange::Reset()
{
  DoSetRange(nullptr, 0, nullptr, 0, nullptr);
}

/******************************************************
 * public functionality
 ******************************************************/

NS_IMETHODIMP
nsRange::GetStartContainer(nsIDOMNode** aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mStartParent, aStartParent);
}

nsINode*
nsRange::GetStartContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mStartParent;
}

NS_IMETHODIMP
nsRange::GetStartOffset(int32_t* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aStartOffset = mStartOffset;

  return NS_OK;
}

uint32_t
nsRange::GetStartOffset(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  return mStartOffset;
}

NS_IMETHODIMP
nsRange::GetEndContainer(nsIDOMNode** aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mEndParent, aEndParent);
}

nsINode*
nsRange::GetEndContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mEndParent;
}

NS_IMETHODIMP
nsRange::GetEndOffset(int32_t* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aEndOffset = mEndOffset;

  return NS_OK;
}

uint32_t
nsRange::GetEndOffset(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  return mEndOffset;
}

NS_IMETHODIMP
nsRange::GetCollapsed(bool* aIsCollapsed)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aIsCollapsed = Collapsed();

  return NS_OK;
}

nsINode*
nsRange::GetCommonAncestorContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return nsContentUtils::GetCommonAncestor(mStartParent, mEndParent);
}

NS_IMETHODIMP
nsRange::GetCommonAncestorContainer(nsIDOMNode** aCommonParent)
{
  ErrorResult rv;
  nsINode* commonAncestor = GetCommonAncestorContainer(rv);
  if (commonAncestor) {
    NS_ADDREF(*aCommonParent = commonAncestor->AsDOMNode());
  } else {
    *aCommonParent = nullptr;
  }

  return rv.ErrorCode();
}

nsINode*
nsRange::IsValidBoundary(nsINode* aNode)
{
  if (!aNode) {
    return nullptr;
  }

  if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIContent* content = static_cast<nsIContent*>(aNode);
    if (content->Tag() == nsGkAtoms::documentTypeNodeName) {
      return nullptr;
    }

    if (!mMaySpanAnonymousSubtrees) {
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
  nsINode* root = aNode->GetCurrentDoc();
  if (root) {
    return root;
  }

  root = aNode;
  while ((aNode = aNode->GetParentNode())) {
    root = aNode;
  }

  NS_ASSERTION(!root->IsNodeOfType(nsINode::eDOCUMENT),
               "GetCurrentDoc should have returned a doc");

#ifdef DEBUG_smaug
  NS_WARN_IF_FALSE(root->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT) ||
                   root->IsNodeOfType(nsINode::eATTRIBUTE),
                   "Creating a DOM Range using root which isn't in DOM!");
#endif

  // We allow this because of backward compatibility.
  return root;
}

void
nsRange::SetStart(nsINode& aNode, uint32_t aOffset, ErrorResult& aRv)
{
 if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetStart(&aNode, aOffset);
}

NS_IMETHODIMP
nsRange::SetStart(nsIDOMNode* aParent, int32_t aOffset)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  if (!parent) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStart(*parent, aOffset, rv);
  return rv.ErrorCode();
}

/* virtual */ nsresult
nsRange::SetStart(nsINode* aParent, int32_t aOffset)
{
  nsINode* newRoot = IsValidBoundary(aParent);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  if (aOffset < 0 || uint32_t(aOffset) > aParent->Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new start is after end.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(aParent, aOffset,
                                    mEndParent, mEndOffset) == 1) {
    DoSetRange(aParent, aOffset, aParent, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(aParent, aOffset, mEndParent, mEndOffset, mRoot);
  
  return NS_OK;
}

void
nsRange::SetStartBefore(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetStart(aNode.GetParentNode(), IndexOf(&aNode));
}

NS_IMETHODIMP
nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStartBefore(*sibling, rv);
  return rv.ErrorCode();
}

void
nsRange::SetStartAfter(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetStart(aNode.GetParentNode(), IndexOf(&aNode) + 1);
}

NS_IMETHODIMP
nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStartAfter(*sibling, rv);
  return rv.ErrorCode();
}

void
nsRange::SetEnd(nsINode& aNode, uint32_t aOffset, ErrorResult& aRv)
{
 if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }
  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetEnd(&aNode, aOffset);
}

NS_IMETHODIMP
nsRange::SetEnd(nsIDOMNode* aParent, int32_t aOffset)
{
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  if (!parent) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEnd(*parent, aOffset, rv);
  return rv.ErrorCode();
}

/* virtual */ nsresult
nsRange::SetEnd(nsINode* aParent, int32_t aOffset)
{
  nsINode* newRoot = IsValidBoundary(aParent);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  if (aOffset < 0 || uint32_t(aOffset) > aParent->Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new end is before start.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(mStartParent, mStartOffset,
                                    aParent, aOffset) == 1) {
    DoSetRange(aParent, aOffset, aParent, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(mStartParent, mStartOffset, aParent, aOffset, mRoot);

  return NS_OK;
}

void
nsRange::SetEndBefore(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetEnd(aNode.GetParentNode(), IndexOf(&aNode));
}

NS_IMETHODIMP
nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEndBefore(*sibling, rv);
  return rv.ErrorCode();
}

void
nsRange::SetEndAfter(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  aRv = SetEnd(aNode.GetParentNode(), IndexOf(&aNode) + 1);
}

NS_IMETHODIMP
nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEndAfter(*sibling, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
nsRange::Collapse(bool aToStart)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  AutoInvalidateSelection atEndOfBlock(this);
  if (aToStart)
    DoSetRange(mStartParent, mStartOffset, mStartParent, mStartOffset, mRoot);
  else
    DoSetRange(mEndParent, mEndOffset, mEndParent, mEndOffset, mRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsRange::SelectNode(nsIDOMNode* aN)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  NS_ENSURE_TRUE(node, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  ErrorResult rv;
  SelectNode(*node, rv);
  return rv.ErrorCode();
}

void
nsRange::SelectNode(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsINode* parent = aNode.GetParentNode();
  nsINode* newRoot = IsValidBoundary(parent);
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  int32_t index = parent->IndexOf(&aNode);
  if (index < 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  DoSetRange(parent, index, parent, index + 1, newRoot);
}

NS_IMETHODIMP
nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  NS_ENSURE_TRUE(node, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  ErrorResult rv;
  SelectNodeContents(*node, rv);
  return rv.ErrorCode();
}

void
nsRange::SelectNodeContents(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsINode* newRoot = IsValidBoundary(&aNode);
  if (!newRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  DoSetRange(&aNode, 0, &aNode, aNode.Length(), newRoot);
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

  nsCOMPtr<nsIDOMNode> mStart;
  nsCOMPtr<nsIDOMNode> mEnd;

public:

  RangeSubtreeIterator()
    : mIterState(eDone)
  {
  }
  ~RangeSubtreeIterator()
  {
  }

  nsresult Init(nsIDOMRange *aRange);
  already_AddRefed<nsIDOMNode> GetCurrentNode();
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
RangeSubtreeIterator::Init(nsIDOMRange *aRange)
{
  mIterState = eDone;
  bool collapsed;
  aRange->GetCollapsed(&collapsed);
  if (collapsed) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> node;

  // Grab the start point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  nsresult res = aRange->GetStartContainer(getter_AddRefs(node));
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMCharacterData> startData = do_QueryInterface(node);
  if (startData) {
    mStart = node;
  } else {
    int32_t startIndex;
    aRange->GetStartOffset(&startIndex);
    nsCOMPtr<nsINode> iNode = do_QueryInterface(node);
    if (iNode->IsElement() && 
        int32_t(iNode->AsElement()->GetChildCount()) == startIndex) {
      mStart = node;
    }
  }

  // Grab the end point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  res = aRange->GetEndContainer(getter_AddRefs(node));
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMCharacterData> endData = do_QueryInterface(node);
  if (endData) {
    mEnd = node;
  } else {
    int32_t endIndex;
    aRange->GetEndOffset(&endIndex);
    nsCOMPtr<nsINode> iNode = do_QueryInterface(node);
    if (iNode->IsElement() && endIndex == 0) {
      mEnd = node;
    }
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

    res = mIter->Init(aRange);
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

already_AddRefed<nsIDOMNode>
RangeSubtreeIterator::GetCurrentNode()
{
  nsIDOMNode *node = nullptr;

  if (mIterState == eUseStart && mStart) {
    NS_ADDREF(node = mStart);
  } else if (mIterState == eUseEnd && mEnd)
    NS_ADDREF(node = mEnd);
  else if (mIterState == eUseIterator && mIter)
  {
    nsINode* n = mIter->GetCurrentNode();

    if (n) {
      CallQueryInterface(n, &node);
    }
  }

  return node;
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
CollapseRangeAfterDelete(nsIDOMRange *aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Check if range gravity took care of collapsing the range for us!

  bool isCollapsed = false;
  nsresult res = aRange->GetCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  if (isCollapsed)
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

  nsCOMPtr<nsIDOMNode> commonAncestor;
  res = aRange->GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  if(NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> startContainer, endContainer;

  res = aRange->GetStartContainer(getter_AddRefs(startContainer));
  if (NS_FAILED(res)) return res;

  res = aRange->GetEndContainer(getter_AddRefs(endContainer));
  if (NS_FAILED(res)) return res;

  // Collapse to one of the end points if they are already in the
  // commonAncestor. This should work ok since this method is called
  // immediately after a delete or extract that leaves no content
  // between the 2 end points!

  if (startContainer == commonAncestor)
    return aRange->Collapse(true);
  if (endContainer == commonAncestor)
    return aRange->Collapse(false);

  // End points are at differing levels. We want to collapse to the
  // point that is between the 2 subtrees that contain each point,
  // under the common ancestor.

  nsCOMPtr<nsIDOMNode> nodeToSelect(startContainer), parent;

  while (nodeToSelect)
  {
    nsresult res = nodeToSelect->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;

    if (parent == commonAncestor)
      break; // We found the nodeToSelect!

    nodeToSelect = parent;
  }

  if (!nodeToSelect)
    return NS_ERROR_FAILURE; // This should never happen!

  res = aRange->SelectNode(nodeToSelect);
  if (NS_FAILED(res)) return res;

  return aRange->Collapse(false);
}

/**
 * Split a data node into two parts.
 *
 * @param aStartNode          The original node we are trying to split.
 * @param aStartIndex         The index at which to split.
 * @param aEndNode            The second node.
 * @param aCloneAfterOriginal Set false if the original node should be the
 *                            latter one after split.
 */
static nsresult SplitDataNode(nsIDOMCharacterData* aStartNode,
                              uint32_t aStartIndex,
                              nsIDOMCharacterData** aEndNode,
                              bool aCloneAfterOriginal = true)
{
  nsresult rv;
  nsCOMPtr<nsINode> node = do_QueryInterface(aStartNode);
  NS_ENSURE_STATE(node && node->IsNodeOfType(nsINode::eDATA_NODE));
  nsGenericDOMDataNode* dataNode = static_cast<nsGenericDOMDataNode*>(node.get());

  nsCOMPtr<nsIContent> newData;
  rv = dataNode->SplitData(aStartIndex, getter_AddRefs(newData),
                           aCloneAfterOriginal);
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(newData, aEndNode);
}

NS_IMETHODIMP
PrependChild(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIDOMNode> first, tmpNode;
  aParent->GetFirstChild(getter_AddRefs(first));
  return aParent->InsertBefore(aChild, first, getter_AddRefs(tmpNode));
}

// Helper function for CutContents, making sure that the current node wasn't
// removed by mutation events (bug 766426)
static bool
ValidateCurrentNode(nsRange* aRange, RangeSubtreeIterator& aIter)
{
  bool before, after;
  nsCOMPtr<nsIDOMNode> domNode = aIter.GetCurrentNode();
  if (!domNode) {
    // We don't have to worry that the node was removed if it doesn't exist,
    // e.g., the iterator is done.
    return true;
  }
  nsCOMPtr<nsINode> node = do_QueryInterface(domNode);
  MOZ_ASSERT(node);

  nsresult res = nsRange::CompareNodeToRange(node, aRange, &before, &after);

  return NS_SUCCEEDED(res) && !before && !after;
}

nsresult
nsRange::CutContents(dom::DocumentFragment** aFragment)
{ 
  if (aFragment) {
    *aFragment = nullptr;
  }

  nsCOMPtr<nsIDocument> doc = mStartParent->OwnerDoc();

  nsCOMPtr<nsIDOMNode> commonAncestor;
  nsresult rv = GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  NS_ENSURE_SUCCESS(rv, rv);

  // If aFragment isn't null, create a temporary fragment to hold our return.
  nsRefPtr<dom::DocumentFragment> retval;
  if (aFragment) {
    retval = new dom::DocumentFragment(doc->NodeInfoManager());
  }
  nsCOMPtr<nsIDOMNode> commonCloneAncestor = retval.get();

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(mRoot ? mRoot->OwnerDoc(): nullptr, nullptr);

  // Save the range end points locally to avoid interference
  // of Range gravity during our edits!

  nsCOMPtr<nsIDOMNode> startContainer = do_QueryInterface(mStartParent);
  int32_t              startOffset = mStartOffset;
  nsCOMPtr<nsIDOMNode> endContainer = do_QueryInterface(mEndParent);
  int32_t              endOffset = mEndOffset;

  if (retval) {
    // For extractContents(), abort early if there's a doctype (bug 719533).
    // This can happen only if the common ancestor is a document, in which case
    // we just need to find its doctype child and check if that's in the range.
    nsCOMPtr<nsIDOMDocument> commonAncestorDocument(do_QueryInterface(commonAncestor));
    if (commonAncestorDocument) {
      nsCOMPtr<nsIDOMDocumentType> doctype;
      rv = commonAncestorDocument->GetDoctype(getter_AddRefs(doctype));
      NS_ENSURE_SUCCESS(rv, rv);

      if (doctype &&
          nsContentUtils::ComparePoints(startContainer, startOffset,
                                        doctype.get(), 0) < 0 &&
          nsContentUtils::ComparePoints(doctype.get(), 0,
                                        endContainer, endOffset) < 0) {
        return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
      }
    }
  }

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  rv = iter.Init(this);
  if (NS_FAILED(rv)) return rv;

  if (iter.IsDone())
  {
    // There's nothing for us to delete.
    rv = CollapseRangeAfterDelete(this);
    if (NS_SUCCEEDED(rv) && aFragment) {
      NS_ADDREF(*aFragment = retval);
    }
    return rv;
  }

  // We delete backwards to avoid iterator problems!

  iter.Last();

  bool handled = false;

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsIDOMNode> nodeToResult;
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());

    // Before we delete anything, advance the iterator to the
    // next subtree.

    iter.Prev();

    handled = false;

    // If it's CharacterData, make sure we might need to delete
    // part of the data, instead of removing the whole node.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(node));

    if (charData)
    {
      uint32_t dataLength = 0;

      if (node == startContainer)
      {
        if (node == endContainer)
        {
          // This range is completely contained within a single text node.
          // Delete or extract the data between startOffset and endOffset.

          if (endOffset > startOffset)
          {
            if (retval) {
              nsAutoString cutValue;
              rv = charData->SubstringData(startOffset, endOffset - startOffset,
                                           cutValue);
              NS_ENSURE_SUCCESS(rv, rv);
              nsCOMPtr<nsIDOMNode> clone;
              rv = charData->CloneNode(false, 1, getter_AddRefs(clone));
              NS_ENSURE_SUCCESS(rv, rv);
              clone->SetNodeValue(cutValue);
              nodeToResult = clone;
            }

            nsMutationGuard guard;
            rv = charData->DeleteData(startOffset, endOffset - startOffset);
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_STATE(!guard.Mutated(0) ||
                            ValidateCurrentNode(this, iter));
          }

          handled = true;
        }
        else
        {
          // Delete or extract everything after startOffset.

          rv = charData->GetLength(&dataLength);
          NS_ENSURE_SUCCESS(rv, rv);

          if (dataLength >= (uint32_t)startOffset)
          {
            nsMutationGuard guard;
            nsCOMPtr<nsIDOMCharacterData> cutNode;
            rv = SplitDataNode(charData, startOffset, getter_AddRefs(cutNode));
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_STATE(!guard.Mutated(1) ||
                            ValidateCurrentNode(this, iter));
            nodeToResult = cutNode;
          }

          handled = true;
        }
      }
      else if (node == endContainer)
      {
        // Delete or extract everything before endOffset.

        if (endOffset >= 0)
        {
          nsMutationGuard guard;
          nsCOMPtr<nsIDOMCharacterData> cutNode;
          /* The Range spec clearly states clones get cut and original nodes
             remain behind, so use false as the last parameter.
          */
          rv = SplitDataNode(charData, endOffset, getter_AddRefs(cutNode),
                             false);
          NS_ENSURE_SUCCESS(rv, rv);
          NS_ENSURE_STATE(!guard.Mutated(1) ||
                          ValidateCurrentNode(this, iter));
          nodeToResult = cutNode;
        }

        handled = true;
      }       
    }

    if (!handled && (node == endContainer || node == startContainer))
    {
      nsCOMPtr<nsINode> iNode = do_QueryInterface(node);
      if (iNode && iNode->IsElement() &&
          ((node == endContainer && endOffset == 0) ||
           (node == startContainer &&
            int32_t(iNode->AsElement()->GetChildCount()) == startOffset)))
      {
        if (retval) {
          nsCOMPtr<nsIDOMNode> clone;
          rv = node->CloneNode(false, 1, getter_AddRefs(clone));
          NS_ENSURE_SUCCESS(rv, rv);
          nodeToResult = clone;
        }
        handled = true;
      }
    }

    if (!handled)
    {
      // node was not handled above, so it must be completely contained
      // within the range. Just remove it from the tree!
      nodeToResult = node;
    }

    uint32_t parentCount = 0;
    nsCOMPtr<nsIDOMNode> tmpNode;
    // Set the result to document fragment if we have 'retval'.
    if (retval) {
      nsCOMPtr<nsIDOMNode> oldCommonAncestor = commonAncestor;
      if (!iter.IsDone()) {
        // Setup the parameters for the next iteration of the loop.
        nsCOMPtr<nsIDOMNode> prevNode(iter.GetCurrentNode());
        NS_ENSURE_STATE(prevNode);

        // Get node's and prevNode's common parent. Do this before moving
        // nodes from original DOM to result fragment.
        nsContentUtils::GetCommonAncestor(node, prevNode,
                                          getter_AddRefs(commonAncestor));
        NS_ENSURE_STATE(commonAncestor);

        nsCOMPtr<nsIDOMNode> parentCounterNode = node;
        while (parentCounterNode && parentCounterNode != commonAncestor)
        {
          ++parentCount;
          tmpNode = parentCounterNode;
          tmpNode->GetParentNode(getter_AddRefs(parentCounterNode));
          NS_ENSURE_STATE(parentCounterNode);
        }
      }

      // Clone the parent hierarchy between commonAncestor and node.
      nsCOMPtr<nsIDOMNode> closestAncestor, farthestAncestor;
      rv = CloneParentsBetween(oldCommonAncestor, node,
                               getter_AddRefs(closestAncestor),
                               getter_AddRefs(farthestAncestor));
      NS_ENSURE_SUCCESS(rv, rv);

      if (farthestAncestor)
      {
        rv = PrependChild(commonCloneAncestor, farthestAncestor);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      nsMutationGuard guard;
      nsCOMPtr<nsIDOMNode> parent;
      nodeToResult->GetParentNode(getter_AddRefs(parent));
      rv = closestAncestor ? PrependChild(closestAncestor, nodeToResult)
                           : PrependChild(commonCloneAncestor, nodeToResult);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_STATE(!guard.Mutated(parent ? 2 : 1) ||
                      ValidateCurrentNode(this, iter));
    } else if (nodeToResult) {
      nsMutationGuard guard;
      nsCOMPtr<nsINode> node = do_QueryInterface(nodeToResult);
      nsINode* parent = node->GetParentNode();
      if (parent) {
        mozilla::ErrorResult error;
        parent->RemoveChild(*node, error);
        NS_ENSURE_FALSE(error.Failed(), error.ErrorCode());
      }
      NS_ENSURE_STATE(!guard.Mutated(1) ||
                      ValidateCurrentNode(this, iter));
    }

    if (!iter.IsDone() && retval) {
      // Find the equivalent of commonAncestor in the cloned tree.
      nsCOMPtr<nsIDOMNode> newCloneAncestor = nodeToResult;
      for (uint32_t i = parentCount; i; --i)
      {
        tmpNode = newCloneAncestor;
        tmpNode->GetParentNode(getter_AddRefs(newCloneAncestor));
        NS_ENSURE_STATE(newCloneAncestor);
      }
      commonCloneAncestor = newCloneAncestor;
    }
  }

  rv = CollapseRangeAfterDelete(this);
  if (NS_SUCCEEDED(rv) && aFragment) {
    NS_ADDREF(*aFragment = retval);
  }
  return rv;
}

NS_IMETHODIMP
nsRange::DeleteContents()
{
  return CutContents(nullptr);
}

void
nsRange::DeleteContents(ErrorResult& aRv)
{
  aRv = CutContents(nullptr);
}

NS_IMETHODIMP
nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  nsRefPtr<dom::DocumentFragment> fragment;
  nsresult rv = CutContents(getter_AddRefs(fragment));
  fragment.forget(aReturn);
  return rv;
}

already_AddRefed<dom::DocumentFragment>
nsRange::ExtractContents(ErrorResult& rv)
{
  nsRefPtr<dom::DocumentFragment> fragment;
  rv = CutContents(getter_AddRefs(fragment));
  return fragment.forget();
}

NS_IMETHODIMP
nsRange::CompareBoundaryPoints(uint16_t aHow, nsIDOMRange* aOtherRange,
                               int16_t* aCmpRet)
{
  nsRange* otherRange = static_cast<nsRange*>(aOtherRange);
  NS_ENSURE_TRUE(otherRange, NS_ERROR_NULL_POINTER);

  ErrorResult rv;
  *aCmpRet = CompareBoundaryPoints(aHow, *otherRange, rv);
  return rv.ErrorCode();
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
  int32_t ourOffset, otherOffset;

  switch (aHow) {
    case nsIDOMRange::START_TO_START:
      ourNode = mStartParent;
      ourOffset = mStartOffset;
      otherNode = aOtherRange.GetStartParent();
      otherOffset = aOtherRange.StartOffset();
      break;
    case nsIDOMRange::START_TO_END:
      ourNode = mEndParent;
      ourOffset = mEndOffset;
      otherNode = aOtherRange.GetStartParent();
      otherOffset = aOtherRange.StartOffset();
      break;
    case nsIDOMRange::END_TO_START:
      ourNode = mStartParent;
      ourOffset = mStartOffset;
      otherNode = aOtherRange.GetEndParent();
      otherOffset = aOtherRange.EndOffset();
      break;
    case nsIDOMRange::END_TO_END:
      ourNode = mEndParent;
      ourOffset = mEndOffset;
      otherNode = aOtherRange.GetEndParent();
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

  return nsContentUtils::ComparePoints(ourNode, ourOffset,
                                       otherNode, otherOffset);
}

/* static */ nsresult
nsRange::CloneParentsBetween(nsIDOMNode *aAncestor,
                             nsIDOMNode *aNode,
                             nsIDOMNode **aClosestAncestor,
                             nsIDOMNode **aFarthestAncestor)
{
  NS_ENSURE_ARG_POINTER((aAncestor && aNode && aClosestAncestor && aFarthestAncestor));

  *aClosestAncestor  = nullptr;
  *aFarthestAncestor = nullptr;

  if (aAncestor == aNode)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> parent, firstParent, lastParent;

  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));

  while(parent && parent != aAncestor)
  {
    nsCOMPtr<nsIDOMNode> clone, tmpNode;

    res = parent->CloneNode(false, 1, getter_AddRefs(clone));

    if (NS_FAILED(res)) return res;
    if (!clone)         return NS_ERROR_FAILURE;

    if (! firstParent)
      firstParent = lastParent = clone;
    else
    {
      res = clone->AppendChild(lastParent, getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;

      lastParent = clone;
    }

    tmpNode = parent;
    res = tmpNode->GetParentNode(getter_AddRefs(parent));
  }

  *aClosestAncestor  = firstParent;
  NS_IF_ADDREF(*aClosestAncestor);

  *aFarthestAncestor = lastParent;
  NS_IF_ADDREF(*aFarthestAncestor);

  return NS_OK;
}

NS_IMETHODIMP
nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  ErrorResult rv;
  *aReturn = CloneContents(rv).get();
  return rv.ErrorCode();
}

already_AddRefed<dom::DocumentFragment>
nsRange::CloneContents(ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMNode> commonAncestor;
  aRv = GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  MOZ_ASSERT(!aRv.Failed(), "GetCommonAncestorContainer() shouldn't fail!");

  nsCOMPtr<nsIDOMDocument> document =
    do_QueryInterface(mStartParent->OwnerDoc());
  NS_ASSERTION(document, "CloneContents needs a document to continue.");
  if (!document) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Create a new document fragment in the context of this document,
  // which might be null


  nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));

  nsRefPtr<dom::DocumentFragment> clonedFrag =
    new dom::DocumentFragment(doc->NodeInfoManager());

  nsCOMPtr<nsIDOMNode> commonCloneAncestor = clonedFrag.get();
  if (!commonCloneAncestor) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

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
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());
    nsCOMPtr<nsINode> iNode = do_QueryInterface(node);
    bool deepClone = !iNode->IsElement() ||
                       (!(iNode == mEndParent && mEndOffset == 0) &&
                        !(iNode == mStartParent &&
                          mStartOffset ==
                            int32_t(iNode->AsElement()->GetChildCount())));

    // Clone the current subtree!

    nsCOMPtr<nsIDOMNode> clone;
    aRv = node->CloneNode(deepClone, 1, getter_AddRefs(clone));
    if (aRv.Failed()) {
      return nullptr;
    }

    // If it's CharacterData, make sure we only clone what
    // is in the range.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(clone));

    if (charData)
    {
      if (iNode == mEndParent)
      {
        // We only need the data before mEndOffset, so get rid of any
        // data after it.

        uint32_t dataLength = 0;
        aRv = charData->GetLength(&dataLength);
        if (aRv.Failed()) {
          return nullptr;
        }

        if (dataLength > (uint32_t)mEndOffset)
        {
          aRv = charData->DeleteData(mEndOffset, dataLength - mEndOffset);
          if (aRv.Failed()) {
            return nullptr;
          }
        }
      }       

      if (iNode == mStartParent)
      {
        // We don't need any data before mStartOffset, so just
        // delete it!

        if (mStartOffset > 0)
        {
          aRv = charData->DeleteData(0, mStartOffset);
          if (aRv.Failed()) {
            return nullptr;
          }
        }
      }
    }

    // Clone the parent hierarchy between commonAncestor and node.

    nsCOMPtr<nsIDOMNode> closestAncestor, farthestAncestor;

    aRv = CloneParentsBetween(commonAncestor, node,
                              getter_AddRefs(closestAncestor),
                              getter_AddRefs(farthestAncestor));

    if (aRv.Failed()) {
      return nullptr;
    }

    // Hook the parent hierarchy/context of the subtree into the clone tree.

    nsCOMPtr<nsIDOMNode> tmpNode;

    if (farthestAncestor)
    {
      aRv = commonCloneAncestor->AppendChild(farthestAncestor,
                                             getter_AddRefs(tmpNode));

      if (aRv.Failed()) {
        return nullptr;
      }
    }

    // Place the cloned subtree into the cloned doc frag tree!

    if (closestAncestor)
    {
      // Append the subtree under closestAncestor since it is the
      // immediate parent of the subtree.

      aRv = closestAncestor->AppendChild(clone, getter_AddRefs(tmpNode));
    }
    else
    {
      // If we get here, there is no missing parent hierarchy between 
      // commonAncestor and node, so just append clone to commonCloneAncestor.

      aRv = commonCloneAncestor->AppendChild(clone, getter_AddRefs(tmpNode));
    }
    if (aRv.Failed()) {
      return nullptr;
    }

    // Get the next subtree to be processed. The idea here is to setup
    // the parameters for the next iteration of the loop.

    iter.Next();

    if (iter.IsDone())
      break; // We must be done!

    nsCOMPtr<nsIDOMNode> nextNode(iter.GetCurrentNode());
    if (!nextNode) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    // Get node and nextNode's common parent.
    nsContentUtils::GetCommonAncestor(node, nextNode, getter_AddRefs(commonAncestor));

    if (!commonAncestor) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    // Find the equivalent of commonAncestor in the cloned tree!

    while (node && node != commonAncestor)
    {
      tmpNode = node;
      aRv = tmpNode->GetParentNode(getter_AddRefs(node));
      if (aRv.Failed()) {
        return nullptr;
      }

      if (!node) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      tmpNode = clone;
      aRv = tmpNode->GetParentNode(getter_AddRefs(clone));
      if (aRv.Failed()) {
        return nullptr;
      }

      if (!clone) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
    }

    commonCloneAncestor = clone;
  }

  return clonedFrag.forget();
}

already_AddRefed<nsRange>
nsRange::CloneRange() const
{
  nsRefPtr<nsRange> range = new nsRange(mOwner);

  range->SetMaySpanAnonymousSubtrees(mMaySpanAnonymousSubtrees);

  range->DoSetRange(mStartParent, mStartOffset, mEndParent, mEndOffset, mRoot);

  return range.forget();
}

NS_IMETHODIMP
nsRange::CloneRange(nsIDOMRange** aReturn)
{
  *aReturn = CloneRange().get();
  return NS_OK;
}

NS_IMETHODIMP
nsRange::InsertNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!node) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  InsertNode(*node, rv);
  return rv.ErrorCode();
}

void
nsRange::InsertNode(nsINode& aNode, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNode)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  int32_t tStartOffset = StartOffset();

  nsCOMPtr<nsIDOMNode> tStartContainer;
  aRv = this->GetStartContainer(getter_AddRefs(tStartContainer));
  if (aRv.Failed()) {
    return;
  }

  // This is the node we'll be inserting before, and its parent
  nsCOMPtr<nsIDOMNode> referenceNode;
  nsCOMPtr<nsIDOMNode> referenceParentNode = tStartContainer;

  nsCOMPtr<nsIDOMText> startTextNode(do_QueryInterface(tStartContainer));
  nsCOMPtr<nsIDOMNodeList> tChildList;
  if (startTextNode) {
    aRv = tStartContainer->GetParentNode(getter_AddRefs(referenceParentNode));
    if (aRv.Failed()) {
      return;
    }

    if (!referenceParentNode) {
      aRv.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return;
    }

    nsCOMPtr<nsIDOMText> secondPart;
    aRv = startTextNode->SplitText(tStartOffset, getter_AddRefs(secondPart));
    if (aRv.Failed()) {
      return;
    }

    referenceNode = secondPart;
  } else {
    aRv = tStartContainer->GetChildNodes(getter_AddRefs(tChildList));
    if (aRv.Failed()) {
      return;
    }

    // find the insertion point in the DOM and insert the Node
    aRv = tChildList->Item(tStartOffset, getter_AddRefs(referenceNode));
    if (aRv.Failed()) {
      return;
    }
  }

  // We might need to update the end to include the new node (bug 433662).
  // Ideally we'd only do this if needed, but it's tricky to know when it's
  // needed in advance (bug 765799).
  int32_t newOffset;

  if (referenceNode) {
    newOffset = IndexOf(referenceNode);
  } else {
    uint32_t length;
    aRv = tChildList->GetLength(&length);
    if (aRv.Failed()) {
      return;
    }

    newOffset = length;
  }

  if (aNode.NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    newOffset += aNode.GetChildCount();
  } else {
    newOffset++;
  }

  // Now actually insert the node
  nsCOMPtr<nsIDOMNode> tResultNode;
  nsCOMPtr<nsIDOMNode> node = aNode.AsDOMNode();
  if (!node) {
    aRv.Throw(NS_ERROR_DOM_NOT_OBJECT_ERR);
    return;
  }
  aRv = referenceParentNode->InsertBefore(node, referenceNode, getter_AddRefs(tResultNode));
  if (aRv.Failed()) {
    return;
  }

  if (Collapsed()) {
    aRv = SetEnd(referenceParentNode, newOffset);
  }
}

NS_IMETHODIMP
nsRange::SurroundContents(nsIDOMNode* aNewParent)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNewParent);
  if (!node) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  ErrorResult rv;
  SurroundContents(*node, rv);
  return rv.ErrorCode();
}

void
nsRange::SurroundContents(nsINode& aNewParent, ErrorResult& aRv)
{
  if (!nsContentUtils::CanCallerAccess(&aNewParent)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!mRoot) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  // INVALID_STATE_ERROR: Raised if the Range partially selects a non-text
  // node.
  if (mStartParent != mEndParent) {
    bool startIsText = mStartParent->IsNodeOfType(nsINode::eTEXT);
    bool endIsText = mEndParent->IsNodeOfType(nsINode::eTEXT);
    nsINode* startGrandParent = mStartParent->GetParentNode();
    nsINode* endGrandParent = mEndParent->GetParentNode();
    if (!((startIsText && endIsText &&
           startGrandParent &&
           startGrandParent == endGrandParent) ||
          (startIsText &&
           startGrandParent &&
           startGrandParent == mEndParent) ||
          (endIsText &&
           endGrandParent &&
           endGrandParent == mStartParent))) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
  }

  // INVALID_NODE_TYPE_ERROR if aNewParent is something that can't be inserted
  // (Document, DocumentType, DocumentFragment)
  uint16_t nodeType = aNewParent.NodeType();
  if (nodeType == nsIDOMNode::DOCUMENT_NODE ||
      nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE ||
      nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  // Extract the contents within the range.

  nsCOMPtr<nsIDOMDocumentFragment> docFrag;

  aRv = ExtractContents(getter_AddRefs(docFrag));

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

  nsCOMPtr<nsIDOMNode> tmpNode;

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
  aRv = aNewParent.AsDOMNode()->AppendChild(docFrag, getter_AddRefs(tmpNode));
  if (aRv.Failed()) {
    return;
  }

  // Select aNewParent, and its contents.

  SelectNode(aNewParent, aRv);
}

NS_IMETHODIMP
nsRange::ToString(nsAString& aReturn)
{ 
  // clear the string
  aReturn.Truncate();
  
  // If we're unpositioned, return the empty string
  if (!mIsPositioned) {
    return NS_OK;
  }

#ifdef DEBUG_range
      printf("Range dump: -----------------------\n");
#endif /* DEBUG */
    
  // effeciency hack for simple case
  if (mStartParent == mEndParent)
  {
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(mStartParent) );
    
    if (textNode)
    {
#ifdef DEBUG_range
      // If debug, dump it:
      nsCOMPtr<nsIContent> cN (do_QueryInterface(mStartParent));
      if (cN) cN->List(stdout);
      printf("End Range dump: -----------------------\n");
#endif /* DEBUG */

      // grab the text
      if (NS_FAILED(textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn)))
        return NS_ERROR_UNEXPECTED;
      return NS_OK;
    }
  } 
  
  /* complex case: mStartParent != mEndParent, or mStartParent not a text node
     revisit - there are potential optimizations here and also tradeoffs.
  */

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();
  nsresult rv = iter->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);
  
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
    nsCOMPtr<nsIDOMText> textNode(do_QueryInterface(n));
    if (textNode) // if it's a text node, get the text
    {
      if (n == mStartParent) // only include text past start offset
      {
        uint32_t strLength;
        textNode->GetLength(&strLength);
        textNode->SubstringData(mStartOffset,strLength-mStartOffset,tempString);
        aReturn += tempString;
      }
      else if (n == mEndParent)  // only include text before end offset
      {
        textNode->SubstringData(0,mEndOffset,tempString);
        aReturn += tempString;
      }
      else  // grab the whole kit-n-kaboodle
      {
        textNode->GetData(tempString);
        aReturn += tempString;
      }
    }

    iter->Next();
  }

#ifdef DEBUG_range
  printf("End Range dump: -----------------------\n");
#endif /* DEBUG */
  return NS_OK;
}



NS_IMETHODIMP
nsRange::Detach()
{
  // No-op, but still set mIsDetached for telemetry (bug 702948)
  mIsDetached = true;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::CreateContextualFragment(const nsAString& aFragment,
                                  nsIDOMDocumentFragment** aReturn)
{
  if (mIsPositioned) {
    return nsContentUtils::CreateContextualFragment(mStartParent, aFragment,
                                                    false, aReturn);
  }
  return NS_ERROR_FAILURE;
}

already_AddRefed<dom::DocumentFragment>
nsRange::CreateContextualFragment(const nsAString& aFragment, ErrorResult& aRv)
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return nsContentUtils::CreateContextualFragment(mStartParent, aFragment,
                                                  false, aRv);
}

static void ExtractRectFromOffset(nsIFrame* aFrame,
                                  const nsIFrame* aRelativeTo, 
                                  const int32_t aOffset, nsRect* aR, bool aKeepLeft)
{
  nsPoint point;
  aFrame->GetPointFromOffset(aOffset, &point);

  point += aFrame->GetOffsetTo(aRelativeTo);

  //given a point.x, extract left or right portion of rect aR
  //point.x has to be within this rect
  NS_ASSERTION(aR->x <= point.x && point.x <= aR->XMost(),
                   "point.x should not be outside of rect r");

  if (aKeepLeft) {
    aR->width = point.x - aR->x;
  } else {
    aR->width = aR->XMost() - point.x;
    aR->x = point.x;
  }
}

static nsresult GetPartialTextRect(nsLayoutUtils::RectCallback* aCallback,
                                   nsIContent* aContent, int32_t aStartOffset, int32_t aEndOffset)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame && frame->GetType() == nsGkAtoms::textFrame) {
    nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
    nsIFrame* relativeTo = nsLayoutUtils::GetContainingBlockForClientRect(textFrame);
    for (nsTextFrame* f = textFrame; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
      int32_t fstart = f->GetContentOffset(), fend = f->GetContentEnd();
      if (fend <= aStartOffset || fstart >= aEndOffset)
        continue;

      // overlapping with the offset we want
      f->EnsureTextRun(nsTextFrame::eInflated);
      NS_ENSURE_TRUE(f->GetTextRun(nsTextFrame::eInflated), NS_ERROR_OUT_OF_MEMORY);
      bool rtl = f->GetTextRun(nsTextFrame::eInflated)->IsRightToLeft();
      nsRect r(f->GetOffsetTo(relativeTo), f->GetSize());
      if (fstart < aStartOffset) {
        // aStartOffset is within this frame
        ExtractRectFromOffset(f, relativeTo, aStartOffset, &r, rtl);
      }
      if (fend > aEndOffset) {
        // aEndOffset is in the middle of this frame
        ExtractRectFromOffset(f, relativeTo, aEndOffset, &r, !rtl);
      }
      aCallback->AddRect(r);
    }
  }
  return NS_OK;
}

static void CollectClientRects(nsLayoutUtils::RectCallback* aCollector, 
                               nsRange* aRange,
                               nsINode* aStartParent, int32_t aStartOffset,
                               nsINode* aEndParent, int32_t aEndOffset)
{
  // Hold strong pointers across the flush
  nsCOMPtr<nsIDOMNode> startContainer = do_QueryInterface(aStartParent);
  nsCOMPtr<nsIDOMNode> endContainer = do_QueryInterface(aEndParent);

  // Flush out layout so our frames are up to date.
  if (!aStartParent->IsInDoc()) {
    return;
  }

  aStartParent->GetCurrentDoc()->FlushPendingNotifications(Flush_Layout);

  // Recheck whether we're still in the document
  if (!aStartParent->IsInDoc()) {
    return;
  }

  RangeSubtreeIterator iter;

  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) return;

  if (iter.IsDone()) {
    // the range is collapsed, only continue if the cursor is in a text node
    nsCOMPtr<nsIContent> content = do_QueryInterface(aStartParent);
    if (content && content->IsNodeOfType(nsINode::eTEXT)) {
      nsIFrame* frame = content->GetPrimaryFrame();
      if (frame && frame->GetType() == nsGkAtoms::textFrame) {
        nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
        int32_t outOffset;
        nsIFrame* outFrame;
        textFrame->GetChildFrameContainingOffset(aStartOffset, false, 
          &outOffset, &outFrame);
        if (outFrame) {
           nsIFrame* relativeTo = 
             nsLayoutUtils::GetContainingBlockForClientRect(outFrame);
           nsRect r(outFrame->GetOffsetTo(relativeTo), outFrame->GetSize());
           ExtractRectFromOffset(outFrame, relativeTo, aStartOffset, &r, false);
           r.width = 0;
           aCollector->AddRect(r);
        }
      }
    }
    return;
  }

  do {
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());
    iter.Next();
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    if (!content)
      continue;
    if (content->IsNodeOfType(nsINode::eTEXT)) {
       if (node == startContainer) {
         int32_t offset = startContainer == endContainer ? 
           aEndOffset : content->GetText()->GetLength();
         GetPartialTextRect(aCollector, content, aStartOffset, offset);
         continue;
       } else if (node == endContainer) {
         GetPartialTextRect(aCollector, content, 0, aEndOffset);
         continue;
       }
    }

    nsIFrame* frame = content->GetPrimaryFrame();
    if (frame) {
      nsLayoutUtils::GetAllInFlowRects(frame,
        nsLayoutUtils::GetContainingBlockForClientRect(frame), aCollector);
    }
  } while (!iter.IsDone());
}

NS_IMETHODIMP
nsRange::GetBoundingClientRect(nsIDOMClientRect** aResult)
{
  *aResult = GetBoundingClientRect().get();
  return NS_OK;
}

already_AddRefed<nsClientRect>
nsRange::GetBoundingClientRect()
{
  nsRefPtr<nsClientRect> rect = new nsClientRect(ToSupports(this));
  if (!mStartParent) {
    return rect.forget();
  }

  nsLayoutUtils::RectAccumulator accumulator;
  CollectClientRects(&accumulator, this, mStartParent, mStartOffset, 
    mEndParent, mEndOffset);

  nsRect r = accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect : 
    accumulator.mResultRect;
  rect->SetLayoutRect(r);
  return rect.forget();
}

NS_IMETHODIMP
nsRange::GetClientRects(nsIDOMClientRectList** aResult)
{
  *aResult = GetClientRects().get();
  return NS_OK;
}

already_AddRefed<nsClientRectList>
nsRange::GetClientRects()
{
  if (!mStartParent) {
    return nullptr;
  }

  nsRefPtr<nsClientRectList> rectList =
    new nsClientRectList(static_cast<nsIDOMRange*>(this));

  nsLayoutUtils::RectListBuilder builder(rectList);

  CollectClientRects(&builder, this, mStartParent, mStartOffset, 
    mEndParent, mEndOffset);
  return rectList.forget();
}

NS_IMETHODIMP
nsRange::GetUsedFontFaces(nsIDOMFontFaceList** aResult)
{
  *aResult = nullptr;

  NS_ENSURE_TRUE(mStartParent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNode> startContainer = do_QueryInterface(mStartParent);
  nsCOMPtr<nsIDOMNode> endContainer = do_QueryInterface(mEndParent);

  // Flush out layout so our frames are up to date.
  nsIDocument* doc = mStartParent->OwnerDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);
  doc->FlushPendingNotifications(Flush_Frames);

  // Recheck whether we're still in the document
  NS_ENSURE_TRUE(mStartParent->IsInDoc(), NS_ERROR_UNEXPECTED);

  nsRefPtr<nsFontFaceList> fontFaceList = new nsFontFaceList();

  RangeSubtreeIterator iter;
  nsresult rv = iter.Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  while (!iter.IsDone()) {
    // only collect anything if the range is not collapsed
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());
    iter.Next();

    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    if (!content) {
      continue;
    }
    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame) {
      continue;
    }

    if (content->IsNodeOfType(nsINode::eTEXT)) {
       if (node == startContainer) {
         int32_t offset = startContainer == endContainer ? 
           mEndOffset : content->GetText()->GetLength();
         nsLayoutUtils::GetFontFacesForText(frame, mStartOffset, offset,
                                            true, fontFaceList);
         continue;
       }
       if (node == endContainer) {
         nsLayoutUtils::GetFontFacesForText(frame, 0, mEndOffset,
                                            true, fontFaceList);
         continue;
       }
    }

    nsLayoutUtils::GetFontFacesForFrames(frame, fontFaceList);
  }

  fontFaceList.forget(aResult);
  return NS_OK;
}

nsINode*
nsRange::GetRegisteredCommonAncestor()
{
  NS_ASSERTION(IsInSelection(),
               "GetRegisteredCommonAncestor only valid for range in selection");
  nsINode* ancestor = GetNextRangeCommonAncestor(mStartParent);
  while (ancestor) {
    RangeHashTable* ranges =
      static_cast<RangeHashTable*>(ancestor->GetProperty(nsGkAtoms::range));
    if (ranges->GetEntry(this)) {
      break;
    }
    ancestor = GetNextRangeCommonAncestor(ancestor->GetParentNode());
  }
  NS_ASSERTION(ancestor, "can't find common ancestor for selected range");
  return ancestor;
}

/* static */ bool nsRange::AutoInvalidateSelection::mIsNested;

nsRange::AutoInvalidateSelection::~AutoInvalidateSelection()
{
  NS_ASSERTION(mWasInSelection == mRange->IsInSelection(),
               "Range got unselected in AutoInvalidateSelection block");
  if (!mCommonAncestor) {
    return;
  }
  mIsNested = false;
  ::InvalidateAllFrames(mCommonAncestor);
  nsINode* commonAncestor = mRange->GetRegisteredCommonAncestor();
  if (commonAncestor != mCommonAncestor) {
    ::InvalidateAllFrames(commonAncestor);
  }
}
