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
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
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
      nsCOMPtr<nsIDOMNode> resultNode;
      result = mParent->InsertBefore(mNewNode, mNode, getter_AddRefs(resultNode));
      if (NS_SUCCEEDED(result))
      {
        // split the children between the 2 nodes
        // at this point, nNode has all the children
        if (0<=mOffset) // don't bother unless we're going to move at least one child
        {
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = mParent->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes))
          {
            PRUint32 i=0;
            for ( ; ((NS_SUCCEEDED(result)) && (i<mOffset)); i++)
            {
              nsCOMPtr<nsIDOMNode> childNode;
              result = childNodes->Item(i, getter_AddRefs(childNode));
              if ((NS_SUCCEEDED(result)) && (childNode))
              {
                result = mNode->RemoveChild(childNode, getter_AddRefs(resultNode));
                if (NS_SUCCEEDED(result))
                {
                  result = mNewNode->AppendChild(childNode, getter_AddRefs(resultNode));
                }
              }
            }
          }
        }
      }        
    }
  }
  return result;
}

nsresult SplitElementTxn::Undo(void)
{
  return NS_OK;
}

nsresult SplitElementTxn::Redo(void)
{
  return NS_OK;
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
