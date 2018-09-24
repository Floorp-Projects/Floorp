
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef dom_base_ipdl_bindings_ipcinterface_h
#define dom_base_ipdl_bindings_ipcinterface_h

#include <functional>

#include "jsapi.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla {
namespace dom {
class IPDL;
}

namespace ipdl {
class IPDLProtocolInstance;
enum class IPDLSide : bool;

namespace ipc {

class IPCInterface
{
public:
  // Message call return type.
  typedef JS::MutableHandleValue OutObject;
  // Message call argument list type.
  typedef JS::HandleValueArray InArgs;
  // Async message call promise type.
  typedef MozPromise<JS::Value, nsCString, true> AsyncMessagePromise;

  explicit IPCInterface(dom::IPDL* aIPDL)
    : mIPDL(aIPDL)
  {
  }

  virtual ~IPCInterface() = default;

  // Send an async message through IPC.
  virtual RefPtr<AsyncMessagePromise> SendAsyncMessage(
    JSContext* aCx,
    const nsCString& aProtocolName,
    const uint32_t& aChannelId,
    const nsCString& aMessageName,
    const InArgs& aArgs) = 0;

  // Send a sync message through IPC.
  virtual bool SendSyncMessage(JSContext* aCx,
                               const nsCString& aProtocolName,
                               const uint32_t& aChannelId,
                               const nsCString& aMessageName,
                               const InArgs& aArgs,
                               OutObject aRet) = 0;

  // Send an intr message through IPC.
  virtual bool SendIntrMessage(JSContext* aCx,
                               const nsCString& aProtocolName,
                               const uint32_t& aChannelId,
                               const nsCString& aMessageName,
                               const InArgs& aArgs,
                               OutObject aRet) = 0;

  // Set the recv handler to call when we receive a message.
  virtual void SetIPDLInstance(const uint32_t& aChannelId,
                               const nsCString& aProtocolName,
                               IPDLProtocolInstance* aInstance)
  {
    mProtocolInstances.GetOrInsert(aProtocolName).GetOrInsert(aChannelId) =
      aInstance;
  }

  virtual void RemoveIPDLInstance(const uint32_t& aChannelId,
                                  const nsCString& aProtocolName)
  {
    mProtocolInstances.GetOrInsert(aProtocolName).Remove(aChannelId);
  }

  // Receive a message from IPC. Empty return array means error.
  virtual mozilla::ipc::IPCResult RecvMessage(
    const nsCString& aProtocolName,
    const uint32_t& aChannelId,
    const nsCString& aMessage,
    const dom::ClonedMessageData& aData,
    nsTArray<dom::ipc::StructuredCloneData>* aReturnData) = 0;

protected:
  // Inner common function for receiving a message from either the parent or the
  // child. Empty return array means error.
  virtual mozilla::ipc::IPCResult RecvMessageCommon(
    mozilla::ipc::IProtocol* aActor,
    IPDLSide aSide,
    const nsCString& aProtocolName,
    const uint32_t& aChannelId,
    const nsCString& aMessage,
    const dom::ClonedMessageData& aData,
    nsTArray<dom::ipc::StructuredCloneData>* aReturnData);

  // Retrieves the destination instance object for a protocol/channel pair.
  virtual JSObject* GetDestinationObject(const nsCString& aProtocolName,
                                         const uint32_t& aChannelId);

  dom::IPDL* MOZ_NON_OWNING_REF mIPDL;
  nsDataHashtable<nsCStringHashKey,
                  nsDataHashtable<nsUint32HashKey, IPDLProtocolInstance*>>
    mProtocolInstances;
};

} // ipc
} // ipdl
} // mozilla

#endif // dom_base_ipdl_bindings_ipcinterface_h
