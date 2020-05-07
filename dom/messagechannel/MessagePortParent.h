/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortParent_h
#define mozilla_dom_MessagePortParent_h

#include "mozilla/dom/PMessagePortParent.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"

namespace mozilla {
namespace dom {

class MessagePortService;

class MessagePortParent final
    : public PMessagePortParent,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  friend class PMessagePortParent;

 public:
  explicit MessagePortParent(const nsID& aUUID);
  ~MessagePortParent();

  bool Entangle(const nsID& aDestinationUUID, const uint32_t& aSequenceID);

  bool Entangled(nsTArray<MessageData>&& aMessages);

  void Close();
  void CloseAndDelete();

  bool CanSendData() const { return mCanSendData; }

  const nsID& ID() const { return mUUID; }

  static bool ForceClose(const nsID& aUUID, const nsID& aDestinationUUID,
                         const uint32_t& aSequenceID);

 private:
  mozilla::ipc::IPCResult RecvPostMessages(nsTArray<MessageData>&& aMessages);

  mozilla::ipc::IPCResult RecvDisentangle(nsTArray<MessageData>&& aMessages);

  mozilla::ipc::IPCResult RecvStopSendingData();

  mozilla::ipc::IPCResult RecvClose();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MessagePortService> mService;
  const nsID mUUID;
  bool mEntangled;
  bool mCanSendData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MessagePortParent_h
