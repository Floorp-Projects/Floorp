/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientChannelHelper.h"

#include "ClientManager.h"
#include "ClientSource.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/ClientsBinding.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

namespace mozilla::dom {

using mozilla::ipc::PrincipalInfoToPrincipal;

namespace {

// In the default mode, ClientChannelHelper runs in the content process and
// handles all redirects. When we use DocumentChannel, redirects aren't exposed
// to the content process, so we run an instance of this in both processes, one
// to handle redirects in the parent and one to handle the final channel
// replacement (DocumentChannelChild 'redirects' to the final channel) in the
// child.

class ClientChannelHelper : public nsIInterfaceRequestor,
                            public nsIChannelEventSink {
 protected:
  nsCOMPtr<nsIInterfaceRequestor> mOuter;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  virtual ~ClientChannelHelper() = default;

  NS_IMETHOD
  GetInterface(const nsIID& aIID, void** aResultOut) override {
    if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
      *aResultOut = static_cast<nsIChannelEventSink*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }

    if (mOuter) {
      return mOuter->GetInterface(aIID, aResultOut);
    }

    return NS_ERROR_NO_INTERFACE;
  }

  virtual void CreateClient(nsILoadInfo* aLoadInfo, nsIPrincipal* aPrincipal) {
    CreateClientForPrincipal(aLoadInfo, aPrincipal, mEventTarget);
  }

  NS_IMETHOD
  AsyncOnChannelRedirect(nsIChannel* aOldChannel, nsIChannel* aNewChannel,
                         uint32_t aFlags,
                         nsIAsyncVerifyRedirectCallback* aCallback) override {
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv = nsContentUtils::CheckSameOrigin(aOldChannel, aNewChannel);
    if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_DOM_BAD_URI)) {
      return rv;
    }

    nsCOMPtr<nsILoadInfo> oldLoadInfo = aOldChannel->LoadInfo();
    nsCOMPtr<nsILoadInfo> newLoadInfo = aNewChannel->LoadInfo();

    UniquePtr<ClientSource> reservedClient =
        oldLoadInfo->TakeReservedClientSource();

    // If its a same-origin redirect we just move our reserved client to the
    // new channel.
    if (NS_SUCCEEDED(rv)) {
      if (reservedClient) {
        newLoadInfo->GiveReservedClientSource(std::move(reservedClient));
      }

      // It seems sometimes necko passes two channels with the same LoadInfo.
      // We only need to move the reserved/initial ClientInfo over if we
      // actually have a different LoadInfo.
      else if (oldLoadInfo != newLoadInfo) {
        const Maybe<ClientInfo>& reservedClientInfo =
            oldLoadInfo->GetReservedClientInfo();

        const Maybe<ClientInfo>& initialClientInfo =
            oldLoadInfo->GetInitialClientInfo();

        MOZ_DIAGNOSTIC_ASSERT(reservedClientInfo.isNothing() ||
                              initialClientInfo.isNothing());

        if (reservedClientInfo.isSome()) {
          // Create a new client for the case the controller is cleared for the
          // new loadInfo. ServiceWorkerManager::DispatchFetchEvent() called
          // ServiceWorkerManager::StartControllingClient() making the old
          // client to be controlled eventually. However, the controller setting
          // propagation to the child process could happen later than
          // nsGlobalWindowInner::EnsureClientSource(), such that
          // nsGlobalWindowInner will be controlled as unexpected.
          if (oldLoadInfo->GetController().isSome() &&
              newLoadInfo->GetController().isNothing()) {
            nsCOMPtr<nsIPrincipal> foreignPartitionedPrincipal;
            rv = StoragePrincipalHelper::GetPrincipal(
                aNewChannel,
                StaticPrefs::privacy_partition_serviceWorkers()
                    ? StoragePrincipalHelper::eForeignPartitionedPrincipal
                    : StoragePrincipalHelper::eRegularPrincipal,
                getter_AddRefs(foreignPartitionedPrincipal));
            NS_ENSURE_SUCCESS(rv, rv);
            reservedClient.reset();
            CreateClient(newLoadInfo, foreignPartitionedPrincipal);
          } else {
            newLoadInfo->SetReservedClientInfo(reservedClientInfo.ref());
          }
        }

        if (initialClientInfo.isSome()) {
          newLoadInfo->SetInitialClientInfo(initialClientInfo.ref());
        }
      }
    }

