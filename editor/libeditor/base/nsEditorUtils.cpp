/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsEditorUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"

// hooks
#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"
#include "nsISimpleEnumerator.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"


/******************************************************************************
 * nsAutoSelectionReset
 *****************************************************************************/

nsAutoSelectionReset::nsAutoSelectionReset(nsISelection *aSel, nsEditor *aEd) : 
mSel(nsnull)
,mEd(nsnull)
{ 
  if (!aSel || !aEd) return;    // not much we can do, bail.
  if (aEd->ArePreservingSelection()) return;   // we already have initted mSavedSel, so this must be nested call.
  mSel = do_QueryInterface(aSel);
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

nsDOMIterator::nsDOMIterator() :
mIter(nsnull)
{
}
    
nsDOMIterator::~nsDOMIterator()
{
}
    
nsresult
nsDOMIterator::Init(nsIDOMRange* aRange)
{
  nsresult res;
  mIter = do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(mIter, NS_ERROR_FAILURE);
  return mIter->Init(aRange);
}

nsresult
nsDOMIterator::Init(nsIDOMNode* aNode)
{
  nsresult res;
  mIter = do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(mIter, NS_ERROR_FAILURE);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return mIter->Init(content);
}

void
nsDOMIterator::ForEach(nsDomIterFunctor& functor) const
{
  nsCOMPtr<nsIDOMNode> node;
  
  // iterate through dom
  while (!mIter->IsDone())
  {
    node = do_QueryInterface(mIter->GetCurrentNode());
    if (!node)
      return;

    functor(node);
    mIter->Next();
  }
}

nsresult
nsDOMIterator::AppendList(nsBoolDomIterFunctor& functor,
                          nsCOMArray<nsIDOMNode>& arrayOfNodes) const
{
  nsCOMPtr<nsIDOMNode> node;
  
  // iterate through dom and build list
  while (!mIter->IsDone())
  {
    node = do_QueryInterface(mIter->GetCurrentNode());
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    if (functor(node))
    {
      arrayOfNodes.AppendObject(node);
    }
    mIter->Next();
  }
  return NS_OK;
}

nsDOMSubtreeIterator::nsDOMSubtreeIterator()
{
}
    
nsDOMSubtreeIterator::~nsDOMSubtreeIterator()
{
}
    
nsresult
nsDOMSubtreeIterator::Init(nsIDOMRange* aRange)
{
  nsresult res;
  mIter = do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(mIter, NS_ERROR_FAILURE);
  return mIter->Init(aRange);
}

nsresult
nsDOMSubtreeIterator::Init(nsIDOMNode* aNode)
{
  nsresult res;
  mIter = do_CreateInstance("@mozilla.org/content/subtree-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(mIter, NS_ERROR_FAILURE);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return mIter->Init(content);
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

PRBool 
nsEditorUtils::IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 *aOffset) 
{
  NS_ENSURE_TRUE(aNode || aParent, PR_FALSE);
  if (aNode == aParent) return PR_FALSE;
  
  nsCOMPtr<nsIDOMNode> parent, node = do_QueryInterface(aNode);
  nsresult res;
  
  do
  {
    res = node->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(res, PR_FALSE);
    if (parent == aParent) 
    {
      if (aOffset)
      {
        nsCOMPtr<nsIContent> pCon(do_QueryInterface(parent));
        nsCOMPtr<nsIContent> cCon(do_QueryInterface(node));
        if (pCon)
        {
          *aOffset = pCon->IndexOf(cCon);
        }
      }
      return PR_TRUE;
    }
    node = parent;
  } while (parent);
  
  return PR_FALSE;
}

PRBool
nsEditorUtils::IsLeafNode(nsIDOMNode *aNode)
{
  PRBool hasChildren = PR_FALSE;
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

  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  nsCOMPtr<nsIClipboardDragDropHookList> hookObj = do_GetInterface(docShell);
  NS_ENSURE_TRUE(hookObj, NS_ERROR_FAILURE);

  return hookObj->GetHookEnumerator(aResult);
}

PRBool
nsEditorHookUtils::DoInsertionHook(nsIDOMDocument *aDoc, nsIDOMEvent *aDropEvent,  
                                   nsITransferable *aTrans)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  GetHookEnumeratorFromDocument(aDoc, getter_AddRefs(enumerator));
  NS_ENSURE_TRUE(enumerator, PR_TRUE);

  PRBool hasMoreHooks = PR_FALSE;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreHooks)) && hasMoreHooks)
  {
    nsCOMPtr<nsISupports> isupp;
    if (NS_FAILED(enumerator->GetNext(getter_AddRefs(isupp))))
      break;

    nsCOMPtr<nsIClipboardDragDropHooks> override = do_QueryInterface(isupp);
    if (override)
    {
      PRBool doInsert = PR_TRUE;
#ifdef DEBUG
      nsresult hookResult =
#endif
      override->OnPasteOrDrop(aDropEvent, aTrans, &doInsert);
      NS_ASSERTION(NS_SUCCEEDED(hookResult), "hook failure in OnPasteOrDrop");
      NS_ENSURE_TRUE(doInsert, PR_FALSE);
    }
  }

  return PR_TRUE;
}
