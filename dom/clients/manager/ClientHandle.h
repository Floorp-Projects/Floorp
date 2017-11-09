/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandle_h
#define _mozilla_dom_ClientHandle_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {

namespace dom {

class ClientManager;
class ClientHandleChild;
class ClientOpConstructorArgs;
class PClientManagerChild;

// The ClientHandle allows code to take a simple ClientInfo struct and
// convert it into a live actor-backed object attached to a particular
// ClientSource somewhere in the browser.  If the ClientSource is
// destroyed then the ClientHandle will simply begin to reject operations.
// We do not currently provide a way to be notified when the ClientSource
// is destroyed, but this could be added in the future.
class ClientHandle final : public ClientThing<ClientHandleChild>
{
  friend class ClientManager;

  RefPtr<ClientManager> mManager;
  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;
  ClientInfo mClientInfo;

  ~ClientHandle();

  void
  Shutdown();

  already_AddRefed<ClientOpPromise>
  StartOp(const ClientOpConstructorArgs& aArgs);

  // Private methods called by ClientManager
  ClientHandle(ClientManager* aManager,
               nsISerialEventTarget* aSerialEventTarget,
               const ClientInfo& aClientInfo);

  void
  Activate(PClientManagerChild* aActor);

public:
  const ClientInfo&
  Info() const;

  NS_INLINE_DECL_REFCOUNTING(ClientHandle);
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientHandle_h
