/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteTextTransaction_h
#define DeleteTextTransaction_h

#include "mozilla/EditTransactionBase.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class EditorBase;
class RangeUpdater;

/**
 * A transaction that removes text from a content node.
 */
class DeleteTextTransaction final : public EditTransactionBase {
 protected:
  DeleteTextTransaction(EditorBase& aEditorBase, dom::Text& aTextNode,
                        uint32_t aOffset, uint32_t aLengthToDelete);

 public:
  /**
   * Creates a delete text transaction to remove given range.  This returns
   * nullptr if it cannot modify the text node.
   *
   * @param aEditorBase         The provider of basic editing operations.
   * @param aTextNode           The content node to remove text from.
   * @param aOffset             The location in aElement to begin the deletion.
   * @param aLenthToDelete      The length to delete.
   */
  static already_AddRefed<DeleteTextTransaction> MaybeCreate(
      EditorBase& aEditorBase, dom::Text& aTextNode, uint32_t aOffset,
      uint32_t aLengthToDelete);

  /**
   * Creates a delete text transaction to remove a previous or next character.
   * Those methods MAY return nullptr.
   *
   * @param aEditorBase         The provider of basic editing operations.
   * @param aTextNode           The content node to remove text from.
   * @param aOffset             The location in aElement to begin the deletion.
   */
  static already_AddRefed<DeleteTextTransaction>
  MaybeCreateForPreviousCharacter(EditorBase& aEditorBase, dom::Text& aTextNode,
                                  uint32_t aOffset);
  static already_AddRefed<DeleteTextTransaction> MaybeCreateForNextCharacter(
      EditorBase& aEditorBase, dom::Text& aTextNode, uint32_t aOffset);

  /**
   * CanDoIt() returns true if there are enough members and can modify the
   * text.  Otherwise, false.
   */
  bool CanDoIt() const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteTextTransaction,
                                           EditTransactionBase)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(DeleteTextTransaction)

  uint32_t Offset() { return mOffset; }

  uint32_t LengthToDelete() { return mLengthToDelete; }

 protected:
  // The provider of basic editing operations.
  RefPtr<EditorBase> mEditorBase;

  // The CharacterData node to operate upon.
  RefPtr<dom::Text> mTextNode;

  // The offset into mTextNode where the deletion is to take place.
  uint32_t mOffset;

  // The length to delete.
  uint32_t mLengthToDelete;

  // The text that was deleted.
  nsString mDeletedText;
};

}  // namespace mozilla

#endif  // #ifndef DeleteTextTransaction_h
