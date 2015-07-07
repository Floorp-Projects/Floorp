/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundUtils.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "mozilla/LoadInfo.h"
#include "nsNullPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

namespace mozilla {
namespace ipc {

already_AddRefed<nsIPrincipal>
PrincipalInfoToPrincipal(const PrincipalInfo& aPrincipalInfo,
                         nsresult* aOptionalResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::T__None);

  nsresult stackResult;
  nsresult& rv = aOptionalResult ? *aOptionalResult : stackResult;

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> principal;

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      rv = secMan->GetSystemPrincipal(getter_AddRefs(principal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      return principal.forget();
    }

    case PrincipalInfo::TNullPrincipalInfo: {
      principal = do_CreateInstance(NS_NULLPRINCIPAL_CONTRACTID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      return principal.forget();
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
        aPrincipalInfo.get_ContentPrincipalInfo();

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      if (info.appId() == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
        rv = secMan->GetSimpleCodebasePrincipal(uri, getter_AddRefs(principal));
      } else {
        rv = secMan->GetAppCodebasePrincipal(uri,
                                             info.appId(),
                                             info.isInBrowserElement(),
                                             getter_AddRefs(principal));
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      return principal.forget();
    }

    case PrincipalInfo::TExpandedPrincipalInfo: {
      const ExpandedPrincipalInfo& info = aPrincipalInfo.get_ExpandedPrincipalInfo();

      nsTArray< nsCOMPtr<nsIPrincipal> > whitelist;
      nsCOMPtr<nsIPrincipal> wlPrincipal;

      for (uint32_t i = 0; i < info.whitelist().Length(); i++) {
        wlPrincipal = PrincipalInfoToPrincipal(info.whitelist()[i], &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
        // append that principal to the whitelist
        whitelist.AppendElement(wlPrincipal);
      }

      nsRefPtr<nsExpandedPrincipal> expandedPrincipal = new nsExpandedPrincipal(whitelist);
      if (!expandedPrincipal) {
        NS_WARNING("could not instantiate expanded principal");
        return nullptr;
      }

      principal = expandedPrincipal;
      return principal.forget();
    }

    default:
      MOZ_CRASH("Unknown PrincipalInfo type!");
  }

  MOZ_CRASH("Should never get here!");
}

nsresult
PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                         PrincipalInfo* aPrincipalInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aPrincipalInfo);

  bool isNullPointer;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPointer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isNullPointer) {
    *aPrincipalInfo = NullPrincipalInfo();
    return NS_OK;
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isSystemPrincipal;
  rv = secMan->IsSystemPrincipal(aPrincipal, &isSystemPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isSystemPrincipal) {
    *aPrincipalInfo = SystemPrincipalInfo();
    return NS_OK;
  }

  // might be an expanded principal
  nsCOMPtr<nsIExpandedPrincipal> expanded =
    do_QueryInterface(aPrincipal);

  if (expanded) {
    nsTArray<PrincipalInfo> whitelistInfo;
    PrincipalInfo info;

    nsTArray< nsCOMPtr<nsIPrincipal> >* whitelist;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(expanded->GetWhiteList(&whitelist)));

    for (uint32_t i = 0; i < whitelist->Length(); i++) {
      rv = PrincipalToPrincipalInfo((*whitelist)[i], &info);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // append that spec to the whitelist
      whitelistInfo.AppendElement(info);
    }

    *aPrincipalInfo = ExpandedPrincipalInfo(Move(whitelistInfo));
    return NS_OK;
  }

  // must be a content principal

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!uri)) {
    return NS_ERROR_FAILURE;
  }

  nsCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isUnknownAppId;
  rv = aPrincipal->GetUnknownAppId(&isUnknownAppId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t appId;
  if (isUnknownAppId) {
    appId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
  } else {
    rv = aPrincipal->GetAppId(&appId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  bool isInBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aPrincipalInfo = ContentPrincipalInfo(appId, isInBrowserElement, spec);
  return NS_OK;
}

nsresult
LoadInfoToLoadInfoArgs(nsILoadInfo *aLoadInfo,
                       mozilla::net::LoadInfoArgs* aLoadInfoArgs)
{
  nsresult rv = NS_OK;

  if (aLoadInfo) {
    rv = PrincipalToPrincipalInfo(aLoadInfo->LoadingPrincipal(),
                                  &aLoadInfoArgs->requestingPrincipalInfo());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = PrincipalToPrincipalInfo(aLoadInfo->TriggeringPrincipal(),
                                  &aLoadInfoArgs->triggeringPrincipalInfo());
    NS_ENSURE_SUCCESS(rv, rv);
    aLoadInfoArgs->securityFlags() = aLoadInfo->GetSecurityFlags();
    aLoadInfoArgs->contentPolicyType() = aLoadInfo->GetContentPolicyType();
    aLoadInfoArgs->innerWindowID() = aLoadInfo->GetInnerWindowID();
    aLoadInfoArgs->outerWindowID() = aLoadInfo->GetOuterWindowID();
    aLoadInfoArgs->parentOuterWindowID() = aLoadInfo->GetParentOuterWindowID();
    return NS_OK;
  }

  // use default values if no loadInfo is provided
  rv = PrincipalToPrincipalInfo(nsContentUtils::GetSystemPrincipal(),
                                &aLoadInfoArgs->requestingPrincipalInfo());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = PrincipalToPrincipalInfo(nsContentUtils::GetSystemPrincipal(),
                                &aLoadInfoArgs->triggeringPrincipalInfo());
  NS_ENSURE_SUCCESS(rv, rv);
  aLoadInfoArgs->securityFlags() = nsILoadInfo::SEC_NORMAL;
  aLoadInfoArgs->contentPolicyType() = nsIContentPolicy::TYPE_OTHER;
  aLoadInfoArgs->innerWindowID() = 0;
  aLoadInfoArgs->outerWindowID() = 0;
  aLoadInfoArgs->parentOuterWindowID() = 0;
  return NS_OK;
}

nsresult
LoadInfoArgsToLoadInfo(const mozilla::net::LoadInfoArgs& aLoadInfoArgs,
                       nsILoadInfo** outLoadInfo)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> requestingPrincipal =
    PrincipalInfoToPrincipal(aLoadInfoArgs.requestingPrincipalInfo(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
    PrincipalInfoToPrincipal(aLoadInfoArgs.triggeringPrincipalInfo(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(requestingPrincipal,
                          triggeringPrincipal,
                          aLoadInfoArgs.securityFlags(),
                          aLoadInfoArgs.contentPolicyType(),
                          aLoadInfoArgs.innerWindowID(),
                          aLoadInfoArgs.outerWindowID(),
                          aLoadInfoArgs.parentOuterWindowID());

  loadInfo.forget(outLoadInfo);
  return NS_OK;
}

} // namespace ipc
} // namespace mozilla
