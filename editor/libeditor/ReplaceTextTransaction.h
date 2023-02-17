/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReplaceTextTransaction_h
#define ReplaceTextTransaction_h

#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "EditorForwards.h"
#include "EditTransactionBase.h"

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Text.h"
#include "nsDebug.h"
#include "nsString.h"

namespace mozilla {

class ReplaceTextTransaction final : public EditTransactionBase {
 private:
  ReplaceTextTransaction(EditorBase& aEditorBase,
                         const nsAString& aStringToInsert, dom::Text& aTextNode,
                         uint32_t aStartOffset, uint32_t aLength)
      : mEditorBase(&aEditorBase),
        mTextNode(&aTextNode),
        mStringToInsert(aStringToInsert),
        mOffset(aStartOffset) {
    if (aLength) {
      IgnoredErrorResult ignoredError;
      mTextNode->SubstringData(mOffset, aLength, mStringToBeReplaced,
                               ignoredError);
      NS_WARNING_ASSERTION(
          !ignoredError.Failed(),
          "Failed to initialize ReplaceTextTransaction::mStringToBeReplaced, "
          "but ignored");
    }
  }

  virtual ~ReplaceTextTransaction() = default;

 public:
  static already_AddRefed<ReplaceTextTransaction> Create(
      EditorBase& aEditorBase, const nsAString& aStringToInsert,
      dom::Text& aTextNode, uint32_t aStartOffset, uint32_t aLength) {
    MOZ_ASSERT(aLength > 0, "Use InsertTextTransaction instead");
    MOZ_ASSERT(!aStringToInsert.IsEmpty(), "Use DeleteTextTransaction instead");
    MOZ_ASSERT(aTextNode.Length() >= aStartOffset);
    MOZ_ASSERT(aTextNode.Length() >= aStartOffset + aLength);

    RefPtr<ReplaceTextTransaction> transaction = new ReplaceTextTransaction(
        aEditorBase, aStringToInsert, aTextNode, aStartOffset, aLength);
    return transaction.forget();
  }

  ReplaceTextTransaction() = delete;
  ReplaceTextTransaction(const ReplaceTextTransaction&) = delete;
  ReplaceTextTransaction& operator=(const ReplaceTextTransaction&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ReplaceTextTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(ReplaceTextTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() final;

  template <typename EditorDOMPointType>
  EditorDOMPointType SuggestPointToPutCaret() const {
    if (NS_WARN_IF(!mTextNode)) {
      return EditorDOMPointType();
    }
    return EditorDOMPointType(mTextNode, mOffset + mStringToInsert.Length());
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ReplaceTextTransaction& aTransaction);

 private:
  RefPtr<EditorBase> mEditorBase;
  RefPtr<dom::Text> mTextNode;

  nsString mStringToInsert;
  nsString mStringToBeReplaced;

  uint32_t mOffset;
};

}  // namespace mozilla

#endif  // #ifndef ReplaceTextTransaction_
