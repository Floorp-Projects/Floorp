/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerService_h
#define _mozilla_dom_ClientManagerService_h

#include "ClientOpPromise.h"
#include "mozilla/HashTable.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsDataHashtable.h"

namespace mozilla {

namespace ipc {

class PrincipalInfo;

}  // namespace ipc

namespace dom {

class ClientManagerParent;
class ClientSourceParent;
class ClientHandleParent;

typedef MozPromise<ClientSourceParent*, nsresult, /* IsExclusive = */ false>
    SourcePromise;

// Define a singleton service to manage client activity throughout the
// browser.  This service runs on the PBackground thread.  To interact
// it with it please use the ClientManager and ClientHandle classes.
class ClientManagerService final {
 public:
  static already_AddRefed<ClientManagerService> GetOrCreateInstance();

  // Returns nullptr if the service is not already created.
  static already_AddRefed<ClientManagerService> GetInstance();

  bool AddSource(ClientSourceParent* aSource);

  bool RemoveSource(ClientSourceParent* aSource);

  bool ExpectFutureSource(const IPCClientInfo& aClientInfo);

  bool ForgetFutureSource(const IPCClientInfo& aClientInfo);

  // The returned promise rejects if:
  //  - the corresponding `ClientSourceParent` has already removed itself from
  //  the `ClientManagerService` (i.e. the corresponding `ClientSource` has been
  //  detroyed) or if
  //  - it's known that the corresponding `ClientSourceParent` will not exist
  //  (i.e. the corresponding `ClientSource` will not be created).
  RefPtr<SourcePromise> FindSource(
      const nsID& aID, const mozilla::ipc::PrincipalInfo& aPrincipalInfo) const;

  void AddManager(ClientManagerParent* aManager);

  void RemoveManager(ClientManagerParent* aManager);

  RefPtr<ClientOpPromise> Navigate(const ClientNavigateArgs& aArgs);

  RefPtr<ClientOpPromise> MatchAll(const ClientMatchAllArgs& aArgs);

  RefPtr<ClientOpPromise> Claim(const ClientClaimArgs& aArgs);

  RefPtr<ClientOpPromise> GetInfoAndState(
      const ClientGetInfoAndStateArgs& aArgs);

  RefPtr<ClientOpPromise> OpenWindow(
      const ClientOpenWindowArgs& aArgs,
      already_AddRefed<ContentParent> aSourceProcess);

  bool HasWindow(const Maybe<ContentParentId>& aContentParentId,
                 const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                 const nsID& aClientId);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManagerService)

 private:
  ClientManagerService();
  ~ClientManagerService();

  void Shutdown();

  // Returns `nullptr` if the `ClientSourceParent*` doesn't exist.
  ClientSourceParent* FindExistingSource(
      const nsID& aID, const mozilla::ipc::PrincipalInfo& aPrincipalInfo) const;

  // Represents a `ClientSourceParent` that may possibly be created and add
  // itself in the future.
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
      mPromiseHolder.ResolveIfExists(aSource, __func__);
    }

    void RejectPromiseIfExists(nsresult aRv) {
      mPromiseHolder.RejectIfExists(aRv, __func__);
    }

   private:
    const mozilla::ipc::PrincipalInfo mPrincipalInfo;
    MozPromiseHolder<SourcePromise> mPromiseHolder;
  };

  using SourceTableEntry =
      Variant<FutureClientSourceParent, ClientSourceParent*>;

  // Returns `nullptr` if `aEntry` isn't a `ClientSourceParent*`.
  friend inline ClientSourceParent* MaybeUnwrapAsExistingSource(
      const SourceTableEntry& aEntry);

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
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientManagerService_h
