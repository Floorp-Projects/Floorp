/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteredContentIterator.h"

#include <utility>

#include "mozilla/ContentIterator.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/mozalloc.h"
#include "nsAtom.h"
#include "nsComponentManagerUtils.h"
#include "nsComposeTxtSrvFilter.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsRange.h"

namespace mozilla {

using namespace dom;

FilteredContentIterator::FilteredContentIterator(
    UniquePtr<nsComposeTxtSrvFilter> aFilter)
    : mCurrentIterator(nullptr),
      mFilter(std::move(aFilter)),
      mDidSkip(false),
      mIsOutOfRange(false),
      mDirection(eDirNotSet) {}

FilteredContentIterator::~FilteredContentIterator() {}

NS_IMPL_CYCLE_COLLECTION(FilteredContentIterator, mPostIterator, mPreIterator,
                         mRange)

nsresult FilteredContentIterator::Init(nsINode* aRoot) {
  NS_ENSURE_ARG_POINTER(aRoot);
  mIsOutOfRange = false;
  mDirection = eForward;
  mCurrentIterator = &mPreIterator;

  mRange = nsRange::Create(aRoot);
  mRange->SelectNode(*aRoot, IgnoreErrors());

  nsresult rv = mPreIterator.Init(mRange);
  NS_ENSURE_SUCCESS(rv, rv);
  return mPostIterator.Init(mRange);
}

nsresult FilteredContentIterator::Init(const AbstractRange* aAbstractRange) {
  if (NS_WARN_IF(!aAbstractRange)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aAbstractRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  mRange = nsRange::Create(aAbstractRange, IgnoreErrors());
  if (NS_WARN_IF(!mRange)) {
    return NS_ERROR_FAILURE;
  }
  return InitWithRange();
}

nsresult FilteredContentIterator::Init(nsINode* aStartContainer,
                                       uint32_t aStartOffset,
                                       nsINode* aEndContainer,
                                       uint32_t aEndOffset) {
  return Init(RawRangeBoundary(aStartContainer, aStartOffset),
              RawRangeBoundary(aEndContainer, aEndOffset));
}

nsresult FilteredContentIterator::Init(const RawRangeBoundary& aStartBoundary,
                                       const RawRangeBoundary& aEndBoundary) {
  RefPtr<nsRange> range =
      nsRange::Create(aStartBoundary, aEndBoundary, IgnoreErrors());
  if (NS_WARN_IF(!range) || NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(range->StartRef() == aStartBoundary);
  MOZ_ASSERT(range->EndRef() == aEndBoundary);

  mRange = std::move(range);

  return InitWithRange();
}

nsresult FilteredContentIterator::InitWithRange() {
  MOZ_ASSERT(mRange);
  MOZ_ASSERT(mRange->IsPositioned());

  mIsOutOfRange = false;
  mDirection = eForward;
  mCurrentIterator = &mPreIterator;

  nsresult rv = mPreIterator.Init(mRange);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mPostIterator.Init(mRange);
}

nsresult FilteredContentIterator::SwitchDirections(bool aChangeToForward) {
  nsINode* node = mCurrentIterator->GetCurrentNode();

  if (aChangeToForward) {
    mCurrentIterator = &mPreIterator;
    mDirection = eForward;
  } else {
    mCurrentIterator = &mPostIterator;
    mDirection = eBackward;
  }

  if (node) {
    nsresult rv = mCurrentIterator->PositionAt(node);
    if (NS_FAILED(rv)) {
      mIsOutOfRange = true;
      return rv;
    }
  }
  return NS_OK;
}

void FilteredContentIterator::First() {
  if (!mCurrentIterator) {
    NS_ERROR("Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eForward) {
    mCurrentIterator = &mPreIterator;
    mDirection = eForward;
    mIsOutOfRange = false;
  }

  mCurrentIterator->First();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  nsINode* currentNode = mCurrentIterator->GetCurrentNode();

  bool didCross;
  CheckAdvNode(currentNode, didCross, eForward);
}

void FilteredContentIterator::Last() {
  if (!mCurrentIterator) {
    NS_ERROR("Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eBackward) {
    mCurrentIterator = &mPostIterator;
    mDirection = eBackward;
    mIsOutOfRange = false;
  }

  mCurrentIterator->Last();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  nsINode* currentNode = mCurrentIterator->GetCurrentNode();

  bool didCross;
  CheckAdvNode(currentNode, didCross, eBackward);
}

///////////////////////////////////////////////////////////////////////////
// ContentIsInTraversalRange: returns true if content is visited during
// the traversal of the range in the specified mode.
//
static bool ContentIsInTraversalRange(nsIContent* aContent, bool aIsPreMode,
                                      nsINode* aStartContainer,
                                      int32_t aStartOffset,
                                      nsINode* aEndContainer,
                                      int32_t aEndOffset) {
  NS_ENSURE_TRUE(aStartContainer && aEndContainer && aContent, false);

  nsIContent* parentContent = aContent->GetParent();
  if (MOZ_UNLIKELY(NS_WARN_IF(!parentContent))) {
    return false;
  }
  Maybe<uint32_t> offsetInParent = parentContent->ComputeIndexOf(aContent);
  NS_WARNING_ASSERTION(
      offsetInParent.isSome(),
      "Content is not in the parent, is this called during a DOM mutation?");
  if (MOZ_UNLIKELY(NS_WARN_IF(offsetInParent.isNothing()))) {
    return false;
  }

  if (!aIsPreMode) {
    MOZ_ASSERT(*offsetInParent != UINT32_MAX);
    ++(*offsetInParent);
  }

  const Maybe<int32_t> startRes = nsContentUtils::ComparePoints(
      aStartContainer, aStartOffset, parentContent, *offsetInParent);
  if (MOZ_UNLIKELY(NS_WARN_IF(!startRes))) {
    return false;
  }
  const Maybe<int32_t> endRes = nsContentUtils::ComparePoints(
      aEndContainer, aEndOffset, parentContent, *offsetInParent);
  if (MOZ_UNLIKELY(NS_WARN_IF(!endRes))) {
    return false;
  }
  return *startRes <= 0 && *endRes >= 0;
}

static bool ContentIsInTraversalRange(nsRange* aRange, nsIContent* aNextContent,
                                      bool aIsPreMode) {
  // XXXbz we have a caller below (in AdvanceNode) who passes null for
  // aNextContent!
  NS_ENSURE_TRUE(aNextContent && aRange, false);

  return ContentIsInTraversalRange(
      aNextContent, aIsPreMode, aRange->GetStartContainer(),
      static_cast<int32_t>(aRange->StartOffset()), aRange->GetEndContainer(),
      static_cast<int32_t>(aRange->EndOffset()));
}

// Helper function to advance to the next or previous node
nsresult FilteredContentIterator::AdvanceNode(nsINode* aNode,
                                              nsINode*& aNewNode,
                                              eDirectionType aDir) {
  nsCOMPtr<nsIContent> nextNode;
  if (aDir == eForward) {
    nextNode = aNode->GetNextSibling();
  } else {
    nextNode = aNode->GetPreviousSibling();
  }

  if (nextNode) {
    // If we got here, that means we found the nxt/prv node
    // make sure it is in our DOMRange
    bool intersects =
        ContentIsInTraversalRange(mRange, nextNode, aDir == eForward);
    if (intersects) {
      aNewNode = nextNode;
      NS_ADDREF(aNewNode);
      return NS_OK;
    }
  } else {
    // The next node was null so we need to walk up the parent(s)
    nsCOMPtr<nsINode> parent = aNode->GetParentNode();
    NS_ASSERTION(parent, "parent can't be nullptr");

    // Make sure the parent is in the DOMRange before going further
    // XXXbz why are we passing nextNode, not the parent???  If this gets fixed,
    // then ContentIsInTraversalRange can stop null-checking its second arg.
    bool intersects =
        ContentIsInTraversalRange(mRange, nextNode, aDir == eForward);
    if (intersects) {
      // Now find the nxt/prv node after/before this node
      nsresult rv = AdvanceNode(parent, aNewNode, aDir);
      if (NS_SUCCEEDED(rv) && aNewNode) {
        return NS_OK;
      }
    }
  }

  // if we get here it pretty much means
  // we went out of the DOM Range
  mIsOutOfRange = true;

  return NS_ERROR_FAILURE;
}

// Helper function to see if the next/prev node should be skipped
void FilteredContentIterator::CheckAdvNode(nsINode* aNode, bool& aDidSkip,
                                           eDirectionType aDir) {
  aDidSkip = false;
  mIsOutOfRange = false;

  if (aNode && mFilter) {
    nsCOMPtr<nsINode> currentNode = aNode;
    while (1) {
      if (mFilter->Skip(aNode)) {
        aDidSkip = true;
        // Get the next/prev node and then
        // see if we should skip that
        nsCOMPtr<nsINode> advNode;
        nsresult rv = AdvanceNode(aNode, *getter_AddRefs(advNode), aDir);
        if (NS_SUCCEEDED(rv) && advNode) {
          aNode = advNode;
        } else {
          return;  // fell out of range
        }
      } else {
        if (aNode != currentNode) {
          nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
          mCurrentIterator->PositionAt(content);
        }
        return;  // found something
      }
    }
  }
}

void FilteredContentIterator::Next() {
  if (mIsOutOfRange || !mCurrentIterator) {
    NS_ASSERTION(mCurrentIterator, "Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eForward) {
    nsresult rv = SwitchDirections(true);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  mCurrentIterator->Next();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  // If we can't get the current node then
  // don't check to see if we can skip it
  nsINode* currentNode = mCurrentIterator->GetCurrentNode();

  CheckAdvNode(currentNode, mDidSkip, eForward);
}

void FilteredContentIterator::Prev() {
  if (mIsOutOfRange || !mCurrentIterator) {
    NS_ASSERTION(mCurrentIterator, "Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eBackward) {
    nsresult rv = SwitchDirections(false);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  mCurrentIterator->Prev();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  // If we can't get the current node then
  // don't check to see if we can skip it
  nsINode* currentNode = mCurrentIterator->GetCurrentNode();

  CheckAdvNode(currentNode, mDidSkip, eBackward);
}

nsINode* FilteredContentIterator::GetCurrentNode() {
  if (mIsOutOfRange || !mCurrentIterator) {
    return nullptr;
  }

  return mCurrentIterator->GetCurrentNode();
}

bool FilteredContentIterator::IsDone() {
  if (mIsOutOfRange || !mCurrentIterator) {
    return true;
  }

  return mCurrentIterator->IsDone();
}

nsresult FilteredContentIterator::PositionAt(nsINode* aCurNode) {
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);
  mIsOutOfRange = false;
  return mCurrentIterator->PositionAt(aCurNode);
}

}  // namespace mozilla
