/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "IPDLProtocolInstance.h"

#include <cfloat>

#include "IPCInterface.h"
#include "ipdl_ffi_generated.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/Promise.h"
#include "nsJSUtils.h"
#include "wrapper.h"
#include "mozilla/dom/IPDL.h"

namespace mozilla {
namespace ipdl {

NS_IMPL_CYCLE_COLLECTION_CLASS(IPDLProtocolInstance)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IPDLProtocolInstance)
  tmp->mInstanceObject = nullptr;
  tmp->mAsyncReturnOverride = nullptr;
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IPDLProtocolInstance)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IPDLProtocolInstance)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mInstanceObject)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAsyncReturnOverride)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(IPDLProtocolInstance)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IPDLProtocolInstance)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IPDLProtocolInstance)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IPDLProtocolInstance::IPDLProtocolInstance(ipc::IPCInterface* aIPCInterface,
                                           uint32_t aChannelId,
                                           IPDLProtocol* aIPDLProtocol,
                                           JS::HandleObject aInstanceObject)
  : mChannelId(aChannelId)
  , mIPDLProtocol(aIPDLProtocol)
  , mInstanceObject(aInstanceObject)
  , mAsyncReturnOverride(nullptr)
  , mConstructing(true)
{
  mozilla::HoldJSObjects(this);

  SetIPCInterface(aIPCInterface);
}

IPDLProtocolInstance::~IPDLProtocolInstance()
{
  mInstanceObject = nullptr;
  mAsyncReturnOverride = nullptr;
  mozilla::DropJSObjects(this);
}

void
IPDLProtocolInstance::SetIPCInterface(ipc::IPCInterface* aIPCInterface)
{
  mIPCInterface = aIPCInterface;

  if (mIPCInterface) {
    // If we're setting the interface, it means that we're done with the
    // constructor.
    mConstructing = false;

    NS_NAMED_LITERAL_CSTRING(parentSuffix, "Parent");
    NS_NAMED_LITERAL_CSTRING(childSuffix, "Child");

    // Hook up this instance to the IPC interface for receiving messages.
    mIPCInterface->SetIPDLInstance(
      mChannelId,
      mIPDLProtocol->GetProtocolName() +
        (mIPDLProtocol->GetSide() == IPDLSide::Child ? childSuffix
                                                     : parentSuffix),
      this);
  }
}

bool
IPDLProtocolInstance::SendConstructor(JSContext* aCx,
                                      JS::HandleObject aThisObj,
                                      const nsCString& aFuncName,
                                      JS::CallArgs aArgs)
{
  // Make sure the instance can send stuff.
  if (!CheckIsAvailable(aCx)) {
    return false;
  }

  NS_NAMED_LITERAL_CSTRING(sendPrefix, "send");
  NS_NAMED_LITERAL_CSTRING(constructorSuffix, "Constructor");

  nsAutoCString allocName(NS_LITERAL_CSTRING("alloc"));
  allocName.Append(Substring(aFuncName,
                             sendPrefix.Length(),
                             aFuncName.Length() - sendPrefix.Length() -
                               constructorSuffix.Length()));

  JS::HandleValueArray argsArray(aArgs);

  // Construct the sub protocol by calling our alloc function.
  JS::RootedValue constructedValue(aCx);
  if (!JS_CallFunctionName(
        aCx, aThisObj, allocName.get(), argsArray, &constructedValue)) {
    aArgs.rval().setUndefined();
    return false;
  }

  JS::RootedObject constructedObject(aCx, &constructedValue.toObject());

  // Add the IPC interface to the newly constructed object.
  auto instance = static_cast<IPDLProtocolInstance*>(JS_GetPrivate(constructedObject.get()));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Couldn't get protocol instance object from private date field");
    return false;
  }

  instance->SetIPCInterface(mIPCInterface);

  // Create new call arguments from the current call arguments to be passed to
  // SendMessage.
  JS::AutoValueVector newArgsVec(aCx);
  if (!newArgsVec.initCapacity(aArgs.length() +
                               3)) { // this + callee + channelID
    JS_ReportErrorUTF8(
      aCx, "Could not initialize new argument vector for constructor");
    aArgs.rval().setUndefined();
    return false;
  }

  JS::CallArgs newArgs =
    JS::CallArgsFromVp(aArgs.length() + 1, newArgsVec.begin());
  newArgs.setThis(aArgs.thisv().get());
  newArgs.setCallee(aArgs.calleev().get());

  // We add the channel ID as the first argument.
  JS::RootedValue channelID(aCx, JS::NumberValue(instance->ChannelId()));
  newArgs[0].set(channelID);
  // We copy all the other arguments.
  for (unsigned int i = 0; i < aArgs.length(); i++) {
    newArgs[i + 1].set(aArgs[i]);
  }

  // We send the construction message, overriding the return result
  // with the constructed object.
  bool res = SendMessage(aCx, aFuncName, newArgs, constructedObject);

  aArgs.rval().set(newArgs.rval());

  return res;
}

