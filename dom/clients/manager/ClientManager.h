/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManager_h
#define _mozilla_dom_ClientManager_h

#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"

class nsIPrincipal;

namespace mozilla {
namespace ipc {
class PBackgroundChild;
class PrincipalInfo;
} // namespace ipc
namespace dom {

class ClientClaimArgs;
class ClientGetInfoAndStateArgs;
class ClientHandle;
class ClientInfo;
class ClientManagerChild;
class ClientMatchAllArgs;
class ClientOpConstructorArgs;
class ClientSource;
enum class ClientType : uint8_t;

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
  friend class ClientSource;

  ClientManager();
  ~ClientManager();

  // Utility method to trigger a shutdown of the ClientManager.  This
  // is called in various error conditions or when the last reference
  // is dropped.
  void
  Shutdown();

  UniquePtr<ClientSource>
  CreateSourceInternal(ClientType aType,
                       nsISerialEventTarget* aEventTarget,
                       const mozilla::ipc::PrincipalInfo& aPrincipal);

  already_AddRefed<ClientHandle>
  CreateHandleInternal(const ClientInfo& aClientInfo,
                       nsISerialEventTarget* aSerialEventTarget);

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

  static UniquePtr<ClientSource>
  CreateSource(ClientType aType, nsISerialEventTarget* aEventTarget,
               nsIPrincipal* aPrincipal);

  static UniquePtr<ClientSource>
  CreateSource(ClientType aType, nsISerialEventTarget* aEventTarget,
               const mozilla::ipc::PrincipalInfo& aPrincipal);

  static already_AddRefed<ClientHandle>
  CreateHandle(const ClientInfo& aClientInfo,
               nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise>
  MatchAll(const ClientMatchAllArgs& aArgs, nsISerialEventTarget* aTarget);

  static RefPtr<ClientOpPromise>
  Claim(const ClientClaimArgs& aArgs, nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise>
  GetInfoAndState(const ClientGetInfoAndStateArgs& aArgs,
                  nsISerialEventTarget* aSerialEventTarget);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManager)
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientManager_h
