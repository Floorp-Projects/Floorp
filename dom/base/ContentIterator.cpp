/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentIterator.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/Result.h"

#include "nsContentUtils.h"
#include "nsElementTable.h"
#include "nsIContent.h"
#include "nsRange.h"

namespace mozilla {

static bool ComparePostMode(const RawRangeBoundary& aStart,
                            const RawRangeBoundary& aEnd, nsINode& aNode) {
  nsINode* parent = aNode.GetParentNode();
  if (!parent) {
    return false;
  }

  // aNode should always be content, as we have a parent, but let's just be
  // extra careful and check.
  nsIContent* content =
      NS_WARN_IF(!aNode.IsContent()) ? nullptr : aNode.AsContent();

  // Post mode: start < node <= end.
  RawRangeBoundary afterNode(parent, content);
  const auto isStartLessThanAfterNode = [&]() {
    const Maybe<int32_t> startComparedToAfterNode =
        nsContentUtils::ComparePoints(aStart, afterNode);
    return !NS_WARN_IF(!startComparedToAfterNode) &&
           (*startComparedToAfterNode < 0);
  };

  const auto isAfterNodeLessOrEqualToEnd = [&]() {
    const Maybe<int32_t> afterNodeComparedToEnd =
        nsContentUtils::ComparePoints(afterNode, aEnd);
    return !NS_WARN_IF(!afterNodeComparedToEnd) &&
           (*afterNodeComparedToEnd <= 0);
  };

  return isStartLessThanAfterNode() && isAfterNodeLessOrEqualToEnd();
}

static bool ComparePreMode(const RawRangeBoundary& aStart,
                           const RawRangeBoundary& aEnd, nsINode& aNode) {
  nsINode* parent = aNode.GetParentNode();
  if (!parent) {
    return false;
  }

  // Pre mode: start <= node < end.
  RawRangeBoundary beforeNode(parent, aNode.GetPreviousSibling());

  const auto isStartLessOrEqualToBeforeNode = [&]() {
    const Maybe<int32_t> startComparedToBeforeNode =
        nsContentUtils::ComparePoints(aStart, beforeNode);
    return !NS_WARN_IF(!startComparedToBeforeNode) &&
           (*startComparedToBeforeNode <= 0);
  };

  const auto isBeforeNodeLessThanEndNode = [&]() {
    const Maybe<int32_t> beforeNodeComparedToEnd =
        nsContentUtils::ComparePoints(beforeNode, aEnd);
    return !NS_WARN_IF(!beforeNodeComparedToEnd) &&
           (*beforeNodeComparedToEnd < 0);
  };

  return isStartLessOrEqualToBeforeNode() && isBeforeNodeLessThanEndNode();
}

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

  if (aIsPreMode) {
    return ComparePreMode(aStart, aEnd, *aNode);
  }

  return ComparePostMode(aStart, aEnd, *aNode);
}

// Each concreate class of ContentIteratorBase may be owned by another class
// which may be owned by JS.  Therefore, all of them should be in the cycle
// collection.  However, we cannot make non-refcountable classes only with the
// macros.  So, we need to make them cycle collectable without the macros.
void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                 ContentIteratorBase& aField, const char* aName,
                                 uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mCurNode, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mFirst, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mLast, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mClosestCommonInclusiveAncestor,
                              aName, aFlags);
}

void ImplCycleCollectionUnlink(ContentIteratorBase& aField) {
  ImplCycleCollectionUnlink(aField.mCurNode);
  ImplCycleCollectionUnlink(aField.mFirst);
  ImplCycleCollectionUnlink(aField.mLast);
  ImplCycleCollectionUnlink(aField.mClosestCommonInclusiveAncestor);
}

ContentIteratorBase::ContentIteratorBase(Order aOrder)
    : mIsDone(false), mOrder(aOrder) {}

ContentIteratorBase::~ContentIteratorBase() = default;

/******************************************************
 * Init routines
 ******************************************************/