bool
IPDLProtocolInstance::SendDelete(JSContext* aCx,
                                 const nsCString& aFuncName,
                                 JS::CallArgs aArgs)
{
  // Make sure the instance can send stuff.
  if (!CheckIsAvailable(aCx)) {
    return false;
  }

  bool res = SendMessage(aCx, aFuncName, aArgs);
  return res;
}

bool
IPDLProtocolInstance::SendMessage(JSContext* aCx,
                                  const nsCString& aFuncName,
                                  JS::CallArgs aArgs,
                                  JS::HandleObject aReturnOverride)
{
  // Make sure the instance can send stuff.
  if (!CheckIsAvailable(aCx)) {
    return false;
  }

  // Get the corresponding message AST struct from our lookup table.
  auto message = mIPDLProtocol->GetMessageDecl(aFuncName);
  // Sanity check. Usually the JS engine will trigger a type error saying it
  // doesn't find the function before we reach this stage. If this assertion
  // is triggered, it means there is a flaw in the way we add messages.
  MOZ_ASSERT(message);

  auto& inParams = message->in_params;

  auto expectedArgCount = inParams.Length();
  // Since our arg count reflects a well defined protocol, should we
  // require strict arg count?
  if (!aArgs.requireAtLeast(aCx, aFuncName.get(), expectedArgCount)) {
    return false;
  }


  // Check that every param and args have matching types.
  for (size_t i = 0; i < inParams.Length(); i++) {
    if (!mIPDLProtocol->CheckParamTypeSpec(
          aCx, aArgs.get(i), inParams.ElementAt(i))) {
      JS_ReportErrorUTF8(
        aCx,
        "Type mismatch on %zuth argument of `%s`: expected %s, got %s",
        i,
        aFuncName.get(),
        IPDLProtocol::JoinQualifiedId(inParams.ElementAt(i).type_spec.spec)
          .get(),
        InformalValueTypeName(aArgs.get(i)));
      aArgs.rval().setUndefined();
      return false;
    }
  }

  NS_NAMED_LITERAL_CSTRING(sendPrefix, "send");

  nsAutoCString messageName(
    Substring(aFuncName,
              sendPrefix.Length(),
              aFuncName.Length() - sendPrefix.Length())); // trim leading `send`

  // Prepare the argument array for sending to the IPC interface.
  auto argArray = JS::HandleValueArray(aArgs);

  // Send the message to the underlying IPC interface depending on the send
  // semantics.
  JS::RootedValue retVal(aCx);
  bool ret;
  switch (message->send_semantics) {
    case ffi::SendSemantics::Async:
      retVal = SendAsyncMessage(aCx, messageName, argArray, aReturnOverride);
      ret = true;
      break;

    case ffi::SendSemantics::Sync:
      ret = SendSyncMessage(aCx, messageName, argArray, &retVal);
      if (aReturnOverride.get()) {
        retVal.setObject(*aReturnOverride);
      }
      break;

    case ffi::SendSemantics::Intr:
      ret = SendIntrMessage(aCx, messageName, argArray, &retVal);
      if (aReturnOverride.get()) {
        retVal.setObject(*aReturnOverride);
      }
      break;
  }

  aArgs.rval().set(retVal);

  return ret;
}

