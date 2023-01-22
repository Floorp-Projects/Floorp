/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_
#define DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_

#include "nsISupportsImpl.h"
#include "mozilla/dom/PWebTransportChild.h"
#include "mozilla/ipc/DataPipe.h"

namespace mozilla::dom {

class WebTransport;

class WebTransportChild : public PWebTransportChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebTransportChild)
  explicit WebTransportChild(WebTransport* aTransport)
      : mTransport(aTransport) {}

  virtual void CloseAll();

  virtual void Shutdown();

  ::mozilla::ipc::IPCResult RecvCloseAll(CloseAllResolver&& aResolver);

  ::mozilla::ipc::IPCResult RecvIncomingBidirectionalStream(
      const RefPtr<mozilla::ipc::DataPipeReceiver>& aIncoming,
      const RefPtr<mozilla::ipc::DataPipeSender>& aOutgoing);

  ::mozilla::ipc::IPCResult RecvIncomingUnidirectionalStream(
      const RefPtr<mozilla::ipc::DataPipeReceiver>& aStream);

 protected:
  WebTransport* mTransport;  // WebTransport holds a strong reference to us, and
                             // calls Shutdown() before releasing it
  virtual ~WebTransportChild() { MOZ_ASSERT(!mTransport); }
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_