nsresult ContentIteratorBase::Init(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot)) {
    return NS_ERROR_NULL_POINTER;
  }

  mIsDone = false;

  if (mOrder == Order::Pre) {
    mFirst = aRoot;
    mLast = ContentIteratorBase::GetDeepLastChild(aRoot);
    NS_WARNING_ASSERTION(mLast, "GetDeepLastChild returned null");
  } else {
    mFirst = ContentIteratorBase::GetDeepFirstChild(aRoot);
    NS_WARNING_ASSERTION(mFirst, "GetDeepFirstChild returned null");
    mLast = aRoot;
  }

  mClosestCommonInclusiveAncestor = aRoot;
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

class MOZ_STACK_CLASS ContentIteratorBase::Initializer final {
 public:
  Initializer(ContentIteratorBase& aIterator, const RawRangeBoundary& aStart,
              const RawRangeBoundary& aEnd)
      : mIterator{aIterator},
        mStart{aStart},
        mEnd{aEnd},
        mStartIsCharacterData{mStart.Container()->IsCharacterData()} {
    MOZ_ASSERT(mStart.IsSetAndValid());
    MOZ_ASSERT(mEnd.IsSetAndValid());
  }

  nsresult Run();

 private:
  /**
   * @return may be nullptr.
   */
  nsINode* DetermineFirstNode() const;

  /**
   * @return may be nullptr.
   */
  [[nodiscard]] Result<nsINode*, nsresult> DetermineLastNode() const;

  bool IsCollapsedNonCharacterRange() const;
  bool IsSingleNodeCharacterRange() const;

  ContentIteratorBase& mIterator;
  const RawRangeBoundary& mStart;
  const RawRangeBoundary& mEnd;
  const bool mStartIsCharacterData;
};

nsresult ContentIteratorBase::InitInternal(const RawRangeBoundary& aStart,
                                           const RawRangeBoundary& aEnd) {
  Initializer initializer{*this, aStart, aEnd};
  return initializer.Run();
}

bool ContentIteratorBase::Initializer::IsCollapsedNonCharacterRange() const {
  return !mStartIsCharacterData && mStart == mEnd;
}

bool ContentIteratorBase::Initializer::IsSingleNodeCharacterRange() const {
  return mStartIsCharacterData && mStart.Container() == mEnd.Container();
}

nsresult ContentIteratorBase::Initializer::Run() {
  // get common content parent
  mIterator.mClosestCommonInclusiveAncestor =
      nsContentUtils::GetClosestCommonInclusiveAncestor(mStart.Container(),
                                                        mEnd.Container());
  if (NS_WARN_IF(!mIterator.mClosestCommonInclusiveAncestor)) {
    return NS_ERROR_FAILURE;
  }

  // Check to see if we have a collapsed range, if so, there is nothing to
  // iterate over.
  //
  // XXX: CharacterDataNodes (text nodes) are currently an exception, since
  //      we always want to be able to iterate text nodes at the end points
  //      of a range.

  if (IsCollapsedNonCharacterRange()) {
    mIterator.SetEmpty();
    return NS_OK;
  }

  if (IsSingleNodeCharacterRange()) {
    mIterator.mFirst = mStart.Container()->AsContent();
    mIterator.mLast = mIterator.mFirst;
    mIterator.mCurNode = mIterator.mFirst;

    return NS_OK;
  }

  mIterator.mFirst = DetermineFirstNode();

  if (Result<nsINode*, nsresult> lastNode = DetermineLastNode();
      NS_WARN_IF(lastNode.isErr())) {
    return lastNode.unwrapErr();
  } else {
    mIterator.mLast = lastNode.unwrap();
  }

  // If either first or last is null, they both have to be null!
  if (!mIterator.mFirst || !mIterator.mLast) {
    mIterator.mFirst = nullptr;
    mIterator.mLast = nullptr;
  }

  mIterator.mCurNode = mIterator.mFirst;
  mIterator.mIsDone = !mIterator.mCurNode;

  return NS_OK;
}

