/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteMultipleRangesTransaction.h"

#include "DeleteContentTransactionBase.h"
#include "DeleteRangeTransaction.h"
#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "EditTransactionBase.h"

#include "nsDebug.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteMultipleRangesTransaction,
                                   EditAggregateTransaction)

NS_IMPL_ADDREF_INHERITED(DeleteMultipleRangesTransaction,
                         EditAggregateTransaction)
NS_IMPL_RELEASE_INHERITED(DeleteMultipleRangesTransaction,
                          EditAggregateTransaction)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteMultipleRangesTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTransaction)

NS_IMETHODIMP DeleteMultipleRangesTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  nsresult rv = EditAggregateTransaction::DoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditAggregateTransaction::DoTransaction() failed");

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

NS_IMETHODIMP DeleteMultipleRangesTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

  nsresult rv = EditAggregateTransaction::UndoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditAggregateTransaction::UndoTransaction() failed");

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

NS_IMETHODIMP DeleteMultipleRangesTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "Start==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));

  nsresult rv = EditAggregateTransaction::RedoTransaction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditAggregateTransaction::RedoTransaction() failed");

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteMultipleRangesTransaction::%s this={ mName=%s } "
           "End==============================",
           this, __FUNCTION__,
           nsAtomCString(mName ? mName.get() : nsGkAtoms::_empty).get()));
  return rv;
}

void DeleteMultipleRangesTransaction::AppendChild(
    DeleteContentTransactionBase& aTransaction) {
  mChildren.AppendElement(aTransaction);
}

void DeleteMultipleRangesTransaction::AppendChild(
    DeleteRangeTransaction& aTransaction) {
  mChildren.AppendElement(aTransaction);
}

EditorDOMPoint DeleteMultipleRangesTransaction::SuggestPointToPutCaret() const {
  for (const OwningNonNull<EditTransactionBase>& transaction :
       Reversed(mChildren)) {
    if (const DeleteContentTransactionBase* deleteContentTransaction =
            transaction->GetAsDeleteContentTransactionBase()) {
      EditorDOMPoint pointToPutCaret =
          deleteContentTransaction->SuggestPointToPutCaret();
      if (pointToPutCaret.IsSet()) {
        return pointToPutCaret;
      }
      continue;
    }
    if (const DeleteRangeTransaction* deleteRangeTransaction =
            transaction->GetAsDeleteRangeTransaction()) {
      EditorDOMPoint pointToPutCaret =
          deleteRangeTransaction->SuggestPointToPutCaret();
      if (pointToPutCaret.IsSet()) {
        return pointToPutCaret;
      }
      continue;
    }
    MOZ_ASSERT_UNREACHABLE(
        "Child transactions must be DeleteContentTransactionBase or "
        "DeleteRangeTransaction");
  }
  return EditorDOMPoint();
}

}  // namespace mozilla
