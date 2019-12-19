/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentIterator.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RangeUtils.h"

#include "nsContentUtils.h"
#include "nsElementTable.h"
#include "nsIContent.h"
#include "nsRange.h"

namespace mozilla {

///////////////////////////////////////////////////////////////////////////
// NodeIsInTraversalRange: returns true if content is visited during
// the traversal of the range in the specified mode.
//
static bool NodeIsInTraversalRange(nsINode* aNode, bool aIsPreMode,
                                   const RawRangeBoundary& aStart,
                                   const RawRangeBoundary& aEnd) {
  if (NS_WARN_IF(!aStart.IsSet()) || NS_WARN_IF(!aEnd.IsSet()) ||
      NS_WARN_IF(!aNode)) {
    return false;
  }

  // If a leaf node contains an end point of the traversal range, it is
  // always in the traversal range.
  if (aNode == aStart.Container() || aNode == aEnd.Container()) {
    if (aNode->IsCharacterData()) {
      return true;  // text node or something
    }
    if (!aNode->HasChildren()) {
      MOZ_ASSERT(
          aNode != aStart.Container() || aStart.IsStartOfContainer(),
          "aStart.Container() doesn't have children and not a data node, "
          "aStart should be at the beginning of its container");
      MOZ_ASSERT(aNode != aEnd.Container() || aEnd.IsStartOfContainer(),
                 "aEnd.Container() doesn't have children and not a data node, "
                 "aEnd should be at the beginning of its container");
      return true;
    }
  }

  nsINode* parent = aNode->GetParentNode();
  if (!parent) {
    return false;
  }

  if (!aIsPreMode) {
    // aNode should always be content, as we have a parent, but let's just be
    // extra careful and check.
    nsIContent* content =
        NS_WARN_IF(!aNode->IsContent()) ? nullptr : aNode->AsContent();
    // Post mode: start < node <= end.
    RawRangeBoundary afterNode(parent, content);
    return nsContentUtils::ComparePoints_Deprecated(aStart, afterNode) < 0 &&
           nsContentUtils::ComparePoints_Deprecated(aEnd, afterNode) >= 0;
  }

  // Pre mode: start <= node < end.
  RawRangeBoundary beforeNode(parent, aNode->GetPreviousSibling());
  return nsContentUtils::ComparePoints_Deprecated(aStart, beforeNode) <= 0 &&
         nsContentUtils::ComparePoints_Deprecated(aEnd, beforeNode) > 0;
}

ContentIteratorBase::ContentIteratorBase(bool aPre)
    : mIsDone(false), mPre(aPre) {}

/******************************************************
 * Init routines
 ******************************************************/

nsresult ContentIteratorBase::Init(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot)) {
    return NS_ERROR_NULL_POINTER;
  }

  mIsDone = false;

  if (mPre) {
    mFirst = aRoot;
    mLast = GetDeepLastChild(aRoot);
    NS_WARNING_ASSERTION(mLast, "GetDeepLastChild returned null");
  } else {
    mFirst = GetDeepFirstChild(aRoot);
    NS_WARNING_ASSERTION(mFirst, "GetDeepFirstChild returned null");
    mLast = aRoot;
  }

  mCommonParent = aRoot;
  mCurNode = mFirst;
  return NS_OK;
}

