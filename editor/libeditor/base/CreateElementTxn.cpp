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
#include "editor.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

// note that aEditor is not refcounted
CreateElementTxn::CreateElementTxn(nsEditor *aEditor,
                                   nsIDOMDocument *aDoc,
                                   const nsString& aTag,
                                   nsIDOMNode *aParent,
                                   PRUint32 aOffsetInParent)
  : EditTxn(aEditor)
{
  mDoc = aDoc;
  NS_ADDREF(mDoc);
  mTag = aTag;
  mParent = aParent;
  NS_ADDREF(mParent);
  mOffsetInParent = aOffsetInParent;
  mNewNode = nsnull;
  mRefNode = nsnull;
}

CreateElementTxn::~CreateElementTxn()
{
  NS_IF_RELEASE(mDoc);
  NS_IF_RELEASE(mParent);
  NS_IF_RELEASE(mNewNode);
}

nsresult CreateElementTxn::Do(void)
{
  // create a new node
  nsresult result = mDoc->CreateElement(mTag, &mNewNode);
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (nsnull!=mNewNode)), "could not create element.");

  if ((NS_SUCCEEDED(result)) && (nsnull!=mNewNode))
  {
    // insert the new node
    nsIDOMNode *resultNode=nsnull;
    if (CreateElementTxn::eAppend==mOffsetInParent)
    {
      result = mParent->AppendChild(mNewNode, &resultNode);
      if ((NS_SUCCEEDED(result)) && (nsnull!=resultNode))
        NS_RELEASE(resultNode); // this object already holds a ref from CreateElement
    }
    else
    {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      result = mParent->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(result)) && (childNodes))
      {
        result = childNodes->Item(mOffsetInParent, &mRefNode);
        if ((NS_SUCCEEDED(result)) && (nsnull!=mRefNode))
        {
          result = mParent->InsertBefore(mNewNode, mRefNode, &resultNode);
          if ((NS_SUCCEEDED(result)) && (nsnull!=resultNode))
            NS_RELEASE(resultNode); // this object already holds a ref from CreateElement
        }
      }
    }
  }
  return result;
}

nsresult CreateElementTxn::Undo(void)
{
  nsIDOMNode *resultNode=nsnull;
  nsresult result = mParent->RemoveChild(mNewNode, &resultNode);
  if ((NS_SUCCEEDED(result)) && (nsnull!=resultNode))
    NS_RELEASE(resultNode);
  return result;
}

nsresult CreateElementTxn::Redo(void)
{
  nsIDOMNode *resultNode=nsnull;
  nsresult result = mParent->InsertBefore(mNewNode, mRefNode, &resultNode);
   if ((NS_SUCCEEDED(result)) && (nsnull!=resultNode))
    NS_RELEASE(resultNode);
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
