/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WebTransportChild.h"
#include "mozilla/dom/WebTransportLog.h"

namespace mozilla::dom {

void WebTransportChild::Shutdown() {
  if (!CanSend()) {
    return;
  }

  Close();
  mTransport = nullptr;
}

void WebTransportChild::CloseAll() {
  // XXX need impl
}

::mozilla::ipc::IPCResult WebTransportChild::RecvCloseAll(
    CloseAllResolver&& aResolver) {
  CloseAll();
  aResolver(NS_OK);
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvIncomingBidirectionalStream(
    const RefPtr<DataPipeReceiver>& aIncoming,
    const RefPtr<DataPipeSender>& aOutgoing) {
  mTransport->NewBidirectionalStream(aIncoming, aOutgoing);
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvIncomingUnidirectionalStream(
    const RefPtr<DataPipeReceiver>& aStream) {
  mTransport->NewUnidirectionalStream(aStream);
  return IPC_OK();
}

}  // namespace mozilla::dom
