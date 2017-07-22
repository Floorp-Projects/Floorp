/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMText.h"
#include "nsError.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsGenericDOMDataNode.h"
#include "nsTextFrame.h"
#include "nsFontFaceList.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Likely.h"
#include "nsCSSFrameConstructor.h"
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsComputedDOMStyle.h"

using namespace mozilla;
using namespace mozilla::dom;

JSObject*
nsRange::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return RangeBinding::Wrap(aCx, this, aGivenProto);
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
    uint32_t childCount = aNode->GetChildCount();
    MOZ_ASSERT(childCount <= INT32_MAX,
               "There shouldn't be over INT32_MAX children");
    nodeEnd = static_cast<int32_t>(childCount);
  }
  else {
    nodeStart = parent->IndexOf(aNode);
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
  NS_PRECONDITION(aNode, "bad arg");

  nsINode* n = GetNextRangeCommonAncestor(aNode);
  NS_ASSERTION(n || !aNode->IsSelectionDescendant(),
               "orphan selection descendant");

  // Collect the potential ranges and their selection objects.
  RangeHashTable ancestorSelectionRanges;
  nsTHashtable<nsPtrHashKey<Selection>> ancestorSelections;
  uint32_t maxRangeCount = 0;
  for (; n; n = GetNextRangeCommonAncestor(n->GetParentNode())) {
    nsTHashtable<nsPtrHashKey<nsRange>>* ranges =
      n->GetExistingCommonAncestorRanges();
    if (!ranges) {
      continue;
    }
    for (auto iter = ranges->ConstIter(); !iter.Done(); iter.Next()) {
      nsRange* range = iter.Get()->GetKey();
      if (range->IsInSelection() && !range->Collapsed()) {
        ancestorSelectionRanges.PutEntry(range);
        Selection* selection = range->mSelection;
        ancestorSelections.PutEntry(selection);
        maxRangeCount = std::max(maxRangeCount, selection->RangeCount());
      }
    }
  }

  if (!ancestorSelectionRanges.IsEmpty()) {
    nsTArray<const nsRange*> sortedRanges(maxRangeCount);
    for (auto iter = ancestorSelections.ConstIter(); !iter.Done(); iter.Next()) {
      Selection* selection = iter.Get()->GetKey();
      // Sort the found ranges for |selection| in document order
      // (Selection::GetRangeAt returns its ranges ordered).
      for (uint32_t i = 0, len = selection->RangeCount(); i < len; ++i) {
        nsRange* range = selection->GetRangeAt(i);
        if (ancestorSelectionRanges.Contains(range)) {
          sortedRanges.AppendElement(range);
        }
      }
      MOZ_ASSERT(!sortedRanges.IsEmpty());
      // Binary search the now sorted ranges.
      IsItemInRangeComparator comparator = { aNode, aStartOffset, aEndOffset };
      size_t unused;
      if (mozilla::BinarySearchIf(sortedRanges, 0, sortedRanges.Length(), comparator, &unused)) {
        return true;
      }
      sortedRanges.ClearAndRetainStorage();
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
  DoSetRange(nullptr, 0, nullptr, 0, nullptr);
}

nsRange::nsRange(nsINode* aNode)
  : mRoot(nullptr)
  , mStartOffset(0)
  , mEndOffset(0)
  , mIsPositioned(false)
  , mMaySpanAnonymousSubtrees(false)
  , mIsGenerated(false)
  , mStartOffsetWasIncremented(false)
  , mEndOffsetWasIncremented(false)
  , mCalledByJS(false)
#ifdef DEBUG
  , mAssertNextInsertOrAppendIndex(-1)
  , mAssertNextInsertOrAppendNode(nullptr)
#endif
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
nsRange::CreateRange(nsIDOMNode* aStartContainer, uint32_t aStartOffset,
                     nsIDOMNode* aEndParent, uint32_t aEndOffset,
                     nsRange** aRange)
{
  nsCOMPtr<nsINode> startContainer = do_QueryInterface(aStartContainer);
  nsCOMPtr<nsINode> endContainer = do_QueryInterface(aEndParent);
  return CreateRange(startContainer, aStartOffset,
                     endContainer, aEndOffset, aRange);
}

/* static */
nsresult
nsRange::CreateRange(nsIDOMNode* aStartContainer, uint32_t aStartOffset,
                     nsIDOMNode* aEndParent, uint32_t aEndOffset,
                     nsIDOMRange** aRange)
{
  RefPtr<nsRange> range;
  nsresult rv = nsRange::CreateRange(aStartContainer, aStartOffset, aEndParent,
                                     aEndOffset, getter_AddRefs(range));
  range.forget(aRange);
  return rv;
}

/******************************************************
 * nsISupports
 ******************************************************/

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsRange,
                                                   DoSetRange(nullptr, 0, nullptr, 0, nullptr))

// QueryInterface implementation for nsRange
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsRange)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMRange)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMRange)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsRange)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);
  tmp->Reset();

  // This needs to be unlinked after Reset() is called, as it controls
  // the result of IsInSelection() which is used by tmp->Reset().
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelection);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStartContainer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndContainer)
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
  NS_PRECONDITION(aNode, "bad arg");
  NS_ASSERTION(IsInSelection(), "registering range not in selection");

  MarkDescendants(aNode);

  UniquePtr<nsTHashtable<nsPtrHashKey<nsRange>>>& ranges =
    aNode->GetCommonAncestorRangesPtr();
  if (!ranges) {
    ranges = MakeUnique<nsRange::RangeHashTable>();
  }
  ranges->PutEntry(this);
  aNode->SetCommonAncestorForRangeInSelection();
}

