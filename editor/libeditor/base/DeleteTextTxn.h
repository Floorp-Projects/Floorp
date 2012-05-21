/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteTextTxn_h__
#define DeleteTextTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsCOMPtr.h"

class nsRangeUpdater;

/**
 * A transaction that removes text from a content node. 
 */
class DeleteTextTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aEditor  the provider of basic editing operations
    * @param aElement the content node to remove text from
    * @param aOffset  the location in aElement to begin the deletion
    * @param aNumCharsToDelete  the number of characters to delete.  Not the number of bytes!
    */
  NS_IMETHOD Init(nsIEditor *aEditor,
                  nsIDOMCharacterData *aElement,
                  PRUint32 aOffset,
                  PRUint32 aNumCharsToDelete,
                  nsRangeUpdater *aRangeUpdater);

  DeleteTextTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteTextTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

  PRUint32 GetOffset() { return mOffset; }

  PRUint32 GetNumCharsToDelete() { return mNumCharsToDelete; }

protected:

  /** the provider of basic editing operations */
  nsIEditor* mEditor;

  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the deletion is to take place */
  PRUint32 mOffset;

  /** the number of characters to delete */
  PRUint32 mNumCharsToDelete;

  /** the text that was deleted */
  nsString mDeletedText;

  /** range updater object */
  nsRangeUpdater *mRangeUpdater;
};

#endif
