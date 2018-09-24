/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef dom_base_ipdl_bindings_IPDLProtocolInstance_h
#define dom_base_ipdl_bindings_IPDLProtocolInstance_h

#include "jsapi.h"
#include "nsStringFwd.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {
class IPDL;
} // dom

namespace ipdl {
class IPDLProtocol;

namespace ipc {
class IPCInterface;
} // ipc

namespace ffi {
struct AST;
struct Union;
struct Struct;
struct NamedProtocol;
struct MessageDecl;
struct TranslationUnit;
struct Param;
struct TypeSpec;
struct QualifiedId;
struct Namespace;
} // ffi

/**
 * Represents an IPDL protocol object instance, either created automatically if the protocol is top level,
 * or allocated from the parent protocol.
 */
class IPDLProtocolInstance : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IPDLProtocolInstance)

  IPDLProtocolInstance(ipc::IPCInterface* aIPCInterface,
                       uint32_t aChannelId,
                       IPDLProtocol* aIPDLProtocol,
                       JS::HandleObject aInstanceObject);

  // Returns the JSObject corresponding to this instance.
  JSObject* GetInstanceObject();

  // Returns the channel ID of this instance.
  uint32_t ChannelId() { return mChannelId; };

  // Represents the IPDL sendSubProtocolConstructor functions.
  bool SendConstructor(JSContext* aCx,
                       JS::HandleObject aThisObj,
                       const nsCString& aFuncName,
                       JS::CallArgs aArgs);
  // Represents the IPDL send__delete__ function.
  bool SendDelete(JSContext* aCx, const nsCString& aFuncName, JS::CallArgs aArgs);

  // Performs param/arg type checking and calls a sub-function depending on
  // the message send semantics.
  bool SendMessage(JSContext* aCx,
                   const nsCString& aFuncName,
                   JS::CallArgs aArgs,
                   JS::HandleObject aReturnOverride = nullptr);

  // Sends an async message by calling the underlying IPC interface, returning
  // a promise to the return value object.
  JS::Value SendAsyncMessage(JSContext* aCx,
                             const nsCString& aFuncName,
                             const JS::HandleValueArray& aArgArray,
                             JS::HandleObject aReturnOverride = nullptr);

  // Sends a sync message by calling the underlying IPC interface, returning
  // the return value object.
  bool SendSyncMessage(JSContext* aCx,
                       const nsCString& aFuncName,
                       const JS::HandleValueArray& aArgArray,
                       JS::MutableHandleValue aRet);

  // Sends an intr message by calling the underlying IPC interface, returning
  // the return value object.
  bool SendIntrMessage(JSContext* aCx,
                       const nsCString& aFuncName,
                       const JS::HandleValueArray& aArgArray,
                       JS::MutableHandleValue aRet);

  // Receives a message from the underlying IPC interface, returning the result
  // if any.
  bool RecvMessage(JSContext* aCx,
                   const nsCString& aMessageName,
                   const JS::HandleValueArray& aArgArray,
                   JS::MutableHandleValue aRet);

  // Sets the IPC interface of the IPDL instance.
  void SetIPCInterface(ipc::IPCInterface* aIPCInterface);

protected:
  virtual ~IPDLProtocolInstance();

  // Whether this protocol is being constructed (we're in the constructor of the instance).
  bool IsConstructing() { return mConstructing; }

  bool CheckIsAvailable(JSContext* aCx)
  {
    if (IsConstructing()) {
      JS_ReportErrorUTF8(aCx,
                        "Protocol is constructing, cannot be used yet.");
      return false;
    }

    return true;
  }

  ipc::IPCInterface* MOZ_NON_OWNING_REF mIPCInterface;
  uint32_t mChannelId;
  IPDLProtocol* MOZ_NON_OWNING_REF mIPDLProtocol;
  JS::Heap<JSObject*> mInstanceObject;
  JS::Heap<JSObject*> mAsyncReturnOverride;
  bool mConstructing;
};

} // ipdl
} // mozilla

#endif // dom_base_ipdl_bindings_IPDLProtocolInstance_h
