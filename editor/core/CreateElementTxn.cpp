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

#include "CreateElementTxn.h"
#include "nsIDOMNodeList.h"

CreateElementTxn::CreateElementTxn()
  : EditTxn()
{
}

nsresult CreateElementTxn::Init(nsIDOMDocument *aDoc,
                                const nsString& aTag,
                                nsIDOMNode *aParent,
                                PRUint32 aOffsetInParent)
{
  if ((nsnull!=aDoc) && (nsnull!=aParent))
  {
    mDoc = aDoc;
    mTag = aTag;
    mParent = aParent;
    mOffsetInParent = aOffsetInParent;
    mNewNode = nsnull;
    mRefNode = nsnull;
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}


CreateElementTxn::~CreateElementTxn()
{
}

nsresult CreateElementTxn::Do(void)
{
  // create a new node
  nsresult result = mDoc->CreateElement(mTag, getter_AddRefs(mNewNode));
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (mNewNode)), "could not create element.");

  if ((NS_SUCCEEDED(result)) && (mNewNode))
  {
    // insert the new node
    nsCOMPtr<nsIDOMNode> resultNode;
    if (CreateElementTxn::eAppend==mOffsetInParent)
    {
      result = mParent->AppendChild(mNewNode, getter_AddRefs(resultNode));
    }
    else
    {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      result = mParent->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(result)) && (childNodes))
      {
        result = childNodes->Item(mOffsetInParent, getter_AddRefs(mRefNode));
        if ((NS_SUCCEEDED(result)) && (mRefNode))
        {
          result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
        }
      }
    }
  }
  return result;
}

nsresult CreateElementTxn::Undo(void)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mNewNode, getter_AddRefs(resultNode));
  return result;
}

nsresult CreateElementTxn::Redo(void)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
  return result;
}

nsresult CreateElementTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult CreateElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

nsresult CreateElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult CreateElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Element: ";
    **aString += mTag;
  }
  return NS_OK;
}

nsresult CreateElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Create Element: ";
    **aString += mTag;
  }
  return NS_OK;
}
