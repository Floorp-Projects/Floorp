/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MaybeNewPSHEntry.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/PSHistoryParent.h"
#include "mozilla/dom/SHEntryChild.h"
#include "mozilla/dom/SHEntryParent.h"
#include "mozilla/dom/SHistoryChild.h"

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<dom::NewPSHEntry> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    dom::NewPSHEntry&& aEntry) {
    MOZ_RELEASE_ASSERT(aActor->GetSide() == ParentSide, "wrong side!");

    WriteIPDLParam(aMsg, aActor, std::move(aEntry.mEndpoint));
    WriteIPDLParam(aMsg, aActor, aEntry.mSHistoryParent);
    WriteIPDLParam(aMsg, aActor, aEntry.mSharedID);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, dom::NewPSHEntry* aEntry) {
    MOZ_RELEASE_ASSERT(aActor->GetSide() == ChildSide, "wrong side!");

    return ReadIPDLParam(aMsg, aIter, aActor, &aEntry->mEndpoint) &&
           ReadIPDLParam(aMsg, aIter, aActor, &aEntry->mSHistoryChild) &&
           ReadIPDLParam(aMsg, aIter, aActor, &aEntry->mSharedID);
  }
};

/* static */
void IPDLParamTraits<dom::CrossProcessSHEntry*>::Write(
    IPC::Message* aMsg, IProtocol* aActor, dom::CrossProcessSHEntry* aEntry) {
  MOZ_DIAGNOSTIC_ASSERT(aActor->ToplevelProtocol()->GetProtocolId() ==
                        PContentMsgStart);
  if (aActor->GetSide() == ChildSide) {
    WriteIPDLParam(aMsg, aActor,
                   static_cast<dom::PSHEntryChild*>(
                       static_cast<dom::SHEntryChild*>(aEntry)));

    return;
  }

  dom::MaybeNewPSHEntryParent entry(static_cast<dom::PSHEntryParent*>(nullptr));
  if (aEntry) {
    entry = static_cast<dom::LegacySHEntry*>(aEntry)->GetOrCreateActor(
        static_cast<dom::PContentParent*>(aActor->ToplevelProtocol()));
  }

  WriteIPDLParam(aMsg, aActor, std::move(entry));
}

/* static */
bool IPDLParamTraits<dom::CrossProcessSHEntry*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter,
    mozilla::ipc::IProtocol* aActor, RefPtr<dom::CrossProcessSHEntry>* aEntry) {
  if (aActor->GetSide() == ParentSide) {
    dom::PSHEntryParent* actor;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &actor)) {
      aActor->FatalError("Error deserializing MaybeNewPSHEntry");
      return false;
    }

    *aEntry =
        actor ? static_cast<dom::SHEntryParent*>(actor)->GetSHEntry() : nullptr;

    return true;
  }
  dom::MaybeNewPSHEntryChild actor(static_cast<dom::PSHEntryChild*>(nullptr));
  if (!ReadIPDLParam(aMsg, aIter, aActor, &actor)) {
    aActor->FatalError("Error deserializing MaybeNewPSHEntry");
    return false;
  }

  return actor.match(
      [&](dom::PSHEntryChild*& entry) {
        *aEntry = static_cast<dom::SHEntryChild*>(entry);
        return true;
      },
      [&](dom::NewPSHEntry& newEntry) {
        RefPtr<dom::SHEntryChild> entry = new dom::SHEntryChild(
            static_cast<dom::SHistoryChild*>(newEntry.mSHistoryChild),
            newEntry.mSharedID);
        dom::ContentChild::GetSingleton()->BindPSHEntryEndpoint(
            std::move(newEntry.mEndpoint), do_AddRef(entry).take());
        *aEntry = std::move(entry);
        return true;
      });
}

}  // namespace ipc
}  // namespace mozilla