JS::Value
IPDLProtocolInstance::SendAsyncMessage(JSContext* aCx,
                                       const nsCString& aFuncName,
                                       const JS::HandleValueArray& aArgArray,
                                       JS::HandleObject aReturnOverride)
{
  // Create a JS/DOM promise to return.
  // It will resolve to the result of the message call.
  ErrorResult aRv;
  RefPtr<dom::Promise> outer =
    dom::Promise::Create(mIPDLProtocol->GetGlobal(), aRv);
  if (aRv.Failed()) {
    return JS::UndefinedValue();
  }

  // This wraps the AsyncMessagePromise that we use for the actual IPC/C++
  // message so that if the global object gets disconnected, that promise also
  // gets disconnected and is not left dangling.
  auto promiseHolder = MakeRefPtr<
    dom::DOMMozPromiseRequestHolder<ipc::IPCInterface::AsyncMessagePromise>>(
    mIPDLProtocol->GetGlobal());

  bool hasReturnOverride = (aReturnOverride.get());
  mAsyncReturnOverride = aReturnOverride;

  // Actually send the message to the IPC interface.
  // If the C++ promise succeeds, then we resolve the JS promise.
  // If it fails, we reject the JS promise.
  // We also make sure that the promise holder tracks our promise.
  mIPCInterface
    ->SendAsyncMessage(
      aCx, mIPDLProtocol->GetProtocolName(), mChannelId, aFuncName, aArgArray)
    ->Then(mIPDLProtocol->GetGlobal()->EventTargetFor(TaskCategory::Other),
           __func__,
           [&asyncReturnOverride = mAsyncReturnOverride, promiseHolder, outer, hasReturnOverride](
             const JS::Value& aResult) {
             // Tell the promise holder that our promise is resolved.
             promiseHolder->Complete();

             // Note, you can access the holder's bound global in
             // your reaction handler.  Its mostly likely set if
             // the handler fires, but you still must check for
             // its existence since something could disconnect
             // the global between when the MozPromise reaction
             // runnable is queued and when it actually runs.
             nsIGlobalObject* global = promiseHolder->GetParentObject();
             NS_ENSURE_TRUE_VOID(global);

             // Resolve the JS promise with our result.
             // If we have an override return value, return it instead.
             if (hasReturnOverride) {
               outer->MaybeResolve(JS::ObjectValue(*asyncReturnOverride));
               asyncReturnOverride = nullptr;
             } else {
               outer->MaybeResolve(aResult);
             }
           },
           [promiseHolder, outer, aCx](nsCString aRv) {
             // Tell the promise holder that our promise is resolved.
             promiseHolder->Complete();

             // Reject the JS promise with our error message.
             JS::RootedValue retVal(
               aCx, JS::StringValue(JS_NewStringCopyZ(aCx, aRv.get())));
             outer->MaybeReject(aCx, retVal);
           })
    ->Track(*promiseHolder);

  // Get the JSObject corresponding to the JS promise.
  JS::RootedObject retObj(aCx, outer->PromiseObj());

  return JS::ObjectValue(*retObj);
}

bool
IPDLProtocolInstance::SendSyncMessage(JSContext* aCx,
                                      const nsCString& aFuncName,
                                      const JS::HandleValueArray& aArgArray,
                                      JS::MutableHandleValue aRet)
{
  // Simply call the underlying IPC interface directly.
  return mIPCInterface->SendSyncMessage(aCx,
                                        mIPDLProtocol->GetProtocolName(),
                                        mChannelId,
                                        aFuncName,
                                        aArgArray,
                                        aRet);
}

