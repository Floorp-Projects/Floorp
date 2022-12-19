/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_
#define DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_

#include "mozilla/dom/PWebTransportChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class WebTransportChild : public PWebTransportChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebTransportChild)

  virtual void CloseAll() {
    // XXX need impl
  }

  virtual void Shutdown() {
    if (!CanSend()) {
      return;
    }

    Close();
  }

  ::mozilla::ipc::IPCResult RecvCloseAll(CloseAllResolver&& aResolver) {
    CloseAll();
    aResolver(NS_OK);
    return IPC_OK();
  }

 protected:
  virtual ~WebTransportChild() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_WEBTRANSPORTCHILD_H_
