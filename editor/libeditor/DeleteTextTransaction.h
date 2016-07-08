/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteTextTransaction_h
#define DeleteTextTransaction_h

#include "mozilla/EditTransactionBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericDOMDataNode.h"
#include "nsID.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class EditorBase;
class RangeUpdater;

/**
 * A transaction that removes text from a content node.
 */
class DeleteTextTransaction final : public EditTransactionBase
{
public:
  /**
   * Initialize the transaction.
   * @param aEditorBase         The provider of basic editing operations.
   * @param aElement            The content node to remove text from.
   * @param aOffset             The location in aElement to begin the deletion.
   * @param aNumCharsToDelete   The number of characters to delete.  Not the
   *                            number of bytes!
   */
  DeleteTextTransaction(EditorBase& aEditorBase,
                        nsGenericDOMDataNode& aCharData,
                        uint32_t aOffset,
                        uint32_t aNumCharsToDelete,
                        RangeUpdater* aRangeUpdater);

  nsresult Init();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteTextTransaction,
                                           EditTransactionBase)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE

  uint32_t GetOffset() { return mOffset; }

  uint32_t GetNumCharsToDelete() { return mNumCharsToDelete; }

protected:
  // The provider of basic editing operations.
  EditorBase& mEditorBase;

  // The CharacterData node to operate upon.
  RefPtr<nsGenericDOMDataNode> mCharData;

  // The offset into mCharData where the deletion is to take place.
  uint32_t mOffset;

  // The number of characters to delete.
  uint32_t mNumCharsToDelete;

  // The text that was deleted.
  nsString mDeletedText;

  // Range updater object.
  RangeUpdater* mRangeUpdater;
};

} // namespace mozilla

#endif // #ifndef DeleteTextTransaction_h
