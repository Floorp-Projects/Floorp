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
enum class Mode {
  Mode_Default,
  Mode_Child,
  Mode_Parent,
};

class ClientChannelHelper final : public nsIInterfaceRequestor,
                                  public nsIChannelEventSink {
  nsCOMPtr<nsIInterfaceRequestor> mOuter;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  ~ClientChannelHelper() = default;

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
      // If we're running in the child, but redirects are handled by the parent
      // then our reserved/initial info should already have been moved to the
      // new channel via the parent. If they still match, then we can copy our
      // reserved client source to the new channel, since that isn't passed
      // between processes.
      if (mMode == Mode::Mode_Child) {
        Maybe<ClientInfo> newClientInfo = newLoadInfo->GetReservedClientInfo();
        if (reservedClient && newClientInfo &&
            reservedClient->Info() == *newClientInfo) {
          newLoadInfo->GiveReservedClientSource(std::move(reservedClient));
        }
      } else {
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

      // If we're managing redirects in the parent, then we don't want
      // to create a new ClientSource (since those need to live with
      // the global), so just allocate a new ClientInfo/id and we can
      // create a ClientSource when the final channel propagates back
      // to the child.
      if (mMode == Mode::Mode_Parent) {
        Maybe<ClientInfo> reservedInfo =
            ClientManager::CreateInfo(ClientType::Window, principal);
        if (reservedInfo) {
          newLoadInfo->SetReservedClientInfo(*reservedInfo);
        }
      } else {
        reservedClient.reset();

        const Maybe<ClientInfo>& reservedClientInfo =
            newLoadInfo->GetReservedClientInfo();
        // If we're in the child, but the parent managed redirects for
        // us then it might have allocated an id for us already. If
        // so, then just create the ClientSource for that existing
        // info.
        if (reservedClientInfo && mMode == Mode::Mode_Child) {
          reservedClient = ClientManager::CreateSourceFromInfo(
              *reservedClientInfo, mEventTarget);
        } else {
          // Create the new ClientSource.  This should only happen for window
          // Clients since support cross-origin redirects are blocked by the
          // same-origin security policy.
          reservedClient = ClientManager::CreateSource(ClientType::Window,
                                                       mEventTarget, principal);
        }
        MOZ_DIAGNOSTIC_ASSERT(reservedClient);

        newLoadInfo->GiveReservedClientSource(std::move(reservedClient));
      }
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
                      nsISerialEventTarget* aEventTarget, Mode aMode)
      : mOuter(aOuter), mEventTarget(aEventTarget), mMode(aMode) {}

  NS_DECL_ISUPPORTS

  Mode mMode;
};

NS_IMPL_ISUPPORTS(ClientChannelHelper, nsIInterfaceRequestor,
                  nsIChannelEventSink);

}  // anonymous namespace

nsresult AddClientChannelHelper(nsIChannel* aChannel,
                                Maybe<ClientInfo>&& aReservedClientInfo,
                                Maybe<ClientInfo>&& aInitialClientInfo,
                                nsISerialEventTarget* aEventTarget,
                                bool aManagedInParent) {
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

  UniquePtr<ClientSource> reservedClient;
  if (initialClientInfo.isNothing() && reservedClientInfo.isNothing()) {
    // Wait to reserve the client until we are reasonably sure this method
    // will succeed.  We should only follow this path for window clients.
    // Workers should always provide a reserved ClientInfo since their
    // ClientSource object is owned by a different thread.
    reservedClient = ClientManager::CreateSource(
        ClientType::Window, aEventTarget, channelPrincipal);
    MOZ_DIAGNOSTIC_ASSERT(reservedClient);
  }

  RefPtr<ClientChannelHelper> helper = new ClientChannelHelper(
      outerCallbacks, aEventTarget,
      aManagedInParent ? Mode::Mode_Child : Mode::Mode_Default);

  // Only set the callbacks helper if we are able to reserve the client
  // successfully.
  rv = aChannel->SetNotificationCallbacks(helper);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally preserve the various client values on the nsILoadInfo once the
  // redirect helper has been added to the channel.
  if (reservedClient) {
    loadInfo->GiveReservedClientSource(std::move(reservedClient));
  }

  if (initialClientInfo.isSome()) {
    loadInfo->SetInitialClientInfo(initialClientInfo.ref());
  }

  if (reservedClientInfo.isSome()) {
    loadInfo->SetReservedClientInfo(reservedClientInfo.ref());
  }

  return NS_OK;
}

nsresult AddClientChannelHelperInParent(nsIChannel* aChannel,
                                        nsISerialEventTarget* aEventTarget) {
  nsCOMPtr<nsIInterfaceRequestor> outerCallbacks;
  nsresult rv =
      aChannel->GetNotificationCallbacks(getter_AddRefs(outerCallbacks));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<ClientChannelHelper> helper =
      new ClientChannelHelper(outerCallbacks, aEventTarget, Mode::Mode_Parent);

  // Only set the callbacks helper if we are able to reserve the client
  // successfully.
  rv = aChannel->SetNotificationCallbacks(helper);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