nsINode* ContentIteratorBase::Initializer::DetermineFirstNode() const {
  nsIContent* cChild = nullptr;

  // Try to get the child at our starting point. This might return null if
  // mStart is immediately after the last node in mStart.Container().
  if (!mStartIsCharacterData) {
    cChild = mStart.GetChildAtOffset();
  }

  if (!cChild) {
    // No children (possibly a <br> or text node), or index is after last child.

    if (mIterator.mOrder == Order::Pre) {
      // XXX: In the future, if start offset is after the last
      //      character in the cdata node, should we set mFirst to
      //      the next sibling?

      // Normally we would skip the start node because the start node is outside
      // of the range in pre mode. However, if aStartOffset == 0, and the node
      // is a non-container node (e.g. <br>), we don't skip the node in this
      // case in order to address bug 1215798.
      bool startIsContainer = true;
      if (mStart.Container()->IsHTMLElement()) {
        nsAtom* name = mStart.Container()->NodeInfo()->NameAtom();
        startIsContainer =
            nsHTMLElement::IsContainer(nsHTMLTags::AtomTagToId(name));
      }
      if (!mStartIsCharacterData &&
          (startIsContainer || !mStart.IsStartOfContainer())) {
        nsINode* const result =
            ContentIteratorBase::GetNextSibling(mStart.Container());
        NS_WARNING_ASSERTION(result, "GetNextSibling returned null");

        // Does mFirst node really intersect the range?  The range could be
        // 'degenerate', i.e., not collapsed but still contain no content.
        if (result &&
            NS_WARN_IF(!NodeIsInTraversalRange(
                result, mIterator.mOrder == Order::Pre, mStart, mEnd))) {
          return nullptr;
        }

        return result;
      }
      return mStart.Container()->AsContent();
    }

    // post-order
    if (NS_WARN_IF(!mStart.Container()->IsContent())) {
      // What else can we do?
      return nullptr;
    }
    return mStart.Container()->AsContent();
  }

  if (mIterator.mOrder == Order::Pre) {
    return cChild;
  }

  // post-order
  nsINode* const result = ContentIteratorBase::GetDeepFirstChild(cChild);
  NS_WARNING_ASSERTION(result, "GetDeepFirstChild returned null");

  // Does mFirst node really intersect the range?  The range could be
  // 'degenerate', i.e., not collapsed but still contain no content.
  if (result && !NodeIsInTraversalRange(result, mIterator.mOrder == Order::Pre,
                                        mStart, mEnd)) {
    return nullptr;
  }

  return result;
}

Result<nsINode*, nsresult> ContentIteratorBase::Initializer::DetermineLastNode()
    const {
  const bool endIsCharacterData = mEnd.Container()->IsCharacterData();

  if (endIsCharacterData || !mEnd.Container()->HasChildren() ||
      mEnd.IsStartOfContainer()) {
    if (mIterator.mOrder == Order::Pre) {
      if (NS_WARN_IF(!mEnd.Container()->IsContent())) {
        // Not much else to do here...
        return nullptr;
      }

      // If the end node is a non-container element and the end offset is 0,
      // the last element should be the previous node (i.e., shouldn't
      // include the end node in the range).
      bool endIsContainer = true;
      if (mEnd.Container()->IsHTMLElement()) {
        nsAtom* name = mEnd.Container()->NodeInfo()->NameAtom();
        endIsContainer =
            nsHTMLElement::IsContainer(nsHTMLTags::AtomTagToId(name));
      }
      if (!endIsCharacterData && !endIsContainer && mEnd.IsStartOfContainer()) {
        nsINode* const result = mIterator.PrevNode(mEnd.Container());
        NS_WARNING_ASSERTION(result, "PrevNode returned null");
        if (result && result != mIterator.mFirst &&
            NS_WARN_IF(!NodeIsInTraversalRange(
                result, mIterator.mOrder == Order::Pre,
                RawRangeBoundary(mIterator.mFirst, 0u), mEnd))) {
          return nullptr;
        }

        return result;
      }

      return mEnd.Container()->AsContent();
    }

    // post-order
    //
    // XXX: In the future, if end offset is before the first character in the
    //      cdata node, should we set mLast to the prev sibling?

    if (!endIsCharacterData) {
      nsINode* const result =
          ContentIteratorBase::GetPrevSibling(mEnd.Container());
      NS_WARNING_ASSERTION(result, "GetPrevSibling returned null");

      if (!NodeIsInTraversalRange(result, mIterator.mOrder == Order::Pre,
                                  mStart, mEnd)) {
        return nullptr;
      }
      return result;
    }
    return mEnd.Container()->AsContent();
  }

  nsIContent* cChild = mEnd.Ref();

  if (NS_WARN_IF(!cChild)) {
    // No child at offset!
    MOZ_ASSERT_UNREACHABLE("ContentIterator::ContentIterator");
    return Err(NS_ERROR_FAILURE);
  }

  if (mIterator.mOrder == Order::Pre) {
    nsINode* const result = ContentIteratorBase::GetDeepLastChild(cChild);
    NS_WARNING_ASSERTION(result, "GetDeepLastChild returned null");

    if (NS_WARN_IF(!NodeIsInTraversalRange(
            result, mIterator.mOrder == Order::Pre, mStart, mEnd))) {
      return nullptr;
    }

    return result;
  }

  // post-order
  return cChild;
}

