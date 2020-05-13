/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MaybeNewPSHEntry_h
#define mozilla_dom_MaybeNewPSHEntry_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Variant.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla {
namespace dom {

class LegacySHEntry;
class PSHistoryChild;
class PSHistoryParent;
class PSHEntryChild;
class PSHEntryParent;
class PSHistoryChild;
class PSHistoryParent;
class SHEntryChild;

struct NewPSHEntry final {
  mozilla::ipc::ManagedEndpoint<PSHEntryChild> mEndpoint;
  PSHistoryParent* mSHistoryParent;
  PSHistoryChild* mSHistoryChild;
  uint64_t mSharedID;
};

typedef Variant<PSHEntryParent*, NewPSHEntry> MaybeNewPSHEntryParent;
typedef Variant<PSHEntryChild*, NewPSHEntry> MaybeNewPSHEntryChild;

// Any IPDL protocol trying to pass this (as argument or return value) needs to
// be managed by PContent.
class CrossProcessSHEntry {
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  SHEntryChild* ToSHEntryChild();
};

}  // namespace dom

namespace ipc {

template <>
struct IPDLParamTraits<dom::CrossProcessSHEntry*> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    dom::CrossProcessSHEntry* aEntry);

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<dom::CrossProcessSHEntry>* aEntry);
};

}  // namespace ipc
}  // namespace mozilla

#endif /* mozilla_dom_MaybeNewPSHEntry_h */
