/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerService_h
#define _mozilla_dom_ClientManagerService_h

#include "ClientHandleParent.h"
#include "ClientOpPromise.h"
#include "mozilla/Assertions.h"
#include "mozilla/HashTable.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Variant.h"
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
  // Placeholder type that represents a ClientSourceParent that may be created
  // in the future (e.g. while a redirect chain is being resolved).
  //
  // Each FutureClientSourceParent has a promise that callbacks may be chained
  // to; the promise will be resolved when the associated ClientSourceParent is
  // created or rejected when it's known that it'll never be created.
  class FutureClientSourceParent {
   public:
    explicit FutureClientSourceParent(const IPCClientInfo& aClientInfo);

    const mozilla::ipc::PrincipalInfo& PrincipalInfo() const {
      return mPrincipalInfo;
    }

    already_AddRefed<SourcePromise> Promise() {
      return mPromiseHolder.Ensure(__func__);
    }

    void ResolvePromiseIfExists(ClientSourceParent* aSource) {
      MOZ_ASSERT(aSource);
      mPromiseHolder.ResolveIfExists(aSource, __func__);
    }

    void RejectPromiseIfExists(const CopyableErrorResult& aRv) {
      MOZ_ASSERT(aRv.Failed());
      mPromiseHolder.RejectIfExists(aRv, __func__);
    }

   private:
    const mozilla::ipc::PrincipalInfo mPrincipalInfo;
    MozPromiseHolder<SourcePromise> mPromiseHolder;
  };

  using SourceTableEntry =
      Variant<FutureClientSourceParent, ClientSourceParent*>;

  struct nsIDHasher {
    using Key = nsID;
    using Lookup = Key;

    static HashNumber hash(const Lookup& aLookup) {
      return HashBytes(&aLookup, sizeof(Lookup));
    }

    static bool match(const Key& aKey, const Lookup& aLookup) {
      return aKey.Equals(aLookup);
    }
  };

  // Store the possible ClientSourceParent objects in a hash table.  We want to
  // optimize for insertion, removal, and lookup by UUID.
  HashMap<nsID, SourceTableEntry, nsIDHasher> mSourceTable;

  nsTArray<ClientManagerParent*> mManagerList;

  bool mShutdown;

  ClientManagerService();
  ~ClientManagerService();

  void Shutdown();

  // Returns nullptr if aEntry isn't a ClientSourceParent (i.e. it's a
  // FutureClientSourceParent).
  ClientSourceParent* MaybeUnwrapAsExistingSource(
      const SourceTableEntry& aEntry) const;

  // Returns nullptr if the ClientSourceParent doesn't exist yet (i.e. it's a
  // FutureClientSourceParent or has already been destroyed) or is frozen.
  ClientSourceParent* FindExistingSource(
      const nsID& aID, const mozilla::ipc::PrincipalInfo& aPrincipalInfo) const;

 public:
  static already_AddRefed<ClientManagerService> GetOrCreateInstance();

  // Returns nullptr if the service is not already created.
  static already_AddRefed<ClientManagerService> GetInstance();

  bool AddSource(ClientSourceParent* aSource);

  bool RemoveSource(ClientSourceParent* aSource);

  // Returns true when a FutureClientSourceParent is successfully added.
  bool ExpectFutureSource(const IPCClientInfo& aClientInfo);

  // May still be called if it's possible that the FutureClientSourceParent
  // no longer exists.
  void ForgetFutureSource(const IPCClientInfo& aClientInfo);

  RefPtr<SourcePromise> FindSource(
      const nsID& aID, const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

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
