/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);

NS_IMPL_ADDREF(EditTxn)
NS_IMPL_RELEASE(EditTxn)

// note that aEditor is not refcounted
EditTxn::EditTxn(nsEditor *aEditor)
{
  NS_ASSERTION(nsnull!=aEditor, "null aEditor arg to EditTxn constructor");
  mEditor = aEditor;
}

nsresult EditTxn::Do(void)
{
  return NS_OK;
}

nsresult EditTxn::Undo(void)
{
  return NS_OK;
}

nsresult EditTxn::Redo(void)
{
  return Do();
}

nsresult EditTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_TRUE;
  return NS_OK;
}

nsresult EditTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_OK;
}

nsresult EditTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult EditTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

nsresult EditTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

nsresult
EditTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}


/* =============================== Static helper methods ================================== */

nsresult 
EditTxn::SplitNode(nsIDOMNode * aNode,
                   PRInt32      aOffset,
                   nsIDOMNode*  aNewNode,
                   nsIDOMNode*  aParent)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = aParent->InsertBefore(aNewNode, aNode, getter_AddRefs(resultNode));
  if (NS_SUCCEEDED(result))
  {
    // split the children between the 2 nodes
    // at this point, nNode has all the children
    if (0<=aOffset) // don't bother unless we're going to move at least one child
    {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      result = aParent->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(result)) && (childNodes))
      {
        PRInt32 i=0;
        for ( ; ((NS_SUCCEEDED(result)) && (i<aOffset)); i++)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          result = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(result)) && (childNode))
          {
            result = aNode->RemoveChild(childNode, getter_AddRefs(resultNode));
            if (NS_SUCCEEDED(result))
            {
              result = aNewNode->AppendChild(childNode, getter_AddRefs(resultNode));
            }
          }
        }
      }        
    }
  }
  return result;
}

nsresult
EditTxn::JoinNodes(nsIDOMNode * aNodeToKeep,
                   nsIDOMNode * aNodeToJoin,
                   nsIDOMNode * aParent,
                   PRBool       aNodeToKeepIsFirst)
{
  nsresult result;
  return result;
}