    // If it's a cross-origin redirect then we discard the old reserved client
    // and create a new one.
    else {
      nsCOMPtr<nsIPrincipal> foreignPartitionedPrincipal;
      rv = StoragePrincipalHelper::GetPrincipal(
          aNewChannel,
          StaticPrefs::privacy_partition_serviceWorkers()
              ? StoragePrincipalHelper::eForeignPartitionedPrincipal
              : StoragePrincipalHelper::eRegularPrincipal,
          getter_AddRefs(foreignPartitionedPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

      reservedClient.reset();
      CreateClient(newLoadInfo, foreignPartitionedPrincipal);
    }

    uint32_t redirectMode = nsIHttpChannelInternal::REDIRECT_MODE_MANUAL;
    nsCOMPtr<nsIHttpChannelInternal> http = do_QueryInterface(aOldChannel);
    if (http) {
      MOZ_ALWAYS_SUCCEEDS(http->GetRedirectMode(&redirectMode));
    }

    // Normally we keep the controller across channel redirects, but we must
    // clear it when a document load redirects.  Only do this for real
    // redirects, however.
    //
    // This is effectively described in step 4.2 of:
    //
    //  https://fetch.spec.whatwg.org/#http-fetch
    //
    // The spec sets the service-workers mode to none when the request is
    // configured to *not* follow redirects.  This prevents any further
    // service workers from intercepting.  The first service worker that
    // had a shot at the FetchEvent remains the controller in this case.
    if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL) &&
        redirectMode != nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW) {
      newLoadInfo->ClearController();
    }

    nsCOMPtr<nsIChannelEventSink> outerSink = do_GetInterface(mOuter);
    if (outerSink) {
      return outerSink->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags,
                                               aCallback);
    }

    aCallback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

 public:
  ClientChannelHelper(nsIInterfaceRequestor* aOuter,
                      nsISerialEventTarget* aEventTarget)
      : mOuter(aOuter), mEventTarget(aEventTarget) {}

  NS_DECL_ISUPPORTS

  virtual void CreateClientForPrincipal(nsILoadInfo* aLoadInfo,
                                        nsIPrincipal* aPrincipal,
                                        nsISerialEventTarget* aEventTarget) {
    // Create the new ClientSource.  This should only happen for window
    // Clients since support cross-origin redirects are blocked by the
    // same-origin security policy.
    UniquePtr<ClientSource> reservedClient = ClientManager::CreateSource(
        ClientType::Window, aEventTarget, aPrincipal);
    MOZ_DIAGNOSTIC_ASSERT(reservedClient);

    aLoadInfo->GiveReservedClientSource(std::move(reservedClient));
  }
};

NS_IMPL_ISUPPORTS(ClientChannelHelper, nsIInterfaceRequestor,
                  nsIChannelEventSink);

class ClientChannelHelperParent final : public ClientChannelHelper {
  ~ClientChannelHelperParent() {
    // This requires that if a load completes, the associated ClientSource is
    // created and registers itself before this ClientChannelHelperParent is
    // destroyed. Otherwise, we may incorrectly "forget" a future ClientSource
    // which will actually be created.
    SetFutureSourceInfo(Nothing());
  }

  void CreateClient(nsILoadInfo* aLoadInfo, nsIPrincipal* aPrincipal) override {
    CreateClientForPrincipal(aLoadInfo, aPrincipal, mEventTarget);
  }

