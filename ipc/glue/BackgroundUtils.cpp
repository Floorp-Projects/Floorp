/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundUtils.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
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
#include "nsTArray.h"

namespace mozilla {
namespace net {
class OptionalLoadInfoArgs;
}

using mozilla::BasePrincipal;
using namespace mozilla::net;

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
      const NullPrincipalInfo& info =
        aPrincipalInfo.get_NullPrincipalInfo();
      principal = nsNullPrincipal::Create(info.attrs());

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

      PrincipalOriginAttributes attrs;
      if (info.attrs().mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
        attrs = info.attrs();
      }
      principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
      rv = principal ? NS_OK : NS_ERROR_FAILURE;
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

      RefPtr<nsExpandedPrincipal> expandedPrincipal = new nsExpandedPrincipal(whitelist);
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

  bool isNullPrin;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isNullPrin) {
    *aPrincipalInfo = NullPrincipalInfo(BasePrincipal::Cast(aPrincipal)->OriginAttributesRef());
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
    MOZ_ALWAYS_SUCCEEDS(expanded->GetWhiteList(&whitelist));

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

  *aPrincipalInfo = ContentPrincipalInfo(BasePrincipal::Cast(aPrincipal)->OriginAttributesRef(),
                                         spec);
  return NS_OK;
}

nsresult
LoadInfoToLoadInfoArgs(nsILoadInfo *aLoadInfo,
                       OptionalLoadInfoArgs* aOptionalLoadInfoArgs)
{
  if (!aLoadInfo) {
    // if there is no loadInfo, then there is nothing to serialize
    *aOptionalLoadInfoArgs = void_t();
    return NS_OK;
  }

  nsresult rv = NS_OK;
  OptionalPrincipalInfo loadingPrincipalInfo = mozilla::void_t();
  if (aLoadInfo->LoadingPrincipal()) {
    PrincipalInfo loadingPrincipalInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->LoadingPrincipal(),
                                  &loadingPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    loadingPrincipalInfo = loadingPrincipalInfoTemp;
  }

  PrincipalInfo triggeringPrincipalInfo;
  rv = PrincipalToPrincipalInfo(aLoadInfo->TriggeringPrincipal(),
                                &triggeringPrincipalInfo);

  nsTArray<PrincipalInfo> redirectChainIncludingInternalRedirects;
  for (const nsCOMPtr<nsIPrincipal>& principal : aLoadInfo->RedirectChainIncludingInternalRedirects()) {
    rv = PrincipalToPrincipalInfo(principal, redirectChainIncludingInternalRedirects.AppendElement());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsTArray<PrincipalInfo> redirectChain;
  for (const nsCOMPtr<nsIPrincipal>& principal : aLoadInfo->RedirectChain()) {
    rv = PrincipalToPrincipalInfo(principal, redirectChain.AppendElement());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aOptionalLoadInfoArgs =
    LoadInfoArgs(
      loadingPrincipalInfo,
      triggeringPrincipalInfo,
      aLoadInfo->GetSecurityFlags(),
      aLoadInfo->InternalContentPolicyType(),
      static_cast<uint32_t>(aLoadInfo->GetTainting()),
      aLoadInfo->GetUpgradeInsecureRequests(),
      aLoadInfo->GetVerifySignedContent(),
      aLoadInfo->GetEnforceSRI(),
      aLoadInfo->GetInnerWindowID(),
      aLoadInfo->GetOuterWindowID(),
      aLoadInfo->GetParentOuterWindowID(),
      aLoadInfo->GetFrameOuterWindowID(),
      aLoadInfo->GetEnforceSecurity(),
      aLoadInfo->GetInitialSecurityCheckDone(),
      aLoadInfo->GetIsInThirdPartyContext(),
      aLoadInfo->GetOriginAttributes(),
      redirectChainIncludingInternalRedirects,
      redirectChain,
      aLoadInfo->CorsUnsafeHeaders(),
      aLoadInfo->GetForcePreflight(),
      aLoadInfo->GetIsPreflight(),
      aLoadInfo->GetForceHSTSPriming(),
      aLoadInfo->GetMixedContentWouldBlock());

  return NS_OK;
}

nsresult
LoadInfoArgsToLoadInfo(const OptionalLoadInfoArgs& aOptionalLoadInfoArgs,
                       nsILoadInfo** outLoadInfo)
{
  if (aOptionalLoadInfoArgs.type() == OptionalLoadInfoArgs::Tvoid_t) {
    *outLoadInfo = nullptr;
    return NS_OK;
  }

  const LoadInfoArgs& loadInfoArgs =
    aOptionalLoadInfoArgs.get_LoadInfoArgs();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  if (loadInfoArgs.requestingPrincipalInfo().type() != OptionalPrincipalInfo::Tvoid_t) {
    loadingPrincipal = PrincipalInfoToPrincipal(loadInfoArgs.requestingPrincipalInfo(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
    PrincipalInfoToPrincipal(loadInfoArgs.triggeringPrincipalInfo(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCOMPtr<nsIPrincipal>> redirectChainIncludingInternalRedirects;
  for (const PrincipalInfo& principalInfo : loadInfoArgs.redirectChainIncludingInternalRedirects()) {
    nsCOMPtr<nsIPrincipal> redirectedPrincipal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChainIncludingInternalRedirects.AppendElement(redirectedPrincipal.forget());
  }

  nsTArray<nsCOMPtr<nsIPrincipal>> redirectChain;
  for (const PrincipalInfo& principalInfo : loadInfoArgs.redirectChain()) {
    nsCOMPtr<nsIPrincipal> redirectedPrincipal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChain.AppendElement(redirectedPrincipal.forget());
  }

  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(loadingPrincipal,
                          triggeringPrincipal,
                          loadInfoArgs.securityFlags(),
                          loadInfoArgs.contentPolicyType(),
                          static_cast<LoadTainting>(loadInfoArgs.tainting()),
                          loadInfoArgs.upgradeInsecureRequests(),
                          loadInfoArgs.verifySignedContent(),
                          loadInfoArgs.enforceSRI(),
                          loadInfoArgs.innerWindowID(),
                          loadInfoArgs.outerWindowID(),
                          loadInfoArgs.parentOuterWindowID(),
                          loadInfoArgs.frameOuterWindowID(),
                          loadInfoArgs.enforceSecurity(),
                          loadInfoArgs.initialSecurityCheckDone(),
                          loadInfoArgs.isInThirdPartyContext(),
                          loadInfoArgs.originAttributes(),
                          redirectChainIncludingInternalRedirects,
                          redirectChain,
                          loadInfoArgs.corsUnsafeHeaders(),
                          loadInfoArgs.forcePreflight(),
                          loadInfoArgs.isPreflight(),
                          loadInfoArgs.forceHSTSPriming(),
                          loadInfoArgs.mixedContentWouldBlock()
                          );

   loadInfo.forget(outLoadInfo);
   return NS_OK;
}

} // namespace ipc
} // namespace mozilla
