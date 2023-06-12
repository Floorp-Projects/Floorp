/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertTextTransaction_h
#define InsertTextTransaction_h

#include "EditTransactionBase.h"  // base class

#include "EditorDOMPoint.h"
#include "EditorForwards.h"

#include "nsCycleCollectionParticipant.h"  // various macros
#include "nsID.h"                          // NS_DECLARE_STATIC_IID_ACCESSOR
#include "nsISupportsImpl.h"               // NS_DECL_ISUPPORTS_INHERITED
#include "nsString.h"                      // nsString members
#include "nscore.h"                        // NS_IMETHOD, nsAString

namespace mozilla {
namespace dom {
class Text;
}  // namespace dom

/**
 * A transaction that inserts text into a content node.
 */
class InsertTextTransaction final : public EditTransactionBase {
 protected:
  InsertTextTransaction(EditorBase& aEditorBase,
                        const nsAString& aStringToInsert,
                        const EditorDOMPointInText& aPointToInsert);

 public:
  /**
   * Creates new InsertTextTransaction instance.  This never returns nullptr.
   *
   * @param aEditorBase     The editor which manages the transaction.
   * @param aPointToInsert  The insertion point.
   * @param aStringToInsert The new string to insert.
   */
  static already_AddRefed<InsertTextTransaction> Create(
      EditorBase& aEditorBase, const nsAString& aStringToInsert,
      const EditorDOMPointInText& aPointToInsert);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertTextTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(InsertTextTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD Merge(nsITransaction* aOtherTransaction, bool* aDidMerge) override;

  /**
   * Return the string data associated with this transaction.
   */
  const nsString& GetData() const { return mStringToInsert; }

  template <typename EditorDOMPointType>
  EditorDOMPointType SuggestPointToPutCaret() const {
    if (NS_WARN_IF(!mTextNode)) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType(mTextNode, mOffset + mStringToInsert.Length());
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const InsertTextTransaction& aTransaction);

 private:
  virtual ~InsertTextTransaction() = default;

  // Return true if aOtherTransaction immediately follows this transaction.
  bool IsSequentialInsert(InsertTextTransaction& aOtherTransaction) const;

  // The Text node to operate upon.
  RefPtr<dom::Text> mTextNode;

  // The offset into mTextNode where the insertion is to take place.
  uint32_t mOffset;

  // The text to insert into mTextNode at mOffset.
  nsString mStringToInsert;

  // The editor, which we'll need to get the selection.
  RefPtr<EditorBase> mEditorBase;
};

}  // namespace mozilla

#endif  // #ifndef InsertTextTransaction_h
