/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManager_h
#define _mozilla_dom_ClientManager_h

#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"
#include "mozilla/dom/PClientManagerChild.h"

class nsIPrincipal;

namespace mozilla {
namespace ipc {
class PBackgroundChild;
class PrincipalInfo;
}  // namespace ipc
namespace dom {

class ClientClaimArgs;
class ClientGetInfoAndStateArgs;
class ClientHandle;
class ClientInfo;
class ClientManagerChild;
class ClientMatchAllArgs;
class ClientNavigateArgs;
class ClientOpConstructorArgs;
class ClientOpenWindowArgs;
class ClientSource;
enum class ClientType : uint8_t;
class WorkerPrivate;

// The ClientManager provides a per-thread singleton interface workering
// with the client subsystem.  It allows globals to create ClientSource
// objects.  It allows other parts of the system to attach to this globals
// by creating ClientHandle objects.  The ClientManager also provides
// methods for querying the list of clients active in the system.
class ClientManager final : public ClientThing<ClientManagerChild> {
  friend class ClientManagerChild;
  friend class ClientSource;

  ClientManager();
  ~ClientManager();

  // Utility method to trigger a shutdown of the ClientManager.  This
  // is called in various error conditions or when the last reference
  // is dropped.
  void Shutdown();

  UniquePtr<ClientSource> CreateSourceInternal(
      ClientType aType, nsISerialEventTarget* aEventTarget,
      const mozilla::ipc::PrincipalInfo& aPrincipal);

  UniquePtr<ClientSource> CreateSourceInternal(
      const ClientInfo& aClientInfo, nsISerialEventTarget* aEventTarget);

  already_AddRefed<ClientHandle> CreateHandleInternal(
      const ClientInfo& aClientInfo, nsISerialEventTarget* aSerialEventTarget);

  // Utility method to perform an IPC operation.  This will create a
  // PClientManagerOp actor tied to a MozPromise.  The promise will
  // resolve or reject with the result of the remote operation.
  [[nodiscard]] RefPtr<ClientOpPromise> StartOp(
      const ClientOpConstructorArgs& aArgs,
      nsISerialEventTarget* aSerialEventTarget);

  // Get or create the TLS singleton.  Currently this is only used
  // internally and external code indirectly calls it by invoking
  // static methods.
  static already_AddRefed<ClientManager> GetOrCreateForCurrentThread();

  // Private methods called by ClientSource
  mozilla::dom::WorkerPrivate* GetWorkerPrivate() const;

  // Don't use - use {Expect,Forget}FutureSource instead.
  static bool ExpectOrForgetFutureSource(
      const ClientInfo& aClientInfo,
      bool (PClientManagerChild::*aMethod)(const IPCClientInfo&));

 public:
  // Asynchronously declare that a ClientSource will possibly be constructed
  // from an equivalent ClientInfo in the future. This must be called before any
  // any ClientHandles are created with the ClientInfo to avoid race conditions
  // when ClientHandles query the ClientManagerService.
  //
  // This method exists so that the ClientManagerService can determine if a
  // particular ClientSource can be expected to exist in the future or has
  // already existed and been destroyed.
  //
  // If it's later known that the expected ClientSource will not be
  // constructed, ForgetFutureSource must be called.
  static bool ExpectFutureSource(const ClientInfo& aClientInfo);

  // May also be called even when the "future" source has become a "real"
  // source, in which case this is a no-op.
  static bool ForgetFutureSource(const ClientInfo& aClientInfo);

  // Initialize the ClientManager at process start.  This
  // does book-keeping like creating a TLS identifier, etc.
  // This should only be called by process startup code.
  static void Startup();

  static UniquePtr<ClientSource> CreateSource(
      ClientType aType, nsISerialEventTarget* aEventTarget,
      nsIPrincipal* aPrincipal);

  static UniquePtr<ClientSource> CreateSource(
      ClientType aType, nsISerialEventTarget* aEventTarget,
      const mozilla::ipc::PrincipalInfo& aPrincipal);

  // Construct a new ClientSource from an existing ClientInfo (and id) rather
  // than allocating a new id.
  static UniquePtr<ClientSource> CreateSourceFromInfo(
      const ClientInfo& aClientInfo, nsISerialEventTarget* aSerialEventTarget);

  // Allocate a new ClientInfo and id without creating a ClientSource. Used
  // when we have a redirect that isn't exposed to the process that owns
  // the global/ClientSource.
  static Maybe<ClientInfo> CreateInfo(ClientType aType,
                                      nsIPrincipal* aPrincipal);

  static already_AddRefed<ClientHandle> CreateHandle(
      const ClientInfo& aClientInfo, nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise> MatchAll(const ClientMatchAllArgs& aArgs,
                                          nsISerialEventTarget* aTarget);

  static RefPtr<ClientOpPromise> Claim(
      const ClientClaimArgs& aArgs, nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise> GetInfoAndState(
      const ClientGetInfoAndStateArgs& aArgs,
      nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise> Navigate(
      const ClientNavigateArgs& aArgs,
      nsISerialEventTarget* aSerialEventTarget);

  static RefPtr<ClientOpPromise> OpenWindow(
      const ClientOpenWindowArgs& aArgs,
      nsISerialEventTarget* aSerialEventTarget);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManager)
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientManager_h