  void SetFutureSourceInfo(Maybe<ClientInfo>&& aClientInfo) {
    if (mRecentFutureSourceInfo) {
      // No-op if the corresponding ClientSource has alrady been created, but
      // it's not known if that's the case here.
      ClientManager::ForgetFutureSource(*mRecentFutureSourceInfo);
    }

    if (aClientInfo) {
      Unused << NS_WARN_IF(!ClientManager::ExpectFutureSource(*aClientInfo));
    }

    mRecentFutureSourceInfo = std::move(aClientInfo);
  }

  // Keep track of the most recent ClientInfo created which isn't backed by a
  // ClientSource, which is used to notify ClientManagerService that the
  // ClientSource won't ever actually be constructed.
  Maybe<ClientInfo> mRecentFutureSourceInfo;

 public:
  void CreateClientForPrincipal(nsILoadInfo* aLoadInfo,
                                nsIPrincipal* aPrincipal,
                                nsISerialEventTarget* aEventTarget) override {
    // If we're managing redirects in the parent, then we don't want
    // to create a new ClientSource (since those need to live with
    // the global), so just allocate a new ClientInfo/id and we can
    // create a ClientSource when the final channel propagates back
    // to the child.
    Maybe<ClientInfo> reservedInfo =
        ClientManager::CreateInfo(ClientType::Window, aPrincipal);
    if (reservedInfo) {
      aLoadInfo->SetReservedClientInfo(*reservedInfo);
      SetFutureSourceInfo(std::move(reservedInfo));
    }
  }
  ClientChannelHelperParent(nsIInterfaceRequestor* aOuter,
                            nsISerialEventTarget* aEventTarget)
      : ClientChannelHelper(aOuter, nullptr) {}
};

class ClientChannelHelperChild final : public ClientChannelHelper {
  ~ClientChannelHelperChild() = default;

  NS_IMETHOD
  AsyncOnChannelRedirect(nsIChannel* aOldChannel, nsIChannel* aNewChannel,
                         uint32_t aFlags,
                         nsIAsyncVerifyRedirectCallback* aCallback) override {
    MOZ_ASSERT(NS_IsMainThread());

    // All ClientInfo allocation should have been handled in the parent process
    // by ClientChannelHelperParent, so the only remaining thing to do is to
    // allocate a ClientSource around the ClientInfo on the channel.
    CreateReservedSourceIfNeeded(aNewChannel, mEventTarget);

    nsCOMPtr<nsIChannelEventSink> outerSink = do_GetInterface(mOuter);
    if (outerSink) {
      return outerSink->AsyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags,
                                               aCallback);
    }

    aCallback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

 public:
  ClientChannelHelperChild(nsIInterfaceRequestor* aOuter,
                           nsISerialEventTarget* aEventTarget)
      : ClientChannelHelper(aOuter, aEventTarget) {}
};

}  // anonymous namespace

