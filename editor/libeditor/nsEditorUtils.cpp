/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEditorUtils.h"

#include "mozilla/dom/OwningNonNull.h"
#include "mozilla/dom/Selection.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIClipboardDragDropHookList.h"
// hooks
#include "nsIClipboardDragDropHooks.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"
#include "nsISimpleEnumerator.h"

class nsISupports;
class nsRange;

using namespace mozilla;
using namespace mozilla::dom;

/******************************************************************************
 * nsAutoSelectionReset
 *****************************************************************************/

nsAutoSelectionReset::nsAutoSelectionReset(Selection* aSel, nsEditor* aEd)
  : mSel(nullptr), mEd(nullptr)
{ 
  if (!aSel || !aEd) return;    // not much we can do, bail.
  if (aEd->ArePreservingSelection()) return;   // we already have initted mSavedSel, so this must be nested call.
  mSel = aSel;
  mEd = aEd;
  if (mSel)
  {
    mEd->PreserveSelectionAcrossActions(mSel);
  }
}

nsAutoSelectionReset::~nsAutoSelectionReset()
{
  NS_ASSERTION(!mSel || mEd, "mEd should be non-null when mSel is");
  if (mSel && mEd->ArePreservingSelection())   // mSel will be null if this was nested call
  {
    mEd->RestorePreservedSelection(mSel);
  }
}

void
nsAutoSelectionReset::Abort()
{
  NS_ASSERTION(!mSel || mEd, "mEd should be non-null when mSel is");
  if (mSel)
    mEd->StopPreservingSelection();
}


/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

nsDOMIterator::nsDOMIterator(nsRange& aRange)
{
  MOZ_ASSERT(aRange.GetStartParent(), "Invalid range");
  mIter = NS_NewContentIterator();
  DebugOnly<nsresult> res = mIter->Init(&aRange);
  MOZ_ASSERT(NS_SUCCEEDED(res));
}

nsDOMIterator::nsDOMIterator(nsINode& aNode)
{
  mIter = NS_NewContentIterator();
  DebugOnly<nsresult> res = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(res));
}

nsDOMIterator::nsDOMIterator()
{
}

nsDOMIterator::~nsDOMIterator()
{
}

void
nsDOMIterator::AppendList(const nsBoolDomIterFunctor& functor,
                          nsTArray<OwningNonNull<nsINode>>& arrayOfNodes) const
{
  // Iterate through dom and build list
  for (; !mIter->IsDone(); mIter->Next()) {
    nsCOMPtr<nsINode> node = mIter->GetCurrentNode();

    if (functor(node)) {
      arrayOfNodes.AppendElement(*node);
    }
  }
}

nsDOMSubtreeIterator::nsDOMSubtreeIterator(nsRange& aRange)
{
  mIter = NS_NewContentSubtreeIterator();
  DebugOnly<nsresult> res = mIter->Init(&aRange);
  MOZ_ASSERT(NS_SUCCEEDED(res));
}

nsDOMSubtreeIterator::~nsDOMSubtreeIterator()
{
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool
nsEditorUtils::IsDescendantOf(nsINode* aNode, nsINode* aParent, int32_t* aOffset)
{
  MOZ_ASSERT(aNode && aParent);
  if (aNode == aParent) {
    return false;
  }

  for (nsCOMPtr<nsINode> node = aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == aParent) {
      if (aOffset) {
        *aOffset = aParent->IndexOf(node);
      }
      return true;
    }
  }

  return false;
}

bool
nsEditorUtils::IsDescendantOf(nsIDOMNode* aNode, nsIDOMNode* aParent, int32_t* aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(node && parent, false);
  return IsDescendantOf(node, parent, aOffset);
}

bool
nsEditorUtils::IsLeafNode(nsIDOMNode *aNode)
{
  bool hasChildren = false;
  if (aNode)
    aNode->HasChildNodes(&hasChildren);
  return !hasChildren;
}

/******************************************************************************
 * utility methods for drag/drop/copy/paste hooks
 *****************************************************************************/

nsresult
nsEditorHookUtils::GetHookEnumeratorFromDocument(nsIDOMDocument *aDoc,
                                                 nsISimpleEnumerator **aResult)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
  nsCOMPtr<nsIClipboardDragDropHookList> hookObj = do_GetInterface(docShell);
  NS_ENSURE_TRUE(hookObj, NS_ERROR_FAILURE);

  return hookObj->GetHookEnumerator(aResult);
}

bool
nsEditorHookUtils::DoInsertionHook(nsIDOMDocument *aDoc, nsIDOMEvent *aDropEvent,  
                                   nsITransferable *aTrans)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  GetHookEnumeratorFromDocument(aDoc, getter_AddRefs(enumerator));
  NS_ENSURE_TRUE(enumerator, true);

  bool hasMoreHooks = false;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreHooks)) && hasMoreHooks)
  {
    nsCOMPtr<nsISupports> isupp;
    if (NS_FAILED(enumerator->GetNext(getter_AddRefs(isupp))))
      break;

    nsCOMPtr<nsIClipboardDragDropHooks> override = do_QueryInterface(isupp);
    if (override)
    {
      bool doInsert = true;
#ifdef DEBUG
      nsresult hookResult =
#endif
      override->OnPasteOrDrop(aDropEvent, aTrans, &doInsert);
      NS_ASSERTION(NS_SUCCEEDED(hookResult), "hook failure in OnPasteOrDrop");
      NS_ENSURE_TRUE(doInsert, false);
    }
  }

  return true;
}
