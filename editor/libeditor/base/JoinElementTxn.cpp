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

#include "JoinElementTxn.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMSelection.h"
#include "nsIEditorSupport.h"

static NS_DEFINE_IID(kIEditorSupportIID,    NS_IEDITORSUPPORT_IID);


JoinElementTxn::JoinElementTxn()
  : EditTxn()
{
}

NS_IMETHODIMP JoinElementTxn::Init(nsIEditor  *aEditor,
                                   nsIDOMNode *aLeftNode,
                                   nsIDOMNode *aRightNode)
{
  mEditor = do_QueryInterface(aEditor);
  mLeftNode = do_QueryInterface(aLeftNode);
  mRightNode = do_QueryInterface(aRightNode);
  mOffset=0;
  return NS_OK;
}

JoinElementTxn::~JoinElementTxn()
{
}

NS_IMETHODIMP JoinElementTxn::Do(void)
{
  nsresult result;

  if ((mLeftNode) && (mRightNode))
  { // get the parent node
    nsCOMPtr<nsIDOMNode>leftParent;
    result = mLeftNode->GetParentNode(getter_AddRefs(leftParent));
    if ((NS_SUCCEEDED(result)) && (leftParent))
    { // verify that mLeftNode and mRightNode have the same parent
      nsCOMPtr<nsIDOMNode>rightParent;
      result = mRightNode->GetParentNode(getter_AddRefs(rightParent));
      if ((NS_SUCCEEDED(result)) && (rightParent))
      {
        if (leftParent==rightParent)
        {
          mParent= do_QueryInterface(leftParent); // set this instance mParent. 
                                                  // Other methods will see a non-null mParent and know all is well
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = mLeftNode->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes)) {
            childNodes->GetLength(&mOffset);
          }
          else 
          {
            nsCOMPtr<nsIDOMCharacterData> leftNodeAsText;
            leftNodeAsText = do_QueryInterface(mLeftNode);
            if (leftNodeAsText) {
              leftNodeAsText->GetLength(&mOffset);
            }
          }
          nsCOMPtr<nsIEditorSupport> editor;
          result = mEditor->QueryInterface(kIEditorSupportIID, getter_AddRefs(editor));
          if (NS_SUCCEEDED(result) && editor) {
            result = editor->JoinNodesImpl(mRightNode, mLeftNode, mParent, PR_FALSE);
            if (NS_SUCCEEDED(result))
            {
              nsCOMPtr<nsIDOMSelection>selection;
              mEditor->GetSelection(getter_AddRefs(selection));
              if (selection)
              {
                selection->Collapse(mRightNode, mOffset);
              }
            }
          }
        }
        else 
        {
          NS_ASSERTION(PR_FALSE, "2 nodes do not have same parent");
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
  }
  return result;
}


NS_IMETHODIMP JoinElementTxn::Undo(void)
{
  nsresult result;
  nsCOMPtr<nsIEditorSupport> editor;
  result = mEditor->QueryInterface(kIEditorSupportIID, getter_AddRefs(editor));
  if (NS_SUCCEEDED(result) && editor) {
    result = editor->SplitNodeImpl(mRightNode, mOffset, mLeftNode, mParent);
    if (NS_SUCCEEDED(result) && mLeftNode)
    {
      nsCOMPtr<nsIDOMSelection>selection;
      mEditor->GetSelection(getter_AddRefs(selection));
      if (selection)
      {
        selection->Collapse(mLeftNode, mOffset);
      }
    }
  }
  else {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP JoinElementTxn::Redo(void)
{
  nsresult result;
  nsCOMPtr<nsIEditorSupport> editor;
  result = mEditor->QueryInterface(kIEditorSupportIID, getter_AddRefs(editor));
  if (NS_SUCCEEDED(result) && editor) 
  {
    result = editor->JoinNodesImpl(mRightNode, mLeftNode, mParent, PR_FALSE);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMSelection>selection;
      mEditor->GetSelection(getter_AddRefs(selection));
      if (selection)
      {
        selection->Collapse(mRightNode, mOffset);
      }
    }
  }
  else {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMETHODIMP JoinElementTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult JoinElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP JoinElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP JoinElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Join Element";
  }
  return NS_OK;
}

NS_IMETHODIMP JoinElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Split Element";
  }
  return NS_OK;
}