template <typename T>
nsresult AddClientChannelHelperInternal(nsIChannel* aChannel,
                                        Maybe<ClientInfo>&& aReservedClientInfo,
                                        Maybe<ClientInfo>&& aInitialClientInfo,
                                        nsISerialEventTarget* aEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<ClientInfo> initialClientInfo(std::move(aInitialClientInfo));
  Maybe<ClientInfo> reservedClientInfo(std::move(aReservedClientInfo));
  MOZ_DIAGNOSTIC_ASSERT(reservedClientInfo.isNothing() ||
                        initialClientInfo.isNothing());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsCOMPtr<nsIPrincipal> channelForeignPartitionedPrincipal;
  nsresult rv = StoragePrincipalHelper::GetPrincipal(
      aChannel,
      StaticPrefs::privacy_partition_serviceWorkers()
          ? StoragePrincipalHelper::eForeignPartitionedPrincipal
          : StoragePrincipalHelper::eRegularPrincipal,
      getter_AddRefs(channelForeignPartitionedPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Only allow the initial ClientInfo to be set if the current channel
  // principal matches.
  if (initialClientInfo.isSome()) {
    auto initialPrincipalOrErr =
        PrincipalInfoToPrincipal(initialClientInfo.ref().PrincipalInfo());

    bool equals = false;
    rv = initialPrincipalOrErr.isErr()
             ? initialPrincipalOrErr.unwrapErr()
             : initialPrincipalOrErr.unwrap()->Equals(
                   channelForeignPartitionedPrincipal, &equals);
    if (NS_FAILED(rv) || !equals) {
      initialClientInfo.reset();
    }
  }

  // Only allow the reserved ClientInfo to be set if the current channel
  // principal matches.
  if (reservedClientInfo.isSome()) {
    auto reservedPrincipalOrErr =
        PrincipalInfoToPrincipal(reservedClientInfo.ref().PrincipalInfo());

    bool equals = false;
    rv = reservedPrincipalOrErr.isErr()
             ? reservedPrincipalOrErr.unwrapErr()
             : reservedPrincipalOrErr.unwrap()->Equals(
                   channelForeignPartitionedPrincipal, &equals);
    if (NS_FAILED(rv) || !equals) {
      reservedClientInfo.reset();
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> outerCallbacks;
  rv = aChannel->GetNotificationCallbacks(getter_AddRefs(outerCallbacks));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<ClientChannelHelper> helper = new T(outerCallbacks, aEventTarget);

  if (initialClientInfo.isNothing() && reservedClientInfo.isNothing()) {
    helper->CreateClientForPrincipal(
        loadInfo, channelForeignPartitionedPrincipal, aEventTarget);
  }

  // Only set the callbacks helper if we are able to reserve the client
  // successfully.
  rv = aChannel->SetNotificationCallbacks(helper);
  NS_ENSURE_SUCCESS(rv, rv);

  if (initialClientInfo.isSome()) {
    loadInfo->SetInitialClientInfo(initialClientInfo.ref());
  }

  if (reservedClientInfo.isSome()) {
    loadInfo->SetReservedClientInfo(reservedClientInfo.ref());
  }

  return NS_OK;
}

nsresult AddClientChannelHelper(nsIChannel* aChannel,
                                Maybe<ClientInfo>&& aReservedClientInfo,
                                Maybe<ClientInfo>&& aInitialClientInfo,
                                nsISerialEventTarget* aEventTarget) {
  return AddClientChannelHelperInternal<ClientChannelHelper>(
      aChannel, std::move(aReservedClientInfo), std::move(aInitialClientInfo),
      aEventTarget);
}

nsresult AddClientChannelHelperInParent(
    nsIChannel* aChannel, Maybe<ClientInfo>&& aInitialClientInfo) {
  Maybe<ClientInfo> emptyReservedInfo;
  return AddClientChannelHelperInternal<ClientChannelHelperParent>(
      aChannel, std::move(emptyReservedInfo), std::move(aInitialClientInfo),
      nullptr);
}

nsresult AddClientChannelHelperInChild(nsIChannel* aChannel,
                                       nsISerialEventTarget* aEventTarget) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIInterfaceRequestor> outerCallbacks;
  nsresult rv =
      aChannel->GetNotificationCallbacks(getter_AddRefs(outerCallbacks));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<ClientChannelHelper> helper =
      new ClientChannelHelperChild(outerCallbacks, aEventTarget);

  // Only set the callbacks helper if we are able to reserve the client
  // successfully.
  rv = aChannel->SetNotificationCallbacks(helper);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void CreateReservedSourceIfNeeded(nsIChannel* aChannel,
                                  nsISerialEventTarget* aEventTarget) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  const Maybe<ClientInfo>& reservedClientInfo =
      loadInfo->GetReservedClientInfo();

  if (reservedClientInfo) {
    UniquePtr<ClientSource> reservedClient =
        ClientManager::CreateSourceFromInfo(*reservedClientInfo, aEventTarget);
    loadInfo->GiveReservedClientSource(std::move(reservedClient));
  }
}

}  // namespace mozilla::dom
