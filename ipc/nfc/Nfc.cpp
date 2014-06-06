/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013, Deutsche Telekom, Inc. */

#include "mozilla/ipc/Nfc.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#undef CHROMIUM_LOG
#if (defined(MOZ_WIDGET_GONK) && defined(DEBUG))
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define CHROMIUM_LOG(args...)
#endif

#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.

USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

namespace {

const char* NFC_SOCKET_NAME = "/dev/socket/nfcd";

// Network port to connect to for adb forwarded sockets when doing
// desktop development.
const uint32_t NFC_TEST_PORT = 6400;

nsRefPtr<mozilla::ipc::NfcConsumer> sNfcConsumer;

class ConnectWorkerToNFC : public WorkerTask
{
public:
    ConnectWorkerToNFC()
    { }

    virtual bool RunTask(JSContext* aCx);
};

class SendNfcSocketDataTask : public nsRunnable
{
public:
    SendNfcSocketDataTask(UnixSocketRawData* aRawData)
        : mRawData(aRawData)
    { }

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());

        if (!sNfcConsumer ||
            sNfcConsumer->GetConnectionStatus() != SOCKET_CONNECTED) {
            // Probably shuting down.
            delete mRawData;
            return NS_OK;
        }

        sNfcConsumer->SendSocketData(mRawData);
        return NS_OK;
    }

private:
    UnixSocketRawData* mRawData;
};

bool
PostToNFC(JSContext* aCx,
          unsigned aArgc,
          JS::Value* aVp)
{
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");

    if (args.length() != 1) {
        JS_ReportError(aCx, "Expecting one argument with the NFC message");
        return false;
    }

    JS::Value v = args[0];

    JSAutoByteString abs;
    void* data;
    size_t size;
    if (v.isString()) {
        JS::Rooted<JSString*> str(aCx, v.toString());
        if (!abs.encodeUtf8(aCx, str)) {
            return false;
        }

        data = abs.ptr();
        size = abs.length();
    } else if (!v.isPrimitive()) {
        JSObject* obj = v.toObjectOrNull();
        if (!JS_IsTypedArrayObject(obj)) {
            JS_ReportError(aCx, "Object passed in wasn't a typed array");
            return false;
        }

        uint32_t type = JS_GetArrayBufferViewType(obj);
        if (type != js::Scalar::Int8 &&
            type != js::Scalar::Uint8 &&
            type != js::Scalar::Uint8Clamped) {
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

    nsRefPtr<SendNfcSocketDataTask> task =
        new SendNfcSocketDataTask(raw);
    NS_DispatchToMainThread(task);
    return true;
}

bool
ConnectWorkerToNFC::RunTask(JSContext* aCx)
{
    // Set up the postNFCMessage on the function for worker -> NFC thread
    // communication.
    NS_ASSERTION(!NS_IsMainThread(), "Expecting to be on the worker thread");
    NS_ASSERTION(!JS_IsRunning(aCx), "Are we being called somehow?");
    JS::Rooted<JSObject*> workerGlobal(aCx, JS::CurrentGlobalOrNull(aCx));

    return !!JS_DefineFunction(aCx, workerGlobal,
                               "postNfcMessage", PostToNFC, 1, 0);
}

class DispatchNFCEvent : public WorkerTask
{
public:
    DispatchNFCEvent(UnixSocketRawData* aMessage)
        : mMessage(aMessage)
    { }

    virtual bool RunTask(JSContext* aCx);

private:
    nsAutoPtr<UnixSocketRawData> mMessage;
};

bool
DispatchNFCEvent::RunTask(JSContext* aCx)
{
    JS::Rooted<JSObject*> obj(aCx, JS::CurrentGlobalOrNull(aCx));

    JSObject* array = JS_NewUint8Array(aCx, mMessage->mSize);
    if (!array) {
        return false;
    }
    JS::Rooted<JS::Value> arrayVal(aCx, JS::ObjectValue(*array));

    memcpy(JS_GetArrayBufferViewData(array), mMessage->mData, mMessage->mSize);
    JS::Rooted<JS::Value> rval(aCx);
    return JS_CallFunctionName(aCx, obj, "onNfcMessage", JS::HandleValueArray(arrayVal), &rval);
}

class NfcConnector : public mozilla::ipc::UnixSocketConnector
{
public:
    NfcConnector()
    {}

    virtual ~NfcConnector()
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
};

int
NfcConnector::Create()
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
        NS_WARNING("Could not open nfc socket!");
        return -1;
    }

    if (!SetUp(fd)) {
        NS_WARNING("Could not set up socket!");
    }
    return fd;
}

bool
NfcConnector::CreateAddr(bool aIsServer,
                         socklen_t& aAddrSize,
                         sockaddr_any& aAddr,
                         const char* aAddress)
{
    // We never open nfc socket as server.
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
        aAddr.in.sin_port = htons(NFC_TEST_PORT);
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
NfcConnector::SetUp(int aFd)
{
    // Nothing to do here.
    return true;
}

bool
NfcConnector::SetUpListenSocket(int aFd)
{
    // Nothing to do here.
    return true;
}

void
NfcConnector::GetSocketAddr(const sockaddr_any& aAddr,
                            nsAString& aAddrStr)
{
    MOZ_CRASH("This should never be called!");
}

} // anonymous namespace

namespace mozilla {
namespace ipc {

NfcConsumer::NfcConsumer(WorkerCrossThreadDispatcher* aDispatcher)
    : mDispatcher(aDispatcher)
    , mShutdown(false)
{
    mAddress = NFC_SOCKET_NAME;

    ConnectSocket(new NfcConnector(), mAddress.get());
}

nsresult
NfcConsumer::Register(WorkerCrossThreadDispatcher* aDispatcher)
{
    MOZ_ASSERT(NS_IsMainThread());

    if (sNfcConsumer) {
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<ConnectWorkerToNFC> connection = new ConnectWorkerToNFC();
    if (!aDispatcher->PostTask(connection)) {
        return NS_ERROR_UNEXPECTED;
    }

    // Now that we're set up, connect ourselves to the NFC thread.
    sNfcConsumer = new NfcConsumer(aDispatcher);
    return NS_OK;
}

void
NfcConsumer::Shutdown()
{
    MOZ_ASSERT(NS_IsMainThread());

    if (sNfcConsumer) {
        sNfcConsumer->mShutdown = true;
        sNfcConsumer->CloseSocket();
        sNfcConsumer = nullptr;
    }
}

void
NfcConsumer::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<DispatchNFCEvent> dre(new DispatchNFCEvent(aMessage.forget()));
    mDispatcher->PostTask(dre);
}

void
NfcConsumer::OnConnectSuccess()
{
    // Nothing to do here.
    CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);
}

void
NfcConsumer::OnConnectError()
{
    CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);
    CloseSocket();
}

void
NfcConsumer::OnDisconnect()
{
    CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);
    if (!mShutdown) {
        ConnectSocket(new NfcConnector(), mAddress.get(),
                      GetSuggestedConnectDelayMs());
    }
}

} // namespace ipc
} // namespace mozilla
