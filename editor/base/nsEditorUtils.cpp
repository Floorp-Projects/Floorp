/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


#include "nsEditorUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"


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
  if (mSel && mEd->ArePreservingSelection())   // mSel will be null if this was nested call
  {
    mEd->RestorePreservedSelection(mSel);
  }
}

void
nsAutoSelectionReset::Abort()
{
  mEd->StopPreservingSelection();
}


/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);

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
  nsresult res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(mIter));
  if (NS_FAILED(res)) return res;
  return mIter->Init(aRange);
}

nsresult
nsDOMIterator::Init(nsIDOMNode* aNode)
{
  nsresult res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(mIter));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return mIter->Init(content);
}

void
nsDOMIterator::ForEach(nsDomIterFunctor& functor) const
{
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  nsresult res;
  
  // iterate through dom
  while (NS_ENUMERATOR_FALSE == mIter->IsDone())
  {
    res = mIter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return;
    node = do_QueryInterface(content);
    if (!node) return;
    functor(node);
    res = mIter->Next();
    if (NS_FAILED(res)) return;
  }
}

nsresult
nsDOMIterator::MakeList(nsBoolDomIterFunctor& functor,
                        nsCOMPtr<nsISupportsArray> *outArrayOfNodes) const
{
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  nsresult res;
  
  // make a array
  res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  return AppendList(functor, *outArrayOfNodes);
}

nsresult
nsDOMIterator::AppendList(nsBoolDomIterFunctor& functor,
                          nsCOMPtr<nsISupportsArray> arrayOfNodes) const
{
  if (!arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  nsresult res;
  
  // iterate through dom and build list
  while (NS_ENUMERATOR_FALSE == mIter->IsDone())
  {
    res = mIter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_NULL_POINTER;
    if (functor(node))
    {
      isupports = do_QueryInterface(node);
      arrayOfNodes->AppendElement(isupports);
    }
    res = mIter->Next();
    if (NS_FAILED(res)) return res;
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
  nsresult res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(mIter));
  if (NS_FAILED(res)) return res;
  return mIter->Init(aRange);
}

nsresult
nsDOMSubtreeIterator::Init(nsIDOMNode* aNode)
{
  nsresult res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(mIter));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return mIter->Init(content);
}

