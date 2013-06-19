/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Nfc_h
#define mozilla_ipc_Nfc_h 1

#include <mozilla/dom/workers/Workers.h>
#include <mozilla/ipc/UnixSocket.h>

namespace mozilla {
namespace ipc {

class NfcConsumer : public mozilla::ipc::UnixSocketConsumer
{
public:
    NfcConsumer(mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher);
    virtual ~NfcConsumer() { }
    void Shutdown();
private:
    virtual void ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage);

    virtual void OnConnectSuccess();
    virtual void OnConnectError();
    virtual void OnDisconnect();

private:
    nsRefPtr<mozilla::dom::workers::WorkerCrossThreadDispatcher> mDispatcher;
    bool mShutdown;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Nfc_h
