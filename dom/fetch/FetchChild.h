/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetchChild_h__
#define mozilla_dom_fetchChild_h__

#include "mozilla/dom/PFetchChild.h"

namespace mozilla::dom {

class FetchChild final : public PFetchChild {
  friend class PFetchChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchChild, override);

  mozilla::ipc::IPCResult Recv__delete__(const nsresult&& aResult);

  mozilla::ipc::IPCResult RecvOnResponseAvailableInternal(
      ParentToChildInternalResponse&& aResponse);

  mozilla::ipc::IPCResult RecvOnResponseEnd(ResponseEndArgs&& aArgs);

  mozilla::ipc::IPCResult RecvOnDataAvailable();

  mozilla::ipc::IPCResult RecvOnFlushConsoleReport(
      nsTArray<net::ConsoleReportCollected>&& aReports);

  mozilla::ipc::IPCResult RecvOnCSPViolationEvent(const nsAString& aJSon);

 private:
  ~FetchChild() = default;

  void ActorDestroy(ActorDestroyReason aReason) override;
};

}  // namespace mozilla::dom

#endif