nsresult ContentIteratorBase::Init(nsRange* aRange) {
  mIsDone = false;

  if (NS_WARN_IF(!aRange)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  return InitInternal(aRange->StartRef().AsRaw(), aRange->EndRef().AsRaw());
}

nsresult ContentIteratorBase::Init(nsINode* aStartContainer,
                                   uint32_t aStartOffset,
                                   nsINode* aEndContainer,
                                   uint32_t aEndOffset) {
  mIsDone = false;

  if (NS_WARN_IF(!RangeUtils::IsValidPoints(aStartContainer, aStartOffset,
                                            aEndContainer, aEndOffset))) {
    return NS_ERROR_INVALID_ARG;
  }

  return InitInternal(RawRangeBoundary(aStartContainer, aStartOffset),
                      RawRangeBoundary(aEndContainer, aEndOffset));
}

nsresult ContentIteratorBase::Init(const RawRangeBoundary& aStart,
                                   const RawRangeBoundary& aEnd) {
  mIsDone = false;

  if (NS_WARN_IF(!RangeUtils::IsValidPoints(aStart, aEnd))) {
    return NS_ERROR_INVALID_ARG;
  }

  return InitInternal(aStart, aEnd);
}

nsresult ContentIteratorBase::InitInternal(const RawRangeBoundary& aStart,
                                           const RawRangeBoundary& aEnd) {
  // get common content parent
  mCommonParent =
      nsContentUtils::GetCommonAncestor(aStart.Container(), aEnd.Container());
  if (NS_WARN_IF(!mCommonParent)) {
    return NS_ERROR_FAILURE;
  }

  bool startIsData = aStart.Container()->IsCharacterData();

  // Check to see if we have a collapsed range, if so, there is nothing to
  // iterate over.
  //
  // XXX: CharacterDataNodes (text nodes) are currently an exception, since
  //      we always want to be able to iterate text nodes at the end points
  //      of a range.

  if (!startIsData && aStart == aEnd) {
    MakeEmpty();
    return NS_OK;
  }

  // Handle ranges within a single character data node.
  if (startIsData && aStart.Container() == aEnd.Container()) {
    mFirst = aStart.Container()->AsContent();
    mLast = mFirst;
    mCurNode = mFirst;

    return NS_OK;
  }

  // Find first node in range.

  nsIContent* cChild = nullptr;

  // Try to get the child at our starting point. This might return null if
  // aStart is immediately after the last node in aStart.Container().
  if (!startIsData) {
    cChild = aStart.GetChildAtOffset();
  }

  if (!cChild) {
    // No children (possibly a <br> or text node), or index is after last child.

    if (mPre) {
      // XXX: In the future, if start offset is after the last
      //      character in the cdata node, should we set mFirst to
      //      the next sibling?

      // Normally we would skip the start node because the start node is outside
      // of the range in pre mode. However, if aStartOffset == 0, and the node
      // is a non-container node (e.g. <br>), we don't skip the node in this
      // case in order to address bug 1215798.
      bool startIsContainer = true;
      if (aStart.Container()->IsHTMLElement()) {
        nsAtom* name = aStart.Container()->NodeInfo()->NameAtom();
        startIsContainer =
            nsHTMLElement::IsContainer(nsHTMLTags::AtomTagToId(name));
      }
      if (!startIsData && (startIsContainer || !aStart.IsStartOfContainer())) {
        mFirst = GetNextSibling(aStart.Container());
        NS_WARNING_ASSERTION(mFirst, "GetNextSibling returned null");

        // Does mFirst node really intersect the range?  The range could be
        // 'degenerate', i.e., not collapsed but still contain no content.
        if (mFirst &&
            NS_WARN_IF(!NodeIsInTraversalRange(mFirst, mPre, aStart, aEnd))) {
          mFirst = nullptr;
        }
      } else {
        mFirst = aStart.Container()->AsContent();
      }
    } else {
      // post-order
      if (NS_WARN_IF(!aStart.Container()->IsContent())) {
        // What else can we do?
        mFirst = nullptr;
      } else {
        mFirst = aStart.Container()->AsContent();
      }
    }
  } else {
    if (mPre) {
      mFirst = cChild;
    } else {
      // post-order
      mFirst = GetDeepFirstChild(cChild);
      NS_WARNING_ASSERTION(mFirst, "GetDeepFirstChild returned null");

      // Does mFirst node really intersect the range?  The range could be
      // 'degenerate', i.e., not collapsed but still contain no content.

      if (mFirst && !NodeIsInTraversalRange(mFirst, mPre, aStart, aEnd)) {
        mFirst = nullptr;
      }
    }
  }

  // Find last node in range.

  bool endIsData = aEnd.Container()->IsCharacterData();

  if (endIsData || !aEnd.Container()->HasChildren() ||
      aEnd.IsStartOfContainer()) {
    if (mPre) {
      if (NS_WARN_IF(!aEnd.Container()->IsContent())) {
        // Not much else to do here...
        mLast = nullptr;
      } else {
        // If the end node is a non-container element and the end offset is 0,
        // the last element should be the previous node (i.e., shouldn't
        // include the end node in the range).
        bool endIsContainer = true;
        if (aEnd.Container()->IsHTMLElement()) {
          nsAtom* name = aEnd.Container()->NodeInfo()->NameAtom();
          endIsContainer =
              nsHTMLElement::IsContainer(nsHTMLTags::AtomTagToId(name));
        }
        if (!endIsData && !endIsContainer && aEnd.IsStartOfContainer()) {
          mLast = PrevNode(aEnd.Container());
          NS_WARNING_ASSERTION(mLast, "PrevNode returned null");
          if (mLast && mLast != mFirst &&
              NS_WARN_IF(!NodeIsInTraversalRange(
                  mLast, mPre, RawRangeBoundary(mFirst, 0u), aEnd))) {
            mLast = nullptr;
          }
        } else {
          mLast = aEnd.Container()->AsContent();
        }
      }
    } else {
      // post-order
      //
      // XXX: In the future, if end offset is before the first character in the
      //      cdata node, should we set mLast to the prev sibling?

      if (!endIsData) {
        mLast = GetPrevSibling(aEnd.Container());
        NS_WARNING_ASSERTION(mLast, "GetPrevSibling returned null");

        if (!NodeIsInTraversalRange(mLast, mPre, aStart, aEnd)) {
          mLast = nullptr;
        }
      } else {
        mLast = aEnd.Container()->AsContent();
      }
    }
  } else {
    cChild = aEnd.Ref();

    if (NS_WARN_IF(!cChild)) {
      // No child at offset!
      MOZ_ASSERT_UNREACHABLE("ContentIterator::ContentIterator");
      return NS_ERROR_FAILURE;
    }

    if (mPre) {
      mLast = GetDeepLastChild(cChild);
      NS_WARNING_ASSERTION(mLast, "GetDeepLastChild returned null");

      if (NS_WARN_IF(!NodeIsInTraversalRange(mLast, mPre, aStart, aEnd))) {
        mLast = nullptr;
      }
    } else {
      // post-order
      mLast = cChild;
    }
  }

  // If either first or last is null, they both have to be null!

  if (!mFirst || !mLast) {
    mFirst = nullptr;
    mLast = nullptr;
  }

  mCurNode = mFirst;
  mIsDone = !mCurNode;

  return NS_OK;
}

void ContentIteratorBase::MakeEmpty() {
  mCurNode = nullptr;
  mFirst = nullptr;
  mLast = nullptr;
  mCommonParent = nullptr;
  mIsDone = true;
}

nsINode* ContentIteratorBase::GetDeepFirstChild(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot) || !aRoot->HasChildren()) {
    return aRoot;
  }

  return GetDeepFirstChild(aRoot->GetFirstChild());
}

