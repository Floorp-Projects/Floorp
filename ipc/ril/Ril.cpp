/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/Ril.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ipc/RilSocket.h"
#include "mozilla/ipc/RilSocketConsumer.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.
#include "RilConnector.h"

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define CHROMIUM_LOG(args...)  printf(args);
#endif

namespace mozilla {
namespace ipc {

USING_WORKERS_NAMESPACE;
using namespace JS;

class RilConsumer;

static const char RIL_SOCKET_NAME[] = "/dev/socket/rilproxy";

static nsTArray<UniquePtr<RilConsumer>> sRilConsumers;

//
// RilConsumer
//

class RilConsumer final : public RilSocketConsumer
{
public:
  RilConsumer();

  nsresult ConnectWorkerToRIL(JSContext* aCx);

  nsresult Register(unsigned long aClientId,
                    WorkerCrossThreadDispatcher* aDispatcher);
  void Unregister();

  // Methods for |RilSocketConsumer|
  //

  void ReceiveSocketData(JSContext* aCx,
                         int aIndex,
                         UniquePtr<UnixSocketBuffer>& aBuffer) override;
  void OnConnectSuccess(int aIndex) override;
  void OnConnectError(int aIndex) override;
  void OnDisconnect(int aIndex) override;

protected:
  static bool PostRILMessage(JSContext* aCx, unsigned aArgc, Value* aVp);

