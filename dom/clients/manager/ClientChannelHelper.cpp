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
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfoToPrincipal;

namespace {

class ClientChannelHelper final : public nsIInterfaceRequestor
                                , public nsIChannelEventSink
{
  nsCOMPtr<nsIInterfaceRequestor> mOuter;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  ~ClientChannelHelper() = default;

  NS_IMETHOD
  GetInterface(const nsIID& aIID, void** aResultOut) override
  {
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

  NS_IMETHOD
  AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                         nsIChannel* aNewChannel,
                         uint32_t aFlags,
                         nsIAsyncVerifyRedirectCallback *aCallback) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsILoadInfo> oldLoadInfo;
    nsresult rv = aOldChannel->GetLoadInfo(getter_AddRefs(oldLoadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> newLoadInfo;
    rv = aNewChannel->GetLoadInfo(getter_AddRefs(newLoadInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsContentUtils::CheckSameOrigin(aOldChannel, aNewChannel);
    if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_DOM_BAD_URI)) {
      return rv;
    }

    UniquePtr<ClientSource> reservedClient = oldLoadInfo->TakeReservedClientSource();

    // If its a same-origin redirect we just move our reserved client to the
    // new channel.
    if (NS_SUCCEEDED(rv)) {
      if (reservedClient) {
        newLoadInfo->GiveReservedClientSource(Move(reservedClient));
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
      rv = ssm->GetChannelResultPrincipal(aNewChannel, getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      // Create the new ClientSource.  This should only happen for window
      // Clients since support cross-origin redirects are blocked by the
      // same-origin security policy.
      reservedClient.reset();
      reservedClient = ClientManager::CreateSource(ClientType::Window,
                                                   mEventTarget, principal);
      MOZ_DIAGNOSTIC_ASSERT(reservedClient);

      newLoadInfo->GiveReservedClientSource(Move(reservedClient));
    }

    // Normally we keep the controller across channel redirects, but we must
    // clear it when a non-subresource load redirects.  Only do this for real
    // redirects, however.
    //
    // There is an open spec question about what to do in this case for
    // worker script redirects.  For now we clear the controller as that
    // seems most sane. See:
    //
    //  https://github.com/w3c/ServiceWorker/issues/1239
    //
    if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
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
    : mOuter(aOuter)
    , mEventTarget(aEventTarget)
  {
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ClientChannelHelper, nsIInterfaceRequestor,
                                       nsIChannelEventSink);

} // anonymous namespace

nsresult
AddClientChannelHelper(nsIChannel* aChannel,
                       Maybe<ClientInfo>&& aReservedClientInfo,
                       Maybe<ClientInfo>&& aInitialClientInfo,
                       nsISerialEventTarget* aEventTarget)
{
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<ClientInfo> initialClientInfo(Move(aInitialClientInfo));
  Maybe<ClientInfo> reservedClientInfo(Move(aReservedClientInfo));
  MOZ_DIAGNOSTIC_ASSERT(reservedClientInfo.isNothing() ||
                        initialClientInfo.isNothing());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(ssm, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Only allow the initial ClientInfo to be set if the current channel
  // principal matches.
  if (initialClientInfo.isSome()) {
    nsCOMPtr<nsIPrincipal> initialPrincipal =
      PrincipalInfoToPrincipal(initialClientInfo.ref().PrincipalInfo(), nullptr);

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
    nsCOMPtr<nsIPrincipal> reservedPrincipal =
      PrincipalInfoToPrincipal(reservedClientInfo.ref().PrincipalInfo(), nullptr);

    bool equals = false;
    rv = reservedPrincipal ? reservedPrincipal->Equals(channelPrincipal, &equals)
                           : NS_ERROR_FAILURE;
    if (NS_FAILED(rv) || !equals) {
      reservedClientInfo.reset();
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> outerCallbacks;
  rv = aChannel->GetNotificationCallbacks(getter_AddRefs(outerCallbacks));
  NS_ENSURE_SUCCESS(rv, rv);

  UniquePtr<ClientSource> reservedClient;
  if (initialClientInfo.isNothing() && reservedClientInfo.isNothing()) {
    // Wait to reserve the client until we are reasonably sure this method
    // will succeed.  We should only follow this path for window clients.
    // Workers should always provide a reserved ClientInfo since their
    // ClientSource object is owned by a different thread.
    reservedClient = ClientManager::CreateSource(ClientType::Window,
                                                 aEventTarget,
                                                 channelPrincipal);
    MOZ_DIAGNOSTIC_ASSERT(reservedClient);
  }

  RefPtr<ClientChannelHelper> helper =
    new ClientChannelHelper(outerCallbacks, aEventTarget);

  // Only set the callbacks helper if we are able to reserve the client
  // successfully.
  rv = aChannel->SetNotificationCallbacks(helper);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally preserve the various client values on the nsILoadInfo once the
  // redirect helper has been added to the channel.
  if (reservedClient) {
    loadInfo->GiveReservedClientSource(Move(reservedClient));
  }

  if (initialClientInfo.isSome()) {
    loadInfo->SetInitialClientInfo(initialClientInfo.ref());
  }

  if (reservedClientInfo.isSome()) {
    loadInfo->SetReservedClientInfo(reservedClientInfo.ref());
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
