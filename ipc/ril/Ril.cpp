/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h> // For gethostbyname.

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

#include "jsfriendapi.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.
#include "Ril.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::ipc;

namespace {

const char* RIL_SOCKET_NAME = "/dev/socket/rilproxy";

// Network port to connect to for adb forwarded sockets when doing
// desktop development.
const uint32_t RIL_TEST_PORT = 6200;

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
    JSObject *obj = JS_GetGlobalObject(aCx);

    JSObject *array = JS_NewUint8Array(aCx, mMessage->mSize);
    if (!array) {
        return false;
    }

    memcpy(JS_GetArrayBufferViewData(array), mMessage->mData, mMessage->mSize);
    jsval argv[] = { OBJECT_TO_JSVAL(array) };
    return JS_CallFunctionName(aCx, obj, "onRILMessage", NS_ARRAY_LENGTH(argv),
                               argv, argv);
}

class RilConnector : public mozilla::ipc::UnixSocketConnector
{
public:
  virtual ~RilConnector()
  {}

  virtual int Create();
  virtual void CreateAddr(bool aIsServer,
                          socklen_t& aAddrSize,
                          struct sockaddr *aAddr,
                          const char* aAddress);
  virtual bool SetUp(int aFd);
  virtual void GetSocketAddr(const sockaddr& aAddr,
                             nsAString& aAddrStr);
};

int
RilConnector::Create()
{
    MOZ_ASSERT(!NS_IsMainThread());

    int fd = -1;

#if defined(MOZ_WIDGET_GONK)
    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
#else
    struct hostent *hp;

    hp = gethostbyname("localhost");
    if (hp) {
        fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    }
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

void
RilConnector::CreateAddr(bool aIsServer,
                         socklen_t& aAddrSize,
                         struct sockaddr *aAddr,
                         const char* aAddress)
{
    // We never open ril socket as server.
    MOZ_ASSERT(!aIsServer);

#if defined(MOZ_WIDGET_GONK)
    struct sockaddr_un addr_un;

    memset(&addr_un, 0, sizeof(addr_un));
    strcpy(addr_un.sun_path, aAddress);
    addr_un.sun_family = AF_LOCAL;

    aAddrSize = strlen(aAddress) + offsetof(struct sockaddr_un, sun_path) + 1;
    memcpy(aAddr, &addr_un, aAddrSize);
#else
    struct hostent *hp;
    struct sockaddr_in addr_in;

    hp = gethostbyname("localhost");
    if (!hp) {
        return;
    }

    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = hp->h_addrtype;
    addr_in.sin_port = htons(RIL_TEST_PORT);
    memcpy(&addr_in.sin_addr, hp->h_addr, hp->h_length);

    aAddrSize = sizeof(addr_in);
    memcpy(aAddr, &addr_in, aAddrSize);
#endif
}

bool
RilConnector::SetUp(int aFd)
{
    // Nothing to do here.
    return true;
}

void
RilConnector::GetSocketAddr(const sockaddr& aAddr,
                            nsAString& aAddrStr)
{
    // Unused.
    MOZ_NOT_REACHED("This should never be called!");
}

} // anonymous namespace

namespace mozilla {
namespace ipc {

RilConsumer::RilConsumer(WorkerCrossThreadDispatcher* aDispatcher)
    : mDispatcher(aDispatcher)
    , mShutdown(false)
{
    ConnectSocket(new RilConnector(), RIL_SOCKET_NAME);
}

void
RilConsumer::Shutdown()
{
    mShutdown = true;
    CloseSocket();
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
    LOG("Socket open for RIL\n");
}

void
RilConsumer::OnConnectError()
{
    LOG("%s\n", __FUNCTION__);
    CloseSocket();
}

void
RilConsumer::OnDisconnect()
{
    LOG("%s\n", __FUNCTION__);
    if (!mShutdown) {
        ConnectSocket(new RilConnector(), RIL_SOCKET_NAME, 1000);
    }
}

} // namespace ipc
} // namespace mozilla
