/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/Ril.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h> // For gethostbyname.

#undef CHROMIUM_LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define CHROMIUM_LOG(args...)  printf(args);
#endif

#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.

USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

namespace {

const char* RIL_SOCKET_NAME = "/dev/socket/rilproxy";

// Network port to connect to for adb forwarded sockets when doing
// desktop development.
const uint32_t RIL_TEST_PORT = 6200;

nsTArray<nsRefPtr<mozilla::ipc::RilConsumer> > sRilConsumers;

class ConnectWorkerToRIL : public WorkerTask
{
public:
    ConnectWorkerToRIL()
    { }

    virtual bool RunTask(JSContext *aCx);
};

class SendRilSocketDataTask : public nsRunnable
{
public:
    SendRilSocketDataTask(unsigned long aClientId,
                          UnixSocketRawData *aRawData)
        : mRawData(aRawData)
        , mClientId(aClientId)
    { }

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());

        if (sRilConsumers.Length() <= mClientId ||
            !sRilConsumers[mClientId] ||
            sRilConsumers[mClientId]->GetConnectionStatus() != SOCKET_CONNECTED) {
            // Probably shuting down.
            delete mRawData;
            return NS_OK;
        }

        sRilConsumers[mClientId]->SendSocketData(mRawData);
        return NS_OK;
    }

private:
    UnixSocketRawData *mRawData;
    unsigned long mClientId;
};

bool
PostToRIL(JSContext *aCx,
          unsigned aArgc,
          JS::Value *aArgv)
{
    NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");

    if (aArgc != 2) {
        JS_ReportError(aCx, "Expecting two arguments with the RIL message");
        return false;
    }

    JS::Value cv = JS_ARGV(aCx, aArgv)[0];
    int clientId = cv.toInt32();

    JS::Value v = JS_ARGV(aCx, aArgv)[1];

    JSAutoByteString abs;
    void *data;
    size_t size;
    if (JSVAL_IS_STRING(v)) {
        JSString *str = JSVAL_TO_STRING(v);
        if (!abs.encodeUtf8(aCx, str)) {
            return false;
        }

        data = abs.ptr();
        size = abs.length();
    } else if (!JSVAL_IS_PRIMITIVE(v)) {
        JSObject *obj = JSVAL_TO_OBJECT(v);
        if (!JS_IsTypedArrayObject(obj)) {
            JS_ReportError(aCx, "Object passed in wasn't a typed array");
            return false;
        }

        uint32_t type = JS_GetArrayBufferViewType(obj);
        if (type != js::ArrayBufferView::TYPE_INT8 &&
            type != js::ArrayBufferView::TYPE_UINT8 &&
            type != js::ArrayBufferView::TYPE_UINT8_CLAMPED) {
            JS_ReportError(aCx, "Typed array data is not octets");
            return false;
        }

        size = JS_GetTypedArrayByteLength(obj);
        data = JS_GetArrayBufferViewData(obj);
    } else {
        JS_ReportError(aCx,
                       "Incorrect argument. Expecting a string or a typed array");
        return false;
    }

    UnixSocketRawData* raw = new UnixSocketRawData(data, size);

    nsRefPtr<SendRilSocketDataTask> task =
        new SendRilSocketDataTask(clientId, raw);
    NS_DispatchToMainThread(task);
    return true;
}

bool
ConnectWorkerToRIL::RunTask(JSContext *aCx)
{
    // Set up the postRILMessage on the function for worker -> RIL thread
    // communication.
    NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");
    NS_ASSERTION(!JS_IsRunning(aCx), "Are we being called somehow?");
    JS::Rooted<JSObject*> workerGlobal(aCx, JS::CurrentGlobalOrNull(aCx));

    return !!JS_DefineFunction(aCx, workerGlobal,
                               "postRILMessage", PostToRIL, 2, 0);
}

class DispatchRILEvent : public WorkerTask
{
public:
        DispatchRILEvent(UnixSocketRawData* aMessage)
            : mMessage(aMessage)
        { }

        virtual bool RunTask(JSContext *aCx);

private:
        nsAutoPtr<UnixSocketRawData> mMessage;
};

bool
DispatchRILEvent::RunTask(JSContext *aCx)
{
    JSObject *obj = JS::CurrentGlobalOrNull(aCx);

    JSObject *array = JS_NewUint8Array(aCx, mMessage->mSize);
    if (!array) {
        return false;
    }

    memcpy(JS_GetArrayBufferViewData(array), mMessage->mData, mMessage->mSize);
    JS::Value argv[] = { OBJECT_TO_JSVAL(array) };
    return JS_CallFunctionName(aCx, obj, "onRILMessage",
                               mozilla::ArrayLength(argv), argv, argv);
}

class RilConnector : public mozilla::ipc::UnixSocketConnector
{
public:
  RilConnector(unsigned long aClientId) : mClientId(aClientId)
  {}

  virtual ~RilConnector()
  {}