nsIContent* ContentIteratorBase::GetDeepFirstChild(nsIContent* aRoot) {
  if (NS_WARN_IF(!aRoot)) {
    return nullptr;
  }

  nsIContent* node = aRoot;
  nsIContent* child = node->GetFirstChild();

  while (child) {
    node = child;
    child = node->GetFirstChild();
  }

  return node;
}

nsINode* ContentIteratorBase::GetDeepLastChild(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot) || !aRoot->HasChildren()) {
    return aRoot;
  }

  return GetDeepLastChild(aRoot->GetLastChild());
}

nsIContent* ContentIteratorBase::GetDeepLastChild(nsIContent* aRoot) {
  if (NS_WARN_IF(!aRoot)) {
    return nullptr;
  }

  nsIContent* node = aRoot;
  while (node->HasChildren()) {
    nsIContent* child = node->GetLastChild();
    node = child;
  }
  return node;
}

// Get the next sibling, or parent's next sibling, or grandpa's next sibling...
nsIContent* ContentIteratorBase::GetNextSibling(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }

  if (aNode->GetNextSibling()) {
    return aNode->GetNextSibling();
  }

  nsINode* parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return nullptr;
  }

  // XXX This is a hack to preserve previous behaviour: This should be fixed
  // in bug 1404916. If we were positioned on anonymous content, move to
  // the first child of our parent.
  if (parent->GetLastChild() && parent->GetLastChild() != aNode) {
    return parent->GetFirstChild();
  }

  return GetNextSibling(parent);
}

