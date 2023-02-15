/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteMultipleRangesTransactionBase_h
#define DeleteMultipleRangesTransactionBase_h

#include "DeleteContentTransactionBase.h"
#include "EditAggregateTransaction.h"

#include "EditorForwards.h"

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla {

/**
 * An abstract transaction that removes text or node.
 */
class DeleteMultipleRangesTransaction final : public EditAggregateTransaction {
 public:
  static already_AddRefed<DeleteMultipleRangesTransaction> Create() {
    RefPtr<DeleteMultipleRangesTransaction> transaction =
        new DeleteMultipleRangesTransaction();
    return transaction.forget();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteMultipleRangesTransaction,
                                           EditAggregateTransaction)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(
      DeleteMultipleRangesTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() final;

  void AppendChild(DeleteContentTransactionBase& aTransaction);
  void AppendChild(DeleteRangeTransaction& aTransaction);

  /**
   * Return latest caret point suggestion of child transaction.
   */
  EditorDOMPoint SuggestPointToPutCaret() const;

 protected:
  ~DeleteMultipleRangesTransaction() = default;
};

}  // namespace mozilla

#endif  // #ifndef DeleteMultipleRangesTransaction_h
