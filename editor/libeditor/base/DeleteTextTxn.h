/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteTextTxn_h__
#define DeleteTextTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIDOMCharacterData.h"
#include "nsString.h"
#include "nscore.h"

class nsEditor;
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
  NS_IMETHOD Init(nsEditor* aEditor,
                  nsIDOMCharacterData* aCharData,
                  uint32_t aOffset,
                  uint32_t aNumCharsToDelete,
                  nsRangeUpdater* aRangeUpdater);

  DeleteTextTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteTextTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

  uint32_t GetOffset() { return mOffset; }

  uint32_t GetNumCharsToDelete() { return mNumCharsToDelete; }

protected:

  /** the provider of basic editing operations */
  nsEditor* mEditor;

  /** the CharacterData node to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mCharData;

  /** the offset into mCharData where the deletion is to take place */
  uint32_t mOffset;

  /** the number of characters to delete */
  uint32_t mNumCharsToDelete;

  /** the text that was deleted */
  nsString mDeletedText;

  /** range updater object */
  nsRangeUpdater* mRangeUpdater;
};

#endif