// Get the prev sibling, or parent's prev sibling, or grandpa's prev sibling...
nsIContent* ContentIteratorBase::GetPrevSibling(nsINode* aNode) {
  if (NS_WARN_IF(!aNode)) {
    return nullptr;
  }

  if (aNode->GetPreviousSibling()) {
    return aNode->GetPreviousSibling();
  }

  nsINode* parent = aNode->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return nullptr;
  }

  // XXX This is a hack to preserve previous behaviour: This should be fixed
  // in bug 1404916. If we were positioned on anonymous content, move to
  // the last child of our parent.
  if (parent->GetFirstChild() && parent->GetFirstChild() != aNode) {
    return parent->GetLastChild();
  }

  return GetPrevSibling(parent);
}

nsINode* ContentIteratorBase::NextNode(nsINode* aNode) {
  nsINode* node = aNode;

  // if we are a Pre-order iterator, use pre-order
  if (mPre) {
    // if it has children then next node is first child
    if (node->HasChildren()) {
      nsIContent* firstChild = node->GetFirstChild();
      MOZ_ASSERT(firstChild);

      return firstChild;
    }

    // else next sibling is next
    return GetNextSibling(node);
  }

  // post-order
  nsINode* parent = node->GetParentNode();
  if (NS_WARN_IF(!parent)) {
    MOZ_ASSERT(parent, "The node is the root node but not the last node");
    mIsDone = true;
    return node;
  }

  nsIContent* sibling = node->GetNextSibling();
  if (sibling) {
    // next node is sibling's "deep left" child
    return GetDeepFirstChild(sibling);
  }

  return parent;
}

nsINode* ContentIteratorBase::PrevNode(nsINode* aNode) {
  nsINode* node = aNode;

  // if we are a Pre-order iterator, use pre-order
  if (mPre) {
    nsINode* parent = node->GetParentNode();
    if (NS_WARN_IF(!parent)) {
      MOZ_ASSERT(parent, "The node is the root node but not the first node");
      mIsDone = true;
      return aNode;
    }

    nsIContent* sibling = node->GetPreviousSibling();
    if (sibling) {
      return GetDeepLastChild(sibling);
    }

    return parent;
  }

  // post-order
  if (node->HasChildren()) {
    return node->GetLastChild();
  }

  // else prev sibling is previous
  return GetPrevSibling(node);
}

/******************************************************
 * ContentIteratorBase routines
 ******************************************************/

void ContentIteratorBase::First() {
  if (mFirst) {
    mozilla::DebugOnly<nsresult> rv = PositionAt(mFirst);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");
  }

  mIsDone = mFirst == nullptr;
}

void ContentIteratorBase::Last() {
  // Note that mLast can be nullptr if MakeEmpty() is called in Init() since
  // at that time, Init() returns NS_OK.
  if (!mLast) {
    MOZ_ASSERT(mIsDone);
    return;
  }

  mozilla::DebugOnly<nsresult> rv = PositionAt(mLast);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");

  mIsDone = mLast == nullptr;
}

void ContentIteratorBase::Next() {
  if (mIsDone || NS_WARN_IF(!mCurNode)) {
    return;
  }

  if (mCurNode == mLast) {
    mIsDone = true;
    return;
  }

  mCurNode = NextNode(mCurNode);
}

void ContentIteratorBase::Prev() {
  if (NS_WARN_IF(mIsDone) || NS_WARN_IF(!mCurNode)) {
    return;
  }

  if (mCurNode == mFirst) {
    mIsDone = true;
    return;
  }

  mCurNode = PrevNode(mCurNode);
}

