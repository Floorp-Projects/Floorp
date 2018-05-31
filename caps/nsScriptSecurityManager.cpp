/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptSecurityManager.h"

#include "mozilla/ArrayUtils.h"

#include "xpcpublic.h"
#include "XPCWrapper.h"
#include "nsIInputStreamChannel.h"
#include "nsILoadContext.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"
#include "nsINestedURI.h"
#include "nspr.h"
#include "nsJSPrincipals.h"
#include "mozilla/BasePrincipal.h"
#include "ExpandedPrincipal.h"
#include "SystemPrincipal.h"
#include "NullPrincipal.h"
#include "DomainPolicy.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsDOMCID.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsIEffectiveTLDService.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIZipReader.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"
#include "nsIContent.h"
#include "nsDOMJSUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsIClassInfo.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIChromeRegistry.h"
#include "nsIResProtocolHandler.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingUtils.h"
#include <stdint.h>
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsILoadInfo.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsITreeSelection.h"

// This should be probably defined on some other place... but I couldn't find it
#define WEBAPPS_PERM_NAME "webapps-manage"

using namespace mozilla;
using namespace mozilla::dom;

nsIIOService    *nsScriptSecurityManager::sIOService = nullptr;
JSContext       *nsScriptSecurityManager::sContext   = nullptr;
bool nsScriptSecurityManager::sStrictFileOriginPolicy = true;

namespace {

class BundleHelper
{
public:
  NS_INLINE_DECL_REFCOUNTING(BundleHelper)

  static nsIStringBundle*
  GetOrCreate()
  {
    MOZ_ASSERT(!sShutdown);

    // Already shutting down. Nothing should require the use of the string
    // bundle when shutting down.
    if (sShutdown) {
      return nullptr;
    }

    if (!sSelf) {
      sSelf = new BundleHelper();
    }

    return sSelf->GetOrCreateInternal();
  }

  static void
  Shutdown()
  {
    sSelf = nullptr;
    sShutdown = true;
  }

private:
  ~BundleHelper() = default;

  nsIStringBundle*
  GetOrCreateInternal()
  {
    if (!mBundle) {
      nsCOMPtr<nsIStringBundleService> bundleService =
          mozilla::services::GetStringBundleService();
      if (NS_WARN_IF(!bundleService)) {
        return nullptr;
      }

      nsresult rv =
        bundleService->CreateBundle("chrome://global/locale/security/caps.properties",
                                    getter_AddRefs(mBundle));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }

    return mBundle;
  }

  nsCOMPtr<nsIStringBundle> mBundle;

  static StaticRefPtr<BundleHelper> sSelf;
  static bool sShutdown;
};

StaticRefPtr<BundleHelper> BundleHelper::sSelf;
bool BundleHelper::sShutdown = false;

} // anonymous

///////////////////////////
// Convenience Functions //
///////////////////////////

class nsAutoInPrincipalDomainOriginSetter {
public:
    nsAutoInPrincipalDomainOriginSetter() {
        ++sInPrincipalDomainOrigin;
    }
    ~nsAutoInPrincipalDomainOriginSetter() {
        --sInPrincipalDomainOrigin;
    }
    static uint32_t sInPrincipalDomainOrigin;
};
uint32_t nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin;

