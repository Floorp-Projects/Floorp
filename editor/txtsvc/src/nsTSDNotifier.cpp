/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
nsTSDNotifier::InsertNode(nsIDOMNode *aNode,
                          nsIDOMNode *aParent,
                          PRInt32 aPosition)
{
  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->InsertNode(aNode, aParent, aPosition);
}

NS_IMETHODIMP
nsTSDNotifier::DeleteNode(nsIDOMNode *aChild)
{
  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->DeleteNode(aChild);
}

NS_IMETHODIMP
nsTSDNotifier::SplitNode(nsIDOMNode *aExistingRightNode,
                         PRInt32 aOffset,
                         nsIDOMNode *aNewLeftNode)
{
  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->SplitNode(aExistingRightNode, aOffset, aNewLeftNode);
}

NS_IMETHODIMP
nsTSDNotifier::JoinNodes(nsIDOMNode  *aLeftNode,
                         nsIDOMNode  *aRightNode,
                         nsIDOMNode  *aParent)
{
  if (!mDoc)
    return NS_ERROR_FAILURE;

  return mDoc->JoinNodes(aLeftNode, aRightNode, aParent);
}

