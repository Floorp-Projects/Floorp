/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SyncedContextInlines_h
#define mozilla_dom_SyncedContextInlines_h

#include "mozilla/dom/SyncedContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace dom {
namespace syncedcontext {

template <typename Context>
static nsCString FormatValidationError(IndexSet aFailedFields,
                                       const char* prefix) {
  MOZ_ASSERT(!aFailedFields.isEmpty());
  nsCString error(prefix);
  bool first = true;
  for (auto idx : aFailedFields) {
    if (!first) {
      error.Append(", ");
    }
    first = false;
    error.Append(Context::FieldIndexToName(idx));
  }
  return error;
}

template <typename Context>
nsresult Transaction<Context>::Commit(Context* aOwner) {
  if (NS_WARN_IF(aOwner->IsDiscarded())) {
    return NS_ERROR_FAILURE;
  }

  IndexSet failedFields = Validate(aOwner, nullptr);
  if (!failedFields.isEmpty()) {
    nsCString error = FormatValidationError<Context>(
        failedFields, "CanSet failed for field(s): ");
    MOZ_CRASH_UNSAFE_PRINTF("%s", error.get());
  }

  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();

    // Increment the field epoch for fields affected by this transaction.
    uint64_t epoch = cc->NextBrowsingContextFieldEpoch();
    EachIndex([&](auto idx) {
      if (GetAt(idx, mMaybeFields)) {
        GetAt(idx, GetFieldStorage(aOwner).mEpochs) = epoch;
      }
    });

    // Tell our derived class to send the correct "Commit" IPC message.
    aOwner->SendCommitTransaction(cc, *this, epoch);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

    // Tell our derived class to send the correct "Commit" IPC messages.
    BrowsingContextGroup* group = aOwner->Group();
    group->EachParent([&](ContentParent* aParent) {
      aOwner->SendCommitTransaction(aParent, *this,
                                    aParent->GetBrowsingContextFieldEpoch());
    });
  }

  Apply(aOwner);
  return NS_OK;
}

template <typename Context>
mozilla::ipc::IPCResult Transaction<Context>::CommitFromIPC(
    const MaybeDiscarded<Context>& aOwner, ContentParent* aSource) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  if (aOwner.IsNullOrDiscarded()) {
    MOZ_LOG(Context::GetLog(), LogLevel::Debug,
            ("IPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  Context* owner = aOwner.get();

  // Validate that the set from content is allowed before continuing.
  IndexSet failedFields = Validate(owner, aSource);
  if (!failedFields.isEmpty()) {
    nsCString error = FormatValidationError<Context>(
        failedFields,
        "Invalid Transaction from Child - CanSet failed for field(s): ");
    return IPC_FAIL(aSource, error.get());
  }

  BrowsingContextGroup* group = owner->Group();
  group->EachOtherParent(aSource, [&](ContentParent* aParent) {
    owner->SendCommitTransaction(aParent, *this,
                                 aParent->GetBrowsingContextFieldEpoch());
  });

  Apply(owner);
  return IPC_OK();
}

template <typename Context>
mozilla::ipc::IPCResult Transaction<Context>::CommitFromIPC(
    const MaybeDiscarded<Context>& aOwner, uint64_t aEpoch,
    ContentChild* aSource) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());
  if (aOwner.IsNullOrDiscarded()) {
    MOZ_LOG(Context::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }
  Context* owner = aOwner.get();

  // Clear any fields which have been obsoleted by the epoch.
  EachIndex([&](auto idx) {
    auto& field = GetAt(idx, mMaybeFields);
    if (field && GetAt(idx, GetFieldStorage(owner).mEpochs) > aEpoch) {
      field.reset();
    }
  });

  Apply(owner);
  return IPC_OK();
}

template <typename Context>
void Transaction<Context>::Apply(Context* aOwner) {
  EachIndex([&](auto idx) {
    if (auto& txnField = GetAt(idx, mMaybeFields)) {
      auto& ownerField = GetAt(idx, GetFieldStorage(aOwner).mFields);
      if (ownerField != *txnField) {
        std::swap(ownerField, *txnField);
        aOwner->DidChange(idx, std::move(*txnField));
      }
      txnField.reset();
    }
  });
}

template <typename Context>
IndexSet Transaction<Context>::Validate(Context* aOwner,
                                        ContentParent* aSource) {
  IndexSet failedFields;
  // Validate that the set from content is allowed before continuing.
  EachIndex([&](auto idx) {
    const auto& field = GetAt(idx, mMaybeFields);
    if (field && NS_WARN_IF(!aOwner->CanSet(idx, *field, aSource))) {
      failedFields += idx;
    }
  });
  return failedFields;
}

template <typename Context>
void Transaction<Context>::Write(IPC::Message* aMsg,
                                 mozilla::ipc::IProtocol* aActor) const {
  WriteIPDLParam(aMsg, aActor, mMaybeFields);
}

template <typename Context>
bool Transaction<Context>::Read(const IPC::Message* aMsg, PickleIterator* aIter,
                                mozilla::ipc::IProtocol* aActor) {
  return ReadIPDLParam(aMsg, aIter, aActor, &mMaybeFields);
}

}  // namespace syncedcontext
}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_SyncedContextInlines_h)
