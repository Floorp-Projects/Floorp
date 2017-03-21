/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StylesheetTransactions_h
#define StylesheetTransactions_h

#include "mozilla/EditorBase.h"         // mEditor
#include "mozilla/EditTransactionBase.h" // for EditTransactionBase, etc.
#include "mozilla/StyleSheet.h"   // for mozilla::StyleSheet
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"                       // for REFNSIID
#include "nscore.h"                     // for NS_IMETHOD

namespace mozilla {

class AddStyleSheetTransaction final : public EditTransactionBase
{
public:
  /**
   * @param aEditor     The object providing core editing operations
   * @param aSheet      The stylesheet to add
    */
  AddStyleSheetTransaction(EditorBase& aEditor, StyleSheet* aSheet);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AddStyleSheetTransaction,
                                           EditTransactionBase)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE

protected:
  // The editor that created this transaction.
  RefPtr<EditorBase> mEditorBase;
  // The style sheet to add.
  RefPtr<mozilla::StyleSheet> mSheet;
};


class RemoveStyleSheetTransaction final : public EditTransactionBase
{
public:
  /**
   * @param aEditor     The object providing core editing operations.
   * @param aSheet      The stylesheet to remove.
   */
  RemoveStyleSheetTransaction(EditorBase& aEditor, StyleSheet* aSheet);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RemoveStyleSheetTransaction,
                                           EditTransactionBase)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTRANSACTIONBASE

protected:
  // The editor that created this transaction.
  RefPtr<EditorBase> mEditorBase;
  // The style sheet to remove.
  RefPtr<StyleSheet> mSheet;

};

} // namespace mozilla

#endif // #ifndef StylesheetTransactions_h
