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
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIEditorSupport.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

CreateElementTxn::CreateElementTxn()
  : EditTxn()
{
}

NS_IMETHODIMP CreateElementTxn::Init(nsIEditor *aEditor,
                                     const nsString& aTag,
                                     nsIDOMNode *aParent,
                                     PRUint32 aOffsetInParent)
{
  NS_ASSERTION(aEditor&&aParent, "null args");
  if (aEditor && aParent)
  {
    mEditor = do_QueryInterface(aEditor);
    mTag = aTag;
    mParent = do_QueryInterface(aParent);
    mOffsetInParent = aOffsetInParent;
#ifdef NS_DEBUG
    {
      nsCOMPtr<nsIDOMNodeList> testChildNodes;
      nsresult testResult = mParent->GetChildNodes(getter_AddRefs(testChildNodes));
      NS_ASSERTION(testChildNodes, "bad parent type, can't have children.");
    }
#endif
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}


CreateElementTxn::~CreateElementTxn()
{
}

NS_IMETHODIMP CreateElementTxn::Do(void)
{
  if (gNoisy) { printf("Do Create Element parent = %p, offset = %d\n", 
                        mParent.get(), mOffsetInParent); }
  NS_ASSERTION(mEditor, "bad state -- null editor");
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mEditor)
  {
    // create a new node
    nsCOMPtr<nsIDOMDocument>doc;
    result = mEditor->GetDocument(getter_AddRefs(doc));
    if ((NS_SUCCEEDED(result)) && (doc))
    {
      if (nsIEditor::GetTextNodeTag() == mTag) 
      {
        const nsString stringData;
        nsCOMPtr<nsIDOMText>newTextNode;
        result = doc->CreateTextNode(stringData, getter_AddRefs(newTextNode));
        if (NS_SUCCEEDED(result) && newTextNode) {
          mNewNode = do_QueryInterface(newTextNode);
        }
      }
      else 
      {
        nsCOMPtr<nsIDOMElement>newElement;
        result = doc->CreateElement(mTag, getter_AddRefs(newElement));
        if (NS_SUCCEEDED(result) && newElement) {
          mNewNode = do_QueryInterface(newElement);
        }
      }
      NS_ASSERTION(((NS_SUCCEEDED(result)) && (mNewNode)), "could not create element.");
      if ((NS_SUCCEEDED(result)) && (mNewNode))
      {
        if (gNoisy) { printf("  newNode = %p\n", mNewNode.get()); }
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
            PRUint32 count;
            childNodes->GetLength(&count);
            if (mOffsetInParent>count)
              mOffsetInParent = count;
            result = childNodes->Item(mOffsetInParent, getter_AddRefs(mRefNode));
            if (NS_SUCCEEDED(result)) // note, it's ok for mRefNode to be null.  that means append
            {
              result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
              if (NS_SUCCEEDED(result))
              {
                nsCOMPtr<nsIDOMSelection> selection;
                nsresult selectionResult = mEditor->GetSelection(getter_AddRefs(selection));
                if (NS_SUCCEEDED(selectionResult) && selection) {
                  PRInt32 offset=0;
                  nsIEditorSupport::GetChildOffset(mNewNode, mParent, offset);
                  selectionResult = selection->Collapse(mParent, offset);
                  NS_ASSERTION((NS_SUCCEEDED(selectionResult)), "selection could not be collapsed after undo of insert.");
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

NS_IMETHODIMP CreateElementTxn::Undo(void)
{
  if (gNoisy) { printf("Undo Create Element, mParent = %p, node = %p\n",
                        mParent.get(), mNewNode.get()); }
  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mNewNode, getter_AddRefs(resultNode));
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    nsresult selectionResult = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(selectionResult) && selection) {
      PRInt32 offset=0;
      if (mRefNode) {
        nsIEditorSupport::GetChildOffset(mRefNode, mParent, offset);
      }
      selectionResult = selection->Collapse(mParent, offset);
      NS_ASSERTION((NS_SUCCEEDED(selectionResult)), "selection could not be collapsed after undo of insert.");
    }
  }
  return result;
}

NS_IMETHODIMP CreateElementTxn::Redo(void)
{
  if (gNoisy) { printf("Redo Create Element\n"); }

  // first, reset mNewNode so it has no attributes or content
  nsCOMPtr<nsIDOMCharacterData>nodeAsText;
  nodeAsText = do_QueryInterface(mNewNode);
  if (nodeAsText)
  {
    nsAutoString nullString;
    nodeAsText->SetData(nullString);
  }
  
  // now, reinsert mNewNode
  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      PRInt32 offset=0;
      nsIEditorSupport::GetChildOffset(mNewNode, mParent, offset);
      nsresult selectionResult = selection->Collapse(mParent, offset);
      NS_ASSERTION((NS_SUCCEEDED(selectionResult)), "selection could not be collapsed after undo of insert.");
    }
  }
  return result;
}

NS_IMETHODIMP CreateElementTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Element: ";
    **aString += mTag;
  }
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Create Element: ";
    **aString += mTag;
  }
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetNewNode(nsIDOMNode **aNewNode)
{
  if (!aNewNode)
    return NS_ERROR_NULL_POINTER;
  if (!mNewNode)
    return NS_ERROR_NOT_INITIALIZED;
  *aNewNode = mNewNode;
  NS_ADDREF(*aNewNode);
  return NS_OK;
}