  nsresult Send(JSContext* aCx, const CallArgs& aArgs);
  nsresult Receive(JSContext* aCx,
                   uint32_t aClientId,
                   const UnixSocketBuffer* aBuffer);
  void Close();

private:
  RefPtr<RilSocket> mSocket;
  nsCString mAddress;
  bool mShutdown;
};

RilConsumer::RilConsumer()
  : mShutdown(false)
{ }

nsresult
RilConsumer::ConnectWorkerToRIL(JSContext* aCx)
{
  // Set up the postRILMessage on the function for worker -> RIL thread
  // communication.
  Rooted<JSObject*> workerGlobal(aCx, CurrentGlobalOrNull(aCx));

  // Check whether |postRILMessage| has been defined.  No one but this class
  // should ever define |postRILMessage| in a RIL worker.
  Rooted<Value> val(aCx);
  if (!JS_GetProperty(aCx, workerGlobal, "postRILMessage", &val)) {
    // Just returning failure here will cause the exception on the JSContext to
    // be reported as needed.
    return NS_ERROR_FAILURE;
  }

  // Make sure that |postRILMessage| is a function.
  if (JSTYPE_FUNCTION == JS_TypeOfValue(aCx, val)) {
    return NS_OK;
  }

  JSFunction* postRILMessage = JS_DefineFunction(aCx, workerGlobal,
                                                 "postRILMessage",
                                                 PostRILMessage, 2, 0);
  if (NS_WARN_IF(!postRILMessage)) {
    // Just returning failure here will cause the exception on the JSContext to
    // be reported as needed.
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
RilConsumer::Register(unsigned long aClientId,
                      WorkerCrossThreadDispatcher* aDispatcher)
{
  // Only append client id after RIL_SOCKET_NAME when it's not connected to
  // the first(0) rilproxy for compatibility.
  if (!aClientId) {
    mAddress = RIL_SOCKET_NAME;
  } else {
    struct sockaddr_un addr_un;
    snprintf(addr_un.sun_path, sizeof addr_un.sun_path, "%s%lu",
             RIL_SOCKET_NAME, aClientId);
    mAddress = addr_un.sun_path;
  }

  mSocket = new RilSocket(aDispatcher, this, aClientId);

  nsresult rv = mSocket->Connect(new RilConnector(mAddress, aClientId));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void
RilConsumer::Unregister()
{
  mShutdown = true;
  Close();
}

bool
RilConsumer::PostRILMessage(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (args.length() != 2) {
    JS_ReportError(aCx, "Expecting two arguments with the RIL message");
    return false;
  }

  int clientId = args[0].toInt32();

  if ((ssize_t)sRilConsumers.Length() <= clientId || !sRilConsumers[clientId]) {
    // Probably shutting down.
    return true;
  }

  nsresult rv = sRilConsumers[clientId]->Send(aCx, args);
  if (NS_FAILED(rv)) {
    return false;
  }

  return true;
}

nsresult
RilConsumer::Send(JSContext* aCx, const CallArgs& aArgs)
{
  if (NS_WARN_IF(!mSocket) ||
      NS_WARN_IF(mSocket->GetConnectionStatus() == SOCKET_DISCONNECTED)) {
    // Probably shutting down.
    return NS_OK;
  }

  UniquePtr<UnixSocketRawData> raw;

  Value v = aArgs[1];

  if (v.isString()) {
    JSAutoByteString abs;
    Rooted<JSString*> str(aCx, v.toString());
    if (!abs.encodeUtf8(aCx, str)) {
      return NS_ERROR_FAILURE;
    }

    raw = MakeUnique<UnixSocketRawData>(abs.ptr(), abs.length());
  } else if (!v.isPrimitive()) {
    JSObject* obj = v.toObjectOrNull();
    if (!JS_IsTypedArrayObject(obj)) {
      JS_ReportError(aCx, "Object passed in wasn't a typed array");
      return NS_ERROR_FAILURE;
    }

    uint32_t type = JS_GetArrayBufferViewType(obj);
    if (type != js::Scalar::Int8 &&
        type != js::Scalar::Uint8 &&
        type != js::Scalar::Uint8Clamped) {
      JS_ReportError(aCx, "Typed array data is not octets");
      return NS_ERROR_FAILURE;
    }

    size_t size = JS_GetTypedArrayByteLength(obj);
    bool isShared;
    void* data;
    {
      AutoCheckCannotGC nogc;
      data = JS_GetArrayBufferViewData(obj, &isShared, nogc);
    }
    if (isShared) {
      JS_ReportError(
        aCx, "Incorrect argument.  Shared memory not supported");
      return NS_ERROR_FAILURE;
    }
    raw = MakeUnique<UnixSocketRawData>(data, size);
  } else {
    JS_ReportError(
      aCx, "Incorrect argument. Expecting a string or a typed array");
    return NS_ERROR_FAILURE;
  }

  if (!raw) {
    JS_ReportError(aCx, "Unable to post to RIL");
    return NS_ERROR_FAILURE;
  }

  mSocket->SendSocketData(raw.release());

  return NS_OK;
}

nsresult
RilConsumer::Receive(JSContext* aCx,
                     uint32_t aClientId,
                     const UnixSocketBuffer* aBuffer)
{
  MOZ_ASSERT(aBuffer);

  Rooted<JSObject*> obj(aCx, CurrentGlobalOrNull(aCx));

  Rooted<JSObject*> array(aCx, JS_NewUint8Array(aCx, aBuffer->GetSize()));
  if (NS_WARN_IF(!array)) {
    // Just suppress the exception, since our callers don't have a way to
    // indicate they failed.
    JS_ClearPendingException(aCx);
    return NS_ERROR_FAILURE;
  }
  {
    AutoCheckCannotGC nogc;
    bool isShared;
    memcpy(JS_GetArrayBufferViewData(array, &isShared, nogc),
           aBuffer->GetData(), aBuffer->GetSize());
    MOZ_ASSERT(!isShared);      // Array was constructed above.
  }

  AutoValueArray<2> args(aCx);
  args[0].setNumber(aClientId);
  args[1].setObject(*array);

  Rooted<Value> rval(aCx);
  JS_CallFunctionName(aCx, obj, "onRILMessage", args, &rval);
  // Just suppress the exception, since our callers don't have a way to
  // indicate they failed.
  JS_ClearPendingException(aCx);

  return NS_OK;
}

void
RilConsumer::Close()
{
  if (mSocket) {
    mSocket->Close();
    mSocket = nullptr;
  }
}

// |RilSocketConnector|

void
RilConsumer::ReceiveSocketData(JSContext* aCx,
                               int aIndex,
                               UniquePtr<UnixSocketBuffer>& aBuffer)
{
  Receive(aCx, (uint32_t)aIndex, aBuffer.get());
}

void
RilConsumer::OnConnectSuccess(int aIndex)
{
  // Nothing to do here.
  CHROMIUM_LOG("RIL[%d]: %s\n", aIndex, __FUNCTION__);
}

void
RilConsumer::OnConnectError(int aIndex)
{
  CHROMIUM_LOG("RIL[%d]: %s\n", aIndex, __FUNCTION__);
  Close();
}

void
RilConsumer::OnDisconnect(int aIndex)
{
  CHROMIUM_LOG("RIL[%d]: %s\n", aIndex, __FUNCTION__);
  if (mShutdown) {
    return;
  }
  mSocket->Connect(new RilConnector(mAddress, aIndex),
                   mSocket->GetSuggestedConnectDelayMs());
}

//
// RilWorker
//

nsTArray<UniquePtr<RilWorker>> RilWorker::sRilWorkers;

nsresult
RilWorker::Register(unsigned int aClientId,
                    WorkerCrossThreadDispatcher* aDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());

  sRilWorkers.EnsureLengthAtLeast(aClientId + 1);

  if (sRilWorkers[aClientId]) {
    NS_WARNING("RilWorkers already registered");
    return NS_ERROR_FAILURE;
  }

  // Now that we're set up, connect ourselves to the RIL thread.
  sRilWorkers[aClientId] = MakeUnique<RilWorker>(aDispatcher);

  nsresult rv = sRilWorkers[aClientId]->RegisterConsumer(aClientId);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void
RilWorker::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < sRilWorkers.Length(); ++i) {
    if (!sRilWorkers[i]) {
      continue;
    }
    sRilWorkers[i]->UnregisterConsumer(i);
    sRilWorkers[i] = nullptr;
  }
}

RilWorker::RilWorker(WorkerCrossThreadDispatcher* aDispatcher)
  : mDispatcher(aDispatcher)
{
  MOZ_ASSERT(mDispatcher);
}

class RilWorker::RegisterConsumerTask : public WorkerTask
{
public:
  RegisterConsumerTask(unsigned int aClientId,
                       WorkerCrossThreadDispatcher* aDispatcher)
    : mClientId(aClientId)
    , mDispatcher(aDispatcher)
  {
    MOZ_ASSERT(mDispatcher);
  }

  bool RunTask(JSContext* aCx) override
  {
    sRilConsumers.EnsureLengthAtLeast(mClientId + 1);

    MOZ_ASSERT(!sRilConsumers[mClientId]);

    auto rilConsumer = MakeUnique<RilConsumer>();

    nsresult rv = rilConsumer->ConnectWorkerToRIL(aCx);
    if (NS_FAILED(rv)) {
      return false;
    }

    rv = rilConsumer->Register(mClientId, mDispatcher);
    if (NS_FAILED(rv)) {
      return false;
    }
    sRilConsumers[mClientId] = Move(rilConsumer);

    return true;
  }

private:
  unsigned int mClientId;
  RefPtr<WorkerCrossThreadDispatcher> mDispatcher;
};

nsresult
RilWorker::RegisterConsumer(unsigned int aClientId)
{
  RefPtr<RegisterConsumerTask> task = new RegisterConsumerTask(aClientId,
                                                                 mDispatcher);
  if (!mDispatcher->PostTask(task)) {
    NS_WARNING("Failed to post register-consumer task.");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

class RilWorker::UnregisterConsumerTask : public WorkerTask
{
public:
  UnregisterConsumerTask(unsigned int aClientId)
    : mClientId(aClientId)
  { }

  bool RunTask(JSContext* aCx) override
  {
    MOZ_ASSERT(mClientId < sRilConsumers.Length());
    MOZ_ASSERT(sRilConsumers[mClientId]);

    sRilConsumers[mClientId]->Unregister();
    sRilConsumers[mClientId] = nullptr;

    return true;
  }

private:
  unsigned int mClientId;
};

void
RilWorker::UnregisterConsumer(unsigned int aClientId)
{
  RefPtr<UnregisterConsumerTask> task =
    new UnregisterConsumerTask(aClientId);

  if (!mDispatcher->PostTask(task)) {
    NS_WARNING("Failed to post unregister-consumer task.");
    return;
  }
}

} // namespace ipc
} // namespace mozilla
