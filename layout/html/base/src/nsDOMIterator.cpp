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

#include "nsDOMIterator.h"
#include "nsIDOMNode.h"

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

nsDOMIterator::nsDOMIterator(nsIContent &aContent) : mContent(aContent)
{
  mRefCnt = 1;

  // keep the content alive so the array of children
  // does not go away without "this" to know
  mContent.AddRef();

  mPosition = -1;
  mScriptObject = nsnull;
}

nsDOMIterator::~nsDOMIterator()
{
  mContent.Release();
}

nsresult nsDOMIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMNodeIteratorIID, NS_IDOMNODEITERATOR_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMNodeIteratorIID)) {
    *aInstancePtr = (void*)(nsIDOMNodeIterator*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMNodeIterator*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMIterator)

NS_IMPL_RELEASE(nsDOMIterator)


nsresult nsDOMIterator::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNodeIterator(aContext, this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMIterator::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

// nsIDOMIterator interface
nsresult nsDOMIterator::SetFilter(PRInt32 aFilter, PRBool aFilterOn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDOMIterator::GetLength(PRUint32 *aLength)
{
  *aLength = mContent.ChildCount();
  return NS_OK;
}

nsresult nsDOMIterator::GetCurrentNode(nsIDOMNode **aNode)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  content = mContent.ChildAt(mPosition);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_RELEASE(content);
  }
  else {
    *aNode = nsnull;
  }

  return res;
}

nsresult nsDOMIterator::GetNextNode(nsIDOMNode **aNode)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  content = mContent.ChildAt(++mPosition);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_RELEASE(content);
  }
  else {
    mPosition = mContent.ChildCount();
    *aNode = nsnull;
  }

  return res;
}

nsresult nsDOMIterator::GetPreviousNode(nsIDOMNode **aNode)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  content = mContent.ChildAt(--mPosition);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_RELEASE(content);
  }
  else {
    mPosition = -1;
    *aNode = nsnull;
  }

  return res;
}

nsresult nsDOMIterator::ToFirst(nsIDOMNode **aNode)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  mPosition = 0;
  content = mContent.ChildAt(mPosition);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_RELEASE(content);
  }
  else {
    *aNode = nsnull;
  }

  return res;
}

nsresult nsDOMIterator::ToLast(nsIDOMNode **aNode)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  mPosition = mPosition = mContent.ChildCount();
  content = mContent.ChildAt(mPosition);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_RELEASE(content);
  }
  else {
    *aNode = nsnull;
  }

  return res;
}

nsresult nsDOMIterator::MoveTo(int aNth, nsIDOMNode **aNode)
{
  mPosition = aNth;
  return GetCurrentNode(aNode);
}


