
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef dom_base_ipdl_bindings_pcontentparentipcinterface_h
#define dom_base_ipdl_bindings_pcontentparentipcinterface_h

#include "IPCInterface.h"

namespace mozilla {
namespace dom {
class IPDL;
class ContentParent;
namespace ipc {
class StructuredCloneData;
}
}

namespace ipdl {
namespace ipc {

class PContentParentIPCInterface : public IPCInterface
{
public:
  explicit PContentParentIPCInterface(dom::IPDL* aIPDL, dom::ContentParent* aCp);
  virtual ~PContentParentIPCInterface() = default;

  // Receive a message from IPC.
  virtual mozilla::ipc::IPCResult RecvMessage(
    const nsCString& aProtocolName,
    const uint32_t& aChannelId,
    const nsCString& aMessage,
    const dom::ClonedMessageData& aData,
    nsTArray<dom::ipc::StructuredCloneData>* aReturnData) override;

  // Send an async message through IPC.
  virtual RefPtr<AsyncMessagePromise> SendAsyncMessage(
    JSContext* aCx,
    const nsCString& aProtocolName,
    const uint32_t& aChannelId,
    const nsCString& aMessageName,
    const InArgs& aArgs) override;

  // Send a sync message through IPC.
  virtual bool SendSyncMessage(JSContext* aCx,
                               const nsCString& aProtocolName,
                               const uint32_t& aChannelId,
                               const nsCString& aMessageName,
                               const InArgs& aArgs,
                               OutObject aRet) override
  {
    MOZ_CRASH("Unimplemented");
    aRet.setUndefined();
    return false;
  }

  // Send an intr message through IPC.
  virtual bool SendIntrMessage(JSContext* aCx,
                               const nsCString& aProtocolName,
                               const uint32_t& aChannelId,
                               const nsCString& aMessageName,
                               const InArgs& aArgs,
                               OutObject aRet) override
  {
    MOZ_CRASH("Unimplemented");
    aRet.setUndefined();
    return false;
  }

protected:
  dom::ContentParent* mCp;
};

} // ipc
} // ipdl
} // mozilla

#endif // dom_base_ipdl_bindings_pcontentparentipcinterface_h
