/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManager_h
#define _mozilla_dom_ClientManager_h

#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"

namespace mozilla {
namespace ipc {
class PBackgroundChild;
} // namespace ipc
namespace dom {

class ClientManagerChild;
class ClientOpConstructorArgs;

namespace workers {
class WorkerPrivate;
} // workers namespace

// The ClientManager provides a per-thread singleton interface workering
// with the client subsystem.  It allows globals to create ClientSource
// objects.  It allows other parts of the system to attach to this globals
// by creating ClientHandle objects.  The ClientManager also provides
// methods for querying the list of clients active in the system.
class ClientManager final : public ClientThing<ClientManagerChild>
{
  friend class ClientManagerChild;

  ClientManager();
  ~ClientManager();

  // Utility method to trigger a shutdown of the ClientManager.  This
  // is called in various error conditions or when the last reference
  // is dropped.
  void
  Shutdown();

  // Utility method to perform an IPC operation.  This will create a
  // PClientManagerOp actor tied to a MozPromise.  The promise will
  // resolve or reject with the result of the remote operation.
  already_AddRefed<ClientOpPromise>
  StartOp(const ClientOpConstructorArgs& aArgs,
          nsISerialEventTarget* aSerialEventTarget);

  // Get or create the TLS singleton.  Currently this is only used
  // internally and external code indirectly calls it by invoking
  // static methods.
  static already_AddRefed<ClientManager>
  GetOrCreateForCurrentThread();

  // Private methods called by ClientSource
  mozilla::dom::workers::WorkerPrivate*
  GetWorkerPrivate() const;

public:
  // Initialize the ClientManager at process start.  This
  // does book-keeping like creating a TLS identifier, etc.
  // This should only be called by process startup code.
  static void
  Startup();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManager)
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientManager_h