void ContentIteratorBase::SetEmpty() {
  mCurNode = nullptr;
  mFirst = nullptr;
  mLast = nullptr;
  mClosestCommonInclusiveAncestor = nullptr;
  mIsDone = true;
}

// static
nsINode* ContentIteratorBase::GetDeepFirstChild(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot) || !aRoot->HasChildren()) {
    return aRoot;
  }

  return ContentIteratorBase::GetDeepFirstChild(aRoot->GetFirstChild());
}

// static
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

// static
nsINode* ContentIteratorBase::GetDeepLastChild(nsINode* aRoot) {
  if (NS_WARN_IF(!aRoot) || !aRoot->HasChildren()) {
    return aRoot;
  }

  return ContentIteratorBase::GetDeepLastChild(aRoot->GetLastChild());
}

// static
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
// static
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

  return ContentIteratorBase::GetNextSibling(parent);
}

// Get the prev sibling, or parent's prev sibling, or grandpa's prev sibling...
// static
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

  return ContentIteratorBase::GetPrevSibling(parent);
}

nsINode* ContentIteratorBase::NextNode(nsINode* aNode) {
  nsINode* node = aNode;

  // if we are a Pre-order iterator, use pre-order
  if (mOrder == Order::Pre) {
    // if it has children then next node is first child
    if (node->HasChildren()) {
      nsIContent* firstChild = node->GetFirstChild();
      MOZ_ASSERT(firstChild);

      return firstChild;
    }

    // else next sibling is next
    return ContentIteratorBase::GetNextSibling(node);
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
    return ContentIteratorBase::GetDeepFirstChild(sibling);
  }

  return parent;
}

