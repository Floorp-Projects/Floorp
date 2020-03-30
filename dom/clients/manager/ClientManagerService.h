/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerService_h
#define _mozilla_dom_ClientManagerService_h

#include "ClientOpPromise.h"
#include "nsDataHashtable.h"

namespace mozilla {

namespace ipc {

class PrincipalInfo;

}  // namespace ipc

namespace dom {

class ClientManagerParent;
class ClientSourceParent;
class ClientHandleParent;
class ContentParent;

// Define a singleton service to manage client activity throughout the
// browser.  This service runs on the PBackground thread.  To interact
// it with it please use the ClientManager and ClientHandle classes.
class ClientManagerService final {
  // Store the ClientSourceParent objects in a hash table.  We want to
  // optimize for insertion, removal, and lookup by UUID.
  nsDataHashtable<nsIDHashKey, ClientSourceParent*> mSourceTable;

  // The set of handles waiting for their corresponding ClientSourceParent
  // to be created.
  nsDataHashtable<nsIDHashKey, nsTArray<ClientHandleParent*>> mPendingHandles;

  nsTArray<ClientManagerParent*> mManagerList;

  bool mShutdown;

  ClientManagerService();
  ~ClientManagerService();

  void Shutdown();

 public:
  static already_AddRefed<ClientManagerService> GetOrCreateInstance();

  // Returns nullptr if the service is not already created.
  static already_AddRefed<ClientManagerService> GetInstance();

  bool AddSource(ClientSourceParent* aSource);

  bool RemoveSource(ClientSourceParent* aSource);

  ClientSourceParent* FindSource(
      const nsID& aID, const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

  // Called when a ClientHandle is created before the corresponding
  // ClientSource. Will call FoundSource on the ClientHandleParent when it
  // becomes available.
  void WaitForSource(ClientHandleParent* aHandle, const nsID& aID);
  void StopWaitingForSource(ClientHandleParent* aHandle, const nsID& aID);

  void AddManager(ClientManagerParent* aManager);

  void RemoveManager(ClientManagerParent* aManager);

  RefPtr<ClientOpPromise> Navigate(const ClientNavigateArgs& aArgs);

  RefPtr<ClientOpPromise> MatchAll(const ClientMatchAllArgs& aArgs);

  RefPtr<ClientOpPromise> Claim(const ClientClaimArgs& aArgs);

  RefPtr<ClientOpPromise> GetInfoAndState(
      const ClientGetInfoAndStateArgs& aArgs);

  RefPtr<ClientOpPromise> OpenWindow(const ClientOpenWindowArgs& aArgs);

  bool HasWindow(const Maybe<ContentParentId>& aContentParentId,
                 const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                 const nsID& aClientId);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManagerService)
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientManagerService_h