void
nsRange::UnregisterCommonAncestor(nsINode* aNode)
{
  NS_PRECONDITION(aNode, "bad arg");
  NS_ASSERTION(aNode->IsCommonAncestorForRangeInSelection(), "wrong node");
  nsTHashtable<nsPtrHashKey<nsRange>>* ranges =
    aNode->GetExistingCommonAncestorRanges();
  MOZ_ASSERT(ranges);
  NS_ASSERTION(ranges->GetEntry(this), "unknown range");

  if (ranges->Count() == 1) {
    aNode->ClearCommonAncestorForRangeInSelection();
    aNode->GetCommonAncestorRangesPtr().reset();
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
    if (parentNode == mEndContainer && mEndOffset > 0) {
      index = parentNode->IndexOf(aContent);
      NS_WARNING_ASSERTION(index >= 0,
        "Shouldn't be called during removing the node or something");
      if (static_cast<uint32_t>(index + 1) == mEndOffset) {
        newEndNode = mEndContainer;
        newEndOffset = mEndOffset + 1;
        MOZ_ASSERT(IsValidOffset(newEndOffset));
        mEndOffsetWasIncremented = true;
      }
    }
    if (parentNode == mStartContainer && mStartOffset > 0) {
      if (index <= 0) {
        index = parentNode->IndexOf(aContent);
      }
      if (static_cast<uint32_t>(index + 1) == mStartOffset) {
        newStartNode = mStartContainer;
        newStartOffset = mStartOffset + 1;
        MOZ_ASSERT(IsValidOffset(newStartOffset));
        mStartOffsetWasIncremented = true;
      }
    }
#ifdef DEBUG
    if (mStartOffsetWasIncremented || mEndOffsetWasIncremented) {
      mAssertNextInsertOrAppendIndex =
        (mStartOffsetWasIncremented ? newStartOffset : newEndOffset) - 1;
      mAssertNextInsertOrAppendNode = aInfo->mDetails->mNextSibling;
    }
#endif
  }

  // If the changed node contains our start boundary and the change starts
  // before the boundary we'll need to adjust the offset.
  if (aContent == mStartContainer && aInfo->mChangeStart < mStartOffset) {
    if (aInfo->mDetails) {
      // splitText(), aInfo->mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo->mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(mStartOffset <= aInfo->mChangeEnd + 1,
                   "mStartOffset is beyond the end of this node");
      newStartOffset = mStartOffset - aInfo->mChangeStart;
      newStartNode = aInfo->mDetails->mNextSibling;
      if (MOZ_UNLIKELY(aContent == mRoot)) {
        newRoot = IsValidBoundary(newStartNode);
      }

      bool isCommonAncestor =
        IsInSelection() && mStartContainer == mEndContainer;
      if (isCommonAncestor) {
        UnregisterCommonAncestor(mStartContainer);
        RegisterCommonAncestor(newStartNode);
      }
      if (mStartContainer->IsDescendantOfCommonAncestorForRangeInSelection()) {
        newStartNode->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      // If boundary is inside changed text, position it before change
      // else adjust start offset for the change in length.
      newStartNode = mStartContainer;
      newStartOffset = mStartOffset <= aInfo->mChangeEnd ?
        aInfo->mChangeStart :
        mStartOffset + aInfo->mChangeStart - aInfo->mChangeEnd +
          aInfo->mReplaceLength;
    }
  }

  // Do the same thing for the end boundary, except for splitText of a node
  // with no parent then only switch to the new node if the start boundary
  // did so too (otherwise the range would end up with disconnected nodes).
  if (aContent == mEndContainer && aInfo->mChangeStart < mEndOffset) {
    if (aInfo->mDetails && (aContent->GetParentNode() || newStartNode)) {
      // splitText(), aInfo->mDetails->mNextSibling is the new text node
      NS_ASSERTION(aInfo->mDetails->mType ==
                   CharacterDataChangeInfo::Details::eSplit,
                   "only a split can start before the end");
      NS_ASSERTION(mEndOffset <= aInfo->mChangeEnd + 1,
                   "mEndOffset is beyond the end of this node");
      newEndOffset = mEndOffset - aInfo->mChangeStart;
      newEndNode = aInfo->mDetails->mNextSibling;

      bool isCommonAncestor =
        IsInSelection() && mStartContainer == mEndContainer;
      if (isCommonAncestor && !newStartNode) {
        // The split occurs inside the range.
        UnregisterCommonAncestor(mStartContainer);
        RegisterCommonAncestor(mStartContainer->GetParentNode());
        newEndNode->SetDescendantOfCommonAncestorForRangeInSelection();
      } else if (mEndContainer->
                   IsDescendantOfCommonAncestorForRangeInSelection()) {
        newEndNode->SetDescendantOfCommonAncestorForRangeInSelection();
      }
    } else {
      newEndNode = mEndContainer;
      newEndOffset = mEndOffset <= aInfo->mChangeEnd ?
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
    if (removed == mStartContainer) {
      newStartOffset = mStartOffset + aInfo->mChangeStart;
      newStartNode = aContent;
      if (MOZ_UNLIKELY(removed == mRoot)) {
        newRoot = IsValidBoundary(newStartNode);
      }
    }
    if (removed == mEndContainer) {
      newEndOffset = mEndOffset + aInfo->mChangeStart;
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
    if (parentNode == mStartContainer && mStartOffset > 0 &&
        mStartOffset < parentNode->GetChildCount() &&
        removed == parentNode->GetChildAt(mStartOffset)) {
      newStartNode = aContent;
      newStartOffset = aInfo->mChangeStart;
    }
    if (parentNode == mEndContainer && mEndOffset > 0 &&
        mEndOffset < parentNode->GetChildCount() &&
        removed == parentNode->GetChildAt(mEndOffset)) {
      newEndNode = aContent;
      newEndOffset = aInfo->mChangeEnd;
    }
  }

  if (newStartNode || newEndNode) {
    if (!newStartNode) {
      newStartNode = mStartContainer;
      newStartOffset = mStartOffset;
    }
    if (!newEndNode) {
      newEndNode = mEndContainer;
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

  bool rangeChanged = false;
  uint32_t newStartOffset = mStartOffset;
  uint32_t newEndOffset = mEndOffset;
  nsINode* container = NODE_FROM(aContainer, aDocument);

  // Adjust position if a sibling was inserted.
  if (container == mStartContainer &&
      (NS_WARN_IF(aIndexInContainer < 0) ||
       static_cast<uint32_t>(aIndexInContainer) < mStartOffset) &&
      !mStartOffsetWasIncremented) {
    ++newStartOffset;
    MOZ_ASSERT(IsValidOffset(newStartOffset));
    rangeChanged = true;
  }
  if (container == mEndContainer &&
      (NS_WARN_IF(aIndexInContainer < 0) ||
       static_cast<uint32_t>(aIndexInContainer) < mEndOffset) &&
      !mEndOffsetWasIncremented) {
    ++newEndOffset;
    MOZ_ASSERT(IsValidOffset(newEndOffset));
    rangeChanged = true;
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

  if (rangeChanged) {
    DoSetRange(mStartContainer, newStartOffset,
               mEndContainer, newEndOffset, mRoot);
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
  bool didCheckStartParentDescendant = false;
  bool rangeChanged = false;
  uint32_t newStartOffset = mStartOffset;
  uint32_t newEndOffset = mEndOffset;

  // Adjust position if a sibling was removed...
  if (container == mStartContainer) {
    if (aIndexInContainer < static_cast<int32_t>(mStartOffset)) {
      --newStartOffset;
      rangeChanged = true;
    }
  } else { // ...or gravitate if an ancestor was removed.
    didCheckStartParentDescendant = true;
    gravitateStart =
      nsContentUtils::ContentIsDescendantOf(mStartContainer, aChild);
  }

  // Do same thing for end boundry.
  if (container == mEndContainer) {
    if (aIndexInContainer < static_cast<int32_t>(mEndOffset)) {
      --newEndOffset;
      rangeChanged = true;
    }
  } else if (didCheckStartParentDescendant &&
             mStartContainer == mEndContainer) {
    gravitateEnd = gravitateStart;
  } else {
    gravitateEnd = nsContentUtils::ContentIsDescendantOf(mEndContainer, aChild);
  }

  if (gravitateStart || gravitateEnd || rangeChanged) {
    DoSetRange(gravitateStart ? container : mStartContainer.get(),
               gravitateStart ? aIndexInContainer : newStartOffset,
               gravitateEnd ? container : mEndContainer.get(),
               gravitateEnd ? aIndexInContainer : newEndOffset,
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
  nsINode* newRoot = IsValidBoundary(mStartContainer);
  NS_ASSERTION(newRoot, "No valid boundary or root found!");
  if (newRoot != IsValidBoundary(mEndContainer)) {
    // Sometimes ordering involved in cycle collection can lead to our
    // start parent and/or end parent being disconnected from our root
    // without our getting a ContentRemoved notification.
    // See bug 846096 for more details.
    NS_ASSERTION(mEndContainer->IsInNativeAnonymousSubtree(),
                 "This special case should happen only with "
                 "native-anonymous content");
    // When that happens, bail out and set pointers to null; since we're
    // in cycle collection and unreachable it shouldn't matter.
    Reset();
    return;
  }
  // This is safe without holding a strong ref to self as long as the change
  // of mRoot is the last thing in DoSetRange.
  DoSetRange(mStartContainer, mStartOffset, mEndContainer, mEndOffset, newRoot);
}

/******************************************************
 * Utilities for comparing points: API from nsIDOMRange
 ******************************************************/
NS_IMETHODIMP
nsRange::IsPointInRange(nsIDOMNode* aContainer, uint32_t aOffset, bool* aResult)
{
  nsCOMPtr<nsINode> container = do_QueryInterface(aContainer);
  if (!container) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  if (NS_WARN_IF(!IsValidOffset(aOffset))) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  ErrorResult rv;
  *aResult = IsPointInRange(*container, aOffset, rv);
  return rv.StealNSResult();
}

bool
nsRange::IsPointInRange(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv)
{
  uint16_t compareResult = ComparePoint(aContainer, aOffset, aRv);
  // If the node isn't in the range's document, it clearly isn't in the range.
  if (aRv.ErrorCodeIs(NS_ERROR_DOM_WRONG_DOCUMENT_ERR)) {
    aRv.SuppressException();
    return false;
  }

  return compareResult == 0;
}

// returns -1 if point is before range, 0 if point is in range,
// 1 if point is after range.
NS_IMETHODIMP
nsRange::ComparePoint(nsIDOMNode* aContainer, uint32_t aOffset,
                      int16_t* aResult)
{
  nsCOMPtr<nsINode> container = do_QueryInterface(aContainer);
  NS_ENSURE_TRUE(container, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);

  ErrorResult rv;
  *aResult = ComparePoint(*container, aOffset, rv);
  return rv.StealNSResult();
}

int16_t
nsRange::ComparePoint(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv)
{
  // our range is in a good state?
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  if (!nsContentUtils::ContentIsDescendantOf(&aContainer, mRoot)) {
    aRv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
    return 0;
  }

  if (aContainer.NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return 0;
  }

  if (aOffset > aContainer.Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

  int32_t cmp =
    nsContentUtils::ComparePoints(&aContainer,
                                  static_cast<int32_t>(aOffset),
                                  mStartContainer,
                                  static_cast<int32_t>(mStartOffset));
  if (cmp <= 0) {
    return cmp;
  }
  if (nsContentUtils::ComparePoints(mEndContainer,
                                    static_cast<int32_t>(mEndOffset),
                                    &aContainer,
                                    static_cast<int32_t>(aOffset)) == -1) {
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
  return rv.StealNSResult();
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
  bool result =
    nsContentUtils::ComparePoints(mStartContainer,
                                  static_cast<int32_t>(mStartOffset),
                                  parent, nodeIndex + 1,
                                  &disconnected) < 0 &&
    nsContentUtils::ComparePoints(parent, nodeIndex,
                                  mEndContainer,
                                  static_cast<int32_t>(mEndOffset),
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
nsRange::DoSetRange(nsINode* aStartN, uint32_t aStartOffset,
                    nsINode* aEndN, uint32_t aEndOffset,
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
  MOZ_ASSERT(IsValidOffset(aStartOffset));
  MOZ_ASSERT(IsValidOffset(aEndOffset));

  if (mRoot != aRoot) {
    if (mRoot) {
      mRoot->RemoveMutationObserver(this);
    }
    if (aRoot) {
      aRoot->AddMutationObserver(this);
    }
  }
  bool checkCommonAncestor =
    (mStartContainer != aStartN || mEndContainer != aEndN) &&
    IsInSelection() && !aNotInsertedYet;
  nsINode* oldCommonAncestor = checkCommonAncestor ? GetCommonAncestor() : nullptr;
  mStartContainer = aStartN;
  mStartOffset = aStartOffset;
  mEndContainer = aEndN;
  mEndOffset = aEndOffset;
  mIsPositioned = !!mStartContainer;
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
        mSelection = nullptr;
      }
    }
  }

  // This needs to be the last thing this function does, other than notifying
  // selection listeners. See comment in ParentChainChanged.
  mRoot = aRoot;

  // Notify any selection listeners. This has to occur last because otherwise the world
  // could be observed by a selection listener while the range was in an invalid state.
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

static int32_t
IndexOf(nsINode* aChild)
{
  nsINode* parent = aChild->GetParentNode();

  return parent ? parent->IndexOf(aChild) : -1;
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

  mSelection = aSelection;
  nsINode* commonAncestor = GetCommonAncestor();
  NS_ASSERTION(commonAncestor, "unexpected disconnected nodes");
  if (mSelection) {
    RegisterCommonAncestor(commonAncestor);
  } else {
    UnregisterCommonAncestor(commonAncestor);
  }
}

nsINode*
nsRange::GetCommonAncestor() const
{
  return mIsPositioned ?
    nsContentUtils::GetCommonAncestor(mStartContainer, mEndContainer) :
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
nsRange::GetStartContainer(nsIDOMNode** aStartContainer)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mStartContainer, aStartContainer);
}

nsINode*
nsRange::GetStartContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mStartContainer;
}

NS_IMETHODIMP
nsRange::GetStartOffset(uint32_t* aStartOffset)
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
nsRange::GetEndContainer(nsIDOMNode** aEndContainer)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mEndContainer, aEndContainer);
}

nsINode*
nsRange::GetEndContainer(ErrorResult& aRv) const
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }

  return mEndContainer;
}

NS_IMETHODIMP
nsRange::GetEndOffset(uint32_t* aEndOffset)
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

  return nsContentUtils::GetCommonAncestor(mStartContainer, mEndContainer);
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

  return rv.StealNSResult();
}

/* static */
bool
nsRange::IsValidOffset(nsINode* aNode, uint32_t aOffset)
{
  return aNode &&
         IsValidOffset(aOffset) &&
         static_cast<size_t>(aOffset) <= aNode->Length();
}

nsINode*
nsRange::IsValidBoundary(nsINode* aNode)
{
  if (!aNode) {
    return nullptr;
  }

  if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
    if (aNode->NodeInfo()->NameAtom() == nsGkAtoms::documentTypeNodeName) {
      return nullptr;
    }

    nsIContent* content = static_cast<nsIContent*>(aNode);

    if (!mMaySpanAnonymousSubtrees) {
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

  NS_ASSERTION(!root->IsNodeOfType(nsINode::eDOCUMENT),
               "GetUncomposedDoc should have returned a doc");

  // We allow this because of backward compatibility.
  return root;
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
  aRv = SetStart(&aNode, aOffset);
}

NS_IMETHODIMP
nsRange::SetStart(nsIDOMNode* aContainer, uint32_t aOffset)
{
  nsCOMPtr<nsINode> container = do_QueryInterface(aContainer);
  if (!container) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStart(*container, aOffset, rv);
  return rv.StealNSResult();
}

/* virtual */ nsresult
nsRange::SetStart(nsINode* aContainer, uint32_t aOffset)
{
  nsINode* newRoot = IsValidBoundary(aContainer);
  if (!newRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }

  if (!IsValidOffset(aContainer, aOffset)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new start is after end.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(aContainer,
                                    static_cast<int32_t>(aOffset),
                                    mEndContainer,
                                    static_cast<int32_t>(mEndOffset)) == 1) {
    DoSetRange(aContainer, aOffset, aContainer, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(aContainer, aOffset, mEndContainer, mEndOffset, mRoot);

  return NS_OK;
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

NS_IMETHODIMP
nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStartBefore(*sibling, rv);
  return rv.StealNSResult();
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

NS_IMETHODIMP
nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetStartAfter(*sibling, rv);
  return rv.StealNSResult();
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
  aRv = SetEnd(&aNode, aOffset);
}

NS_IMETHODIMP
nsRange::SetEnd(nsIDOMNode* aContainer, uint32_t aOffset)
{
  nsCOMPtr<nsINode> container = do_QueryInterface(aContainer);
  if (!container) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEnd(*container, aOffset, rv);
  return rv.StealNSResult();
}

/* virtual */ nsresult
nsRange::SetEnd(nsINode* aContainer, uint32_t aOffset)
{
  nsINode* newRoot = IsValidBoundary(aContainer);
  if (!newRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }

  if (!IsValidOffset(aContainer, aOffset)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new end is before start.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(mStartContainer,
                                    static_cast<int32_t>(mStartOffset),
                                    aContainer,
                                    static_cast<int32_t>(aOffset)) == 1) {
    DoSetRange(aContainer, aOffset, aContainer, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(mStartContainer, mStartOffset, aContainer, aOffset, mRoot);

  return NS_OK;
}

nsresult
nsRange::SetStartAndEnd(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset)
{
  if (NS_WARN_IF(!aStartContainer) || NS_WARN_IF(!aEndContainer)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsINode* newStartRoot = IsValidBoundary(aStartContainer);
  if (!newStartRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!IsValidOffset(aStartContainer, aStartOffset)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aStartContainer == aEndContainer) {
    if (!IsValidOffset(aEndContainer, aEndOffset)) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }
    // If the end offset is less than the start offset, this should be
    // collapsed at the end offset.
    if (aStartOffset > aEndOffset) {
      DoSetRange(aEndContainer, aEndOffset,
                 aEndContainer, aEndOffset, newStartRoot);
    } else {
      DoSetRange(aStartContainer, aStartOffset,
                 aEndContainer, aEndOffset, newStartRoot);
    }
    return NS_OK;
  }

  nsINode* newEndRoot = IsValidBoundary(aEndContainer);
  if (!newEndRoot) {
    return NS_ERROR_DOM_INVALID_NODE_TYPE_ERR;
  }
  if (!IsValidOffset(aEndContainer, aEndOffset)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // If they have different root, this should be collapsed at the end point.
  if (newStartRoot != newEndRoot) {
    DoSetRange(aEndContainer, aEndOffset,
               aEndContainer, aEndOffset, newEndRoot);
    return NS_OK;
  }

  // If the end point is before the start point, this should be collapsed at
  // the end point.
  if (nsContentUtils::ComparePoints(aStartContainer,
                                    static_cast<int32_t>(aStartOffset),
                                    aEndContainer,
                                    static_cast<int32_t>(aEndOffset)) == 1) {
    DoSetRange(aEndContainer, aEndOffset,
               aEndContainer, aEndOffset, newEndRoot);
    return NS_OK;
  }

  // Otherwise, set the range as specified.
  DoSetRange(aStartContainer, aStartOffset,
             aEndContainer, aEndOffset, newStartRoot);
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

NS_IMETHODIMP
nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEndBefore(*sibling, rv);
  return rv.StealNSResult();
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

NS_IMETHODIMP
nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsINode> sibling = do_QueryInterface(aSibling);
  if (!sibling) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }

  ErrorResult rv;
  SetEndAfter(*sibling, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsRange::Collapse(bool aToStart)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  AutoInvalidateSelection atEndOfBlock(this);
  if (aToStart) {
    DoSetRange(mStartContainer, mStartOffset,
               mStartContainer, mStartOffset, mRoot);
  } else {
    DoSetRange(mEndContainer, mEndOffset,
               mEndContainer, mEndOffset, mRoot);
  }

  return NS_OK;
}

void
nsRange::CollapseJS(bool aToStart)
{
  AutoCalledByJSRestore calledByJSRestorer(*this);
  mCalledByJS = true;
  Unused << Collapse(aToStart);
}

NS_IMETHODIMP
nsRange::SelectNode(nsIDOMNode* aN)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  NS_ENSURE_TRUE(node, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  ErrorResult rv;
  SelectNode(*node, rv);
  return rv.StealNSResult();
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

  int32_t index = container->IndexOf(&aNode);
  if (NS_WARN_IF(index < 0) ||
      !IsValidOffset(static_cast<uint32_t>(index)) ||
      !IsValidOffset(static_cast<uint32_t>(index) + 1)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  AutoInvalidateSelection atEndOfBlock(this);
  DoSetRange(container, index, container, index + 1, newRoot);
}

NS_IMETHODIMP
nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  NS_ENSURE_TRUE(node, NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);

  ErrorResult rv;
  SelectNodeContents(*node, rv);
  return rv.StealNSResult();
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

  nsCOMPtr<nsIDOMCharacterData> startData = do_QueryInterface(node);
  if (startData || (node->IsElement() &&
                    node->AsElement()->GetChildCount() == aRange->GetStartOffset(rv))) {
    mStart = node;
  }

  // Grab the end point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  node = aRange->GetEndContainer(rv);
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMCharacterData> endData = do_QueryInterface(node);
  if (endData || (node->IsElement() && aRange->GetEndOffset(rv) == 0)) {
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

  if (startContainer == commonAncestor)
    return aRange->Collapse(true);
  if (endContainer == commonAncestor)
    return aRange->Collapse(false);

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

  return aRange->Collapse(false);
}

/**
 * Split a data node into two parts.
 *
 * @param aStartContainer     The original node we are trying to split.
 * @param aStartOffset        The offset at which to split.
 * @param aEndContainer       The second node.
 * @param aCloneAfterOriginal Set false if the original node should be the
 *                            latter one after split.
 */
static nsresult SplitDataNode(nsIDOMCharacterData* aStartContainer,
                              uint32_t aStartOffset,
                              nsIDOMCharacterData** aEndContainer,
                              bool aCloneAfterOriginal = true)
{
  nsresult rv;
  nsCOMPtr<nsINode> node = do_QueryInterface(aStartContainer);
  NS_ENSURE_STATE(node && node->IsNodeOfType(nsINode::eDATA_NODE));
  nsGenericDOMDataNode* dataNode = static_cast<nsGenericDOMDataNode*>(node.get());

  nsCOMPtr<nsIContent> newData;
  rv = dataNode->SplitData(aStartOffset, getter_AddRefs(newData),
                           aCloneAfterOriginal);
  NS_ENSURE_SUCCESS(rv, rv);
  return CallQueryInterface(newData, aEndContainer);
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
    nsCOMPtr<nsIDOMCharacterData> charData = do_QueryInterface(node);
    if (charData) {
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

  nsCOMPtr<nsIDocument> doc = mStartContainer->OwnerDoc();

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

  nsCOMPtr<nsINode> startContainer = mStartContainer;
  uint32_t startOffset = mStartOffset;
  nsCOMPtr<nsINode> endContainer = mEndContainer;
  uint32_t endOffset = mEndOffset;

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

  // We delete backwards to avoid iterator problems!

  iter.Last();

  bool handled = false;

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsINode> nodeToResult;
    nsCOMPtr<nsINode> node = iter.GetCurrentNode();

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
              nodeToResult = do_QueryInterface(clone);
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

          if (dataLength >= startOffset) {
            nsMutationGuard guard;
            nsCOMPtr<nsIDOMCharacterData> cutNode;
            rv = SplitDataNode(charData, startOffset, getter_AddRefs(cutNode));
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_STATE(!guard.Mutated(1) ||
                            ValidateCurrentNode(this, iter));
            nodeToResult = do_QueryInterface(cutNode);
          }

          handled = true;
        }
      }
      else if (node == endContainer)
      {
        // Delete or extract everything before endOffset.
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
        nodeToResult = do_QueryInterface(cutNode);
        handled = true;
      }
    }

    if (!handled && (node == endContainer || node == startContainer))
    {
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

    if (!handled)
    {
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
        nsCOMPtr<nsINode> prevNode = iter.GetCurrentNode();
        NS_ENSURE_STATE(prevNode);

        // Get node's and prevNode's common parent. Do this before moving
        // nodes from original DOM to result fragment.
        commonAncestor = nsContentUtils::GetCommonAncestor(node, prevNode);
        NS_ENSURE_STATE(commonAncestor);

        nsCOMPtr<nsINode> parentCounterNode = node;
        while (parentCounterNode && parentCounterNode != commonAncestor)
        {
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

      if (farthestAncestor)
      {
        nsCOMPtr<nsINode> n = do_QueryInterface(commonCloneAncestor);
        rv = PrependChild(n, farthestAncestor);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      nsMutationGuard guard;
      nsCOMPtr<nsINode> parent = nodeToResult->GetParentNode();
      rv = closestAncestor ? PrependChild(closestAncestor, nodeToResult)
                           : PrependChild(commonCloneAncestor, nodeToResult);
      NS_ENSURE_SUCCESS(rv, rv);
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
      for (uint32_t i = parentCount; i; --i)
      {
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
  RefPtr<DocumentFragment> fragment;
  nsresult rv = CutContents(getter_AddRefs(fragment));
  fragment.forget(aReturn);
  return rv;
}

already_AddRefed<DocumentFragment>
nsRange::ExtractContents(ErrorResult& rv)
{
  RefPtr<DocumentFragment> fragment;
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
  return rv.StealNSResult();
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
    case nsIDOMRange::START_TO_START:
      ourNode = mStartContainer;
      ourOffset = mStartOffset;
      otherNode = aOtherRange.GetStartContainer();
      otherOffset = aOtherRange.StartOffset();
      break;
    case nsIDOMRange::START_TO_END:
      ourNode = mEndContainer;
      ourOffset = mEndOffset;
      otherNode = aOtherRange.GetStartContainer();
      otherOffset = aOtherRange.StartOffset();
      break;
    case nsIDOMRange::END_TO_START:
      ourNode = mStartContainer;
      ourOffset = mStartOffset;
      otherNode = aOtherRange.GetEndContainer();
      otherOffset = aOtherRange.EndOffset();
      break;
    case nsIDOMRange::END_TO_END:
      ourNode = mEndContainer;
      ourOffset = mEndOffset;
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

NS_IMETHODIMP
nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  ErrorResult rv;
  *aReturn = CloneContents(rv).take();
  return rv.StealNSResult();
}

already_AddRefed<DocumentFragment>
nsRange::CloneContents(ErrorResult& aRv)
{
  nsCOMPtr<nsINode> commonAncestor = GetCommonAncestorContainer(aRv);
  MOZ_ASSERT(!aRv.Failed(), "GetCommonAncestorContainer() shouldn't fail!");

  nsCOMPtr<nsIDocument> doc = mStartContainer->OwnerDoc();
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
                       (!(node == mEndContainer && mEndOffset == 0) &&
                        !(node == mStartContainer &&
                          mStartOffset == node->AsElement()->GetChildCount()));

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

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(clone));

    if (charData)
    {
      if (node == mEndContainer) {
        // We only need the data before mEndOffset, so get rid of any
        // data after it.

        uint32_t dataLength = 0;
        aRv = charData->GetLength(&dataLength);
        if (aRv.Failed()) {
          return nullptr;
        }

        if (dataLength > mEndOffset) {
          aRv = charData->DeleteData(mEndOffset, dataLength - mEndOffset);
          if (aRv.Failed()) {
            return nullptr;
          }
        }
      }

      if (node == mStartContainer) {
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

  range->DoSetRange(mStartContainer, mStartOffset,
                    mEndContainer, mEndOffset, mRoot);

  return range.forget();
}

NS_IMETHODIMP
nsRange::CloneRange(nsIDOMRange** aReturn)
{
  *aReturn = CloneRange().take();
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
  return rv.StealNSResult();
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

  nsCOMPtr<nsIDOMText> startTextNode(do_QueryInterface(tStartContainer));
  nsCOMPtr<nsIDOMNodeList> tChildList;
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

    nsCOMPtr<nsIDOMText> secondPart;
    aRv = startTextNode->SplitText(tStartOffset, getter_AddRefs(secondPart));
    if (aRv.Failed()) {
      return;
    }

    referenceNode = do_QueryInterface(secondPart);
  } else {
    aRv = tStartContainer->AsDOMNode()->GetChildNodes(getter_AddRefs(tChildList));
    if (aRv.Failed()) {
      return;
    }

    // find the insertion point in the DOM and insert the Node
    nsCOMPtr<nsIDOMNode> q;
    aRv = tChildList->Item(tStartOffset, getter_AddRefs(q));
    referenceNode = do_QueryInterface(q);
    if (aRv.Failed()) {
      return;
    }

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
    aRv = tChildList->GetLength(&newOffset);
    if (aRv.Failed()) {
      return;
    }
  }

  if (aNode.NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
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

NS_IMETHODIMP
nsRange::SurroundContents(nsIDOMNode* aNewParent)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNewParent);
  if (!node) {
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  ErrorResult rv;
  SurroundContents(*node, rv);
  return rv.StealNSResult();
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
  if (mStartContainer != mEndContainer) {
    bool startIsText = mStartContainer->IsNodeOfType(nsINode::eTEXT);
    bool endIsText = mEndContainer->IsNodeOfType(nsINode::eTEXT);
    nsINode* startGrandParent = mStartContainer->GetParentNode();
    nsINode* endGrandParent = mEndContainer->GetParentNode();
    if (!((startIsText && endIsText &&
           startGrandParent &&
           startGrandParent == endGrandParent) ||
          (startIsText &&
           startGrandParent &&
           startGrandParent == mEndContainer) ||
          (endIsText &&
           endGrandParent &&
           endGrandParent == mStartContainer))) {
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
  if (mStartContainer == mEndContainer) {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(mStartContainer);

    if (textNode)
    {
#ifdef DEBUG_range
      // If debug, dump it:
      nsCOMPtr<nsIContent> cN = do_QueryInterface(mStartContainer);
      if (cN) cN->List(stdout);
      printf("End Range dump: -----------------------\n");
#endif /* DEBUG */

      // grab the text
      if (NS_FAILED(textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn)))
        return NS_ERROR_UNEXPECTED;
      return NS_OK;
    }
  }

  /* complex case: mStartContainer != mEndContainer, or mStartParent not a text
     node revisit - there are potential optimizations here and also tradeoffs.
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
      if (n == mStartContainer) { // only include text past start offset
        uint32_t strLength;
        textNode->GetLength(&strLength);
        textNode->SubstringData(mStartOffset,strLength-mStartOffset,tempString);
        aReturn += tempString;
      } else if (n == mEndContainer) { // only include text before end offset
        textNode->SubstringData(0,mEndOffset,tempString);
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
  return NS_OK;
}



NS_IMETHODIMP
nsRange::Detach()
{
  return NS_OK;
}

NS_IMETHODIMP
nsRange::CreateContextualFragment(const nsAString& aFragment,
                                  nsIDOMDocumentFragment** aReturn)
{
  if (mIsPositioned) {
    return nsContentUtils::CreateContextualFragment(mStartContainer, aFragment,
                                                    false, aReturn);
  }
  return NS_ERROR_FAILURE;
}

already_AddRefed<DocumentFragment>
nsRange::CreateContextualFragment(const nsAString& aFragment, ErrorResult& aRv)
{
  if (!mIsPositioned) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return nsContentUtils::CreateContextualFragment(mStartContainer, aFragment,
                                                  false, aRv);
}

static void ExtractRectFromOffset(nsIFrame* aFrame,
                                  const int32_t aOffset, nsRect* aR, bool aKeepLeft,
                                  bool aClampToEdge)
{
  nsPoint point;
  aFrame->GetPointFromOffset(aOffset, &point);

  if (!aClampToEdge && !aR->Contains(point)) {
    aR->width = 0;
    aR->x = point.x;
    return;
  }

  if (aClampToEdge) {
    point = aR->ClampPoint(point);
  }

  if (aKeepLeft) {
    aR->width = point.x - aR->x;
  } else {
    aR->width = aR->XMost() - point.x;
    aR->x = point.x;
  }
}

static nsTextFrame*
GetTextFrameForContent(nsIContent* aContent, bool aFlushLayout)
{
  nsIPresShell* presShell = aContent->OwnerDoc()->GetShell();
  if (presShell) {
    presShell->FrameConstructor()->EnsureFrameForTextNode(
        static_cast<nsGenericDOMDataNode*>(aContent));

    if (aFlushLayout) {
      aContent->OwnerDoc()->FlushPendingNotifications(FlushType::Layout);
    }

    nsIFrame* frame = aContent->GetPrimaryFrame();
    if (frame && frame->IsTextFrame()) {
      return static_cast<nsTextFrame*>(frame);
    }
  }
  return nullptr;
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
  if (!aStartContainer->IsInUncomposedDoc()) {
    return;
  }

  if (aFlushLayout) {
    aStartContainer->OwnerDoc()->FlushPendingNotifications(FlushType::Layout);
    // Recheck whether we're still in the document
    if (!aStartContainer->IsInUncomposedDoc()) {
      return;
    }
  }

  RangeSubtreeIterator iter;

  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) return;

  if (iter.IsDone()) {
    // the range is collapsed, only continue if the cursor is in a text node
    nsCOMPtr<nsIContent> content = do_QueryInterface(aStartContainer);
    if (content && content->IsNodeOfType(nsINode::eTEXT)) {
      nsTextFrame* textFrame = GetTextFrameForContent(content, aFlushLayout);
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
           r.width = 0;
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
    if (content->IsNodeOfType(nsINode::eTEXT)) {
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

NS_IMETHODIMP
nsRange::GetBoundingClientRect(nsIDOMClientRect** aResult)
{
  *aResult = GetBoundingClientRect(true).take();
  return NS_OK;
}

already_AddRefed<DOMRect>
nsRange::GetBoundingClientRect(bool aClampToEdge, bool aFlushLayout)
{
  RefPtr<DOMRect> rect = new DOMRect(ToSupports(this));
  if (!mStartContainer) {
    return rect.forget();
  }

  nsLayoutUtils::RectAccumulator accumulator;
  CollectClientRectsAndText(&accumulator, nullptr, this, mStartContainer,
    mStartOffset, mEndContainer, mEndOffset, aClampToEdge, aFlushLayout);

  nsRect r = accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect :
    accumulator.mResultRect;
  rect->SetLayoutRect(r);
  return rect.forget();
}

NS_IMETHODIMP
nsRange::GetClientRects(nsIDOMClientRectList** aResult)
{
  *aResult = GetClientRects(true).take();
  return NS_OK;
}

already_AddRefed<DOMRectList>
nsRange::GetClientRects(bool aClampToEdge, bool aFlushLayout)
{
  if (!mStartContainer) {
    return nullptr;
  }

  RefPtr<DOMRectList> rectList =
    new DOMRectList(static_cast<nsIDOMRange*>(this));

  nsLayoutUtils::RectListBuilder builder(rectList);

  CollectClientRectsAndText(&builder, nullptr, this, mStartContainer,
    mStartOffset, mEndContainer, mEndOffset, aClampToEdge, aFlushLayout);
  return rectList.forget();
}

void
nsRange::GetClientRectsAndTexts(
  mozilla::dom::ClientRectsAndTexts& aResult,
  ErrorResult& aErr)
{
  if (!mStartContainer) {
    return;
  }

  aResult.mRectList = new DOMRectList(static_cast<nsIDOMRange*>(this));

  nsLayoutUtils::RectListBuilder builder(aResult.mRectList);

  CollectClientRectsAndText(&builder, &aResult.mTextList, this,
    mStartContainer, mStartOffset, mEndContainer, mEndOffset, true, true);
}

NS_IMETHODIMP
nsRange::GetUsedFontFaces(nsIDOMFontFaceList** aResult)
{
  *aResult = nullptr;

  NS_ENSURE_TRUE(mStartContainer, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsINode> startContainer = do_QueryInterface(mStartContainer);
  nsCOMPtr<nsINode> endContainer = do_QueryInterface(mEndContainer);

  // Flush out layout so our frames are up to date.
  nsIDocument* doc = mStartContainer->OwnerDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);
  doc->FlushPendingNotifications(FlushType::Frames);

  // Recheck whether we're still in the document
  NS_ENSURE_TRUE(mStartContainer->IsInUncomposedDoc(), NS_ERROR_UNEXPECTED);

  RefPtr<nsFontFaceList> fontFaceList = new nsFontFaceList();

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
  nsINode* ancestor = GetNextRangeCommonAncestor(mStartContainer);
  while (ancestor) {
    nsTHashtable<nsPtrHashKey<nsRange>>* ranges =
      ancestor->GetExistingCommonAncestorRanges();
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
  if (commonAncestor && commonAncestor != mCommonAncestor) {
    ::InvalidateAllFrames(commonAncestor);
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
  return aContent->IsNodeOfType(nsINode::eTEXT) &&
    aContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE);
}

void
nsRange::ExcludeNonSelectableNodes(nsTArray<RefPtr<nsRange>>* aOutRanges)
{
  MOZ_ASSERT(mIsPositioned);
  MOZ_ASSERT(mEndContainer);
  MOZ_ASSERT(mStartContainer);

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
          nsINode* endContainer = range->mEndContainer;
          int32_t endOffset = range->mEndOffset;

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
              startOffset = parent->IndexOf(startContainer);
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
  RefPtr<nsStyleContext> sc =
    nsComputedDOMStyle::GetStyleContextNoFlush(aElement, nullptr, nullptr);
  return sc && sc->StyleVisibility()->IsVisible();
}

static void
AppendTransformedText(InnerTextAccumulator& aResult,
                      nsGenericDOMDataNode* aTextNode,
                      uint32_t aStart, uint32_t aEnd)
{
  nsIFrame* frame = aTextNode->GetPrimaryFrame();
  if (!IsVisibleAndNotInReplacedElement(frame)) {
    return;
  }
  nsIFrame::RenderedText text = frame->GetRenderedText(aStart, aEnd);
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
                             nsIContent* aStartContainer, uint32_t aStartOffset,
                             nsIContent* aEndContainer, uint32_t aEndOffset)
{
  InnerTextAccumulator result(aValue);
  nsIContent* currentNode = aStartContainer;
  TreeTraversalState currentState = AFTER_NODE;
  if (aStartContainer->IsNodeOfType(nsINode::eTEXT)) {
    auto t = static_cast<nsGenericDOMDataNode*>(aStartContainer);
    if (aStartContainer == aEndContainer) {
      AppendTransformedText(result, t, aStartOffset, aEndOffset);
      return;
    }
    AppendTransformedText(result, t, aStartOffset, t->TextLength());
  } else {
    if (uint32_t(aStartOffset) < aStartContainer->GetChildCount()) {
      currentNode = aStartContainer->GetChildAt(aStartOffset);
      currentState = AT_NODE;
    }
  }

  nsIContent* endNode = aEndContainer;
  TreeTraversalState endState = AFTER_NODE;
  if (aEndContainer->IsNodeOfType(nsINode::eTEXT)) {
    endState = AT_NODE;
  } else {
    if (aEndOffset < aEndContainer->GetChildCount()) {
      endNode = aEndContainer->GetChildAt(aEndOffset);
      endState = AT_NODE;
    }
  }

  while (currentNode != endNode || currentState != endState) {
    nsIFrame* f = currentNode->GetPrimaryFrame();
    bool isVisibleAndNotReplaced = IsVisibleAndNotInReplacedElement(f);
    if (currentState == AT_NODE) {
      bool isText = currentNode->IsNodeOfType(nsINode::eTEXT);
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

  if (aEndContainer->IsNodeOfType(nsINode::eTEXT)) {
    nsGenericDOMDataNode* t = static_cast<nsGenericDOMDataNode*>(aEndContainer);
    AppendTransformedText(result, t, 0, aEndOffset);
  }
  // Do not flush trailing line breaks! Required breaks at the end of the text
  // are suppressed.
}
