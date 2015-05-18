/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=8 et ft=cpp: */
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
#include "mozilla/ipc/NfcConnector.h"
#include "nsThreadUtils.h" // For NS_IsMainThread.

using namespace mozilla::ipc;

namespace {

class SendNfcSocketDataTask final : public nsRunnable
{
public:
  SendNfcSocketDataTask(NfcConsumer* aConsumer, UnixSocketRawData* aRawData)
    : mConsumer(aConsumer)
    , mRawData(aRawData)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mConsumer ||
      mConsumer->GetConnectionStatus() != SOCKET_CONNECTED) {
      // Probably shuting down.
      return NS_OK;
    }

    mConsumer->SendSocketData(mRawData.forget());
    return NS_OK;
  }

private:
  NfcConsumer* mConsumer;
  nsAutoPtr<UnixSocketRawData> mRawData;
};

} // anonymous namespace

namespace mozilla {
namespace ipc {

//
// NfcListenSocket
//

NfcListenSocket::NfcListenSocket(NfcSocketListener* aListener)
  : mListener(aListener)
{ }

void
NfcListenSocket::OnConnectSuccess()
{
  if (mListener) {
    mListener->OnConnectSuccess(NfcSocketListener::LISTEN_SOCKET);
  }
}

void
NfcListenSocket::OnConnectError()
{
  if (mListener) {
    mListener->OnConnectError(NfcSocketListener::LISTEN_SOCKET);
  }
}

void
NfcListenSocket::OnDisconnect()
{
  if (mListener) {
    mListener->OnDisconnect(NfcSocketListener::LISTEN_SOCKET);
  }
}

//
// NfcConsumer
//

NfcConsumer::NfcConsumer(NfcSocketListener* aListener)
  : mListener(aListener)
{ }

void
NfcConsumer::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  Close();
}

bool
NfcConsumer::PostToNfcDaemon(const uint8_t* aData, size_t aSize)
{
  MOZ_ASSERT(!NS_IsMainThread());

  UnixSocketRawData* raw = new UnixSocketRawData(aData, aSize);
  nsRefPtr<SendNfcSocketDataTask> task = new SendNfcSocketDataTask(this, raw);
  NS_DispatchToMainThread(task);
  return true;
}

void
NfcConsumer::ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mListener) {
    mListener->ReceiveSocketData(aBuffer);
  }
}

void
NfcConsumer::OnConnectSuccess()
{
  CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);

  if (mListener) {
    mListener->OnConnectSuccess(NfcSocketListener::STREAM_SOCKET);
  }
  // Nothing to do here.
}

void
NfcConsumer::OnConnectError()
{
  CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);

  if (mListener) {
    mListener->OnConnectError(NfcSocketListener::STREAM_SOCKET);
  }
}

void
NfcConsumer::OnDisconnect()
{
  CHROMIUM_LOG("NFC: %s\n", __FUNCTION__);

  if (mListener) {
    mListener->OnDisconnect(NfcSocketListener::STREAM_SOCKET);
  }
}

ConnectionOrientedSocketIO*
NfcConsumer::GetIO()
{
  return PrepareAccept(new NfcConnector(NS_LITERAL_CSTRING("nfcd")));
}

} // namespace ipc
} // namespace mozilla
