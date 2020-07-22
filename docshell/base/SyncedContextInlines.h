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
nsCString FormatValidationError(IndexSet aFailedFields, const char* prefix) {
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
      if (mModified.contains(idx)) {
        FieldEpoch(idx, aOwner) = epoch;
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
    if (mModified.contains(idx) && FieldEpoch(idx, owner) > aEpoch) {
      mModified -= idx;
    }
  });

  Apply(owner);
  return IPC_OK();
}

template <typename Context>
void Transaction<Context>::Apply(Context* aOwner) {
  EachIndex([&](auto idx) {
    if (mModified.contains(idx)) {
      auto& txnField = mValues.Get(idx);
      auto& ownerField = aOwner->mFields.mValues.Get(idx);
      std::swap(ownerField, txnField);
      aOwner->DidSet(idx);
      aOwner->DidSet(idx, std::move(txnField));
    }
  });
  mModified.clear();
}

template <typename Context>
IndexSet Transaction<Context>::Validate(Context* aOwner,
                                        ContentParent* aSource) {
  IndexSet failedFields;
  // Validate that the set from content is allowed before continuing.
  EachIndex([&](auto idx) {
    if (mModified.contains(idx) &&
        NS_WARN_IF(!aOwner->CanSet(idx, mValues.Get(idx), aSource))) {
      failedFields += idx;
    }
  });
  return failedFields;
}

template <typename Context>
void Transaction<Context>::Write(IPC::Message* aMsg,
                                 mozilla::ipc::IProtocol* aActor) const {
  // Record which field indices will be included, and then write those fields
  // out.
  uint64_t modified = mModified.serialize();
  WriteIPDLParam(aMsg, aActor, modified);
  EachIndex([&](auto idx) {
    if (mModified.contains(idx)) {
      WriteIPDLParam(aMsg, aActor, mValues.Get(idx));
    }
  });
}

template <typename Context>
bool Transaction<Context>::Read(const IPC::Message* aMsg, PickleIterator* aIter,
                                mozilla::ipc::IProtocol* aActor) {
  // Read in which field indices were sent by the remote, followed by the fields
  // identified by those indices.
  uint64_t modified = 0;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &modified)) {
    return false;
  }
  mModified.deserialize(modified);

  bool ok = true;
  EachIndex([&](auto idx) {
    if (ok && mModified.contains(idx)) {
      ok = ReadIPDLParam(aMsg, aIter, aActor, &mValues.Get(idx));
    }
  });
  return ok;
}

template <typename Base, size_t Count>
void FieldValues<Base, Count>::Write(IPC::Message* aMsg,
                                     mozilla::ipc::IProtocol* aActor) const {
  // XXX The this-> qualification is necessary to work around a bug in older gcc
  // versions causing an ICE.
  EachIndex([&](auto idx) { WriteIPDLParam(aMsg, aActor, this->Get(idx)); });
}

template <typename Base, size_t Count>
bool FieldValues<Base, Count>::Read(const IPC::Message* aMsg,
                                    PickleIterator* aIter,
                                    mozilla::ipc::IProtocol* aActor) {
  bool ok = true;
  EachIndex([&](auto idx) {
    if (ok) {
      // XXX The this-> qualification is necessary to work around a bug in older
      // gcc versions causing an ICE.
      ok = ReadIPDLParam(aMsg, aIter, aActor, &this->Get(idx));
    }
  });
  return ok;
}

}  // namespace syncedcontext
}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_SyncedContextInlines_h)
