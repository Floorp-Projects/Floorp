/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsString.h"
#include "nsIEditActionListener.h"
#include "nsTSDNotifier.h"
#include "nsTextServicesDocument.h"


nsTSDNotifier::nsTSDNotifier(nsTextServicesDocument *aDoc) : mDoc(aDoc)
{
}

nsTSDNotifier::~nsTSDNotifier()
{
  mDoc = 0;
}

#define DEBUG_TSD_NOTIFIER_REFCNT 1

#ifdef DEBUG_TSD_NOTIFIER_REFCNT

nsrefcnt nsTSDNotifier::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsTSDNotifier::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsTSDNotifier)
NS_IMPL_RELEASE(nsTSDNotifier)

#endif

NS_IMPL_QUERY_INTERFACE1(nsTSDNotifier, nsIEditActionListener)

NS_IMETHODIMP
nsTSDNotifier::WillInsertNode(nsIDOMNode *aNode,
                              nsIDOMNode *aParent,
                              PRInt32     aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidInsertNode(nsIDOMNode *aNode,
                             nsIDOMNode *aParent,
                             PRInt32     aPosition,
                             nsresult    aResult)
{
  if (NS_FAILED(aResult))
    return NS_OK;

  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->InsertNode(aNode, aParent, aPosition);
}

NS_IMETHODIMP
nsTSDNotifier::WillDeleteNode(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  if (NS_FAILED(aResult))
    return NS_OK;

  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->DeleteNode(aChild);
}

NS_IMETHODIMP
nsTSDNotifier::WillSplitNode(nsIDOMNode *aExistingRightNode,
                             PRInt32     aOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidSplitNode(nsIDOMNode *aExistingRightNode,
                            PRInt32     aOffset,
                            nsIDOMNode *aNewLeftNode,
                            nsresult    aResult)
{
  if (NS_FAILED(aResult))
    return NS_OK;

  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->SplitNode(aExistingRightNode, aOffset, aNewLeftNode);
}

NS_IMETHODIMP
nsTSDNotifier::WillJoinNodes(nsIDOMNode  *aLeftNode,
                             nsIDOMNode  *aRightNode,
                             nsIDOMNode  *aParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidJoinNodes(nsIDOMNode  *aLeftNode,
                            nsIDOMNode  *aRightNode,
                            nsIDOMNode  *aParent,
                            nsresult     aResult)
{
  if (NS_FAILED(aResult))
    return NS_OK;

  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->JoinNodes(aLeftNode, aRightNode, aParent);
}

// -------------------------------
// stubs for unused listen methods
// -------------------------------

NS_IMETHODIMP
nsTSDNotifier::WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::WillDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}