bool ContentIteratorBase::IsDone() { return mIsDone; }

// Keeping arrays of indexes for the stack of nodes makes PositionAt
// interesting...
nsresult ContentIteratorBase::PositionAt(nsINode* aCurNode) {
  if (NS_WARN_IF(!aCurNode)) {
    return NS_ERROR_NULL_POINTER;
  }

  // take an early out if this doesn't actually change the position
  if (mCurNode == aCurNode) {
    mIsDone = false;
    return NS_OK;
  }
  mCurNode = aCurNode;

  // Check to see if the node falls within the traversal range.

  RawRangeBoundary first(mFirst, 0u);
  RawRangeBoundary last(mLast, 0u);

  if (mFirst && mLast) {
    if (mPre) {
      // In pre we want to record the point immediately before mFirst, which is
      // the point immediately after mFirst's previous sibling.
      first = {mFirst->GetParentNode(), mFirst->GetPreviousSibling()};

      // If mLast has no children, then we want to make sure to include it.
      if (!mLast->HasChildren()) {
        last = {mLast->GetParentNode(), mLast->AsContent()};
      }
    } else {
      // If the first node has any children, we want to be immediately after the
      // last. Otherwise we want to be immediately before mFirst.
      if (mFirst->HasChildren()) {
        first = {mFirst, mFirst->GetLastChild()};
      } else {
        first = {mFirst->GetParentNode(), mFirst->GetPreviousSibling()};
      }

      // Set the last point immediately after the final node.
      last = {mLast->GetParentNode(), mLast->AsContent()};
    }
  }

  NS_WARNING_ASSERTION(first.IsSetAndValid(), "first is not valid");
  NS_WARNING_ASSERTION(last.IsSetAndValid(), "last is not valid");

  // The end positions are always in the range even if it has no parent.  We
  // need to allow that or 'iter->Init(root)' would assert in Last() or First()
  // for example, bug 327694.
  if (mFirst != mCurNode && mLast != mCurNode &&
      (NS_WARN_IF(!first.IsSet()) || NS_WARN_IF(!last.IsSet()) ||
       NS_WARN_IF(!NodeIsInTraversalRange(mCurNode, mPre, first, last)))) {
    mIsDone = true;
    return NS_ERROR_FAILURE;
  }

  mIsDone = false;
  return NS_OK;
}

nsINode* ContentIteratorBase::GetCurrentNode() {
  if (mIsDone) {
    return nullptr;
  }

  NS_ASSERTION(mCurNode, "Null current node in an iterator that's not done!");

  return mCurNode;
}

/******************************************************
 * ContentSubtreeIterator init routines
 ******************************************************/

