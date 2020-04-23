/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditTransactionBase_h
#define mozilla_EditTransactionBase_h

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsITransaction.h"
#include "nscore.h"

already_AddRefed<mozilla::EditTransactionBase>
nsITransaction::GetAsEditTransactionBase() {
  RefPtr<mozilla::EditTransactionBase> editTransactionBase;
  return NS_SUCCEEDED(
             GetAsEditTransactionBase(getter_AddRefs(editTransactionBase)))
             ? editTransactionBase.forget()
             : nullptr;
}

namespace mozilla {

class ChangeAttributeTransaction;
class ChangeStyleTransaction;
class CompositionTransaction;
class CreateElementTransaction;
class DeleteNodeTransaction;
class DeleteRangeTransaction;
class DeleteTextTransaction;
class EditAggregateTransaction;
class InsertNodeTransaction;
class InsertTextTransaction;
class JoinNodeTransaction;
class PlaceholderTransaction;
class SplitNodeTransaction;

#define NS_DECL_GETASTRANSACTION_BASE(aClass) \
  virtual aClass* GetAs##aClass();            \
  virtual const aClass* GetAs##aClass() const;

/**
 * Base class for all document editing transactions.
 */
class EditTransactionBase : public nsITransaction {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditTransactionBase, nsITransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction(void) override;
  NS_IMETHOD GetIsTransient(bool* aIsTransient) override;
  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;
  NS_IMETHOD GetAsEditTransactionBase(
      EditTransactionBase** aEditTransactionBase) final {
    MOZ_ASSERT(aEditTransactionBase);
    MOZ_ASSERT(!*aEditTransactionBase);
    *aEditTransactionBase = do_AddRef(this).take();
    return NS_OK;
  }

  NS_DECL_GETASTRANSACTION_BASE(ChangeAttributeTransaction)
  NS_DECL_GETASTRANSACTION_BASE(ChangeStyleTransaction)
  NS_DECL_GETASTRANSACTION_BASE(CompositionTransaction)
  NS_DECL_GETASTRANSACTION_BASE(CreateElementTransaction)
  NS_DECL_GETASTRANSACTION_BASE(DeleteNodeTransaction)
  NS_DECL_GETASTRANSACTION_BASE(DeleteRangeTransaction)
  NS_DECL_GETASTRANSACTION_BASE(DeleteTextTransaction)
  NS_DECL_GETASTRANSACTION_BASE(EditAggregateTransaction)
  NS_DECL_GETASTRANSACTION_BASE(InsertNodeTransaction)
  NS_DECL_GETASTRANSACTION_BASE(InsertTextTransaction)
  NS_DECL_GETASTRANSACTION_BASE(JoinNodeTransaction)
  NS_DECL_GETASTRANSACTION_BASE(PlaceholderTransaction)
  NS_DECL_GETASTRANSACTION_BASE(SplitNodeTransaction)

 protected:
  virtual ~EditTransactionBase() = default;
};

#undef NS_DECL_GETASTRANSACTION_BASE

}  // namespace mozilla

#define NS_DECL_EDITTRANSACTIONBASE                       \
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD DoTransaction() override; \
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD UndoTransaction() override;

#define NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(aClass) \
  aClass* GetAs##aClass() final { return this; }                  \
  const aClass* GetAs##aClass() const final { return this; }

#endif  // #ifndef mozilla_EditTransactionBase_h