static
nsresult
GetOriginFromURI(nsIURI* aURI, nsACString& aOrigin)
{
  if (nsAutoInPrincipalDomainOriginSetter::sInPrincipalDomainOrigin > 1) {
      // Allow a single recursive call to GetPrincipalDomainOrigin, since that
      // might be happening on a different principal from the first call.  But
      // after that, cut off the recursion; it just indicates that something
      // we're doing in this method causes us to reenter a security check here.
      return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoInPrincipalDomainOriginSetter autoSetter;

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString hostPort;

  nsresult rv = uri->GetHostPort(hostPort);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    aOrigin = scheme + NS_LITERAL_CSTRING("://") + hostPort;
  }
  else {
    // Some URIs (e.g., nsSimpleURI) don't support host. Just
    // get the full spec.
    rv = uri->GetSpec(aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static
nsresult
GetPrincipalDomainOrigin(nsIPrincipal* aPrincipal,
                         nsACString& aOrigin)
{

  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetDomain(getter_AddRefs(uri));
  if (!uri) {
    aPrincipal->GetURI(getter_AddRefs(uri));
  }
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  return GetOriginFromURI(uri, aOrigin);
}

inline void SetPendingExceptionASCII(JSContext *cx, const char *aMsg)
{
    JS_ReportErrorASCII(cx, "%s", aMsg);
}

inline void SetPendingException(JSContext *cx, const char16_t *aMsg)
{
    NS_ConvertUTF16toUTF8 msg(aMsg);
    JS_ReportErrorUTF8(cx, "%s", msg.get());
}

/* static */
bool
nsScriptSecurityManager::SecurityCompareURIs(nsIURI* aSourceURI,
                                             nsIURI* aTargetURI)
{
    return NS_SecurityCompareURIs(aSourceURI, aTargetURI, sStrictFileOriginPolicy);
}

// SecurityHashURI is consistent with SecurityCompareURIs because NS_SecurityHashURI
// is consistent with NS_SecurityCompareURIs.  See nsNetUtil.h.
uint32_t
nsScriptSecurityManager::SecurityHashURI(nsIURI* aURI)
{
    return NS_SecurityHashURI(aURI);
}

/*
 * GetChannelResultPrincipal will return the principal that the resource
 * returned by this channel will use.  For example, if the resource is in
 * a sandbox, it will return the nullprincipal.  If the resource is forced
 * to inherit principal, it will return the principal of its parent.  If
 * the load doesn't require sandboxing or inheriting, it will return the same
 * principal as GetChannelURIPrincipal. Namely the principal of the URI
 * that is being loaded.
 */
NS_IMETHODIMP
nsScriptSecurityManager::GetChannelResultPrincipal(nsIChannel* aChannel,
                                                   nsIPrincipal** aPrincipal)
{
  return GetChannelResultPrincipal(aChannel, aPrincipal,
                                   /*aIgnoreSandboxing*/ false);
}

nsresult
nsScriptSecurityManager::GetChannelResultPrincipalIfNotSandboxed(nsIChannel* aChannel,
                                                                 nsIPrincipal** aPrincipal)
{
  return GetChannelResultPrincipal(aChannel, aPrincipal,
                                   /*aIgnoreSandboxing*/ true);
}

static void
InheritAndSetCSPOnPrincipalIfNeeded(nsIChannel* aChannel, nsIPrincipal* aPrincipal)
{
  // loading a data: URI into an iframe, or loading frame[srcdoc] need
  // to inherit the CSP (see Bug 1073952, 1381761).
  MOZ_ASSERT(aChannel && aPrincipal, "need a valid channel and principal");
  if (!aChannel) {
    return;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo ||
      loadInfo->GetExternalContentPolicyType() != nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS_VOID(rv);
  nsAutoCString URISpec;
  rv = uri->GetSpec(URISpec);
  NS_ENSURE_SUCCESS_VOID(rv);

  bool isSrcDoc = URISpec.EqualsLiteral("about:srcdoc");
  bool isData = (NS_SUCCEEDED(uri->SchemeIs("data", &isData)) && isData);

  if (!isSrcDoc && !isData) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principalToInherit =
    loadInfo->FindPrincipalToInherit(aChannel);

  nsCOMPtr<nsIContentSecurityPolicy> originalCSP;
  principalToInherit->GetCsp(getter_AddRefs(originalCSP));
  if (!originalCSP) {
    return;
  }

  // if the principalToInherit had a CSP, add it to the before
  // created NullPrincipal (unless it already has one)
  MOZ_ASSERT(aPrincipal->GetIsNullPrincipal(),
             "inheriting the CSP only valid for NullPrincipal");
  nsCOMPtr<nsIContentSecurityPolicy> nullPrincipalCSP;
  aPrincipal->GetCsp(getter_AddRefs(nullPrincipalCSP));
  if (nullPrincipalCSP) {
    MOZ_ASSERT(nullPrincipalCSP == originalCSP,
               "There should be no other CSP here.");
    // CSPs are equal, no need to set it again.
    return;
  }
  aPrincipal->SetCsp(originalCSP);
}

nsresult
nsScriptSecurityManager::GetChannelResultPrincipal(nsIChannel* aChannel,
                                                   nsIPrincipal** aPrincipal,
                                                   bool aIgnoreSandboxing)
{
  MOZ_ASSERT(aChannel, "Must have channel!");

  // Check whether we have an nsILoadInfo that says what we should do.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (loadInfo && loadInfo->GetForceInheritPrincipalOverruleOwner()) {
    nsCOMPtr<nsIPrincipal> principalToInherit =
      loadInfo->FindPrincipalToInherit(aChannel);
    principalToInherit.forget(aPrincipal);
    return NS_OK;
  }

  nsCOMPtr<nsISupports> owner;
  aChannel->GetOwner(getter_AddRefs(owner));
  if (owner) {
    CallQueryInterface(owner, aPrincipal);
    if (*aPrincipal) {
      return NS_OK;
    }
  }

  if (loadInfo) {
        if (!aIgnoreSandboxing && loadInfo->GetLoadingSandboxed()) {
          MOZ_ALWAYS_TRUE(NS_SUCCEEDED(loadInfo->GetSandboxedLoadingPrincipal(aPrincipal)));
          MOZ_ASSERT(*aPrincipal);
          InheritAndSetCSPOnPrincipalIfNeeded(aChannel, *aPrincipal);
          return NS_OK;
        }

    bool forceInherit = loadInfo->GetForceInheritPrincipal();
    if (aIgnoreSandboxing && !forceInherit) {
      // Check if SEC_FORCE_INHERIT_PRINCIPAL was dropped because of
      // sandboxing:
      if (loadInfo->GetLoadingSandboxed() &&
        loadInfo->GetForceInheritPrincipalDropped()) {
        forceInherit = true;
      }
    }
    if (forceInherit) {
      nsCOMPtr<nsIPrincipal> principalToInherit =
        loadInfo->FindPrincipalToInherit(aChannel);
      principalToInherit.forget(aPrincipal);
      return NS_OK;
    }

    auto securityMode = loadInfo->GetSecurityMode();
    // The data: inheritance flags should only apply to the initial load,
    // not to loads that it might have redirected to.
    if (loadInfo->RedirectChain().IsEmpty() &&
        (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS ||
         securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS ||
         securityMode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS)) {

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPrincipal> principalToInherit =
        loadInfo->FindPrincipalToInherit(aChannel);
      bool inheritForAboutBlank = loadInfo->GetAboutBlankInherits();

      if (nsContentUtils::ChannelShouldInheritPrincipal(principalToInherit,
                                                        uri,
                                                        inheritForAboutBlank,
                                                        false)) {
        principalToInherit.forget(aPrincipal);
        return NS_OK;
      }
    }
  }
  nsresult rv = GetChannelURIPrincipal(aChannel, aPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);
  InheritAndSetCSPOnPrincipalIfNeeded(aChannel, *aPrincipal);
  return NS_OK;
}

/* The principal of the URI that this channel is loading. This is never
 * affected by things like sandboxed loads, or loads where we forcefully
 * inherit the principal.  Think of this as the principal of the server
 * which this channel is loading from.  Most callers should use
 * GetChannelResultPrincipal instead of GetChannelURIPrincipal.  Only
 * call GetChannelURIPrincipal if you are sure that you want the
 * principal that matches the uri, even in cases when the load is
 * sandboxed or when the load could be a blob or data uri (i.e even when
 * you encounter loads that may or may not be sandboxed and loads
 * that may or may not inherit)."
 */
NS_IMETHODIMP
nsScriptSecurityManager::GetChannelURIPrincipal(nsIChannel* aChannel,
                                                nsIPrincipal** aPrincipal)
{
    MOZ_ASSERT(aChannel, "Must have channel!");

    // Get the principal from the URI.  Make sure this does the same thing
    // as nsDocument::Reset and XULDocument::StartDocumentLoad.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadInfo> loadInfo;
    aChannel->GetLoadInfo(getter_AddRefs(loadInfo));

    // Inherit the origin attributes from loadInfo.
    // If this is a top-level document load, the origin attributes of the
    // loadInfo will be set from nsDocShell::DoURILoad.
    // For subresource loading, the origin attributes of the loadInfo is from
    // its loadingPrincipal.
    OriginAttributes attrs;

    // For addons loadInfo might be null.
    if (loadInfo) {
      attrs = loadInfo->GetOriginAttributes();
    }

    nsCOMPtr<nsIPrincipal> prin =
      BasePrincipal::CreateCodebasePrincipal(uri, attrs);
    prin.forget(aPrincipal);
    return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::IsSystemPrincipal(nsIPrincipal* aPrincipal,
                                           bool* aIsSystem)
{
    *aIsSystem = (aPrincipal == mSystemPrincipal);
    return NS_OK;
}

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////

////////////////////////////////////
// Methods implementing ISupports //
////////////////////////////////////
NS_IMPL_ISUPPORTS(nsScriptSecurityManager,
                  nsIScriptSecurityManager,
                  nsIObserver)

///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

///////////////// Security Checks /////////////////

bool
nsScriptSecurityManager::ContentSecurityPolicyPermitsJSAction(JSContext *cx)
{
    MOZ_ASSERT(cx == nsContentUtils::GetCurrentJSContext());
    nsCOMPtr<nsIPrincipal> subjectPrincipal = nsContentUtils::SubjectPrincipal();
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    nsresult rv = subjectPrincipal->GetCsp(getter_AddRefs(csp));
    NS_ASSERTION(NS_SUCCEEDED(rv), "CSP: Failed to get CSP from principal.");

    // don't do anything unless there's a CSP
    if (!csp)
        return true;

    bool evalOK = true;
    bool reportViolation = false;
    rv = csp->GetAllowsEval(&reportViolation, &evalOK);

    if (NS_FAILED(rv))
    {
        NS_WARNING("CSP: failed to get allowsEval");
        return true; // fail open to not break sites.
    }

    if (reportViolation) {
        nsAutoString fileName;
        unsigned lineNum = 0;
        NS_NAMED_LITERAL_STRING(scriptSample, "call to eval() or related function blocked by CSP");

        JS::AutoFilename scriptFilename;
        if (JS::DescribeScriptedCaller(cx, &scriptFilename, &lineNum)) {
            if (const char *file = scriptFilename.get()) {
                CopyUTF8toUTF16(nsDependentCString(file), fileName);
            }
        } else {
            MOZ_ASSERT(!JS_IsExceptionPending(cx));
        }
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                                 fileName,
                                 scriptSample,
                                 lineNum,
                                 EmptyString(),
                                 EmptyString());
    }

    return evalOK;
}

// static
bool
nsScriptSecurityManager::JSPrincipalsSubsume(JSPrincipals *first,
                                             JSPrincipals *second)
{
    return nsJSPrincipals::get(first)->Subsumes(nsJSPrincipals::get(second));
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckSameOriginURI(nsIURI* aSourceURI,
                                            nsIURI* aTargetURI,
                                            bool reportError)
{
    if (!SecurityCompareURIs(aSourceURI, aTargetURI))
    {
         if (reportError) {
            ReportError(nullptr, "CheckSameOriginError",
                        aSourceURI, aTargetURI);
         }
         return NS_ERROR_DOM_BAD_URI;
    }
    return NS_OK;
}

/*static*/ uint32_t
nsScriptSecurityManager::HashPrincipalByOrigin(nsIPrincipal* aPrincipal)
{
    nsCOMPtr<nsIURI> uri;
    aPrincipal->GetDomain(getter_AddRefs(uri));
    if (!uri)
        aPrincipal->GetURI(getter_AddRefs(uri));
    return SecurityHashURI(uri);
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIFromScript(JSContext *cx, nsIURI *aURI)
{
    // Get principal of currently executing script.
    MOZ_ASSERT(cx == nsContentUtils::GetCurrentJSContext());
    nsIPrincipal* principal = nsContentUtils::SubjectPrincipal();
    nsresult rv = CheckLoadURIWithPrincipal(principal, aURI,
                                            nsIScriptSecurityManager::STANDARD);
    if (NS_SUCCEEDED(rv)) {
        // OK to load
        return NS_OK;
    }

    // Report error.
    nsAutoCString spec;
    if (NS_FAILED(aURI->GetAsciiSpec(spec)))
        return NS_ERROR_FAILURE;
    nsAutoCString msg("Access to '");
    msg.Append(spec);
    msg.AppendLiteral("' from script denied");
    SetPendingExceptionASCII(cx, msg.get());
    return NS_ERROR_DOM_BAD_URI;
}

/**
 * Helper method to handle cases where a flag passed to
 * CheckLoadURIWithPrincipal means denying loading if the given URI has certain
 * nsIProtocolHandler flags set.
 * @return if success, access is allowed. Otherwise, deny access
 */
static nsresult
DenyAccessIfURIHasFlags(nsIURI* aURI, uint32_t aURIFlags)
{
    MOZ_ASSERT(aURI, "Must have URI!");

    bool uriHasFlags;
    nsresult rv =
        NS_URIChainHasFlags(aURI, aURIFlags, &uriHasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (uriHasFlags) {
        return NS_ERROR_DOM_BAD_URI;
    }

    return NS_OK;
}

static bool
EqualOrSubdomain(nsIURI* aProbeArg, nsIURI* aBase)
{
    nsresult rv;
    nsCOMPtr<nsIURI> probe = aProbeArg;

    nsCOMPtr<nsIEffectiveTLDService> tldService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(tldService, false);
    while (true) {
        if (nsScriptSecurityManager::SecurityCompareURIs(probe, aBase)) {
            return true;
        }

        nsAutoCString host, newHost;
        rv = probe->GetHost(host);
        NS_ENSURE_SUCCESS(rv, false);

        rv = tldService->GetNextSubDomain(host, newHost);
        if (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
            return false;
        }
        NS_ENSURE_SUCCESS(rv, false);
        rv = NS_MutateURI(probe)
               .SetHost(newHost)
               .Finalize(probe);
        NS_ENSURE_SUCCESS(rv, false);
    }
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
                                                   nsIURI *aTargetURI,
                                                   uint32_t aFlags)
{
    MOZ_ASSERT(aPrincipal, "CheckLoadURIWithPrincipal must have a principal");

    // If someone passes a flag that we don't understand, we should
    // fail, because they may need a security check that we don't
    // provide.
    NS_ENSURE_FALSE(aFlags & ~(nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
                               nsIScriptSecurityManager::ALLOW_CHROME |
                               nsIScriptSecurityManager::DISALLOW_SCRIPT |
                               nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL |
                               nsIScriptSecurityManager::DONT_REPORT_ERRORS),
                    NS_ERROR_UNEXPECTED);
    NS_ENSURE_ARG_POINTER(aPrincipal);
    NS_ENSURE_ARG_POINTER(aTargetURI);

    // If DISALLOW_INHERIT_PRINCIPAL is set, we prevent loading of URIs which
    // would do such inheriting. That would be URIs that do not have their own
    // security context. We do this even for the system principal.
    if (aFlags & nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL) {
        nsresult rv =
            DenyAccessIfURIHasFlags(aTargetURI,
                                    nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aPrincipal == mSystemPrincipal) {
        // Allow access
        return NS_OK;
    }

    nsCOMPtr<nsIURI> sourceURI;
    aPrincipal->GetURI(getter_AddRefs(sourceURI));
    if (!sourceURI) {
        auto* basePrin = BasePrincipal::Cast(aPrincipal);
        if (basePrin->Is<ExpandedPrincipal>()) {
            auto expanded = basePrin->As<ExpandedPrincipal>();
            for (auto& prin : expanded->WhiteList()) {
                nsresult rv = CheckLoadURIWithPrincipal(prin,
                                                        aTargetURI,
                                                        aFlags);
                if (NS_SUCCEEDED(rv)) {
                    // Allow access if it succeeded with one of the white listed principals
                    return NS_OK;
                }
            }
            // None of our whitelisted principals worked.
            return NS_ERROR_DOM_BAD_URI;
        }
        NS_ERROR("Non-system principals or expanded principal passed to CheckLoadURIWithPrincipal "
                 "must have a URI!");
        return NS_ERROR_UNEXPECTED;
    }

    // Automatic loads are not allowed from certain protocols.
    if (aFlags & nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT) {
        nsresult rv =
            DenyAccessIfURIHasFlags(sourceURI,
                                    nsIProtocolHandler::URI_FORBIDS_AUTOMATIC_DOCUMENT_REPLACEMENT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // If either URI is a nested URI, get the base URI
    nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(sourceURI);
    nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

    //-- get the target scheme
    nsAutoCString targetScheme;
    nsresult rv = targetBaseURI->GetScheme(targetScheme);
    if (NS_FAILED(rv)) return rv;

    //-- Some callers do not allow loading javascript:
    if ((aFlags & nsIScriptSecurityManager::DISALLOW_SCRIPT) &&
         targetScheme.EqualsLiteral("javascript"))
    {
       return NS_ERROR_DOM_BAD_URI;
    }

    // Check for uris that are only loadable by principals that subsume them
    bool hasFlags;
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasFlags) {
        // check nothing else in the URI chain has flags that prevent
        // access:
        rv = CheckLoadURIFlags(sourceURI, aTargetURI, sourceBaseURI,
                               targetBaseURI, aFlags);
        NS_ENSURE_SUCCESS(rv, rv);
        // Check the principal is allowed to load the target.
        return aPrincipal->CheckMayLoad(targetBaseURI, true, false);
    }

    //-- get the source scheme
    nsAutoCString sourceScheme;
    rv = sourceBaseURI->GetScheme(sourceScheme);
    if (NS_FAILED(rv)) return rv;

    // When comparing schemes, if the relevant pref is set, view-source URIs
    // are reachable from same-protocol (so e.g. file: can link to
    // view-source:file). This is required for reftests.
    static bool sViewSourceReachableFromInner = false;
    static bool sCachedViewSourcePref = false;
    if (!sCachedViewSourcePref) {
        sCachedViewSourcePref = true;
        mozilla::Preferences::AddBoolVarCache(&sViewSourceReachableFromInner,
            "security.view-source.reachable-from-inner-protocol");
    }

    bool targetIsViewSource = false;

    if (sourceScheme.LowerCaseEqualsLiteral(NS_NULLPRINCIPAL_SCHEME)) {
        // A null principal can target its own URI.
        if (sourceURI == aTargetURI) {
            return NS_OK;
        }
    }
    else if (sViewSourceReachableFromInner &&
             sourceScheme.EqualsIgnoreCase(targetScheme.get()) &&
             NS_SUCCEEDED(aTargetURI->SchemeIs("view-source", &targetIsViewSource)) &&
             targetIsViewSource)
    {
        // exception for foo: linking to view-source:foo for reftests...
        return NS_OK;
    }
    else if (sourceScheme.EqualsIgnoreCase("file") &&
             targetScheme.EqualsIgnoreCase("moz-icon"))
    {
        // exception for file: linking to moz-icon://.ext?size=...
        // Note that because targetScheme is the base (innermost) URI scheme,
        // this does NOT allow file -> moz-icon:file:///... links.
        // This is intentional.
        return NS_OK;
    }

    // Check for webextension
    rv = NS_URIChainHasFlags(aTargetURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_EXTENSIONS,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasFlags && BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
      return NS_OK;
    }

    // If we get here, check all the schemes can link to each other, from the top down:
    nsCaseInsensitiveCStringComparator stringComparator;
    nsCOMPtr<nsIURI> currentURI = sourceURI;
    nsCOMPtr<nsIURI> currentOtherURI = aTargetURI;

    bool denySameSchemeLinks = false;
    rv = NS_URIChainHasFlags(aTargetURI, nsIProtocolHandler::URI_SCHEME_NOT_SELF_LINKABLE,
                             &denySameSchemeLinks);
    if (NS_FAILED(rv)) return rv;

    while (currentURI && currentOtherURI) {
        nsAutoCString scheme, otherScheme;
        currentURI->GetScheme(scheme);
        currentOtherURI->GetScheme(otherScheme);

        bool schemesMatch = scheme.Equals(otherScheme, stringComparator);
        bool isSamePage = false;
        // about: URIs are special snowflakes.
        if (scheme.EqualsLiteral("about") && schemesMatch) {
            nsAutoCString moduleName, otherModuleName;
            // about: pages can always link to themselves:
            isSamePage =
              NS_SUCCEEDED(NS_GetAboutModuleName(currentURI, moduleName)) &&
              NS_SUCCEEDED(NS_GetAboutModuleName(currentOtherURI, otherModuleName)) &&
              moduleName.Equals(otherModuleName);
            if (!isSamePage) {
                // We will have allowed the load earlier if the source page has
                // system principal. So we know the source has a content
                // principal, and it's trying to link to something else.
                // Linkable about: pages are always reachable, even if we hit
                // the CheckLoadURIFlags call below.
                // We punch only 1 other hole: iff the source is unlinkable,
                // we let them link to other pages explicitly marked SAFE
                // for content. This avoids world-linkable about: pages linking
                // to non-world-linkable about: pages.
                nsCOMPtr<nsIAboutModule> module, otherModule;
                bool knowBothModules =
                    NS_SUCCEEDED(NS_GetAboutModule(currentURI, getter_AddRefs(module))) &&
                    NS_SUCCEEDED(NS_GetAboutModule(currentOtherURI, getter_AddRefs(otherModule)));
                uint32_t aboutModuleFlags = 0;
                uint32_t otherAboutModuleFlags = 0;
                knowBothModules = knowBothModules &&
                    NS_SUCCEEDED(module->GetURIFlags(currentURI, &aboutModuleFlags)) &&
                    NS_SUCCEEDED(otherModule->GetURIFlags(currentOtherURI, &otherAboutModuleFlags));
                if (knowBothModules) {
                    isSamePage =
                        !(aboutModuleFlags & nsIAboutModule::MAKE_LINKABLE) &&
                        (otherAboutModuleFlags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT);
                    if (isSamePage && otherAboutModuleFlags & nsIAboutModule::MAKE_LINKABLE) {
                        //XXXgijs: this is a hack. The target will be nested
                        // (with innerURI of moz-safe-about:whatever), and
                        // the source isn't, so we won't pass if we finish
                        // the loop. We *should* pass, though, so return here.
                        // This hack can go away when bug 1228118 is fixed.
                        return NS_OK;
                    }
                }
            }
        } else {
            bool equalExceptRef = false;
            rv = currentURI->EqualsExceptRef(currentOtherURI, &equalExceptRef);
            isSamePage = NS_SUCCEEDED(rv) && equalExceptRef;
        }

        // If schemes are not equal, or they're equal but the target URI
        // is different from the source URI and doesn't always allow linking
        // from the same scheme, check if the URI flags of the current target
        // URI allow the current source URI to link to it.
        // The policy is specified by the protocol flags on both URIs.
        if (!schemesMatch || (denySameSchemeLinks && !isSamePage)) {
            return CheckLoadURIFlags(currentURI, currentOtherURI,
                                     sourceBaseURI, targetBaseURI, aFlags);
        }
        // Otherwise... check if we can nest another level:
        nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(currentURI);
        nsCOMPtr<nsINestedURI> nestedOtherURI = do_QueryInterface(currentOtherURI);

        // If schemes match and neither URI is nested further, we're OK.
        if (!nestedURI && !nestedOtherURI) {
            return NS_OK;
        }
        // If one is nested and the other isn't, something is wrong.
        if (!nestedURI != !nestedOtherURI) {
            return NS_ERROR_DOM_BAD_URI;
        }
        // Otherwise, both should be nested and we'll go through the loop again.
        nestedURI->GetInnerURI(getter_AddRefs(currentURI));
        nestedOtherURI->GetInnerURI(getter_AddRefs(currentOtherURI));
    }

    // We should never get here. We should always return from inside the loop.
    return NS_ERROR_DOM_BAD_URI;
}

/**
 * Helper method to check whether the target URI and its innermost ("base") URI
 * has protocol flags that should stop it from being loaded by the source URI
 * (and/or the source URI's innermost ("base") URI), taking into account any
 * nsIScriptSecurityManager flags originally passed to
 * CheckLoadURIWithPrincipal and friends.
 *
 * @return if success, access is allowed. Otherwise, deny access
 */
nsresult
nsScriptSecurityManager::CheckLoadURIFlags(nsIURI *aSourceURI,
                                           nsIURI *aTargetURI,
                                           nsIURI *aSourceBaseURI,
                                           nsIURI *aTargetBaseURI,
                                           uint32_t aFlags)
{
    // Note that the order of policy checks here is very important!
    // We start from most restrictive and work our way down.
    bool reportErrors = !(aFlags & nsIScriptSecurityManager::DONT_REPORT_ERRORS);
    const char* errorTag = "CheckLoadURIError";

    nsAutoCString targetScheme;
    nsresult rv = aTargetBaseURI->GetScheme(targetScheme);
    if (NS_FAILED(rv)) return rv;

    // Check for system target URI
    rv = DenyAccessIfURIHasFlags(aTargetURI,
                                 nsIProtocolHandler::URI_DANGEROUS_TO_LOAD);
    if (NS_FAILED(rv)) {
        // Deny access, since the origin principal is not system
        if (reportErrors) {
            ReportError(nullptr, errorTag, aSourceURI, aTargetURI);
        }
        return rv;
    }

    // Check for chrome target URI
    bool hasFlags = false;
    rv = NS_URIChainHasFlags(aTargetURI,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME) {
            // Allow a URI_IS_UI_RESOURCE source to link to a URI_IS_UI_RESOURCE
            // target if ALLOW_CHROME is set.
            //
            // ALLOW_CHROME is a flag that we pass on all loads _except_ docshell
            // loads (since docshell loads run the loaded content with its origin
            // principal). So we're effectively allowing resource://, chrome://,
            // and moz-icon:// source URIs to load resource://, chrome://, and
            // moz-icon:// files, so long as they're not loading it as a document.
            bool sourceIsUIResource;
            rv = NS_URIChainHasFlags(aSourceBaseURI,
                                     nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                     &sourceIsUIResource);
            NS_ENSURE_SUCCESS(rv, rv);
            if (sourceIsUIResource) {
                return NS_OK;
            }

            if (targetScheme.EqualsLiteral("resource")) {
                // Mochitests that need to load resource:// URIs not declared
                // content-accessible in manifests should set the preference
                // "security.all_resource_uri_content_accessible" true.
                static bool sSecurityPrefCached = false;
                static bool sAllResourceUriContentAccessible = false;
                if (!sSecurityPrefCached) {
                    sSecurityPrefCached = true;
                    Preferences::AddBoolVarCache(
                            &sAllResourceUriContentAccessible,
                            "security.all_resource_uri_content_accessible",
                            false);
                }
                if (sAllResourceUriContentAccessible) {
                    return NS_OK;
                }

                nsCOMPtr<nsIProtocolHandler> ph;
                rv = sIOService->GetProtocolHandler("resource", getter_AddRefs(ph));
                NS_ENSURE_SUCCESS(rv, rv);
                if (!ph) {
                    return NS_ERROR_DOM_BAD_URI;
                }

                nsCOMPtr<nsIResProtocolHandler> rph = do_QueryInterface(ph);
                if (!rph) {
                    return NS_ERROR_DOM_BAD_URI;
                }

                bool accessAllowed = false;
                rph->AllowContentToAccess(aTargetBaseURI, &accessAllowed);
                if (accessAllowed) {
                    return NS_OK;
                }
            } else if (targetScheme.EqualsLiteral("chrome")) {
                // Allow the load only if the chrome package is whitelisted.
                nsCOMPtr<nsIXULChromeRegistry> reg(
                        do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
                if (reg) {
                    bool accessAllowed = false;
                    reg->AllowContentToAccess(aTargetBaseURI, &accessAllowed);
                    if (accessAllowed) {
                        return NS_OK;
                    }
                }
            }
        }

        if (reportErrors) {
            ReportError(nullptr, errorTag, aSourceURI, aTargetURI);
        }
        return NS_ERROR_DOM_BAD_URI;
    }

    // Check for target URI pointing to a file
    rv = NS_URIChainHasFlags(aTargetURI,
                             nsIProtocolHandler::URI_IS_LOCAL_FILE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        // Allow domains that were whitelisted in the prefs. In 99.9% of cases,
        // this array is empty.
        bool isWhitelisted;
        MOZ_ALWAYS_SUCCEEDS(InFileURIWhitelist(aSourceURI, &isWhitelisted));
        if (isWhitelisted) {
            return NS_OK;
        }

        // Allow chrome://
        bool isChrome = false;
        if (NS_SUCCEEDED(aSourceBaseURI->SchemeIs("chrome", &isChrome)) && isChrome) {
            return NS_OK;
        }

        // Nothing else.
        if (reportErrors) {
            ReportError(nullptr, errorTag, aSourceURI, aTargetURI);
        }
        return NS_ERROR_DOM_BAD_URI;
    }

    // OK, everyone is allowed to load this, since unflagged handlers are
    // deprecated but treated as URI_LOADABLE_BY_ANYONE.  But check whether we
    // need to warn.  At some point we'll want to make this warning into an
    // error and treat unflagged handlers as URI_DANGEROUS_TO_LOAD.
    rv = NS_URIChainHasFlags(aTargetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_ANYONE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    // NB: we also get here if the base URI is URI_LOADABLE_BY_SUBSUMERS,
    // and none of the rest of the nested chain of URIs for aTargetURI
    // prohibits the load, so avoid warning in that case:
    bool hasSubsumersFlag = false;
    rv = NS_URIChainHasFlags(aTargetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS,
                             &hasSubsumersFlag);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasFlags && !hasSubsumersFlag) {
        nsCOMPtr<nsIStringBundle> bundle = BundleHelper::GetOrCreate();
        if (bundle) {
            nsAutoString message;
            NS_ConvertASCIItoUTF16 ucsTargetScheme(targetScheme);
            const char16_t* formatStrings[] = { ucsTargetScheme.get() };
            rv = bundle->FormatStringFromName("ProtocolFlagError",
                                              formatStrings,
                                              ArrayLength(formatStrings),
                                              message);
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIConsoleService> console(
                  do_GetService("@mozilla.org/consoleservice;1"));
                NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

                console->LogStringMessage(message.get());
            }
        }
    }

    return NS_OK;
}

nsresult
nsScriptSecurityManager::ReportError(JSContext* cx, const char* aMessageTag,
                                     nsIURI* aSource, nsIURI* aTarget)
{
    nsresult rv;
    NS_ENSURE_TRUE(aSource && aTarget, NS_ERROR_NULL_POINTER);

    // Get the source URL spec
    nsAutoCString sourceSpec;
    rv = aSource->GetAsciiSpec(sourceSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the target URL spec
    nsAutoCString targetSpec;
    rv = aTarget->GetAsciiSpec(targetSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> bundle = BundleHelper::GetOrCreate();
    if (NS_WARN_IF(!bundle)) {
      return NS_OK;
    }

    // Localize the error message
    nsAutoString message;
    NS_ConvertASCIItoUTF16 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUTF16 ucsTargetSpec(targetSpec);
    const char16_t *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
    rv = bundle->FormatStringFromName(aMessageTag,
                                      formatStrings,
                                      ArrayLength(formatStrings),
                                      message);
    NS_ENSURE_SUCCESS(rv, rv);

    // If a JS context was passed in, set a JS exception.
    // Otherwise, print the error message directly to the JS console
    // and to standard output
    if (cx)
    {
        SetPendingException(cx, message.get());
    }
    else // Print directly to the console
    {
        nsCOMPtr<nsIConsoleService> console(
            do_GetService("@mozilla.org/consoleservice;1"));
        NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

        console->LogStringMessage(message.get());
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIStrWithPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aTargetURIStr,
                                                      uint32_t aFlags)
{
    nsresult rv;
    nsCOMPtr<nsIURI> target;
    rv = NS_NewURI(getter_AddRefs(target), aTargetURIStr,
                   nullptr, nullptr, sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CheckLoadURIWithPrincipal(aPrincipal, target, aFlags);
    if (rv == NS_ERROR_DOM_BAD_URI) {
        // Don't warn because NS_ERROR_DOM_BAD_URI is one of the expected
        // return values.
        return rv;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    // Now start testing fixup -- since aTargetURIStr is a string, not
    // an nsIURI, we may well end up fixing it up before loading.
    // Note: This needs to stay in sync with the nsIURIFixup api.
    nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
    if (!fixup) {
        return rv;
    }

    uint32_t flags[] = {
        nsIURIFixup::FIXUP_FLAG_NONE,
        nsIURIFixup::FIXUP_FLAG_FIX_SCHEME_TYPOS,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP,
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
        nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP |
        nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI
    };

    for (uint32_t i = 0; i < ArrayLength(flags); ++i) {
        rv = fixup->CreateFixupURI(aTargetURIStr, flags[i], nullptr,
                                   getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CheckLoadURIWithPrincipal(aPrincipal, target, aFlags);
        if (rv == NS_ERROR_DOM_BAD_URI) {
            // Don't warn because NS_ERROR_DOM_BAD_URI is one of the expected
            // return values.
            return rv;
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return rv;
}

NS_IMETHODIMP
nsScriptSecurityManager::InFileURIWhitelist(nsIURI* aUri, bool* aResult)
{
    MOZ_ASSERT(aUri);
    MOZ_ASSERT(aResult);

    *aResult = false;
    for (nsIURI* uri : EnsureFileURIWhitelist()) {
        if (EqualOrSubdomain(aUri, uri)) {
            *aResult = true;
            return NS_OK;
        }
    }

    return NS_OK;
}

///////////////// Principals ///////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::GetSystemPrincipal(nsIPrincipal **result)
{
    NS_ADDREF(*result = mSystemPrincipal);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI* aURI, JS::Handle<JS::Value> aOriginAttributes,
                                                 JSContext* aCx, nsIPrincipal** aPrincipal)
{
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::CreateCodebasePrincipalFromOrigin(const nsACString& aOrigin,
                                                           nsIPrincipal** aPrincipal)
{
  if (StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("["))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (StringBeginsWith(aOrigin, NS_LITERAL_CSTRING(NS_NULLPRINCIPAL_SCHEME ":"))) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aOrigin);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::CreateNullPrincipal(JS::Handle<JS::Value> aOriginAttributes,
                                             JSContext* aCx, nsIPrincipal** aPrincipal)
{
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIPrincipal> prin = NullPrincipal::Create(attrs);
  prin.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::
  GetLoadContextCodebasePrincipal(nsIURI* aURI,
                                  nsILoadContext* aLoadContext,
                                  nsIPrincipal** aPrincipal)
{
  NS_ENSURE_STATE(aLoadContext);
  OriginAttributes docShellAttrs;
  aLoadContext->GetOriginAttributes(docShellAttrs);

  nsCOMPtr<nsIPrincipal> prin =
    BasePrincipal::CreateCodebasePrincipal(aURI, docShellAttrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetDocShellCodebasePrincipal(nsIURI* aURI,
                                                      nsIDocShell* aDocShell,
                                                      nsIPrincipal** aPrincipal)
{
  nsCOMPtr<nsIPrincipal> prin =
    BasePrincipal::CreateCodebasePrincipal(aURI, nsDocShell::Cast(aDocShell)->GetOriginAttributes());
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *cx,
                                          const nsIID &aIID,
                                          nsISupports *aObj,
                                          nsIClassInfo *aClassInfo)
{
// XXX Special case for Exception ?

    // We give remote-XUL whitelisted domains a free pass here. See bug 932906.
    JS::Rooted<JS::Realm*> contextRealm(cx, JS::GetCurrentRealmOrNull(cx));
    MOZ_RELEASE_ASSERT(contextRealm);
    if (!xpc::AllowContentXBLScope(contextRealm)) {
        return NS_OK;
    }

    if (nsContentUtils::IsCallerChrome()) {
        return NS_OK;
    }

    //-- Access denied, report an error
    nsAutoCString originUTF8;
    nsIPrincipal* subjectPrincipal = nsContentUtils::SubjectPrincipal();
    GetPrincipalDomainOrigin(subjectPrincipal, originUTF8);
    NS_ConvertUTF8toUTF16 originUTF16(originUTF8);
    nsAutoCString classInfoNameUTF8;
    if (aClassInfo) {
      aClassInfo->GetClassDescription(classInfoNameUTF8);
    }
    if (classInfoNameUTF8.IsEmpty()) {
      classInfoNameUTF8.AssignLiteral("UnnamedClass");
    }

    nsCOMPtr<nsIStringBundle> bundle = BundleHelper::GetOrCreate();
    if (NS_WARN_IF(!bundle)) {
      return NS_OK;
    }

    NS_ConvertUTF8toUTF16 classInfoUTF16(classInfoNameUTF8);
    nsresult rv;
    nsAutoString errorMsg;
    if (originUTF16.IsEmpty()) {
        const char16_t* formatStrings[] = { classInfoUTF16.get() };
        rv = bundle->FormatStringFromName("CreateWrapperDenied",
                                          formatStrings,
                                          1,
                                          errorMsg);
    } else {
        const char16_t* formatStrings[] = { classInfoUTF16.get(),
                                            originUTF16.get() };
        rv = bundle->FormatStringFromName("CreateWrapperDeniedForOrigin",
                                          formatStrings,
                                          2,
                                          errorMsg);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    SetPendingException(cx, errorMsg.get());
    return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateInstance(JSContext *cx,
                                           const nsCID &aCID)
{
    if (nsContentUtils::IsCallerChrome()) {
        return NS_OK;
    }

    //-- Access denied, report an error
    nsAutoCString errorMsg("Permission denied to create instance of class. CID=");
    char cidStr[NSID_LENGTH];
    aCID.ToProvidedString(cidStr);
    errorMsg.Append(cidStr);
    SetPendingExceptionASCII(cx, errorMsg.get());
    return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetService(JSContext *cx,
                                       const nsCID &aCID)
{
    if (nsContentUtils::IsCallerChrome()) {
        return NS_OK;
    }

    //-- Access denied, report an error
    nsAutoCString errorMsg("Permission denied to get service. CID=");
    char cidStr[NSID_LENGTH];
    aCID.ToProvidedString(cidStr);
    errorMsg.Append(cidStr);
    SetPendingExceptionASCII(cx, errorMsg.get());
    return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
}

/////////////////////////////////////
// Method implementing nsIObserver //
/////////////////////////////////////
const char sJSEnabledPrefName[] = "javascript.enabled";
const char sFileOriginPolicyPrefName[] =
    "security.fileuri.strict_origin_policy";

static const char* kObservedPrefs[] = {
  sJSEnabledPrefName,
  sFileOriginPolicyPrefName,
  "capability.policy.",
  nullptr
};


NS_IMETHODIMP
nsScriptSecurityManager::Observe(nsISupports* aObject, const char* aTopic,
                                 const char16_t* aMessage)
{
    ScriptSecurityPrefChanged();
    return NS_OK;
}

/////////////////////////////////////////////
// Constructor, Destructor, Initialization //
/////////////////////////////////////////////
nsScriptSecurityManager::nsScriptSecurityManager(void)
    : mPrefInitialized(false)
    , mIsJavaScriptEnabled(false)
{
    static_assert(sizeof(intptr_t) == sizeof(void*),
                  "intptr_t and void* have different lengths on this platform. "
                  "This may cause a security failure with the SecurityLevel union.");
}

nsresult nsScriptSecurityManager::Init()
{
    nsresult rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
    NS_ENSURE_SUCCESS(rv, rv);

    InitPrefs();

    // Create our system principal singleton
    RefPtr<SystemPrincipal> system = SystemPrincipal::Create();

    mSystemPrincipal = system;

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    sContext = danger::GetJSContext();

    static const JSSecurityCallbacks securityCallbacks = {
        ContentSecurityPolicyPermitsJSAction,
        JSPrincipalsSubsume,
    };

    MOZ_ASSERT(!JS_GetSecurityCallbacks(sContext));
    JS_SetSecurityCallbacks(sContext, &securityCallbacks);
    JS_InitDestroyPrincipalsCallback(sContext, nsJSPrincipals::Destroy);

    JS_SetTrustedPrincipals(sContext, system);

    return NS_OK;
}

static StaticRefPtr<nsScriptSecurityManager> gScriptSecMan;

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    Preferences::RemoveObservers(this, kObservedPrefs);
    if (mDomainPolicy) {
        mDomainPolicy->Deactivate();
    }
    // ContentChild might hold a reference to the domain policy,
    // and it might release it only after the security manager is
    // gone. But we can still assert this for the main process.
    MOZ_ASSERT_IF(XRE_IsParentProcess(),
                  !mDomainPolicy);
}

void
nsScriptSecurityManager::Shutdown()
{
    if (sContext) {
        JS_SetSecurityCallbacks(sContext, nullptr);
        JS_SetTrustedPrincipals(sContext, nullptr);
        sContext = nullptr;
    }

    NS_IF_RELEASE(sIOService);
    BundleHelper::Shutdown();
}

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    return gScriptSecMan;
}

/* static */ void
nsScriptSecurityManager::InitStatics()
{
    RefPtr<nsScriptSecurityManager> ssManager = new nsScriptSecurityManager();
    nsresult rv = ssManager->Init();
    if (NS_FAILED(rv)) {
        MOZ_CRASH("ssManager->Init() failed");
    }

    ClearOnShutdown(&gScriptSecMan);
    gScriptSecMan = ssManager;
}

// Currently this nsGenericFactory constructor is used only from FastLoad
// (XPCOM object deserialization) code, when "creating" the system principal
// singleton.
already_AddRefed<SystemPrincipal>
nsScriptSecurityManager::SystemPrincipalSingletonConstructor()
{
    if (gScriptSecMan)
        return do_AddRef(gScriptSecMan->mSystemPrincipal).downcast<SystemPrincipal>();
    return nullptr;
}

struct IsWhitespace {
    static bool Test(char aChar) { return NS_IsAsciiWhitespace(aChar); };
};
struct IsWhitespaceOrComma {
    static bool Test(char aChar) { return aChar == ',' || NS_IsAsciiWhitespace(aChar); };
};

template <typename Predicate>
uint32_t SkipPast(const nsCString& str, uint32_t base)
{
    while (base < str.Length() && Predicate::Test(str[base])) {
        ++base;
    }
    return base;
}

template <typename Predicate>
uint32_t SkipUntil(const nsCString& str, uint32_t base)
{
    while (base < str.Length() && !Predicate::Test(str[base])) {
        ++base;
    }
    return base;
}

inline void
nsScriptSecurityManager::ScriptSecurityPrefChanged()
{
    MOZ_ASSERT(mPrefInitialized);
    mIsJavaScriptEnabled =
        Preferences::GetBool(sJSEnabledPrefName, mIsJavaScriptEnabled);
    sStrictFileOriginPolicy =
        Preferences::GetBool(sFileOriginPolicyPrefName, false);
    mFileURIWhitelist.reset();
}

void
nsScriptSecurityManager::AddSitesToFileURIWhitelist(const nsCString& aSiteList)
{
    for (uint32_t base = SkipPast<IsWhitespace>(aSiteList, 0), bound = 0;
         base < aSiteList.Length();
         base = SkipPast<IsWhitespace>(aSiteList, bound))
    {
        // Grab the current site.
        bound = SkipUntil<IsWhitespace>(aSiteList, base);
        nsAutoCString site(Substring(aSiteList, base, bound - base));

        // Check if the URI is schemeless. If so, add both http and https.
        nsAutoCString unused;
        if (NS_FAILED(sIOService->ExtractScheme(site, unused))) {
            AddSitesToFileURIWhitelist(NS_LITERAL_CSTRING("http://") + site);
            AddSitesToFileURIWhitelist(NS_LITERAL_CSTRING("https://") + site);
            continue;
        }

        // Convert it to a URI and add it to our list.
        nsCOMPtr<nsIURI> uri;
        nsresult rv = NS_NewURI(getter_AddRefs(uri), site, nullptr, nullptr, sIOService);
        if (NS_SUCCEEDED(rv)) {
            mFileURIWhitelist.ref().AppendElement(uri);
        } else {
            nsCOMPtr<nsIConsoleService> console(do_GetService("@mozilla.org/consoleservice;1"));
            if (console) {
                nsAutoString msg = NS_LITERAL_STRING("Unable to to add site to file:// URI whitelist: ") +
                                   NS_ConvertASCIItoUTF16(site);
                console->LogStringMessage(msg.get());
            }
        }
    }
}

nsresult
nsScriptSecurityManager::InitPrefs()
{
    nsIPrefBranch* branch = Preferences::GetRootBranch();
    NS_ENSURE_TRUE(branch, NS_ERROR_FAILURE);

    mPrefInitialized = true;

    // Set the initial value of the "javascript.enabled" prefs
    ScriptSecurityPrefChanged();

    // set observer callbacks in case the value of the prefs change
    Preferences::AddStrongObservers(this, kObservedPrefs);

    OriginAttributes::InitPrefs();

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetDomainPolicyActive(bool *aRv)
{
    *aRv = !!mDomainPolicy;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::ActivateDomainPolicy(nsIDomainPolicy** aRv)
{
    if (!XRE_IsParentProcess()) {
        return NS_ERROR_SERVICE_NOT_AVAILABLE;
    }

    return ActivateDomainPolicyInternal(aRv);
}

NS_IMETHODIMP
nsScriptSecurityManager::ActivateDomainPolicyInternal(nsIDomainPolicy** aRv)
{
    // We only allow one domain policy at a time. The holder of the previous
    // policy must explicitly deactivate it first.
    if (mDomainPolicy) {
        return NS_ERROR_SERVICE_NOT_AVAILABLE;
    }

    mDomainPolicy = new DomainPolicy();
    nsCOMPtr<nsIDomainPolicy> ptr = mDomainPolicy;
    ptr.forget(aRv);
    return NS_OK;
}

// Intentionally non-scriptable. Script must have a reference to the
// nsIDomainPolicy to deactivate it.
void
nsScriptSecurityManager::DeactivateDomainPolicy()
{
    mDomainPolicy = nullptr;
}

void
nsScriptSecurityManager::CloneDomainPolicy(DomainPolicyClone* aClone)
{
    MOZ_ASSERT(aClone);
    if (mDomainPolicy) {
        mDomainPolicy->CloneDomainPolicy(aClone);
    } else {
        aClone->active() = false;
    }
}

NS_IMETHODIMP
nsScriptSecurityManager::PolicyAllowsScript(nsIURI* aURI, bool *aRv)
{
    nsresult rv;

    // Compute our rule. If we don't have any domain policy set up that might
    // provide exceptions to this rule, we're done.
    *aRv = mIsJavaScriptEnabled;
    if (!mDomainPolicy) {
        return NS_OK;
    }

    // We have a domain policy. Grab the appropriate set of exceptions to the
    // rule (either the blacklist or the whitelist, depending on whether script
    // is enabled or disabled by default).
    nsCOMPtr<nsIDomainSet> exceptions;
    nsCOMPtr<nsIDomainSet> superExceptions;
    if (*aRv) {
        mDomainPolicy->GetBlacklist(getter_AddRefs(exceptions));
        mDomainPolicy->GetSuperBlacklist(getter_AddRefs(superExceptions));
    } else {
        mDomainPolicy->GetWhitelist(getter_AddRefs(exceptions));
        mDomainPolicy->GetSuperWhitelist(getter_AddRefs(superExceptions));
    }

    bool contains;
    rv = exceptions->Contains(aURI, &contains);
    NS_ENSURE_SUCCESS(rv, rv);
    if (contains) {
        *aRv = !*aRv;
        return NS_OK;
    }
    rv = superExceptions->ContainsSuperDomain(aURI, &contains);
    NS_ENSURE_SUCCESS(rv, rv);
    if (contains) {
        *aRv = !*aRv;
    }

    return NS_OK;
}

const nsTArray<nsCOMPtr<nsIURI>>&
nsScriptSecurityManager::EnsureFileURIWhitelist()
{
    if (mFileURIWhitelist.isSome()) {
        return mFileURIWhitelist.ref();
    }

    //
    // Rebuild the set of principals for which we allow file:// URI loads. This
    // implements a small subset of an old pref-based CAPS people that people
    // have come to depend on. See bug 995943.
    //

    mFileURIWhitelist.emplace();
    nsAutoCString policies;
    mozilla::Preferences::GetCString("capability.policy.policynames", policies);
    for (uint32_t base = SkipPast<IsWhitespaceOrComma>(policies, 0), bound = 0;
         base < policies.Length();
         base = SkipPast<IsWhitespaceOrComma>(policies, bound))
    {
        // Grab the current policy name.
        bound = SkipUntil<IsWhitespaceOrComma>(policies, base);
        auto policyName = Substring(policies, base, bound - base);

        // Figure out if this policy allows loading file:// URIs. If not, we can skip it.
        nsCString checkLoadURIPrefName = NS_LITERAL_CSTRING("capability.policy.") +
                                         policyName +
                                         NS_LITERAL_CSTRING(".checkloaduri.enabled");
        nsAutoString value;
        nsresult rv = Preferences::GetString(checkLoadURIPrefName.get(), value);
        if (NS_FAILED(rv) || !value.LowerCaseEqualsLiteral("allaccess")) {
            continue;
        }

        // Grab the list of domains associated with this policy.
        nsCString domainPrefName = NS_LITERAL_CSTRING("capability.policy.") +
                                   policyName +
                                   NS_LITERAL_CSTRING(".sites");
        nsAutoCString siteList;
        Preferences::GetCString(domainPrefName.get(), siteList);
        AddSitesToFileURIWhitelist(siteList);
    }

    return mFileURIWhitelist.ref();
}
