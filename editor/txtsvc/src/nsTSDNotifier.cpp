/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsString.h"
#include "nsIEditActionListener.h"
#include "nsTSDNotifier.h"
#include "nsTextServicesDocument.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEditActionListenerIID, NS_IEDITACTIONLISTENER_IID);

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

NS_IMETHODIMP
nsTSDNotifier::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditActionListenerIID)) {
    *aInstancePtr = (void*)(nsIEditActionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

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
nsTSDNotifier::WillCreateNode(const nsAReadableString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidCreateNode(const nsAReadableString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAReadableString &aString)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTSDNotifier::DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAReadableString &aString, nsresult aResult)
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



