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

#include "SplitElementTxn.h"
#include "editor.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

// note that aEditor is not refcounted
SplitElementTxn::SplitElementTxn(nsEditor   *aEditor,
                                 nsIDOMNode *aNode,
                                 PRInt32     aOffset)
  : EditTxn(aEditor)
{
  mNode = aNode;
  NS_ADDREF(mNode);
  mOffset = aOffset;
  mNewNode = nsnull;
  mParent = nsnull;
}

SplitElementTxn::~SplitElementTxn()
{
  NS_IF_RELEASE(mNode);
  NS_IF_RELEASE(mNewNode);
  NS_IF_RELEASE(mParent);
}

nsresult SplitElementTxn::Do(void)
{
  // create a new node
  nsresult result = mNode->CloneNode(PR_FALSE, &mNewNode);
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (nsnull!=mNewNode)), "could not create element.");

  if ((NS_SUCCEEDED(result)) && (nsnull!=mNewNode))
  {
    // get the parent node
    result = mNode->GetParentNode(&mParent);
    // insert the new node
    if ((NS_SUCCEEDED(result)) && (nsnull!=mParent))
    {
      result = mEditor->SplitNode(mNode, mOffset, mNewNode, mParent);
    }
  }
  return result;
}

nsresult SplitElementTxn::Undo(void)
{
  // this assumes Do inserted the new node in front of the prior existing node
  nsresult result = mEditor->JoinNodes(mNode, mNewNode, mParent, PR_FALSE);
  return result;
}

nsresult SplitElementTxn::Redo(void)
{
  nsresult result = mEditor->SplitNode(mNode, mOffset, mNewNode, mParent);
  return result;
}

nsresult SplitElementTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult SplitElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_OK;
}

nsresult SplitElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult SplitElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Join Element";
  }
  return NS_OK;
}

nsresult SplitElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Split Element";
  }
  return NS_OK;
}
