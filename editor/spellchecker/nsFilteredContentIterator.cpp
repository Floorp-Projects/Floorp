/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozilla/Move.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFilteredContentIterator.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsINode.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsITextServicesFilter.h"
#include "nsRange.h"

using namespace mozilla;

//------------------------------------------------------------
nsFilteredContentIterator::nsFilteredContentIterator(nsITextServicesFilter* aFilter) :
  mFilter(aFilter),
  mDidSkip(false),
  mIsOutOfRange(false),
  mDirection(eDirNotSet)
{
  mIterator = do_CreateInstance("@mozilla.org/content/post-content-iterator;1");
  mPreIterator = do_CreateInstance("@mozilla.org/content/pre-content-iterator;1");
}

//------------------------------------------------------------
nsFilteredContentIterator::~nsFilteredContentIterator()
{
}

//------------------------------------------------------------
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFilteredContentIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFilteredContentIterator)

NS_INTERFACE_MAP_BEGIN(nsFilteredContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsIContentIterator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentIterator)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsFilteredContentIterator)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsFilteredContentIterator,
                         mCurrentIterator,
                         mIterator,
                         mPreIterator,
                         mFilter,
                         mRange)

//------------------------------------------------------------
nsresult
nsFilteredContentIterator::Init(nsINode* aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  NS_ENSURE_TRUE(mPreIterator, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);
  mIsOutOfRange    = false;
  mDirection       = eForward;
  mCurrentIterator = mPreIterator;

  mRange = new nsRange(aRoot);
  mRange->SelectNode(*aRoot, IgnoreErrors());

  nsresult rv = mPreIterator->Init(mRange);
  NS_ENSURE_SUCCESS(rv, rv);
  return mIterator->Init(mRange);
}

