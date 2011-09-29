/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CreateElementTxn.h"
#include "nsEditor.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsIDOMElement.h"
#include "nsReadableUtils.h"

//included for new nsEditor::CreateContent()
#include "nsIContent.h"

#ifdef NS_DEBUG
static bool gNoisy = false;
#endif

CreateElementTxn::CreateElementTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(CreateElementTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CreateElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNewNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CreateElementTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNewNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRefNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(CreateElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(CreateElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)
NS_IMETHODIMP CreateElementTxn::Init(nsEditor      *aEditor,
                                     const nsAString &aTag,
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


NS_IMETHODIMP CreateElementTxn::DoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy)
  {
    char* nodename = ToNewCString(mTag);
    printf("Do Create Element parent = %p <%s>, offset = %d\n", 
           static_cast<void*>(mParent.get()), nodename, mOffsetInParent);
    nsMemory::Free(nodename);
  }
#endif

  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIContent> newContent;
 
  //new call to use instead to get proper HTML element, bug# 39919
  nsresult result = mEditor->CreateHTMLContent(mTag, getter_AddRefs(newContent));
  NS_ENSURE_SUCCESS(result, result);
  nsCOMPtr<nsIDOMElement>newElement = do_QueryInterface(newContent);
  NS_ENSURE_TRUE(newElement, NS_ERROR_NULL_POINTER);
  mNewNode = do_QueryInterface(newElement);
  // Try to insert formatting whitespace for the new node:
  mEditor->MarkNodeDirty(mNewNode);
 
  NS_ASSERTION(((NS_SUCCEEDED(result)) && (mNewNode)), "could not create element.");
  NS_ENSURE_TRUE(mNewNode, NS_ERROR_NULL_POINTER);

#ifdef NS_DEBUG
  if (gNoisy)
  {
    printf("  newNode = %p\n", static_cast<void*>(mNewNode.get()));
  }
#endif

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
      NS_ENSURE_SUCCESS(result, result); // note, it's ok for mRefNode to be null.  that means append

      result = mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
      NS_ENSURE_SUCCESS(result, result); 

      // only set selection to insertion point if editor gives permission
      bool bAdjustSelection;
      mEditor->ShouldTxnSetSelection(&bAdjustSelection);
      if (bAdjustSelection)
      {
        nsCOMPtr<nsISelection> selection;
        result = mEditor->GetSelection(getter_AddRefs(selection));
        NS_ENSURE_SUCCESS(result, result);
        NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

        PRInt32 offset=0;
        result = nsEditor::GetChildOffset(mNewNode, mParent, offset);
        NS_ENSURE_SUCCESS(result, result);

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
#ifdef NS_DEBUG
  if (gNoisy)
  {
    printf("Undo Create Element, mParent = %p, node = %p\n",
           static_cast<void*>(mParent.get()),
           static_cast<void*>(mNewNode.get()));
  }
#endif

  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->RemoveChild(mNewNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP CreateElementTxn::RedoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Redo Create Element\n"); }
#endif

  NS_ASSERTION(mEditor && mParent, "bad state");
  NS_ENSURE_TRUE(mEditor && mParent, NS_ERROR_NOT_INITIALIZED);

  // first, reset mNewNode so it has no attributes or content
  nsCOMPtr<nsIDOMCharacterData>nodeAsText = do_QueryInterface(mNewNode);
  if (nodeAsText)
  {
    nodeAsText->SetData(EmptyString());
  }
  
  // now, reinsert mNewNode
  nsCOMPtr<nsIDOMNode> resultNode;
  return mParent->InsertBefore(mNewNode, mRefNode, getter_AddRefs(resultNode));
}

NS_IMETHODIMP CreateElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("CreateElementTxn: ");
  aString += mTag;
  return NS_OK;
}

NS_IMETHODIMP CreateElementTxn::GetNewNode(nsIDOMNode **aNewNode)
{
  NS_ENSURE_TRUE(aNewNode, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mNewNode, NS_ERROR_NOT_INITIALIZED);
  *aNewNode = mNewNode;
  NS_ADDREF(*aNewNode);
  return NS_OK;
}