bool
IPDLProtocolInstance::SendIntrMessage(JSContext* aCx,
                                      const nsCString& aFuncName,
                                      const JS::HandleValueArray& aArgArray,
                                      JS::MutableHandleValue aRet)
{
  // Simply call the underlying IPC interface directly.
  return mIPCInterface->SendIntrMessage(aCx,
                                        mIPDLProtocol->GetProtocolName(),
                                        mChannelId,
                                        aFuncName,
                                        aArgArray,
                                        aRet);
}

bool
IPDLProtocolInstance::RecvMessage(JSContext* aCx,
                                  const nsCString& aMessageName,
                                  const JS::HandleValueArray& aArgArray,
                                  JS::MutableHandleValue aRet)
{
  // Make sure the instance can receive stuff.
  if (!CheckIsAvailable(aCx)) {
    return false;
  }

  const nsAutoCString& funcName = NS_LITERAL_CSTRING("recv") + aMessageName;
  JS::RootedValue prop(aCx);

  NS_NAMED_LITERAL_CSTRING(constructorSuffix, "Constructor");

  // Check if the message name ends with the constructor suffix.
  if (aMessageName.RFind(
        constructorSuffix.get(), false, -1, constructorSuffix.Length()) != kNotFound) {
    nsAutoCString protocolName(Substring(
      aMessageName, 0, aMessageName.Length() - constructorSuffix.Length()));
    nsAutoCString allocName(protocolName);
    allocName.InsertLiteral("alloc", 0);

    // Allocate the new sub protocol instance by calling the alloc method.
    JS::RootedValue allocatedProtocolValue(aCx);
    JS::RootedObject objectInstance(aCx, mInstanceObject);
    JS_CallFunctionName(
      aCx,
      objectInstance,
      allocName.get(),
      // Ignore the leading channel ID.
      JS::HandleValueArray::subarray(aArgArray, 1, aArgArray.length() - 1),
      &allocatedProtocolValue);

    JS::RootedObject allocatedProtocol(aCx, &allocatedProtocolValue.toObject());

    auto instance = static_cast<IPDLProtocolInstance*>(JS_GetPrivate(allocatedProtocol.get()));

    if (!instance) {
      JS_ReportErrorUTF8(aCx, "Couldn't get protocol instance object from private date field");
      return false;
    }

    // Set the IPC interface of the constructed protocol object.
    instance->SetIPCInterface(mIPCInterface);

    nsAutoCString constructorName(protocolName);
    constructorName.InsertLiteral("recv", 0);
    constructorName.AppendLiteral("Constructor");

    // Call the recvConstructor function
    JS::RootedValue unusedResult(aCx);
    JS_CallFunctionName(aCx,
                        objectInstance,
                        constructorName.get(),
                        JS::HandleValueArray(allocatedProtocolValue),
                        &unusedResult);

    return true;
  }

  // Otherwise if we have a regular message, just call it.
  JS::RootedObject objInstance(aCx, mInstanceObject);
  if (!JS_GetProperty(aCx, objInstance, funcName.get(), &prop)) {
    aRet.setUndefined();
    return false;
  }
  if (!JS_CallFunctionValue(aCx, objInstance, prop, aArgArray, aRet)) {
    aRet.setUndefined();
    return false;
  }

  // If we receive a delete, mark our protocol instance as deleted.
  if (aMessageName.Equals("__delete__")) {
    aRet.setUndefined();

    JS_SetPrivate(mInstanceObject, nullptr);
    mIPCInterface->RemoveIPDLInstance(mChannelId,
                                      mIPDLProtocol->GetProtocolName());
    // ALWAYS PUT THIS LAST, DO NOT ACCESS `this` AFTERWARDS
    mIPDLProtocol->RemoveInstance(this);
  }

  return true;
}

JSObject*
IPDLProtocolInstance::GetInstanceObject()
{
  return mInstanceObject;
}

} // ipdl
} // mozilla