//------------------------------------------------------------
nsresult
nsFilteredContentIterator::Init(nsRange* aRange)
{
  if (NS_WARN_IF(!aRange)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  mRange = aRange->CloneRange();

  return InitWithRange();
}

//------------------------------------------------------------
nsresult
nsFilteredContentIterator::Init(nsINode* aStartContainer, uint32_t aStartOffset,
                                nsINode* aEndContainer, uint32_t aEndOffset)
{
  return Init(RawRangeBoundary(aStartContainer, aStartOffset),
              RawRangeBoundary(aEndContainer, aEndOffset));
}

nsresult
nsFilteredContentIterator::Init(const RawRangeBoundary& aStart,
                                const RawRangeBoundary& aEnd)
{
  RefPtr<nsRange> range;
  nsresult rv = nsRange::CreateRange(aStart, aEnd, getter_AddRefs(range));
  if (NS_WARN_IF(NS_FAILED(rv)) || NS_WARN_IF(!range) ||
      NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(range->StartRef() == aStart);
  MOZ_ASSERT(range->EndRef() == aEnd);

  mRange = std::move(range);

  return InitWithRange();
}

nsresult
nsFilteredContentIterator::InitWithRange()
{
  MOZ_ASSERT(mRange);
  MOZ_ASSERT(mRange->IsPositioned());

  if (NS_WARN_IF(!mPreIterator) || NS_WARN_IF(!mIterator)) {
    return NS_ERROR_FAILURE;
  }

  mIsOutOfRange = false;
  mDirection = eForward;
  mCurrentIterator = mPreIterator;

  nsresult rv = mPreIterator->Init(mRange);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mIterator->Init(mRange);
}

//------------------------------------------------------------
nsresult
nsFilteredContentIterator::SwitchDirections(bool aChangeToForward)
{
  nsINode *node = mCurrentIterator->GetCurrentNode();

  if (aChangeToForward) {
    mCurrentIterator = mPreIterator;
    mDirection       = eForward;
  } else {
    mCurrentIterator = mIterator;
    mDirection       = eBackward;
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

//------------------------------------------------------------
void
nsFilteredContentIterator::First()
{
  if (!mCurrentIterator) {
    NS_ERROR("Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eForward) {
    mCurrentIterator = mPreIterator;
    mDirection       = eForward;
    mIsOutOfRange    = false;
  }

  mCurrentIterator->First();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  nsINode *currentNode = mCurrentIterator->GetCurrentNode();

  bool didCross;
  CheckAdvNode(currentNode, didCross, eForward);
}

//------------------------------------------------------------
void
nsFilteredContentIterator::Last()
{
  if (!mCurrentIterator) {
    NS_ERROR("Missing iterator!");

    return;
  }

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eBackward) {
    mCurrentIterator = mIterator;
    mDirection       = eBackward;
    mIsOutOfRange    = false;
  }

  mCurrentIterator->Last();

  if (mCurrentIterator->IsDone()) {
    return;
  }

  nsINode *currentNode = mCurrentIterator->GetCurrentNode();

  bool didCross;
  CheckAdvNode(currentNode, didCross, eBackward);
}

///////////////////////////////////////////////////////////////////////////
// ContentToParentOffset: returns the content node's parent and offset.
//
static void
ContentToParentOffset(nsIContent* aContent, nsIContent** aParent,
                      int32_t* aOffset)
{
  if (!aParent || !aOffset)
    return;

  *aParent = nullptr;
  *aOffset  = 0;

  if (!aContent)
    return;

  nsCOMPtr<nsIContent> parent = aContent->GetParent();
  if (!parent)
    return;

  *aOffset = parent->ComputeIndexOf(aContent);
  parent.forget(aParent);
}

///////////////////////////////////////////////////////////////////////////
// ContentIsInTraversalRange: returns true if content is visited during
// the traversal of the range in the specified mode.
//
static bool
ContentIsInTraversalRange(nsIContent* aContent, bool aIsPreMode,
                          nsINode* aStartContainer, int32_t aStartOffset,
                          nsINode* aEndContainer, int32_t aEndOffset)
{
  NS_ENSURE_TRUE(aStartContainer && aEndContainer && aContent, false);

  nsCOMPtr<nsIContent> parentNode;
  int32_t indx = 0;

  ContentToParentOffset(aContent, getter_AddRefs(parentNode), &indx);

  NS_ENSURE_TRUE(parentNode, false);

  if (!aIsPreMode)
    ++indx;

  int32_t startRes =
    nsContentUtils::ComparePoints(aStartContainer, aStartOffset,
                                  parentNode, indx);
  int32_t endRes = nsContentUtils::ComparePoints(aEndContainer, aEndOffset,
                                                 parentNode, indx);
  return (startRes <= 0) && (endRes >= 0);
}

static bool
ContentIsInTraversalRange(nsRange* aRange, nsIContent* aNextContent, bool aIsPreMode)
{
  // XXXbz we have a caller below (in AdvanceNode) who passes null for
  // aNextContent!
  NS_ENSURE_TRUE(aNextContent && aRange, false);

  return ContentIsInTraversalRange(aNextContent, aIsPreMode,
                                   aRange->GetStartContainer(),
                                   static_cast<int32_t>(aRange->StartOffset()),
                                   aRange->GetEndContainer(),
                                   static_cast<int32_t>(aRange->EndOffset()));
}

//------------------------------------------------------------
// Helper function to advance to the next or previous node
nsresult
nsFilteredContentIterator::AdvanceNode(nsINode* aNode, nsINode*& aNewNode, eDirectionType aDir)
{
  nsCOMPtr<nsIContent> nextNode;
  if (aDir == eForward) {
    nextNode = aNode->GetNextSibling();
  } else {
    nextNode = aNode->GetPreviousSibling();
  }

  if (nextNode) {
    // If we got here, that means we found the nxt/prv node
    // make sure it is in our DOMRange
    bool intersects = ContentIsInTraversalRange(mRange, nextNode,
                                                aDir == eForward);
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
    bool intersects = ContentIsInTraversalRange(mRange, nextNode, aDir == eForward);
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

//------------------------------------------------------------
// Helper function to see if the next/prev node should be skipped
void
nsFilteredContentIterator::CheckAdvNode(nsINode* aNode, bool& aDidSkip, eDirectionType aDir)
{
  aDidSkip      = false;
  mIsOutOfRange = false;

  if (aNode && mFilter) {
    nsCOMPtr<nsINode> currentNode = aNode;
    bool skipIt;
    while (1) {
      nsresult rv = mFilter->Skip(aNode, &skipIt);
      if (NS_SUCCEEDED(rv) && skipIt) {
        aDidSkip = true;
        // Get the next/prev node and then
        // see if we should skip that
        nsCOMPtr<nsINode> advNode;
        rv = AdvanceNode(aNode, *getter_AddRefs(advNode), aDir);
        if (NS_SUCCEEDED(rv) && advNode) {
          aNode = advNode;
        } else {
          return; // fell out of range
        }
      } else {
        if (aNode != currentNode) {
          nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
          mCurrentIterator->PositionAt(content);
        }
        return; // found something
      }
    }
  }
}

void
nsFilteredContentIterator::Next()
{
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
  nsINode *currentNode = mCurrentIterator->GetCurrentNode();

  CheckAdvNode(currentNode, mDidSkip, eForward);
}

void
nsFilteredContentIterator::Prev()
{
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
  nsINode *currentNode = mCurrentIterator->GetCurrentNode();

  CheckAdvNode(currentNode, mDidSkip, eBackward);
}

nsINode *
nsFilteredContentIterator::GetCurrentNode()
{
  if (mIsOutOfRange || !mCurrentIterator) {
    return nullptr;
  }

  return mCurrentIterator->GetCurrentNode();
}

bool
nsFilteredContentIterator::IsDone()
{
  if (mIsOutOfRange || !mCurrentIterator) {
    return true;
  }

  return mCurrentIterator->IsDone();
}

nsresult
nsFilteredContentIterator::PositionAt(nsINode* aCurNode)
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);
  mIsOutOfRange = false;
  return mCurrentIterator->PositionAt(aCurNode);
}
