/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrincipal.h"

#include "mozIThirdPartyUtil.h"
#include "nscore.h"
#include "nsScriptSecurityManager.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "pratom.h"
#include "nsIURI.h"
#include "nsJSPrincipals.h"
#include "nsIEffectiveTLDService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIClassInfoImpl.h"
#include "nsIProtocolHandler.h"
#include "nsError.h"
#include "nsIContentSecurityPolicy.h"
#include "nsNetCID.h"
#include "jswrapper.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/HashFunctions.h"

#include "nsIAppsService.h"
#include "mozIApplication.h"

using namespace mozilla;

static bool gIsWhitelistingTestDomains = false;
static bool gCodeBasePrincipalSupport = false;

static bool URIIsImmutable(nsIURI* aURI)
{
  nsCOMPtr<nsIMutable> mutableObj(do_QueryInterface(aURI));
  bool isMutable;
  return
    mutableObj &&
    NS_SUCCEEDED(mutableObj->GetMutable(&isMutable)) &&
    !isMutable;
}

NS_IMPL_CLASSINFO(nsPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_PRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsPrincipal,
                           nsIPrincipal,
                           nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(nsPrincipal,
                            nsIPrincipal,
                            nsISerializable)

// Called at startup:
/* static */ void
nsPrincipal::InitializeStatics()
{
  Preferences::AddBoolVarCache(
    &gIsWhitelistingTestDomains,
    "layout.css.unprefixing-service.include-test-domains");

  Preferences::AddBoolVarCache(&gCodeBasePrincipalSupport,
                               "signed.applets.codebase_principal_support",
                               false);
}

nsPrincipal::nsPrincipal()
  : mAppId(nsIScriptSecurityManager::UNKNOWN_APP_ID)
  , mInMozBrowser(false)
  , mCodebaseImmutable(false)
  , mDomainImmutable(false)
  , mInitialized(false)
{ }

nsPrincipal::~nsPrincipal()
{ }

nsresult
nsPrincipal::Init(nsIURI *aCodebase,
                  uint32_t aAppId,
                  bool aInMozBrowser)
{
  NS_ENSURE_STATE(!mInitialized);
  NS_ENSURE_ARG(aCodebase);

  mInitialized = true;

  mCodebase = NS_TryToMakeImmutable(aCodebase);
  mCodebaseImmutable = URIIsImmutable(mCodebase);

  mAppId = aAppId;
  mInMozBrowser = aInMozBrowser;

  return NS_OK;
}

void
nsPrincipal::GetScriptLocation(nsACString &aStr)
{
  mCodebase->GetSpec(aStr);
}

/* static */ nsresult
nsPrincipal::GetOriginForURI(nsIURI* aURI, nsACString& aOrigin)
{
  if (!aURI) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> origin = NS_GetInnermostURI(aURI);
  if (!origin) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString hostPort;

  // chrome: URLs don't have a meaningful origin, so make
  // sure we just get the full spec for them.
  // XXX this should be removed in favor of the solution in
  // bug 160042.
  bool isChrome;
  nsresult rv = origin->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && !isChrome) {
    rv = origin->GetAsciiHost(hostPort);
    // Some implementations return an empty string, treat it as no support
    // for asciiHost by that implementation.
    if (hostPort.IsEmpty()) {
      rv = NS_ERROR_FAILURE;
    }
  }

  int32_t port;
  if (NS_SUCCEEDED(rv) && !isChrome) {
    rv = origin->GetPort(&port);
  }

  if (NS_SUCCEEDED(rv) && !isChrome) {
    if (port != -1) {
      hostPort.Append(':');
      hostPort.AppendInt(port, 10);
    }

    rv = origin->GetScheme(aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    aOrigin.AppendLiteral("://");
    aOrigin.Append(hostPort);
  }
  else {
    rv = origin->GetAsciiSpec(aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetOrigin(nsACString& aOrigin)
{
  return GetOriginForURI(mCodebase, aOrigin);
}

NS_IMETHODIMP
nsPrincipal::EqualsConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = false;

  if (!aOther) {
    NS_WARNING("Need a principal to compare this to!");
    return NS_OK;
  }

  if (aOther == this) {
    *aResult = true;
    return NS_OK;
  }

  if (!nsScriptSecurityManager::AppAttributesEqual(this, aOther)) {
      return NS_OK;
  }

  // If either the subject or the object has changed its principal by
  // explicitly setting document.domain then the other must also have
  // done so in order to be considered the same origin. This prevents
  // DNS spoofing based on document.domain (154930)

  nsCOMPtr<nsIURI> thisURI;
  this->GetDomain(getter_AddRefs(thisURI));
  bool thisSetDomain = !!thisURI;
  if (!thisURI) {
      this->GetURI(getter_AddRefs(thisURI));
  }

  nsCOMPtr<nsIURI> otherURI;
  aOther->GetDomain(getter_AddRefs(otherURI));
  bool otherSetDomain = !!otherURI;
  if (!otherURI) {
      aOther->GetURI(getter_AddRefs(otherURI));
  }

  *aResult = thisSetDomain == otherSetDomain &&
             nsScriptSecurityManager::SecurityCompareURIs(thisURI, otherURI);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = false;

  if (!aOther) {
    NS_WARNING("Need a principal to compare this to!");
    return NS_OK;
  }

  if (aOther == this) {
    *aResult = true;
    return NS_OK;
  }

  if (!nsScriptSecurityManager::AppAttributesEqual(this, aOther)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> otherURI;
  nsresult rv = aOther->GetURI(getter_AddRefs(otherURI));
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(mCodebase,
               "shouldn't be calling this on principals from preferences");

  // Compare codebases.
  *aResult = nsScriptSecurityManager::SecurityCompareURIs(mCodebase,
                                                          otherURI);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  return Equals(aOther, aResult);
}

NS_IMETHODIMP
nsPrincipal::SubsumesConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  return EqualsConsideringDomain(aOther, aResult);
}

NS_IMETHODIMP
nsPrincipal::GetURI(nsIURI** aURI)
{
  if (mCodebaseImmutable) {
    NS_ADDREF(*aURI = mCodebase);
    return NS_OK;
  }

  if (!mCodebase) {
    *aURI = nullptr;
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mCodebase, aURI);
}

NS_IMETHODIMP
nsPrincipal::CheckMayLoad(nsIURI* aURI, bool aReport, bool aAllowIfInheritsPrincipal)
{
   if (aAllowIfInheritsPrincipal) {
    // If the caller specified to allow loads of URIs that inherit
    // our principal, allow the load if this URI inherits its principal
    if (nsPrincipal::IsPrincipalInherited(aURI)) {
      return NS_OK;
    }
  }

  // See if aURI is something like a Blob URI that is actually associated with
  // a principal.
  nsCOMPtr<nsIURIWithPrincipal> uriWithPrin = do_QueryInterface(aURI);
  nsCOMPtr<nsIPrincipal> uriPrin;
  if (uriWithPrin) {
    uriWithPrin->GetPrincipal(getter_AddRefs(uriPrin));
  }
  if (uriPrin && nsIPrincipal::Subsumes(uriPrin)) {
      return NS_OK;
  }

  if (nsScriptSecurityManager::SecurityCompareURIs(mCodebase, aURI)) {
    return NS_OK;
  }

  // If strict file origin policy is in effect, local files will always fail
  // SecurityCompareURIs unless they are identical. Explicitly check file origin
  // policy, in that case.
  if (nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
      NS_URIIsLocalFile(aURI) &&
      NS_RelaxStrictFileOriginPolicy(aURI, mCodebase)) {
    return NS_OK;
  }

  if (aReport) {
    nsScriptSecurityManager::ReportError(nullptr, NS_LITERAL_STRING("CheckSameOriginError"), mCodebase, aURI);
  }
  return NS_ERROR_DOM_BAD_URI;
}

void
nsPrincipal::SetURI(nsIURI* aURI)
{
  mCodebase = NS_TryToMakeImmutable(aURI);
  mCodebaseImmutable = URIIsImmutable(mCodebase);
}

NS_IMETHODIMP
nsPrincipal::GetHashValue(uint32_t* aValue)
{
  NS_PRECONDITION(mCodebase, "Need a codebase");

  *aValue = nsScriptSecurityManager::HashPrincipalByOrigin(this);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetDomain(nsIURI** aDomain)
{
  if (!mDomain) {
    *aDomain = nullptr;
    return NS_OK;
  }

  if (mDomainImmutable) {
    NS_ADDREF(*aDomain = mDomain);
    return NS_OK;
  }

  return NS_EnsureSafeToReturn(mDomain, aDomain);
}

NS_IMETHODIMP
nsPrincipal::SetDomain(nsIURI* aDomain)
{
  mDomain = NS_TryToMakeImmutable(aDomain);
  mDomainImmutable = URIIsImmutable(mDomain);

  // Recompute all wrappers between compartments using this principal and other
  // non-chrome compartments.
  AutoSafeJSContext cx;
  JSPrincipals *principals = nsJSPrincipals::get(static_cast<nsIPrincipal*>(this));
  bool success = js::RecomputeWrappers(cx, js::ContentCompartmentsOnly(),
                                       js::CompartmentsWithPrincipals(principals));
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  success = js::RecomputeWrappers(cx, js::CompartmentsWithPrincipals(principals),
                                  js::ContentCompartmentsOnly());
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetJarPrefix(nsACString& aJarPrefix)
{
  MOZ_ASSERT(mAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetJarPrefix(mAppId, mInMozBrowser, aJarPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetAppStatus(uint16_t* aAppStatus)
{
  *aAppStatus = GetAppStatus();
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetAppId(uint32_t* aAppId)
{
  if (mAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    MOZ_ASSERT(false);
    *aAppId = nsIScriptSecurityManager::NO_APP_ID;
    return NS_OK;
  }

  *aAppId = mAppId;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = mInMozBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetUnknownAppId(bool* aUnknownAppId)
{
  *aUnknownAppId = mAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID;
  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  // For a file URI, we return the file path.
  if (NS_URIIsLocalFile(mCodebase)) {
    nsCOMPtr<nsIURL> url = do_QueryInterface(mCodebase);

    if (url) {
      return url->GetFilePath(aBaseDomain);
    }
  }

  bool hasNoRelativeFlag;
  nsresult rv = NS_URIChainHasFlags(mCodebase,
                                    nsIProtocolHandler::URI_NORELATIVE,
                                    &hasNoRelativeFlag);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasNoRelativeFlag) {
    return mCodebase->GetSpec(aBaseDomain);
  }

  // For everything else, we ask the TLD service via
  // the ThirdPartyUtil.
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (thirdPartyUtil) {
    return thirdPartyUtil->GetBaseDomain(mCodebase, aBaseDomain);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Read(nsIObjectInputStream* aStream)
{
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIURI> codebase;
  nsresult rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  codebase = do_QueryInterface(supports);

  nsCOMPtr<nsIURI> domain;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  domain = do_QueryInterface(supports);

  uint32_t appId;
  rv = aStream->Read32(&appId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool inMozBrowser;
  rv = aStream->ReadBoolean(&inMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  // This may be null.
  nsCOMPtr<nsIContentSecurityPolicy> csp = do_QueryInterface(supports, &rv);

  rv = Init(codebase, appId, inMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetCsp(csp);
  NS_ENSURE_SUCCESS(rv, rv);

  // need to link in the CSP context here (link in the URI of the protected
  // resource).
  if (csp) {
    csp->SetRequestContext(codebase, nullptr, nullptr);
  }

  SetDomain(domain);

  return NS_OK;
}

NS_IMETHODIMP
nsPrincipal::Write(nsIObjectOutputStream* aStream)
{
  NS_ENSURE_STATE(mCodebase);

  nsresult rv = NS_WriteOptionalCompoundObject(aStream, mCodebase, NS_GET_IID(nsIURI),
                                               true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalCompoundObject(aStream, mDomain, NS_GET_IID(nsIURI),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStream->Write32(mAppId);
  aStream->WriteBoolean(mInMozBrowser);

  rv = NS_WriteOptionalCompoundObject(aStream, mCSP,
                                      NS_GET_IID(nsIContentSecurityPolicy),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // mCodebaseImmutable and mDomainImmutable will be recomputed based
  // on the deserialized URIs in Read().

  return NS_OK;
}

uint16_t
nsPrincipal::GetAppStatus()
{
  if (mAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    NS_WARNING("Asking for app status on a principal with an unknown app id");
    return nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  }
  return nsScriptSecurityManager::AppStatusForPrincipal(this);
}

// Helper-function to indicate whether the CSS Unprefixing Service
// whitelist should include dummy domains that are only intended for
// use in testing. (Controlled by a pref.)
static inline bool
IsWhitelistingTestDomains()
{
  return gIsWhitelistingTestDomains;
}

// Checks if the given URI's host is on our "full domain" whitelist
// (i.e. if it's an exact match against a domain that needs unprefixing)
static bool
IsOnFullDomainWhitelist(nsIURI* aURI)
{
  nsAutoCString hostStr;
  nsresult rv = aURI->GetHost(hostStr);
  NS_ENSURE_SUCCESS(rv, false);

  // NOTE: This static whitelist is expected to be short. If that changes,
  // we should consider a different representation; e.g. hash-set, prefix tree.
  static const nsLiteralCString sFullDomainsOnWhitelist[] = {
    // 0th entry only active when testing:
    NS_LITERAL_CSTRING("test1.example.org"),
    NS_LITERAL_CSTRING("map.baidu.com"),
    NS_LITERAL_CSTRING("3g.163.com"),
    NS_LITERAL_CSTRING("3glogo.gtimg.com"), // for 3g.163.com
    NS_LITERAL_CSTRING("info.3g.qq.com"), // for 3g.qq.com
    NS_LITERAL_CSTRING("3gimg.qq.com"), // for 3g.qq.com
    NS_LITERAL_CSTRING("img.m.baidu.com"), // for [shucheng|ks].baidu.com
    NS_LITERAL_CSTRING("m.mogujie.com"),
    NS_LITERAL_CSTRING("touch.qunar.com"),
    NS_LITERAL_CSTRING("mjs.sinaimg.cn"), // for sina.cn
    NS_LITERAL_CSTRING("static.qiyi.com"), // for m.iqiyi.com
    NS_LITERAL_CSTRING("cdn.kuaidi100.com"), // for m.kuaidi100.com
    NS_LITERAL_CSTRING("m.pc6.com"),
    NS_LITERAL_CSTRING("m.haosou.com"),
    NS_LITERAL_CSTRING("m.mi.com"),
    NS_LITERAL_CSTRING("wappass.baidu.com"),
    NS_LITERAL_CSTRING("m.video.baidu.com"),
    NS_LITERAL_CSTRING("m.video.baidu.com"),
    NS_LITERAL_CSTRING("imgcache.gtimg.cn"), // for m.v.qq.com
    NS_LITERAL_CSTRING("i.yimg.jp"), // for *.yahoo.co.jp
    NS_LITERAL_CSTRING("ai.yimg.jp"), // for *.yahoo.co.jp
    NS_LITERAL_CSTRING("daily.c.yimg.jp"), // for sp.daily.co.jp
    NS_LITERAL_CSTRING("stat100.ameba.jp"), // for ameblo.jp
    NS_LITERAL_CSTRING("user.ameba.jp"), // for ameblo.jp
    NS_LITERAL_CSTRING("www.goo.ne.jp"),
    NS_LITERAL_CSTRING("s.tabelog.jp"),
    NS_LITERAL_CSTRING("x.gnst.jp"), // for mobile.gnavi.co.jp
    NS_LITERAL_CSTRING("c.x.gnst.jp"), // for mobile.gnavi.co.jp
    NS_LITERAL_CSTRING("www.smbc-card.com"),
    NS_LITERAL_CSTRING("static.card.jp.rakuten-static.com"), // for rakuten-card.co.jp
    NS_LITERAL_CSTRING("img.mixi.net"), // for mixi.jp
    NS_LITERAL_CSTRING("girlschannel.net"),
    NS_LITERAL_CSTRING("www.fancl.co.jp"),
    NS_LITERAL_CSTRING("s.cosme.net"),
    NS_LITERAL_CSTRING("www.sapporobeer.jp"),
    NS_LITERAL_CSTRING("www.mapion.co.jp"),
    NS_LITERAL_CSTRING("touch.navitime.co.jp"),
    NS_LITERAL_CSTRING("sp.mbga.jp"),
    NS_LITERAL_CSTRING("ava-a.sp.mbga.jp"), // for sp.mbga.jp
    NS_LITERAL_CSTRING("www.ntv.co.jp"),
    NS_LITERAL_CSTRING("mobile.suntory.co.jp"), // for suntory.jp
    NS_LITERAL_CSTRING("www.aeonsquare.net"),
    NS_LITERAL_CSTRING("mw.nikkei.com"),
    NS_LITERAL_CSTRING("www.nhk.or.jp"),
    NS_LITERAL_CSTRING("www.tokyo-sports.co.jp"),
    NS_LITERAL_CSTRING("www.bellemaison.jp"),
    NS_LITERAL_CSTRING("www.kuronekoyamato.co.jp"),
    NS_LITERAL_CSTRING("s.tsite.jp"),
    NS_LITERAL_CSTRING("formassist.jp"), // for orico.jp
  };
  static const size_t sNumFullDomainsOnWhitelist =
    MOZ_ARRAY_LENGTH(sFullDomainsOnWhitelist);

  // Skip 0th (dummy) entry in whitelist, unless a pref is enabled.
  const size_t firstWhitelistIdx = IsWhitelistingTestDomains() ? 0 : 1;

  for (size_t i = firstWhitelistIdx; i < sNumFullDomainsOnWhitelist; ++i) {
    if (hostStr == sFullDomainsOnWhitelist[i]) {
      return true;
    }
  }
  return false;
}

// Checks if the given URI's host is on our "base domain" whitelist
// (i.e. if it's a subdomain of some host that we've whitelisted as needing
// unprefixing for all its subdomains)
static bool
IsOnBaseDomainWhitelist(nsIURI* aURI)
{
  static const nsLiteralCString sBaseDomainsOnWhitelist[] = {
    // 0th entry only active when testing:
    NS_LITERAL_CSTRING("test2.example.org"),
    NS_LITERAL_CSTRING("tbcdn.cn"), // for m.taobao.com
    NS_LITERAL_CSTRING("dpfile.com"), // for m.dianping.com
    NS_LITERAL_CSTRING("hao123img.com"), // for hao123.com
  };
  static const size_t sNumBaseDomainsOnWhitelist =
    MOZ_ARRAY_LENGTH(sBaseDomainsOnWhitelist);

  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  if (tldService) {
    // Skip 0th test-entry in whitelist, unless the testing pref is enabled.
    const size_t firstWhitelistIdx = IsWhitelistingTestDomains() ? 0 : 1;

    // Right now, the test base-domain "test2.example.org" is the only entry in
    // its whitelist with a nonzero "depth". So we'll only bother going beyond
    // 0 depth (to 1) if that entry is enabled. (No point in slowing down the
    // normal codepath, for the benefit of a disabled test domain.)  If we add a
    // "real" base-domain with a depth of >= 1 to our whitelist, we can get rid
    // of this conditional & just make this a static variable.
    const uint32_t maxSubdomainDepth = IsWhitelistingTestDomains() ? 1 : 0;

    for (uint32_t subdomainDepth = 0;
         subdomainDepth <= maxSubdomainDepth; ++subdomainDepth) {

      // Get the base domain (to depth |subdomainDepth|) from passed-in URI:
      nsAutoCString baseDomainStr;
      nsresult rv = tldService->GetBaseDomain(aURI, subdomainDepth,
                                              baseDomainStr);
      if (NS_FAILED(rv)) {
        // aURI doesn't have |subdomainDepth| levels of subdomains. If we got
        // here without a match yet, then aURI is not on our whitelist.
        return false;
      }

      // Compare the base domain against each entry in our whitelist:
      for (size_t i = firstWhitelistIdx; i < sNumBaseDomainsOnWhitelist; ++i) {
        if (baseDomainStr == sBaseDomainsOnWhitelist[i]) {
          return true;
        }
      }
    }
  }

  return false;
}

// The actual (non-cached) implementation of IsOnCSSUnprefixingWhitelist():
static bool
IsOnCSSUnprefixingWhitelistImpl(nsIURI* aURI)
{
  // Check scheme, so we can drop any non-HTTP/HTTPS URIs right away
  nsAutoCString schemeStr;
  nsresult rv = aURI->GetScheme(schemeStr);
  NS_ENSURE_SUCCESS(rv, false);

  // Only proceed if scheme is "http" or "https"
  if (!(StringBeginsWith(schemeStr, NS_LITERAL_CSTRING("http")) &&
        (schemeStr.Length() == 4 ||
         (schemeStr.Length() == 5 && schemeStr[4] == 's')))) {
    return false;
  }

  return (IsOnFullDomainWhitelist(aURI) ||
          IsOnBaseDomainWhitelist(aURI));
}


bool
nsPrincipal::IsOnCSSUnprefixingWhitelist()
{
  if (mIsOnCSSUnprefixingWhitelist.isNothing()) {
    // Value not cached -- perform our lazy whitelist-check.
    // (NOTE: If our URI is mutable, we just assume it's not on the whitelist,
    // since our caching strategy won't work. This isn't expected to be common.)
    mIsOnCSSUnprefixingWhitelist.emplace(
      mCodebaseImmutable &&
      IsOnCSSUnprefixingWhitelistImpl(mCodebase));
  }

  return *mIsOnCSSUnprefixingWhitelist;
}

/************************************************************************************************************************/

NS_IMPL_CLASSINFO(nsExpandedPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_EXPANDEDPRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsExpandedPrincipal,
                           nsIPrincipal,
                           nsIExpandedPrincipal)
NS_IMPL_CI_INTERFACE_GETTER(nsExpandedPrincipal,
                             nsIPrincipal,
                             nsIExpandedPrincipal)

struct OriginComparator
{
  bool LessThan(nsIPrincipal* a, nsIPrincipal* b) const
  {
    nsAutoCString originA;
    nsresult rv = a->GetOrigin(originA);
    NS_ENSURE_SUCCESS(rv, false);
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    NS_ENSURE_SUCCESS(rv, false);
    return originA < originB;
  }

  bool Equals(nsIPrincipal* a, nsIPrincipal* b) const
  {
    nsAutoCString originA;
    nsresult rv = a->GetOrigin(originA);
    NS_ENSURE_SUCCESS(rv, false);
    nsAutoCString originB;
    rv = b->GetOrigin(originB);
    NS_ENSURE_SUCCESS(rv, false);
    return a == b;
  }
};

nsExpandedPrincipal::nsExpandedPrincipal(nsTArray<nsCOMPtr <nsIPrincipal> > &aWhiteList)
{
  // We force the principals to be sorted by origin so that nsExpandedPrincipal
  // origins can have a canonical form.
  OriginComparator c;
  for (size_t i = 0; i < aWhiteList.Length(); ++i) {
    mPrincipals.InsertElementSorted(aWhiteList[i], c);
  }
}

nsExpandedPrincipal::~nsExpandedPrincipal()
{ }

NS_IMETHODIMP
nsExpandedPrincipal::GetDomain(nsIURI** aDomain)
{
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::SetDomain(nsIURI* aDomain)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetOrigin(nsACString& aOrigin)
{
  aOrigin.AssignLiteral("[Expanded Principal [");
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (i != 0) {
      aOrigin.AppendLiteral(", ");
    }

    nsAutoCString subOrigin;
    nsresult rv = mPrincipals.ElementAt(i)->GetOrigin(subOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
    aOrigin.Append(subOrigin);
  }

  aOrigin.Append("]]");
  return NS_OK;
}

typedef nsresult (NS_STDCALL nsIPrincipal::*nsIPrincipalMemFn)(nsIPrincipal* aOther,
                                                               bool* aResult);
#define CALL_MEMBER_FUNCTION(THIS,MEM_FN)  ((THIS)->*(MEM_FN))

// nsExpandedPrincipal::Equals and nsExpandedPrincipal::EqualsConsideringDomain
// shares the same logic. The difference only that Equals requires 'this'
// and 'aOther' to Subsume each other while EqualsConsideringDomain requires
// bidirectional SubsumesConsideringDomain.
static nsresult
Equals(nsExpandedPrincipal* aThis, nsIPrincipalMemFn aFn, nsIPrincipal* aOther,
       bool* aResult)
{
  // If (and only if) 'aThis' and 'aOther' both Subsume/SubsumesConsideringDomain
  // each other, then they are Equal.
  *aResult = false;
  // Calling the corresponding subsume function on this (aFn).
  nsresult rv = CALL_MEMBER_FUNCTION(aThis, aFn)(aOther, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!*aResult)
    return NS_OK;

  // Calling the corresponding subsume function on aOther (aFn).
  rv = CALL_MEMBER_FUNCTION(aOther, aFn)(aThis, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::Equals(nsIPrincipal* aOther, bool* aResult)
{
  return ::Equals(this, &nsIPrincipal::Subsumes, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::EqualsConsideringDomain(nsIPrincipal* aOther, bool* aResult)
{
  return ::Equals(this, &nsIPrincipal::SubsumesConsideringDomain, aOther, aResult);
}

// nsExpandedPrincipal::Subsumes and nsExpandedPrincipal::SubsumesConsideringDomain
// shares the same logic. The difference only that Subsumes calls are replaced
//with SubsumesConsideringDomain calls in the second case.
static nsresult
Subsumes(nsExpandedPrincipal* aThis, nsIPrincipalMemFn aFn, nsIPrincipal* aOther,
         bool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aOther);
  if (expanded) {
    // If aOther is an ExpandedPrincipal too, check if all of its
    // principals are subsumed.
    nsTArray< nsCOMPtr<nsIPrincipal> >* otherList;
    expanded->GetWhiteList(&otherList);
    for (uint32_t i = 0; i < otherList->Length(); ++i){
      rv = CALL_MEMBER_FUNCTION(aThis, aFn)((*otherList)[i], aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!*aResult) {
        // If we don't subsume at least one principal of aOther, return false.
        return NS_OK;
      }
    }
  } else {
    // For a regular aOther, one of our principals must subsume it.
    nsTArray< nsCOMPtr<nsIPrincipal> >* list;
    aThis->GetWhiteList(&list);
    for (uint32_t i = 0; i < list->Length(); ++i){
      rv = CALL_MEMBER_FUNCTION((*list)[i], aFn)(aOther, aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      if (*aResult) {
        // If one of our principal subsumes it, return true.
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

#undef CALL_MEMBER_FUNCTION

NS_IMETHODIMP
nsExpandedPrincipal::Subsumes(nsIPrincipal* aOther, bool* aResult)
{
  return ::Subsumes(this, &nsIPrincipal::Subsumes, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::SubsumesConsideringDomain(nsIPrincipal* aOther, bool* aResult)
{
  return ::Subsumes(this, &nsIPrincipal::SubsumesConsideringDomain, aOther, aResult);
}

NS_IMETHODIMP
nsExpandedPrincipal::CheckMayLoad(nsIURI* uri, bool aReport, bool aAllowIfInheritsPrincipal)
{
  nsresult rv;
  for (uint32_t i = 0; i < mPrincipals.Length(); ++i){
    rv = mPrincipals[i]->CheckMayLoad(uri, aReport, aAllowIfInheritsPrincipal);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetHashValue(uint32_t* result)
{
  MOZ_CRASH("extended principal should never be used as key in a hash map");
}

NS_IMETHODIMP
nsExpandedPrincipal::GetURI(nsIURI** aURI)
{
  *aURI = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetWhiteList(nsTArray<nsCOMPtr<nsIPrincipal> >** aWhiteList)
{
  *aWhiteList = &mPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetJarPrefix(nsACString& aJarPrefix)
{
  aJarPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetAppStatus(uint16_t* aAppStatus)
{
  *aAppStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetAppId(uint32_t* aAppId)
{
  *aAppId = nsIScriptSecurityManager::NO_APP_ID;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = false;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetUnknownAppId(bool* aUnknownAppId)
{
  *aUnknownAppId = false;
  return NS_OK;
}

NS_IMETHODIMP
nsExpandedPrincipal::GetBaseDomain(nsACString& aBaseDomain)
{
  return NS_ERROR_NOT_AVAILABLE;
}

bool
nsExpandedPrincipal::IsOnCSSUnprefixingWhitelist()
{
  // CSS Unprefixing Whitelist is a per-origin thing; doesn't really make sense
  // for an expanded principal. (And probably shouldn't be needed.)
  return false;
}


void
nsExpandedPrincipal::GetScriptLocation(nsACString& aStr)
{
  aStr.Assign("[Expanded Principal [");
  for (size_t i = 0; i < mPrincipals.Length(); ++i) {
    if (i != 0) {
      aStr.AppendLiteral(", ");
    }

    nsAutoCString spec;
    nsJSPrincipals::get(mPrincipals.ElementAt(i))->GetScriptLocation(spec);

    aStr.Append(spec);

  }
  aStr.Append("]]");
}

//////////////////////////////////////////
// Methods implementing nsISerializable //
//////////////////////////////////////////

NS_IMETHODIMP
nsExpandedPrincipal::Read(nsIObjectInputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExpandedPrincipal::Write(nsIObjectOutputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