  virtual int Create();
  virtual bool CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          sockaddr_any& aAddr,
                          const char* aAddress);
  virtual bool SetUp(int aFd);
  virtual bool SetUpListenSocket(int aFd);
  virtual void GetSocketAddr(const sockaddr_any& aAddr,
                             nsAString& aAddrStr);

private:
  unsigned long mClientId;
};

int
RilConnector::Create()
{
    MOZ_ASSERT(!NS_IsMainThread());

    int fd = -1;

#if defined(MOZ_WIDGET_GONK)
    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
#else
    // If we can't hit a local loopback, fail later in connect.
    fd = socket(AF_INET, SOCK_STREAM, 0);
#endif

    if (fd < 0) {
        NS_WARNING("Could not open ril socket!");
        return -1;
    }

    if (!SetUp(fd)) {
        NS_WARNING("Could not set up socket!");
    }
    return fd;
}

bool
RilConnector::CreateAddr(bool aIsServer,
                         socklen_t& aAddrSize,
                         sockaddr_any& aAddr,
                         const char* aAddress)
{
    // We never open ril socket as server.
    MOZ_ASSERT(!aIsServer);
    uint32_t af;
#if defined(MOZ_WIDGET_GONK)
    af = AF_LOCAL;
#else
    af = AF_INET;
#endif
    switch (af) {
    case AF_LOCAL:
        aAddr.un.sun_family = af;
        if(strlen(aAddress) > sizeof(aAddr.un.sun_path)) {
            NS_WARNING("Address too long for socket struct!");
            return false;
        }
        strcpy((char*)&aAddr.un.sun_path, aAddress);
        aAddrSize = strlen(aAddress) + offsetof(struct sockaddr_un, sun_path) + 1;
        break;
    case AF_INET:
        aAddr.in.sin_family = af;
        aAddr.in.sin_port = htons(RIL_TEST_PORT + mClientId);
        aAddr.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        aAddrSize = sizeof(sockaddr_in);
        break;
    default:
        NS_WARNING("Socket type not handled by connector!");
        return false;
    }
    return true;
}

bool
RilConnector::SetUp(int aFd)
{
    // Nothing to do here.
    return true;
}

bool
RilConnector::SetUpListenSocket(int aFd)
{
    // Nothing to do here.
    return true;
}

void
RilConnector::GetSocketAddr(const sockaddr_any& aAddr,
                            nsAString& aAddrStr)
{
    MOZ_CRASH("This should never be called!");
}

} // anonymous namespace

namespace mozilla {
namespace ipc {

RilConsumer::RilConsumer(unsigned long aClientId,
                         WorkerCrossThreadDispatcher* aDispatcher)
    : mDispatcher(aDispatcher)
    , mClientId(aClientId)
    , mShutdown(false)
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

    ConnectSocket(new RilConnector(mClientId), mAddress.get());
}

nsresult
RilConsumer::Register(unsigned int aClientId,
                      WorkerCrossThreadDispatcher* aDispatcher)
{
    MOZ_ASSERT(NS_IsMainThread());

    sRilConsumers.EnsureLengthAtLeast(aClientId + 1);

    if (sRilConsumers[aClientId]) {
        NS_WARNING("RilConsumer already registered");
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<ConnectWorkerToRIL> connection = new ConnectWorkerToRIL();
    if (!aDispatcher->PostTask(connection)) {
        NS_WARNING("Failed to connect worker to ril");
        return NS_ERROR_UNEXPECTED;
    }

    // Now that we're set up, connect ourselves to the RIL thread.
    sRilConsumers[aClientId] = new RilConsumer(aClientId, aDispatcher);
    return NS_OK;
}

void
RilConsumer::Shutdown()
{
    MOZ_ASSERT(NS_IsMainThread());

    for (unsigned long i = 0; i < sRilConsumers.Length(); i++) {
        nsRefPtr<RilConsumer>& instance = sRilConsumers[i];
        if (!instance) {
            continue;
        }

        instance->mShutdown = true;
        instance->CloseSocket();
        instance = nullptr;
    }
}

void
RilConsumer::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<DispatchRILEvent> dre(new DispatchRILEvent(aMessage.forget()));
    mDispatcher->PostTask(dre);
}

void
RilConsumer::OnConnectSuccess()
{
    // Nothing to do here.
    CHROMIUM_LOG("RIL[%lu]: %s\n", mClientId, __FUNCTION__);
}

void
RilConsumer::OnConnectError()
{
    CHROMIUM_LOG("RIL[%lu]: %s\n", mClientId, __FUNCTION__);
    CloseSocket();
}

void
RilConsumer::OnDisconnect()
{
    CHROMIUM_LOG("RIL[%lu]: %s\n", mClientId, __FUNCTION__);
    if (!mShutdown) {
        ConnectSocket(new RilConnector(mClientId), mAddress.get(), 1000);
    }
}

} // namespace ipc
} // namespace mozilla
