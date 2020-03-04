/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientChannelHelper.h"

#include "ClientManager.h"
#include "ClientSource.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

namespace mozilla {
namespace dom {

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
          newLoadInfo->SetReservedClientInfo(reservedClientInfo.ref());
        }

        if (initialClientInfo.isSome()) {
          newLoadInfo->SetInitialClientInfo(initialClientInfo.ref());
        }
      }
    }

    // If it's a cross-origin redirect then we discard the old reserved client
    // and create a new one.
    else {
      // If CheckSameOrigin() worked, then the security manager must exist.
      nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
      MOZ_DIAGNOSTIC_ASSERT(ssm);

      nsCOMPtr<nsIPrincipal> principal;
      rv = ssm->GetChannelResultPrincipal(aNewChannel,
                                          getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      reservedClient.reset();
      CreateClient(newLoadInfo, principal);
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

  static void CreateClientForPrincipal(nsILoadInfo* aLoadInfo,
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
  ~ClientChannelHelperParent() = default;

  void CreateClient(nsILoadInfo* aLoadInfo, nsIPrincipal* aPrincipal) override {
    CreateClientForPrincipal(aLoadInfo, aPrincipal, mEventTarget);
  }

 public:
  static void CreateClientForPrincipal(nsILoadInfo* aLoadInfo,
                                       nsIPrincipal* aPrincipal,
                                       nsISerialEventTarget* aEventTarget) {
    // If we're managing redirects in the parent, then we don't want
    // to create a new ClientSource (since those need to live with
    // the global), so just allocate a new ClientInfo/id and we can
    // create a ClientSource when the final channel propagates back
    // to the child.
    Maybe<ClientInfo> reservedInfo =
        ClientManager::CreateInfo(ClientType::Window, aPrincipal);
    if (reservedInfo) {
      aLoadInfo->SetReservedClientInfo(*reservedInfo);
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

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(ssm, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = ssm->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(channelPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Only allow the initial ClientInfo to be set if the current channel
  // principal matches.
  if (initialClientInfo.isSome()) {
    nsCOMPtr<nsIPrincipal> initialPrincipal = PrincipalInfoToPrincipal(
        initialClientInfo.ref().PrincipalInfo(), nullptr);

    bool equals = false;
    rv = initialPrincipal ? initialPrincipal->Equals(channelPrincipal, &equals)
                          : NS_ERROR_FAILURE;
    if (NS_FAILED(rv) || !equals) {
      initialClientInfo.reset();
    }
  }

  // Only allow the reserved ClientInfo to be set if the current channel
  // principal matches.
  if (reservedClientInfo.isSome()) {
    nsCOMPtr<nsIPrincipal> reservedPrincipal = PrincipalInfoToPrincipal(
        reservedClientInfo.ref().PrincipalInfo(), nullptr);

    bool equals = false;
    rv = reservedPrincipal
             ? reservedPrincipal->Equals(channelPrincipal, &equals)
             : NS_ERROR_FAILURE;
    if (NS_FAILED(rv) || !equals) {
      reservedClientInfo.reset();
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> outerCallbacks;
  rv = aChannel->GetNotificationCallbacks(getter_AddRefs(outerCallbacks));
  NS_ENSURE_SUCCESS(rv, rv);

  if (initialClientInfo.isNothing() && reservedClientInfo.isNothing()) {
    T::CreateClientForPrincipal(loadInfo, channelPrincipal, aEventTarget);
  }

  RefPtr<ClientChannelHelper> helper = new T(outerCallbacks, aEventTarget);

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

}  // namespace dom
}  // namespace mozilla
