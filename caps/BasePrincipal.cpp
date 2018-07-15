/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasePrincipal.h"

#include "nsDocShell.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIStandardURL.h"

#include "ContentPrincipal.h"
#include "ExpandedPrincipal.h"
#include "nsNetUtil.h"
#include "nsIURIWithPrincipal.h"
#include "NullPrincipal.h"
#include "nsScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/CSPDictionariesBinding.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {

BasePrincipal::BasePrincipal(PrincipalKind aKind)
  : mKind(aKind)
  , mHasExplicitDomain(false)
  , mInitialized(false)
{}

BasePrincipal::~BasePrincipal()
{}

NS_IMETHODIMP
BasePrincipal::GetOrigin(nsACString& aOrigin)
{
  MOZ_ASSERT(mInitialized);

  nsresult rv = GetOriginNoSuffix(aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = GetOriginSuffix(suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  aOrigin.Append(suffix);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetOriginNoSuffix(nsACString& aOrigin)
{
  MOZ_ASSERT(mInitialized);
  mOriginNoSuffix->ToUTF8String(aOrigin);
  return NS_OK;
}

bool
BasePrincipal::Subsumes(nsIPrincipal* aOther, DocumentDomainConsideration aConsideration)
{
  MOZ_ASSERT(aOther);
  MOZ_ASSERT_IF(Kind() == eCodebasePrincipal, mOriginSuffix);

  // Expanded principals handle origin attributes for each of their
  // sub-principals individually, null principals do only simple checks for
  // pointer equality, and system principals are immune to origin attributes
  // checks, so only do this check for codebase principals.
  if (Kind() == eCodebasePrincipal &&
      mOriginSuffix != Cast(aOther)->mOriginSuffix) {
    return false;
  }

  return SubsumesInternal(aOther, aConsideration);
}

NS_IMETHODIMP
BasePrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);

  *aResult = FastEquals(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EqualsConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);

  *aResult = FastEqualsConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);

  *aResult = FastSubsumes(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);

  *aResult = FastSubsumesConsideringDomain(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::SubsumesConsideringDomainIgnoringFPD(nsIPrincipal *aOther,
                                                    bool *aResult)
{
  NS_ENSURE_TRUE(aOther, NS_ERROR_INVALID_ARG);

  *aResult = FastSubsumesConsideringDomainIgnoringFPD(aOther);

  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::CheckMayLoad(nsIURI* aURI, bool aReport, bool aAllowIfInheritsPrincipal)
{
  // Check the internal method first, which allows us to quickly approve loads
  // for the System Principal.
  if (MayLoadInternal(aURI)) {
    return NS_OK;
  }

  nsresult rv;
  if (aAllowIfInheritsPrincipal) {
    // If the caller specified to allow loads of URIs that inherit
    // our principal, allow the load if this URI inherits its principal.
    bool doesInheritSecurityContext;
    rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                             &doesInheritSecurityContext);
    if (NS_SUCCEEDED(rv) && doesInheritSecurityContext) {
      return NS_OK;
    }
  }

  bool fetchableByAnyone;
  rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_FETCHABLE_BY_ANYONE, &fetchableByAnyone);
  if (NS_SUCCEEDED(rv) && fetchableByAnyone) {
    return NS_OK;
  }

  if (aReport) {
    nsCOMPtr<nsIURI> prinURI;
    rv = GetURI(getter_AddRefs(prinURI));
    if (NS_SUCCEEDED(rv) && prinURI) {
      nsScriptSecurityManager::ReportError(nullptr, "CheckSameOriginError",
                                           prinURI, aURI);
    }
  }

  return NS_ERROR_DOM_BAD_URI;
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
  // Never destroy an existing CSP on the principal.
  // This method should only be called in rare cases.

  MOZ_ASSERT(!mCSP, "do not destroy an existing CSP");
  if (mCSP) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mCSP = aCsp;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EnsureCSP(nsIDocument* aDocument,
                         nsIContentSecurityPolicy** aCSP)
{
  if (mCSP) {
    // if there is a CSP already associated with this principal
    // then just return that - do not overwrite it!!!
    NS_IF_ADDREF(*aCSP = mCSP);
    return NS_OK;
  }

  nsresult rv = NS_OK;
  mCSP = do_CreateInstance("@mozilla.org/cspcontext;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the request context for violation reports
  rv = aDocument ? mCSP->SetRequestContext(aDocument, nullptr)
                 : mCSP->SetRequestContext(nullptr, this);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_IF_ADDREF(*aCSP = mCSP);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPreloadCsp(nsIContentSecurityPolicy** aPreloadCSP)
{
  NS_IF_ADDREF(*aPreloadCSP = mPreloadCSP);
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::EnsurePreloadCSP(nsIDocument* aDocument,
                                nsIContentSecurityPolicy** aPreloadCSP)
{
  if (mPreloadCSP) {
    // if there is a speculative CSP already associated with this principal
    // then just return that - do not overwrite it!!!
    NS_IF_ADDREF(*aPreloadCSP = mPreloadCSP);
    return NS_OK;
  }

  nsresult rv = NS_OK;
  mPreloadCSP = do_CreateInstance("@mozilla.org/cspcontext;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the request context for violation reports
  rv = aDocument ? mPreloadCSP->SetRequestContext(aDocument, nullptr)
                 : mPreloadCSP->SetRequestContext(nullptr, this);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_IF_ADDREF(*aPreloadCSP = mPreloadCSP);
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
BasePrincipal::GetIsNullPrincipal(bool* aResult)
{
  *aResult = Kind() == eNullPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsCodebasePrincipal(bool* aResult)
{
  *aResult = Kind() == eCodebasePrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsExpandedPrincipal(bool* aResult)
{
  *aResult = Kind() == eExpandedPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsSystemPrincipal(bool* aResult)
{
  *aResult = Kind() == eSystemPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsAddonOrExpandedAddonPrincipal(bool* aResult)
{
  *aResult = AddonPolicy() || ContentScriptAddonPolicy();
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
  MOZ_ASSERT(mOriginSuffix);
  mOriginSuffix->ToUTF8String(aOriginAttributes);
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
BasePrincipal::GetUserContextId(uint32_t* aUserContextId)
{
  *aUserContextId = UserContextId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetPrivateBrowsingId(uint32_t* aPrivateBrowsingId)
{
  *aPrivateBrowsingId = PrivateBrowsingId();
  return NS_OK;
}

NS_IMETHODIMP
BasePrincipal::GetIsInIsolatedMozBrowserElement(bool* aIsInIsolatedMozBrowserElement)
{
  *aIsInIsolatedMozBrowserElement = IsInIsolatedMozBrowserElement();
  return NS_OK;
}

nsresult
BasePrincipal::GetAddonPolicy(nsISupports** aResult)
{
  RefPtr<extensions::WebExtensionPolicy> policy(AddonPolicy());
  policy.forget(aResult);
  return NS_OK;
}

extensions::WebExtensionPolicy*
BasePrincipal::AddonPolicy()
{
  if (Is<ContentPrincipal>()) {
    return As<ContentPrincipal>()->AddonPolicy();
  }
  return nullptr;
}

bool
BasePrincipal::AddonHasPermission(const nsAtom* aPerm)
{
  if (auto policy = AddonPolicy()) {
    return policy->HasPermission(aPerm);
  }
  return false;
}

nsIPrincipal*
BasePrincipal::PrincipalToInherit(nsIURI* aRequestedURI)
{
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->PrincipalToInherit(aRequestedURI);
  }
  return this;
}

already_AddRefed<BasePrincipal>
BasePrincipal::CreateCodebasePrincipal(nsIURI* aURI,
                                       const OriginAttributes& aAttrs)
{
  MOZ_ASSERT(aURI);

  nsAutoCString originNoSuffix;
  nsresult rv =
    ContentPrincipal::GenerateOriginNoSuffixFromURI(aURI, originNoSuffix);
  if (NS_FAILED(rv)) {
    // If the generation of the origin fails, we still want to have a valid
    // principal. Better to return a null principal here.
    return NullPrincipal::Create(aAttrs);
  }

  return CreateCodebasePrincipal(aURI, aAttrs, originNoSuffix);
}

already_AddRefed<BasePrincipal>
BasePrincipal::CreateCodebasePrincipal(nsIURI* aURI,
                                       const OriginAttributes& aAttrs,
                                       const nsACString& aOriginNoSuffix)
{
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(!aOriginNoSuffix.IsEmpty());

  // If the URI is supposed to inherit the security context of whoever loads it,
  // we shouldn't make a codebase principal for it.
  bool inheritsPrincipal;
  nsresult rv = NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                                    &inheritsPrincipal);
  if (NS_FAILED(rv) || inheritsPrincipal) {
    return NullPrincipal::Create(aAttrs);
  }

  // Check whether the URI knows what its principal is supposed to be.
  nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(aURI);
  if (uriPrinc) {
    nsCOMPtr<nsIPrincipal> principal;
    uriPrinc->GetPrincipal(getter_AddRefs(principal));
    if (!principal) {
      return NullPrincipal::Create(aAttrs);
    }
    RefPtr<BasePrincipal> concrete = Cast(principal);
    return concrete.forget();
  }

  // Mint a codebase principal.
  RefPtr<ContentPrincipal> codebase = new ContentPrincipal();
  rv = codebase->Init(aURI, aAttrs, aOriginNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return codebase.forget();
}

already_AddRefed<BasePrincipal>
BasePrincipal::CreateCodebasePrincipal(const nsACString& aOrigin)
{
  MOZ_ASSERT(!StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("[")),
             "CreateCodebasePrincipal does not support System and Expanded principals");

  MOZ_ASSERT(!StringBeginsWith(aOrigin, NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":")),
             "CreateCodebasePrincipal does not support NullPrincipal");

  nsAutoCString originNoSuffix;
  mozilla::OriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, originNoSuffix)) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return BasePrincipal::CreateCodebasePrincipal(uri, attrs);
}

already_AddRefed<BasePrincipal>
BasePrincipal::CloneStrippingUserContextIdAndFirstPartyDomain()
{
  OriginAttributes attrs = OriginAttributesRef();
  attrs.StripAttributes(OriginAttributes::STRIP_USER_CONTEXT_ID |
                        OriginAttributes::STRIP_FIRST_PARTY_DOMAIN);

  nsAutoCString originNoSuffix;
  nsresult rv = GetOriginNoSuffix(originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return BasePrincipal::CreateCodebasePrincipal(uri, attrs);
}

extensions::WebExtensionPolicy*
BasePrincipal::ContentScriptAddonPolicy()
{
  if (!Is<ExpandedPrincipal>()) {
    return nullptr;
  }

  auto expanded = As<ExpandedPrincipal>();
  for (auto& prin : expanded->WhiteList()) {
    if (auto policy = BasePrincipal::Cast(prin)->AddonPolicy()) {
      return policy;
    }
  }

  return nullptr;
}

bool
BasePrincipal::AddonAllowsLoad(nsIURI* aURI, bool aExplicit /* = false */)
{
  if (Is<ExpandedPrincipal>()) {
    return As<ExpandedPrincipal>()->AddonAllowsLoad(aURI, aExplicit);
  }
  if (auto policy = AddonPolicy()) {
    return policy->CanAccessURI(aURI, aExplicit);
  }
  return false;
}

void
BasePrincipal::FinishInit(const nsACString& aOriginNoSuffix,
                          const OriginAttributes& aOriginAttributes)
{
  mInitialized = true;
  mOriginAttributes = aOriginAttributes;

  // First compute the origin suffix since it's infallible.
  nsAutoCString originSuffix;
  mOriginAttributes.CreateSuffix(originSuffix);
  mOriginSuffix = NS_Atomize(originSuffix);

  MOZ_ASSERT(!aOriginNoSuffix.IsEmpty());
  mOriginNoSuffix = NS_Atomize(aOriginNoSuffix);
}

} // namespace mozilla
