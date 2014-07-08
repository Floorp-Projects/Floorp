/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundUtils.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
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

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t appId;
  rv = aPrincipal->GetAppId(&appId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isInBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aPrincipalInfo = ContentPrincipalInfo(appId, isInBrowserElement, spec);
  return NS_OK;
}

} // namespace ipc
} // namespace mozilla
