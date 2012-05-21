/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteTextTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsISelection.h"
#include "nsSelectionState.h"

#ifdef NS_DEBUG
static bool gNoisy = false;
#endif

DeleteTextTxn::DeleteTextTxn()
: EditTxn()
,mEditor(nsnull)
,mElement()
,mOffset(0)
,mNumCharsToDelete(0)
,mRangeUpdater(nsnull)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DeleteTextTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DeleteTextTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DeleteTextTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP DeleteTextTxn::Init(nsIEditor *aEditor,
                                  nsIDOMCharacterData *aElement,
                                  PRUint32 aOffset,
                                  PRUint32 aNumCharsToDelete,
                                  nsRangeUpdater *aRangeUpdater)
{
  NS_ASSERTION(aEditor&&aElement, "bad arg");
  if (!aEditor || !aElement) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  // do nothing if the node is read-only
  if (!mEditor->IsModifiableNode(mElement)) {
    return NS_ERROR_FAILURE;
  }

  mOffset = aOffset;
  mNumCharsToDelete = aNumCharsToDelete;
  NS_ASSERTION(0!=aNumCharsToDelete, "bad arg, numCharsToDelete");
  PRUint32 count;
  aElement->GetLength(&count);
  NS_ASSERTION(count>=aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  NS_ASSERTION(count>=aOffset+aNumCharsToDelete, "bad arg, numCharsToDelete.  Not enough characters in node");
  mDeletedText.Truncate();
  mRangeUpdater = aRangeUpdater;
  return NS_OK;
}

NS_IMETHODIMP DeleteTextTxn::DoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Do Delete Text\n"); }
#endif

  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }
  // get the text that we're about to delete
  nsresult result = mElement->SubstringData(mOffset, mNumCharsToDelete, mDeletedText);
  NS_ASSERTION(NS_SUCCEEDED(result), "could not get text to delete.");
  result = mElement->DeleteData(mOffset, mNumCharsToDelete);
  NS_ENSURE_SUCCESS(result, result);

  if (mRangeUpdater) 
    mRangeUpdater->SelAdjDeleteText(mElement, mOffset, mNumCharsToDelete);

  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection)
  {
    nsCOMPtr<nsISelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    result = selection->Collapse(mElement, mOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after undo of deletetext.");
  }
  else
  {
    // do nothing - dom range gravity will adjust selection
  }
  return result;
}

//XXX: we may want to store the selection state and restore it properly
//     was it an insertion point or an extended selection?
NS_IMETHODIMP DeleteTextTxn::UndoTransaction(void)
{
#ifdef NS_DEBUG
  if (gNoisy) { printf("Undo Delete Text\n"); }
#endif

  NS_ASSERTION(mEditor && mElement, "bad state");
  if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  return mElement->InsertData(mOffset, mDeletedText);
}

NS_IMETHODIMP DeleteTextTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteTextTxn: ");
  aString += mDeletedText;
  return NS_OK;
}