nsINode* ContentIteratorBase::PrevNode(nsINode* aNode) {
  nsINode* node = aNode;

  // if we are a Pre-order iterator, use pre-order
  if (mOrder == Order::Pre) {
    nsINode* parent = node->GetParentNode();
    if (NS_WARN_IF(!parent)) {
      MOZ_ASSERT(parent, "The node is the root node but not the first node");
      mIsDone = true;
      return aNode;
    }

    nsIContent* sibling = node->GetPreviousSibling();
    if (sibling) {
      return ContentIteratorBase::GetDeepLastChild(sibling);
    }

    return parent;
  }

  // post-order
  if (node->HasChildren()) {
    return node->GetLastChild();
  }

  // else prev sibling is previous
  return ContentIteratorBase::GetPrevSibling(node);
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
  // Note that mLast can be nullptr if SetEmpty() is called in Init()
  // since at that time, Init() returns NS_OK.
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
    if (mOrder == Order::Pre) {
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
       NS_WARN_IF(!NodeIsInTraversalRange(mCurNode, mOrder == Order::Pre, first,
                                          last)))) {
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

void ContentSubtreeIterator::CacheInclusiveAncestorsOfEndContainer() {
  mInclusiveAncestorsOfEndContainer.Clear();
  nsINode* const endContainer = mRange->GetEndContainer();
  nsIContent* endNode =
      endContainer->IsContent() ? endContainer->AsContent() : nullptr;
  while (endNode) {
    mInclusiveAncestorsOfEndContainer.AppendElement(endNode);
    endNode = endNode->GetParent();
  }
}

nsIContent* ContentSubtreeIterator::DetermineCandidateForFirstContent() const {
  nsINode* startContainer = mRange->GetStartContainer();
  nsIContent* firstCandidate = nullptr;
  // find first node in range
  nsINode* node = nullptr;
  if (!startContainer->GetChildCount()) {
    // no children, start at the node itself
    node = startContainer;
  } else {
    nsIContent* child = mRange->GetChildAtStartOffset();
    MOZ_ASSERT(child ==
               startContainer->GetChildAt_Deprecated(mRange->StartOffset()));
    if (!child) {
      // offset after last child
      node = startContainer;
    } else {
      firstCandidate = child;
    }
  }

  if (!firstCandidate) {
    // then firstCandidate is next node after node
    firstCandidate = ContentIteratorBase::GetNextSibling(node);
  }

  if (firstCandidate) {
    firstCandidate = ContentIteratorBase::GetDeepFirstChild(firstCandidate);
  }

  return firstCandidate;
}

nsIContent* ContentSubtreeIterator::DetermineFirstContent() const {
  nsIContent* firstCandidate = DetermineCandidateForFirstContent();
  if (!firstCandidate) {
    return nullptr;
  }

  // confirm that this first possible contained node is indeed contained.  Else
  // we have a range that does not fully contain any node.
  const Maybe<bool> isNodeContainedInRange =
      RangeUtils::IsNodeContainedInRange(*firstCandidate, mRange);
  MOZ_ALWAYS_TRUE(isNodeContainedInRange);
  if (!isNodeContainedInRange.value()) {
    return nullptr;
  }

  // cool, we have the first node in the range.  Now we walk up its ancestors
  // to find the most senior that is still in the range.  That's the real first
  // node.
  return GetTopAncestorInRange(firstCandidate);
}

nsIContent* ContentSubtreeIterator::DetermineCandidateForLastContent() const {
  nsIContent* lastCandidate{nullptr};
  nsINode* endContainer = mRange->GetEndContainer();
  // now to find the last node
  int32_t offset = mRange->EndOffset();
  int32_t numChildren = endContainer->GetChildCount();

  nsINode* node = nullptr;
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
    lastCandidate = ContentIteratorBase::GetPrevSibling(node);
  }

  if (lastCandidate) {
    lastCandidate = ContentIteratorBase::GetDeepLastChild(lastCandidate);
  }

  return lastCandidate;
}

nsresult ContentSubtreeIterator::InitWithRange() {
  MOZ_ASSERT(mRange);
  MOZ_ASSERT(mRange->IsPositioned());

  // get the start node and offset, convert to nsINode
  mClosestCommonInclusiveAncestor = mRange->GetClosestCommonInclusiveAncestor();
  nsINode* startContainer = mRange->GetStartContainer();
  const int32_t startOffset = mRange->StartOffset();
  nsINode* endContainer = mRange->GetEndContainer();
  const int32_t endOffset = mRange->EndOffset();
  MOZ_ASSERT(mClosestCommonInclusiveAncestor && startContainer && endContainer);
  // Bug 767169
  MOZ_ASSERT(uint32_t(startOffset) <= startContainer->Length() &&
             uint32_t(endOffset) <= endContainer->Length());

  // short circuit when start node == end node
  if (startContainer == endContainer) {
    nsINode* child = startContainer->GetFirstChild();

    if (!child || startOffset == endOffset) {
      // Text node, empty container, or collapsed
      SetEmpty();
      return NS_OK;
    }
  }

  CacheInclusiveAncestorsOfEndContainer();

  mFirst = DetermineFirstContent();
  if (!mFirst) {
    SetEmpty();
    return NS_OK;
  }

  mLast = DetermineLastContent();
  if (!mLast) {
    SetEmpty();
    return NS_OK;
  }

  mCurNode = mFirst;

  return NS_OK;
}

