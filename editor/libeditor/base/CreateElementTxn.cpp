/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "CreateElementTxn.h"
#include "nsEditor.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"

//included for new nsEditor::CreateContent()
#include "nsIContent.h"

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

CreateElementTxn::CreateElementTxn()
  : EditTxn()
{
}

NS_IMETHODIMP CreateElementTxn::Init(nsEditor      *aEditor,
                                     const nsAReadableString &aTag,
                                     nsIDOMNode     *aParent,
                                     PRUint32        aOffsetInParent)
{
  NS_ASSERTION(aEditor&&aParent, "null args");
  if (!aEditor || !aParent) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mTag = aTag;
  mParent = do_QueryInterface(aParent);
  mOffsetInParent = aOffsetInParent;
#ifdef NS_DEBUG
  {
    nsCOMPtr<nsIDOMNodeList> testChildNodes;
    nsresult testResult = mParent->GetChildNodes(getter_AddRefs(testChildNodes));
    NS_ASSERTION(testChildNodes, "bad parent type, can't have children.");
    NS_ASSERTION(NS_SUCCEEDED(testResult), "bad result.");
  }
#endif
  return NS_OK;
}


CreateElementTxn::~CreateElementTxn()
{
}

NS_IMETHODIMP CreateElementTxn::DoTransaction(void)
{
  if (gNoisy)
  {
    char* nodename = mTag.ToNewCString();
    printf("Do Create Element parent = %p <%s>, offset = %d\n", 
           mParent.get(), nodename, mOffsetInParent);
    nsMemory::Free(nodename);
  }

  NS_ASSERTION(mEditor && mParent, "bad state");
  if (!mEditor || !mParent) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  // create a new node
  nsCOMPtr<nsIDOMDocument>doc;
  result = mEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(result)) return result;
  if (!doc) return NS_ERROR_NULL_POINTER;

  nsAutoString textNodeTag;
  result = nsEditor::GetTextNodeTag(textNodeTag);
  if (NS_FAILED(result)) { return result; }

  if (textNodeTag == mTag) 
  {
    const nsString stringData;
    nsCOMPtr<nsIDOMText>newTextNode;
    result = doc->CreateTextNode(stringData, getter_AddRefs(newTextNode));
    if (NS_FAILED(result)) return result;
    if (!newTextNode) return NS_ERROR_NULL_POINTER;
    mNewNode = do_QueryInterface(newTextNode);
  }
  else 
  {
    nsCOMPtr<nsIContent> newContent;
    nsCOMPtr<nsIDOMElement>newElement;
 
    //new call to use instead to get proper HTML element, bug# 39919
    result = mEditor->CreateHTMLContent(mTag, getter_AddRefs(newContent));
    newElement = do_QueryInterface(newContent);
    if (NS_FAILED(result)) return result;
    if (!newElement) return NS_ERROR_NULL_POINTER;
    mNewNode = do_QueryInterface(newElement);
    // Try to insert formatting whitespace for the new node:
    mEditor->MarkNodeDirty(mNewNode);
  }
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (mNewNode)), "could not create element.");
  if (!mNewNode) return NS_ERROR_NULL_POINTER;

  if (gNoisy) { printf("  newNode = %p\n", mNewNode.get()); }
  // insert the new node
  nsCOMPtr<nsIDOMNode> resultNode;
  if (CreateElementTxn::eAppend==(PRInt32)mOffsetInParent)
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
      if (NS_FAILED(result)) return result; // note, it's ok for mRefNode to be null.  that means append

      result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
      if (NS_FAILED(result)) return result; 

      // only set selection to insertion point if editor gives permission
      PRBool bAdjustSelection;
      mEditor->ShouldTxnSetSelection(&bAdjustSelection);
      if (bAdjustSelection)
      {
        nsCOMPtr<nsISelection> selection;
        result = mEditor->GetSelection(getter_AddRefs(selection));
        if (NS_FAILED(result)) return result;
        if (!selection) return NS_ERROR_NULL_POINTER;

        PRInt32 offset=0;
        result = nsEditor::GetChildOffset(mNewNode, mParent, offset);
        if (NS_FAILED(result)) return result;

        result = selection->Collapse(mParent, offset+1);
        NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after insert.");
       }
      else
      {
        // do nothing - dom range gravity will adjust selection
      }
    }
  }
  return result;
}

NS_IMETHODIMP CreateElementTxn::UndoTransaction(void)
{
  if (gNoisy) { printf("Undo Create Element, mParent = %p, node = %p\n",
                        mParent.get(), mNewNode.get()); }
  NS_ASSERTION(mEditor && mParent, "bad state");
  if (!mEditor || !mParent) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNode> resultNode;
  nsresult result = mParent->RemoveChild(mNewNode, getter_AddRefs(resultNode));
  return result;
}

NS_IMETHODIMP CreateElementTxn::RedoTransaction(void)
{
  if (gNoisy) { printf("Redo Create Element\n"); }
  NS_ASSERTION(mEditor && mParent, "bad state");
  if (!mEditor || !mParent) return NS_ERROR_NOT_INITIALIZED;

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
  return result;
}

NS_IMETHODIMP CreateElementTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("CreateElementTxn: "));
  aString += mTag;
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
