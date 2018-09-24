#include "PContentChildIPCInterface.h"

#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace ipdl {
namespace ipc {

PContentChildIPCInterface::PContentChildIPCInterface(dom::IPDL* aIPDL,
                                                     dom::ContentChild* aCc)
  : IPCInterface(aIPDL)
  , mCc(aCc)
{
  mCc->RegisterIPDLIPCInterface(this);
}

/* virtual */ mozilla::ipc::IPCResult
PContentChildIPCInterface::RecvMessage(
  const nsCString& aProtocolName,
  const uint32_t& aChannelId,
  const nsCString& aMessage,
  const dom::ClonedMessageData& aData,
  nsTArray<dom::ipc::StructuredCloneData>* aReturnData)
{
  return RecvMessageCommon(mCc,
                           IPDLSide::Child,
                           aProtocolName,
                           aChannelId,
                           aMessage,
                           aData,
                           aReturnData);
}

/* virtual */ RefPtr<IPCInterface::AsyncMessagePromise>
PContentChildIPCInterface::SendAsyncMessage(JSContext* aCx,
                                            const nsCString& aProtocolName,
                                            const uint32_t& aChannelId,
                                            const nsCString& aMessageName,
                                            const InArgs& aArgs)
{
  // Create a new Promise to resolve the async call.
  auto promise = MakeRefPtr<AsyncMessagePromise::Private>(__func__);

  auto promiseHolder = MakeRefPtr<
    dom::DOMMozPromiseRequestHolder<dom::ContentChild::AsyncMessageIPDLPromise>>(
    xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx)));

  // Write arguments to the StructuredCloneData.
  dom::ipc::StructuredCloneData data;
  ErrorResult errRes;

  auto* argsObjPtr = JS_NewArrayObject(aCx, aArgs);

  if (!argsObjPtr) {
    promise->Reject(
      NS_LITERAL_CSTRING("Failed to allocate new array object"),
      __func__);
    return promise;
  }

  JS::RootedObject argsObj(aCx, argsObjPtr);
  JS::RootedValue argsVal(aCx, JS::ObjectValue(*argsObj));

  data.Write(aCx, argsVal, errRes);

  if (NS_WARN_IF(errRes.Failed())) {
    promise->Reject(
      NS_LITERAL_CSTRING("Failed to write arguments to StructuredCloneData"),
      __func__);
    return promise;
  }

  // Wrap the StructuredCloneData in a ClonedMessageData.
  dom::ClonedMessageData cmd;
  if (!data.BuildClonedMessageDataForChild((dom::nsIContentChild*)mCc, cmd)) {
    promise->Reject(
      NS_LITERAL_CSTRING("Failed to write arguments to StructuredCloneData"),
      __func__);
    return promise;
  }

  // Send the actual message through the ContentChild
  mCc->SendAsyncMessageIPDL(aProtocolName, aChannelId, aMessageName, cmd)
    ->Then(promiseHolder->GetParentObject()->EventTargetFor(TaskCategory::Other),
           __func__,
           [promiseHolder, promise](nsTArray<dom::ipc::StructuredCloneData> aRes) {
             promiseHolder->Complete();

             // If the array is empty, it means we got an error.
             if (aRes.IsEmpty()) {
               promise->Reject(NS_LITERAL_CSTRING(
                                 "Error in RecvAsyncMessage in parent process"),
                               __func__);
               return;
             }

             nsIGlobalObject* global = promiseHolder->GetParentObject();
             NS_ENSURE_TRUE_VOID(global);

             dom::AutoEntryScript aes(global, "SendAsyncMessageIPDL");

             JSContext* cx = aes.cx();

             JS::RootedValue ret(cx);
             ErrorResult rv;
             aRes.ElementAt(0).Read(cx, &ret, rv);

             if (NS_WARN_IF(rv.Failed())) {
               promise->Reject(
                 NS_LITERAL_CSTRING(
                   "Failed to read return value from StructuredCloneData"),
                 __func__);
             } else {
               promise->Resolve(ret, __func__);
             }
           },
           [promiseHolder, promise](::mozilla::ipc::ResponseRejectReason aReason) {
             promiseHolder->Complete();
             promise->Reject(nsPrintfCString(
                               "IPC error when calling SendAsyncMessageIPDL, "
                               "see error message above. Reason: %d",
                               (unsigned int)aReason),
                             __func__);
           })->Track(*promiseHolder);

  return promise;
}

/* virtual */ bool
PContentChildIPCInterface::SendSyncMessage(JSContext* aCx,
                                           const nsCString& aProtocolName,
                                           const uint32_t& aChannelId,
                                           const nsCString& aMessageName,
                                           const InArgs& aArgs,
                                           OutObject aRet)
{
  // Write arguments to the StructuredCloneData.
  dom::ipc::StructuredCloneData data;
  ErrorResult errRes;

  auto* argsObjPtr = JS_NewArrayObject(aCx, aArgs);

  if (!argsObjPtr) {
    aRet.setUndefined();
    return false;
  }

  JS::RootedObject argsObj(aCx, argsObjPtr);
  JS::RootedValue argsVal(aCx, JS::ObjectValue(*argsObj));

  data.Write(aCx, argsVal, errRes);

  if (NS_WARN_IF(errRes.Failed())) {
    JS_ReportErrorUTF8(aCx, "Failed to write arguments to StructuredCloneData");
      aRet.setUndefined();
      return false;
  }

  // Wrap the StructuredCloneData in a ClonedMessageData.
  dom::ClonedMessageData cmd;
  if (!data.BuildClonedMessageDataForChild((dom::nsIContentChild*)mCc, cmd)) {
    JS_ReportErrorUTF8(aCx, "Failed to build cloned message data for arguments");
    aRet.setUndefined();
    return false;
  }

  nsTArray<dom::ipc::StructuredCloneData> retData;

  // Send the actual message through the ContentChild
  if (!mCc->SendSyncMessageIPDL(
        aProtocolName, aChannelId, aMessageName, cmd, &retData)) {
    JS_ReportErrorUTF8(aCx, "IPC error when calling SendSyncMessageIPDL");
    aRet.setUndefined();
    return false;
  }

  // If the array is empty, it means we got an error.
  if (retData.IsEmpty()) {
    JS_ReportErrorUTF8(aCx, "Error in RecvSyncMessageIPDL in parent process");
    aRet.setUndefined();
    return false;
  }

  retData.ElementAt(0).Read(aCx, aRet, errRes);

  if (NS_WARN_IF(errRes.Failed())) {
    JS_ReportErrorUTF8(aCx,
                       "Failed to read return value from StructuredCloneData");
    aRet.setUndefined();
    return false;
  }

  return true;
}

} // ipc
} // ipdl
} // mozilla
