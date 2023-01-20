/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchChild.h"

namespace mozilla::dom {

mozilla::ipc::IPCResult FetchChild::Recv__delete__(const nsresult&& aResult) {
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseAvailableInternal(
    ParentToChildInternalResponse&& aResponse) {
  // TODO: Should perform WorkerFetchResolver::OnResponseAvailableInternal here.
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseEnd(ResponseEndArgs&& aArgs) {
  // TODO: Should perform WorkerFetchResolver::OnResponseEnd here.
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnDataAvailable() {
  // TODO: Should perform WorkerFetchResolver::OnDataAvailable here.
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnFlushConsoleReport(
    nsTArray<net::ConsoleReportCollected>&& aReports) {
  // TODO: Should perform WorkerFetchResolver::FlushConsoleReport here.
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnCSPViolationEvent(
    const nsAString& aJSON) {
  // TODO: Should dispatch csp violation event to worker scope.
  return IPC_OK();
}

void FetchChild::ActorDestroy(ActorDestroyReason aReason) {}

}  // namespace mozilla::dom
