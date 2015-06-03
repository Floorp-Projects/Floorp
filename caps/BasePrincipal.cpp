/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

#include "nsPrincipal.h"
#include "nsNetUtil.h"
#include "nsNullPrincipal.h"
#include "nsScriptSecurityManager.h"

#include "mozilla/dom/CSPDictionariesBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/URLSearchParams.h"

namespace mozilla {

using dom::URLSearchParams;

void
OriginAttributes::CreateSuffix(nsACString& aStr) const
{
  MOZ_RELEASE_ASSERT(mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  nsRefPtr<URLSearchParams> usp = new URLSearchParams();
  nsAutoString value;

  if (mAppId != nsIScriptSecurityManager::NO_APP_ID) {
    value.AppendInt(mAppId);
    usp->Set(NS_LITERAL_STRING("appId"), value);
  }

  if (mInBrowser) {
    usp->Set(NS_LITERAL_STRING("inBrowser"), NS_LITERAL_STRING("1"));
  }

  aStr.Truncate();

  usp->Serialize(value);
  if (!value.IsEmpty()) {
    aStr.AppendLiteral("!");
    aStr.Append(NS_ConvertUTF16toUTF8(value));
  }
}

namespace {

class MOZ_STACK_CLASS PopulateFromSuffixIterator final
  : public URLSearchParams::ForEachIterator
{
public:
  explicit PopulateFromSuffixIterator(OriginAttributes* aOriginAttributes)
    : mOriginAttributes(aOriginAttributes)
  {
    MOZ_ASSERT(aOriginAttributes);
  }

  bool URLSearchParamsIterator(const nsString& aName,
                               const nsString& aValue) override
  {
    if (aName.EqualsLiteral("appId")) {
      nsresult rv;
      mOriginAttributes->mAppId = aValue.ToInteger(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (mOriginAttributes->mAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
        return false;
      }

      return true;
    }

    if (aName.EqualsLiteral("inBrowser")) {
      if (!aValue.EqualsLiteral("1")) {
        return false;
      }

      mOriginAttributes->mInBrowser = true;
      return true;
    }

    // No other attributes are supported.
    return false;
  }

private:
  OriginAttributes* mOriginAttributes;
};

} // anonymous namespace

bool
OriginAttributes::PopulateFromSuffix(const nsACString& aStr)
{
  if (aStr.IsEmpty()) {
    return true;
  }

  if (aStr[0] != '!') {
    return false;
  }

  nsRefPtr<URLSearchParams> usp = new URLSearchParams();
  usp->ParseInput(Substring(aStr, 1, aStr.Length() - 1), nullptr);

  PopulateFromSuffixIterator iterator(this);
  return usp->ForEach(iterator);
}

void
OriginAttributes::CookieJar(nsACString& aStr)
{
  mozilla::GetJarPrefix(mAppId, mInBrowser, aStr);
}

BasePrincipal::BasePrincipal()
{}

BasePrincipal::~BasePrincipal()
{}

NS_IMETHODIMP
BasePrincipal::GetOrigin(nsACString& aOrigin)
{
  nsresult rv = GetOriginInternal(aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString suffix;
  mOriginAttributes.CreateSuffix(suffix);
  aOrigin.Append(suffix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginNoSuffix(nsACString& aOrigin)
{
  return GetOriginInternal(aOrigin);
}

bool
BasePrincipal::Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration)
{
  MOZ_ASSERT(aOther);
  return SubsumesInternal(aOther, aConsideration);
}

NS_IMETHODIMP
BasePrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);
  *aResult = Subsumes(aOther, DontConsiderDocumentDomain) &&
             Cast(aOther)->Subsumes(this, DontConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);
  *aResult = Subsumes(aOther, ConsiderDocumentDomain) &&
             Cast(aOther)->Subsumes(this, ConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);
  *aResult = Subsumes(aOther, DontConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);
  *aResult = Subsumes(aOther, ConsiderDocumentDomain);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetCsp(nsIContentSecurityPolicy** aCsp)
{
  NS_IF_ADDREF(*aCsp = mCSP);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SetCsp(nsIContentSecurityPolicy* aCsp)
{
  // If CSP was already set, it should not be destroyed!  Instead, it should
  // get set anew when a new principal is created.
  if (mCSP)
    return NS_ERROR_ALREADY_INITIALIZED;

  mCSP = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetCspJSON(nsAString& outCSPinJSON)
{
  outCSPinJSON.Truncate();
  dom::CSPPolicies jsonPolicies;

  if (!mCSP) {
    jsonPolicies.ToJSON(outCSPinJSON);
    return NS_OK;
  }
  return mCSP->ToJSON(outCSPinJSON);
}

NS_IMETHODIMP
BasePrincipal::GetIsNullPrincipal(bool* aIsNullPrincipal)
{
  *aIsNullPrincipal = false;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetJarPrefix(nsACString& aJarPrefix)
{
  MOZ_ASSERT(AppId() != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mOriginAttributes.CookieJar(aJarPrefix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginAttributes(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  if (NS_WARN_IF(!ToJSValue(aCx, mOriginAttributes, aVal))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginSuffix(nsACString& aOriginAttributes)
{
  mOriginAttributes.CreateSuffix(aOriginAttributes);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetCookieJar(nsACString& aCookieJar)
{
  mOriginAttributes.CookieJar(aCookieJar);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAppStatus(uint16_t* aAppStatus)
{
  if (AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    NS_WARNING("Asking for app status on a principal with an unknown app id");
    *aAppStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    return NS_OK;
  }

  *aAppStatus = nsScriptSecurityManager::AppStatusForPrincipal(this);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetAppId(uint32_t* aAppId)
{
  if (AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    MOZ_ASSERT(false);
    *aAppId = nsIScriptSecurityManager::NO_APP_ID;
    return NS_OK;
  }

  *aAppId = AppId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = IsInBrowserElement();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetUnknownAppId(bool* aUnknownAppId)
{
  *aUnknownAppId = AppId() == nsIScriptSecurityManager::UNKNOWN_APP_ID;
  return NS_OK;
}

already_AddRefed<BasePrincipal>
BasePrincipal::CreateCodebasePrincipal(nsIURI* aURI, OriginAttributes& aAttrs)
{
  // If the URI is supposed to inherit the security context of whoever loads it,
  // we shouldn't make a codebase principal for it.
  bool inheritsPrincipal;
  nsresult rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                                    &inheritsPrincipal);
  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(rv) || inheritsPrincipal) {
    return nsNullPrincipal::Create();
  }

  // Check whether the URI knows what its principal is supposed to be.
  nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(aURI);
  if (uriPrinc) {
    nsCOMPtr<nsIPrincipal> principal;
    uriPrinc->GetPrincipal(getter_AddRefs(principal));
    if (!principal) {
      return nsNullPrincipal::Create();
    }
    nsRefPtr<BasePrincipal> concrete = Cast(principal);
    return concrete.forget();
  }

  // Mint a codebase principal.
  nsRefPtr<nsPrincipal> codebase = new nsPrincipal();
  rv = codebase->Init(aURI, aAttrs);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return codebase.forget();
}

} // namespace mozilla
