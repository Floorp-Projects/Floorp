/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

#include "jsfriendapi.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.
#include "Nfc.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

namespace {

const char* NFC_SOCKET_NAME = "/dev/socket/nfcd";

// Network port to connect to for adb forwarded sockets when doing
// desktop development.
const uint32_t NFCD_TEST_PORT = 6400;

class DispatchNfcEvent : public WorkerTask
{
public:
    DispatchNfcEvent(UnixSocketRawData* aMessage)
      : mMessage(aMessage)
    { }

    virtual bool RunTask(JSContext *aCx);

private:
    nsAutoPtr<UnixSocketRawData> mMessage;
};

bool
DispatchNfcEvent::RunTask(JSContext *aCx)
{
    MOZ_ASSERT(NS_IsMainThread(), "DispatchNfcEvent on main thread");
    MOZ_ASSERT(aCx);

    JSObject *obj = JS_GetGlobalForScopeChain(aCx);
    JSObject *array = JS_NewUint8Array(aCx, mMessage->mSize);
    if (!array) {
        return false;
    }
    memcpy(JS_GetArrayBufferViewData(array), mMessage->mData, mMessage->mSize);
    jsval argv[] = { OBJECT_TO_JSVAL(array) };
    return JS_CallFunctionName(aCx, obj, "onNfcMessage", NS_ARRAY_LENGTH(argv),
                               argv, argv);
}

class NfcConnector : public mozilla::ipc::UnixSocketConnector
{
public:
    NfcConnector() {}
    virtual ~NfcConnector()
    {}

    virtual int Create();
    virtual bool CreateAddr(bool aIsServer,
                            socklen_t& aAddrSize,
                            sockaddr_any& aAddr,
                            const char* aAddress);
    virtual bool SetUp(int aFd);
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
        NS_WARNING("Could not open Nfc socket!");
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
        aAddr.in.sin_port = htons(NFCD_TEST_PORT);
        aAddr.in.sin_addr.s_addr = htons(INADDR_LOOPBACK);
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
    ConnectSocket(new NfcConnector(), NFC_SOCKET_NAME);
}

void
NfcConsumer::Shutdown()
{
    mShutdown = true;
    CloseSocket();
}

void
NfcConsumer::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
    MOZ_ASSERT(NS_IsMainThread());
#ifdef DEBUG
    LOG("ReceiveSocketData\n");
#endif
    nsRefPtr<DispatchNfcEvent> dre(new DispatchNfcEvent(aMessage.forget()));
    mDispatcher->PostTask(dre);
}

void
NfcConsumer::OnConnectSuccess()
{
    // Nothing to do here.
    LOG("Socket open for Nfc\n");
}

void
NfcConsumer::OnConnectError()
{
#ifdef DEBUG
    LOG("%s\n", __FUNCTION__);
#endif
    CloseSocket();
}

void
NfcConsumer::OnDisconnect()
{
#ifdef DEBUG
    LOG("%s\n", __FUNCTION__);
#endif
    if (!mShutdown) {
        ConnectSocket(new NfcConnector(), NFC_SOCKET_NAME, 1000);
    }
}

} // namespace ipc
} // namespace mozilla
