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
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "ExpandedPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "mozilla/LoadInfo.h"
#include "ContentPrincipal.h"
#include "NullPrincipal.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/nsRedirectHistoryEntry.h"
#include "URIUtils.h"

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
    nsContentUtils::GetSecurityManager();
  if (!secMan) {
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

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      principal = NullPrincipal::Create(info.attrs(), uri);
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

      OriginAttributes attrs;
      if (info.attrs().mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
        attrs = info.attrs();
      }
      principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
      if (NS_WARN_IF(!principal)) {
        return nullptr;
      }

      // Origin must match what the_new_principal.getOrigin returns.
      nsAutoCString originNoSuffix;
      rv = principal->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv)) ||
          !info.originNoSuffix().Equals(originNoSuffix)) {
        MOZ_CRASH("Origin must be available when deserialized");
      }

      return principal.forget();
    }

    case PrincipalInfo::TExpandedPrincipalInfo: {
      const ExpandedPrincipalInfo& info = aPrincipalInfo.get_ExpandedPrincipalInfo();

      nsTArray<nsCOMPtr<nsIPrincipal>> whitelist;
      nsCOMPtr<nsIPrincipal> wlPrincipal;

      for (uint32_t i = 0; i < info.whitelist().Length(); i++) {
        wlPrincipal = PrincipalInfoToPrincipal(info.whitelist()[i], &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
        // append that principal to the whitelist
        whitelist.AppendElement(wlPrincipal);
      }

      RefPtr<ExpandedPrincipal> expandedPrincipal =
        ExpandedPrincipal::Create(whitelist, info.attrs());
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

  if (aPrincipal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!uri)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString spec;
    rv = uri->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    *aPrincipalInfo =
      NullPrincipalInfo(aPrincipal->OriginAttributesRef(), spec);
    return NS_OK;
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    nsContentUtils::GetSecurityManager();
  if (!secMan) {
    return NS_ERROR_FAILURE;
  }

  bool isSystemPrincipal;
  nsresult rv = secMan->IsSystemPrincipal(aPrincipal, &isSystemPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isSystemPrincipal) {
    *aPrincipalInfo = SystemPrincipalInfo();
    return NS_OK;
  }

  // might be an expanded principal
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  if (basePrin->Is<ExpandedPrincipal>()) {
    auto* expanded = basePrin->As<ExpandedPrincipal>();

    nsTArray<PrincipalInfo> whitelistInfo;
    PrincipalInfo info;

    for (auto& prin : expanded->WhiteList()) {
      rv = PrincipalToPrincipalInfo(prin, &info);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // append that spec to the whitelist
      whitelistInfo.AppendElement(info);
    }

    *aPrincipalInfo =
      ExpandedPrincipalInfo(aPrincipal->OriginAttributesRef(),
                            Move(whitelistInfo));
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

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString originNoSuffix;
  rv = aPrincipal->GetOriginNoSuffix(originNoSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aPrincipalInfo = ContentPrincipalInfo(aPrincipal->OriginAttributesRef(),
                                         originNoSuffix, spec);
  return NS_OK;
}

bool
IsPincipalInfoPrivate(const PrincipalInfo& aPrincipalInfo)
{
  if (aPrincipalInfo.type() != ipc::PrincipalInfo::TContentPrincipalInfo) {
    return false;
  }

  const ContentPrincipalInfo& info = aPrincipalInfo.get_ContentPrincipalInfo();
  return !!info.attrs().mPrivateBrowsingId;
}

already_AddRefed<nsIRedirectHistoryEntry>
RHEntryInfoToRHEntry(const RedirectHistoryEntryInfo& aRHEntryInfo)
{
  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aRHEntryInfo.principalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> referrerUri = DeserializeURI(aRHEntryInfo.referrerUri());

  nsCOMPtr<nsIRedirectHistoryEntry> entry =
    new nsRedirectHistoryEntry(principal, referrerUri, aRHEntryInfo.remoteAddress());

  return entry.forget();
}

nsresult
RHEntryToRHEntryInfo(nsIRedirectHistoryEntry* aRHEntry,
                     RedirectHistoryEntryInfo* aRHEntryInfo)
{
  MOZ_ASSERT(aRHEntry);
  MOZ_ASSERT(aRHEntryInfo);

  nsresult rv;
  aRHEntry->GetRemoteAddress(aRHEntryInfo->remoteAddress());

  nsCOMPtr<nsIURI> referrerUri;
  rv = aRHEntry->GetReferrerURI(getter_AddRefs(referrerUri));
  NS_ENSURE_SUCCESS(rv, rv);
  SerializeURI(referrerUri, aRHEntryInfo->referrerUri());

  nsCOMPtr<nsIPrincipal> principal;
  rv = aRHEntry->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return PrincipalToPrincipalInfo(principal, &aRHEntryInfo->principalInfo());
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

  OptionalPrincipalInfo principalToInheritInfo = mozilla::void_t();
  if (aLoadInfo->PrincipalToInherit()) {
    PrincipalInfo principalToInheritInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->PrincipalToInherit(),
                                  &principalToInheritInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    principalToInheritInfo = principalToInheritInfoTemp;
  }

  OptionalPrincipalInfo sandboxedLoadingPrincipalInfo = mozilla::void_t();
  if (aLoadInfo->GetLoadingSandboxed()) {
    PrincipalInfo sandboxedLoadingPrincipalInfoTemp;
    nsCOMPtr<nsIPrincipal> sandboxedLoadingPrincipal;
    rv = aLoadInfo->GetSandboxedLoadingPrincipal(
        getter_AddRefs(sandboxedLoadingPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = PrincipalToPrincipalInfo(sandboxedLoadingPrincipal,
                                  &sandboxedLoadingPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    sandboxedLoadingPrincipalInfo = sandboxedLoadingPrincipalInfoTemp;
  }

  OptionalURIParams optionalResultPrincipalURI = mozilla::void_t();
  nsCOMPtr<nsIURI> resultPrincipalURI;
  Unused << aLoadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
  if (resultPrincipalURI) {
    SerializeURI(resultPrincipalURI, optionalResultPrincipalURI);
  }

  nsTArray<RedirectHistoryEntryInfo> redirectChainIncludingInternalRedirects;
  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectEntry :
       aLoadInfo->RedirectChainIncludingInternalRedirects()) {
    RedirectHistoryEntryInfo* entry = redirectChainIncludingInternalRedirects.AppendElement();
    rv = RHEntryToRHEntryInfo(redirectEntry, entry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsTArray<RedirectHistoryEntryInfo> redirectChain;
  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectEntry :
       aLoadInfo->RedirectChain()) {
    RedirectHistoryEntryInfo* entry = redirectChain.AppendElement();
    rv = RHEntryToRHEntryInfo(redirectEntry, entry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsTArray<PrincipalInfo> ancestorPrincipals;
  ancestorPrincipals.SetCapacity(aLoadInfo->AncestorPrincipals().Length());
  for (const auto& principal : aLoadInfo->AncestorPrincipals()) {
    rv = PrincipalToPrincipalInfo(principal, ancestorPrincipals.AppendElement());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aOptionalLoadInfoArgs =
    LoadInfoArgs(
      loadingPrincipalInfo,
      triggeringPrincipalInfo,
      principalToInheritInfo,
      sandboxedLoadingPrincipalInfo,
      optionalResultPrincipalURI,
      aLoadInfo->GetSecurityFlags(),
      aLoadInfo->InternalContentPolicyType(),
      static_cast<uint32_t>(aLoadInfo->GetTainting()),
      aLoadInfo->GetUpgradeInsecureRequests(),
      aLoadInfo->GetVerifySignedContent(),
      aLoadInfo->GetEnforceSRI(),
      aLoadInfo->GetForceAllowDataURI(),
      aLoadInfo->GetForceInheritPrincipalDropped(),
      aLoadInfo->GetInnerWindowID(),
      aLoadInfo->GetOuterWindowID(),
      aLoadInfo->GetParentOuterWindowID(),
      aLoadInfo->GetTopOuterWindowID(),
      aLoadInfo->GetFrameOuterWindowID(),
      aLoadInfo->GetEnforceSecurity(),
      aLoadInfo->GetInitialSecurityCheckDone(),
      aLoadInfo->GetIsInThirdPartyContext(),
      aLoadInfo->GetOriginAttributes(),
      redirectChainIncludingInternalRedirects,
      redirectChain,
      ancestorPrincipals,
      aLoadInfo->AncestorOuterWindowIDs(),
      aLoadInfo->CorsUnsafeHeaders(),
      aLoadInfo->GetForcePreflight(),
      aLoadInfo->GetIsPreflight(),
      aLoadInfo->GetLoadTriggeredFromExternal(),
      aLoadInfo->GetServiceWorkerTaintingSynthesized()
      );

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

  nsCOMPtr<nsIPrincipal> principalToInherit;
  if (loadInfoArgs.principalToInheritInfo().type() != OptionalPrincipalInfo::Tvoid_t) {
    principalToInherit = PrincipalInfoToPrincipal(loadInfoArgs.principalToInheritInfo(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPrincipal> sandboxedLoadingPrincipal;
  if (loadInfoArgs.sandboxedLoadingPrincipalInfo().type() != OptionalPrincipalInfo::Tvoid_t) {
    sandboxedLoadingPrincipal =
      PrincipalInfoToPrincipal(loadInfoArgs.sandboxedLoadingPrincipalInfo(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> resultPrincipalURI;
  if (loadInfoArgs.resultPrincipalURI().type() != OptionalURIParams::Tvoid_t) {
    resultPrincipalURI = DeserializeURI(loadInfoArgs.resultPrincipalURI());
    NS_ENSURE_TRUE(resultPrincipalURI, NS_ERROR_UNEXPECTED);
  }

  RedirectHistoryArray redirectChainIncludingInternalRedirects;
  for (const RedirectHistoryEntryInfo& entryInfo :
      loadInfoArgs.redirectChainIncludingInternalRedirects()) {
    nsCOMPtr<nsIRedirectHistoryEntry> redirectHistoryEntry =
      RHEntryInfoToRHEntry(entryInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChainIncludingInternalRedirects.AppendElement(redirectHistoryEntry.forget());
  }

  RedirectHistoryArray redirectChain;
  for (const RedirectHistoryEntryInfo& entryInfo : loadInfoArgs.redirectChain()) {
    nsCOMPtr<nsIRedirectHistoryEntry> redirectHistoryEntry =
      RHEntryInfoToRHEntry(entryInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChain.AppendElement(redirectHistoryEntry.forget());
  }

  nsTArray<nsCOMPtr<nsIPrincipal>> ancestorPrincipals;
  ancestorPrincipals.SetCapacity(loadInfoArgs.ancestorPrincipals().Length());
  for (const PrincipalInfo& principalInfo : loadInfoArgs.ancestorPrincipals()) {
    nsCOMPtr<nsIPrincipal> ancestorPrincipal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    ancestorPrincipals.AppendElement(ancestorPrincipal.forget());
  }

  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(loadingPrincipal,
                          triggeringPrincipal,
                          principalToInherit,
                          sandboxedLoadingPrincipal,
                          resultPrincipalURI,
                          loadInfoArgs.securityFlags(),
                          loadInfoArgs.contentPolicyType(),
                          static_cast<LoadTainting>(loadInfoArgs.tainting()),
                          loadInfoArgs.upgradeInsecureRequests(),
                          loadInfoArgs.verifySignedContent(),
                          loadInfoArgs.enforceSRI(),
                          loadInfoArgs.forceAllowDataURI(),
                          loadInfoArgs.forceInheritPrincipalDropped(),
                          loadInfoArgs.innerWindowID(),
                          loadInfoArgs.outerWindowID(),
                          loadInfoArgs.parentOuterWindowID(),
                          loadInfoArgs.topOuterWindowID(),
                          loadInfoArgs.frameOuterWindowID(),
                          loadInfoArgs.enforceSecurity(),
                          loadInfoArgs.initialSecurityCheckDone(),
                          loadInfoArgs.isInThirdPartyContext(),
                          loadInfoArgs.originAttributes(),
                          redirectChainIncludingInternalRedirects,
                          redirectChain,
                          Move(ancestorPrincipals),
                          loadInfoArgs.ancestorOuterWindowIDs(),
                          loadInfoArgs.corsUnsafeHeaders(),
                          loadInfoArgs.forcePreflight(),
                          loadInfoArgs.isPreflight(),
                          loadInfoArgs.loadTriggeredFromExternal(),
                          loadInfoArgs.serviceWorkerTaintingSynthesized()
                          );

   loadInfo.forget(outLoadInfo);
   return NS_OK;
}

} // namespace ipc
} // namespace mozilla