nsresult ContentSubtreeIterator::Init(nsINode* aRoot) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult ContentSubtreeIterator::Init(nsRange* aRange) {
  MOZ_ASSERT(aRange);

  mIsDone = false;

  if (NS_WARN_IF(!aRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  mRange = aRange;

  return InitWithRange();
}

nsresult ContentSubtreeIterator::Init(nsINode* aStartContainer,
                                      uint32_t aStartOffset,
                                      nsINode* aEndContainer,
                                      uint32_t aEndOffset) {
  return Init(RawRangeBoundary(aStartContainer, aStartOffset),
              RawRangeBoundary(aEndContainer, aEndOffset));
}

nsresult ContentSubtreeIterator::Init(const RawRangeBoundary& aStartBoundary,
                                      const RawRangeBoundary& aEndBoundary) {
  mIsDone = false;

  RefPtr<nsRange> range =
      nsRange::Create(aStartBoundary, aEndBoundary, IgnoreErrors());
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(range->StartRef() != aStartBoundary) ||
      NS_WARN_IF(range->EndRef() != aEndBoundary)) {
    return NS_ERROR_UNEXPECTED;
  }

  mRange = std::move(range);

  return InitWithRange();
}

nsresult ContentSubtreeIterator::InitWithRange() {
  MOZ_ASSERT(mRange);
  MOZ_ASSERT(mRange->IsPositioned());

  // get the start node and offset, convert to nsINode
  mCommonParent = mRange->GetCommonAncestor();
  nsINode* startContainer = mRange->GetStartContainer();
  int32_t startOffset = mRange->StartOffset();
  nsINode* endContainer = mRange->GetEndContainer();
  int32_t endOffset = mRange->EndOffset();
  MOZ_ASSERT(mCommonParent && startContainer && endContainer);
  // Bug 767169
  MOZ_ASSERT(uint32_t(startOffset) <= startContainer->Length() &&
             uint32_t(endOffset) <= endContainer->Length());

  // short circuit when start node == end node
  if (startContainer == endContainer) {
    nsINode* child = startContainer->GetFirstChild();

    if (!child || startOffset == endOffset) {
      // Text node, empty container, or collapsed
      MakeEmpty();
      return NS_OK;
    }
  }

  // cache ancestors
  mEndNodes.Clear();
  nsIContent* endNode =
      endContainer->IsContent() ? endContainer->AsContent() : nullptr;
  while (endNode) {
    mEndNodes.AppendElement(endNode);
    endNode = endNode->GetParent();
  }

  nsIContent* firstCandidate = nullptr;
  nsIContent* lastCandidate = nullptr;

  // find first node in range
  int32_t offset = mRange->StartOffset();

  nsINode* node = nullptr;
  if (!startContainer->GetChildCount()) {
    // no children, start at the node itself
    node = startContainer;
  } else {
    nsIContent* child = mRange->GetChildAtStartOffset();
    MOZ_ASSERT(child == startContainer->GetChildAt_Deprecated(offset));
    if (!child) {
      // offset after last child
      node = startContainer;
    } else {
      firstCandidate = child;
    }
  }

  if (!firstCandidate) {
    // then firstCandidate is next node after node
    firstCandidate = GetNextSibling(node);

    if (!firstCandidate) {
      MakeEmpty();
      return NS_OK;
    }
  }

  firstCandidate = GetDeepFirstChild(firstCandidate);

  // confirm that this first possible contained node is indeed contained.  Else
  // we have a range that does not fully contain any node.

  bool nodeBefore, nodeAfter;
  MOZ_ALWAYS_SUCCEEDS(RangeUtils::CompareNodeToRange(firstCandidate, mRange,
                                                     &nodeBefore, &nodeAfter));

  if (nodeBefore || nodeAfter) {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the first node in the range.  Now we walk up its ancestors
  // to find the most senior that is still in the range.  That's the real first
  // node.
  mFirst = GetTopAncestorInRange(firstCandidate);

  // now to find the last node
  offset = mRange->EndOffset();
  int32_t numChildren = endContainer->GetChildCount();

  if (offset > numChildren) {
    // Can happen for text nodes
    offset = numChildren;
  }
  if (!offset || !numChildren) {
    node = endContainer;
  } else {
    lastCandidate = mRange->EndRef().Ref();
    MOZ_ASSERT(lastCandidate == endContainer->GetChildAt_Deprecated(--offset));
    NS_ASSERTION(lastCandidate,
                 "tree traversal trouble in ContentSubtreeIterator::Init");
  }

  if (!lastCandidate) {
    // then lastCandidate is prev node before node
    lastCandidate = GetPrevSibling(node);
  }

  if (!lastCandidate) {
    MakeEmpty();
    return NS_OK;
  }

  lastCandidate = GetDeepLastChild(lastCandidate);

  // confirm that this last possible contained node is indeed contained.  Else
  // we have a range that does not fully contain any node.

  MOZ_ALWAYS_SUCCEEDS(RangeUtils::CompareNodeToRange(lastCandidate, mRange,
                                                     &nodeBefore, &nodeAfter));

  if (nodeBefore || nodeAfter) {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the last node in the range.  Now we walk up its ancestors to
  // find the most senior that is still in the range.  That's the real first
  // node.
  mLast = GetTopAncestorInRange(lastCandidate);

  mCurNode = mFirst;

  return NS_OK;
}

/****************************************************************
 * ContentSubtreeIterator overrides of ContentIterator routines
 ****************************************************************/

// we can't call PositionAt in a subtree iterator...
void ContentSubtreeIterator::First() {
  mIsDone = mFirst == nullptr;

  mCurNode = mFirst;
}

// we can't call PositionAt in a subtree iterator...
void ContentSubtreeIterator::Last() {
  mIsDone = mLast == nullptr;

  mCurNode = mLast;
}

void ContentSubtreeIterator::Next() {
  if (mIsDone || !mCurNode) {
    return;
  }

  if (mCurNode == mLast) {
    mIsDone = true;
    return;
  }

  nsINode* nextNode = GetNextSibling(mCurNode);
  NS_ASSERTION(nextNode, "No next sibling!?! This could mean deadlock!");

  int32_t i = mEndNodes.IndexOf(nextNode);
  while (i != -1) {
    // as long as we are finding ancestors of the endpoint of the range,
    // dive down into their children
    nextNode = nextNode->GetFirstChild();
    NS_ASSERTION(nextNode, "Iterator error, expected a child node!");

    // should be impossible to get a null pointer.  If we went all the way
    // down the child chain to the bottom without finding an interior node,
    // then the previous node should have been the last, which was
    // was tested at top of routine.
    i = mEndNodes.IndexOf(nextNode);
  }

  mCurNode = nextNode;

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mLast is in generated content, we need this
  // to stop the iterator when we've walked past past the last node!
  mIsDone = mCurNode == nullptr;
}

void ContentSubtreeIterator::Prev() {
  // Prev should be optimized to use the mStartNodes, just as Next
  // uses mEndNodes.
  if (mIsDone || !mCurNode) {
    return;
  }

  if (mCurNode == mFirst) {
    mIsDone = true;
    return;
  }

  // If any of these function calls return null, so will all succeeding ones,
  // so mCurNode will wind up set to null.
  nsINode* prevNode = GetDeepFirstChild(mCurNode);

  prevNode = PrevNode(prevNode);

  prevNode = GetDeepLastChild(prevNode);

  mCurNode = GetTopAncestorInRange(prevNode);

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mFirst is in generated content, we need this
  // to stop the iterator when we've walked past past the first node!
  mIsDone = mCurNode == nullptr;
}

nsresult ContentSubtreeIterator::PositionAt(nsINode* aCurNode) {
  NS_ERROR("Not implemented!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

/****************************************************************
 * ContentSubtreeIterator helper routines
 ****************************************************************/

nsIContent* ContentSubtreeIterator::GetTopAncestorInRange(nsINode* aNode) {
  if (!aNode || !aNode->GetParentNode()) {
    return nullptr;
  }

  // aNode has a parent, so it must be content.
  nsIContent* content = aNode->AsContent();

  // sanity check: aNode is itself in the range
  bool nodeBefore, nodeAfter;
  nsresult res =
      RangeUtils::CompareNodeToRange(aNode, mRange, &nodeBefore, &nodeAfter);
  NS_ASSERTION(NS_SUCCEEDED(res) && !nodeBefore && !nodeAfter,
               "aNode isn't in mRange, or something else weird happened");
  if (NS_FAILED(res) || nodeBefore || nodeAfter) {
    return nullptr;
  }

  while (content) {
    nsIContent* parent = content->GetParent();
    // content always has a parent.  If its parent is the root, however --
    // i.e., either it's not content, or it is content but its own parent is
    // null -- then we're finished, since we don't go up to the root.
    //
    // We have to special-case this because CompareNodeToRange treats the root
    // node differently -- see bug 765205.
    if (!parent || !parent->GetParentNode()) {
      return content;
    }
    MOZ_ALWAYS_SUCCEEDS(RangeUtils::CompareNodeToRange(
        parent, mRange, &nodeBefore, &nodeAfter));

    if (nodeBefore || nodeAfter) {
      return content;
    }
    content = parent;
  }

  MOZ_CRASH("This should only be possible if aNode was null");
}

}  // namespace mozilla
