#include "IPCInterface.h"

#include "mozilla/ipdl/IPDLProtocolInstance.h"
#include "mozilla/dom/IPDL.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipdl/IPDLProtocol.h"

namespace mozilla {
namespace ipdl {
namespace ipc {

/* virtual */ mozilla::ipc::IPCResult
IPCInterface::RecvMessageCommon(
  mozilla::ipc::IProtocol* aActor,
  IPDLSide aSide,
  const nsCString& aProtocolName,
  const uint32_t& aChannelId,
  const nsCString& aMessage,
  const dom::ClonedMessageData& aData,
  nsTArray<dom::ipc::StructuredCloneData>* aReturnData)
{
  // Borrow from the CloneMessageData to the StructuredCloneData.
  dom::ipc::StructuredCloneData data;
  switch (aSide) {
    case IPDLSide::Child:
      data.BorrowFromClonedMessageDataForChild(aData);
      break;
    case IPDLSide::Parent:
      data.BorrowFromClonedMessageDataForParent(aData);
      break;
  }

  // Get the destination instance object to initiate a new context.
  JS::RootingContext* rcx = CycleCollectedJSContext::Get()->RootingCx();
  JS::RootedObject object(rcx);

  auto sidedProtocolName = IPDLProtocol::GetSidedProtocolName(
    aProtocolName, aSide);

  object = GetDestinationObject(sidedProtocolName, aChannelId);
  dom::AutoEntryScript aes(object, "RecvSyncMessageIPDL");

  JSContext* cx = aes.cx();

  // Read the arguments from the StructuredCloneData.
  JS::RootedValue argsVal(cx);

  ErrorResult errRes;
  data.Read(cx, &argsVal, errRes);

  if (NS_WARN_IF(errRes.Failed())) {
    return IPC_FAIL(aActor,
                    "Could not read arguments from StructuredCloneData");
  }

  JS::RootedObject argsObj(cx, &argsVal.toObject());

  uint32_t arrayLength = 0;
  if (!JS_GetArrayLength(cx, argsObj, &arrayLength)) {
    return IPC_FAIL(aActor, "Could not get argument array length");
  }

  // Extract all the arguments.
  JS::AutoValueVector args(cx);
  for (uint32_t i = 0; i < arrayLength; i++) {
    JS::RootedValue val(cx);

    if (!JS_GetElement(cx, argsObj, i, &val)) {
      return IPC_FAIL(aActor,
                      "Could not get argument value from argument array");
    }

    if (!args.append(val)) {
      return IPC_FAIL(aActor,
                      "Could not append argument value to argument vector");
    }
  }

  JS::HandleValueArray argsHandle(args);

  JS::RootedValue retValue(cx);

  // Call the receive handler in the protocol instance.
  if (!mProtocolInstances.GetOrInsert(sidedProtocolName)
         .Get(aChannelId)
         ->RecvMessage(
           cx, aMessage, argsHandle, &retValue)) {
    NS_WARNING("Error in the RecvMessage handler");
    aReturnData->Clear();
    return IPC_OK();
  }

  // Write back the return value.
  dom::ipc::StructuredCloneData retData;
  retData.Write(cx, retValue, errRes);

  if (NS_WARN_IF(errRes.Failed())) {
    return IPC_FAIL(aActor,
                    "Could not write return value to StructuredCloneData");
  }

  aReturnData->AppendElement(std::move(retData));

  return IPC_OK();
}

/* virtual */ JSObject*
IPCInterface::GetDestinationObject(const nsCString& aProtocolName,
                                   const uint32_t& aChannelId)
{
  // Return the instance object corresponding to the protocol name and channel
  // ID.
  return mProtocolInstances.GetOrInsert(aProtocolName)
    .Get(aChannelId)
    ->GetInstanceObject();
}

} // ipc
} // ipdl
} // mozilla