nsIContent* ContentSubtreeIterator::DetermineLastContent() const {
  nsIContent* lastCandidate = DetermineCandidateForLastContent();
  if (!lastCandidate) {
    return nullptr;
  }

  // confirm that this last possible contained node is indeed contained.  Else
  // we have a range that does not fully contain any node.

  const Maybe<bool> isNodeContainedInRange =
      RangeUtils::IsNodeContainedInRange(*lastCandidate, mRange);
  MOZ_ALWAYS_TRUE(isNodeContainedInRange);
  if (!isNodeContainedInRange.value()) {
    return nullptr;
  }

  // cool, we have the last node in the range.  Now we walk up its ancestors to
  // find the most senior that is still in the range.  That's the real first
  // node.
  return GetTopAncestorInRange(lastCandidate);
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

  nsINode* nextNode = ContentIteratorBase::GetNextSibling(mCurNode);
  NS_ASSERTION(nextNode, "No next sibling!?! This could mean deadlock!");

  int32_t i = mInclusiveAncestorsOfEndContainer.IndexOf(nextNode);
  while (i != -1) {
    // as long as we are finding ancestors of the endpoint of the range,
    // dive down into their children
    nextNode = nextNode->GetFirstChild();
    NS_ASSERTION(nextNode, "Iterator error, expected a child node!");

    // should be impossible to get a null pointer.  If we went all the way
    // down the child chain to the bottom without finding an interior node,
    // then the previous node should have been the last, which was
    // was tested at top of routine.
    i = mInclusiveAncestorsOfEndContainer.IndexOf(nextNode);
  }

  mCurNode = nextNode;

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mLast is in generated content, we need this
  // to stop the iterator when we've walked past past the last node!
  mIsDone = mCurNode == nullptr;
}

void ContentSubtreeIterator::Prev() {
  // Prev should be optimized to use the mStartNodes, just as Next
  // uses mInclusiveAncestorsOfEndContainer.
  if (mIsDone || !mCurNode) {
    return;
  }

  if (mCurNode == mFirst) {
    mIsDone = true;
    return;
  }

  // If any of these function calls return null, so will all succeeding ones,
  // so mCurNode will wind up set to null.
  nsINode* prevNode = ContentIteratorBase::GetDeepFirstChild(mCurNode);

  prevNode = PrevNode(prevNode);

  prevNode = ContentIteratorBase::GetDeepLastChild(prevNode);

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

nsIContent* ContentSubtreeIterator::GetTopAncestorInRange(
    nsINode* aNode) const {
  if (!aNode || !aNode->GetParentNode()) {
    return nullptr;
  }

  // aNode has a parent, so it must be content.
  nsIContent* content = aNode->AsContent();

  // sanity check: aNode is itself in the range
  Maybe<bool> isNodeContainedInRange =
      RangeUtils::IsNodeContainedInRange(*aNode, mRange);
  NS_ASSERTION(isNodeContainedInRange && isNodeContainedInRange.value(),
               "aNode isn't in mRange, or something else weird happened");
  if (!isNodeContainedInRange || !isNodeContainedInRange.value()) {
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

    isNodeContainedInRange =
        RangeUtils::IsNodeContainedInRange(*parent, mRange);
    MOZ_ALWAYS_TRUE(isNodeContainedInRange);
    if (!isNodeContainedInRange.value()) {
      return content;
    }

    content = parent;
  }

  MOZ_CRASH("This should only be possible if aNode was null");
}

}  // namespace mozilla
