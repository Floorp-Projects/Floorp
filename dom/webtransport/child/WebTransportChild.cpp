/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WebTransportChild.h"
#include "mozilla/dom/WebTransportLog.h"

namespace mozilla::dom {

void WebTransportChild::Shutdown(bool aClose) {
  LOG(("WebTransportChild::Shutdown() for %p (%p)", this, mTransport));
  mTransport = nullptr;
  if (!aClose || !CanSend()) {
    return;
  }

  Close();
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

::mozilla::ipc::IPCResult WebTransportChild::RecvRemoteClosed(
    const bool& aCleanly, const uint32_t& aCode, const nsACString& aReason) {
  if (mTransport) {
    mTransport->RemoteClosed(aCleanly, aCode, aReason);
  }
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvIncomingBidirectionalStream(
    const uint64_t& aStreamId, const RefPtr<DataPipeReceiver>& aIncoming,
    const RefPtr<DataPipeSender>& aOutgoing) {
  if (mTransport) {
    mTransport->NewBidirectionalStream(aStreamId, aIncoming, aOutgoing);
  }
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvIncomingUnidirectionalStream(
    const uint64_t& aStreamId, const RefPtr<DataPipeReceiver>& aStream) {
  if (mTransport) {
    mTransport->NewUnidirectionalStream(aStreamId, aStream);
  }
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvIncomingDatagram(
    nsTArray<uint8_t>&& aData, const TimeStamp& aRecvTimeStamp) {
  if (mTransport) {
    mTransport->NewDatagramReceived(std::move(aData), aRecvTimeStamp);
  }
  return IPC_OK();
}

::mozilla::ipc::IPCResult WebTransportChild::RecvOnStreamResetOrStopSending(
    const uint64_t& aStreamId, const StreamResetOrStopSendingError& aError) {
  if (mTransport) {
    mTransport->OnStreamResetOrStopSending(aStreamId, aError);
  }
  return IPC_OK();
}

}  // namespace mozilla::dom
