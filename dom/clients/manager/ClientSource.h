/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSource_h
#define _mozilla_dom_ClientSource_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientThing.h"

namespace mozilla {
namespace dom {

class ClientManager;
class ClientSourceChild;
class ClientSourceConstructorArgs;
class PClientManagerChild;

namespace workers {
class WorkerPrivate;
} // workers namespace

// ClientSource is an RAII style class that is designed to be held via
// a UniquePtr<>.  When created ClientSource will register the existence
// of a client in the cross-process ClientManagerService.  When the
// ClientSource is destroyed then client entry will be removed.  Code
// that represents globals or browsing environments, such as nsGlobalWindow
// or WorkerPrivate, should use ClientManager to create a ClientSource.
class ClientSource final : public ClientThing<ClientSourceChild>
{
  friend class ClientManager;

  NS_DECL_OWNINGTHREAD

  RefPtr<ClientManager> mManager;

  ClientInfo mClientInfo;

  void
  Shutdown();

  // Private methods called by ClientManager
  ClientSource(ClientManager* aManager,
               const ClientSourceConstructorArgs& aArgs);

  void
  Activate(PClientManagerChild* aActor);

public:
  ~ClientSource();
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientSource_h
