/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptSecurityManager.h"

#include "mozilla/ArrayUtils.h"

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsIAppsService.h"
#include "nsIInputStreamChannel.h"
#include "nsILoadContext.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsINestedURI.h"
#include "nspr.h"
#include "nsJSPrincipals.h"
#include "mozilla/BasePrincipal.h"
#include "nsSystemPrincipal.h"
#include "nsPrincipal.h"
#include "nsNullPrincipal.h"
#include "DomainPolicy.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsDOMCID.h"
#include "nsIXPConnect.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "nsIEffectiveTLDService.h"
#include "nsIProperties.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIZipReader.h"
#include "nsIXPConnect.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"
#include "nsDOMJSUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsIClassInfo.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIChromeRegistry.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozIApplication.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingUtils.h"
#include <stdint.h>
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsILoadInfo.h"
#include "nsXPCOMStrings.h"

// This should be probably defined on some other place... but I couldn't find it
#define WEBAPPS_PERM_NAME "webapps-manage"

using namespace mozilla;
using namespace mozilla::dom;

nsIIOService    *nsScriptSecurityManager::sIOService = nullptr;
nsIStringBundle *nsScriptSecurityManager::sStrBundle = nullptr;
JSRuntime       *nsScriptSecurityManager::sRuntime   = 0;
bool nsScriptSecurityManager::sStrictFileOriginPolicy = true;

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

inline void SetPendingException(JSContext *cx, const char *aMsg)
{
    JS_ReportError(cx, "%s", aMsg);
}

inline void SetPendingException(JSContext *cx, const char16_t *aMsg)
{
    JS_ReportError(cx, "%hs", aMsg);
}

// Helper class to get stuff from the ClassInfo and not waste extra time with
// virtual method calls for things it has already gotten
class ClassInfoData
{
public:
    ClassInfoData(nsIClassInfo *aClassInfo, const char *aName)
        : mClassInfo(aClassInfo),
          mName(const_cast<char *>(aName)),
          mDidGetFlags(false),
          mMustFreeName(false)
    {
    }

    ~ClassInfoData()
    {
        if (mMustFreeName)
            free(mName);
    }

    uint32_t GetFlags()
    {
        if (!mDidGetFlags) {
            if (mClassInfo) {
                nsresult rv = mClassInfo->GetFlags(&mFlags);
                if (NS_FAILED(rv)) {
                    mFlags = 0;
                }
            } else {
                mFlags = 0;
            }

            mDidGetFlags = true;
        }

        return mFlags;
    }

    bool IsDOMClass()
    {
        return !!(GetFlags() & nsIClassInfo::DOM_OBJECT);
    }

    const char* GetName()
    {
        if (!mName) {
            if (mClassInfo) {
                mClassInfo->GetClassDescription(&mName);
            }

            if (mName) {
                mMustFreeName = true;
            } else {
                mName = const_cast<char *>("UnnamedClass");
            }
        }

        return mName;
    }

private:
    nsIClassInfo *mClassInfo; // WEAK
    uint32_t mFlags;
    char *mName;
    bool mDidGetFlags;
    bool mMustFreeName;
};

JSContext *
nsScriptSecurityManager::GetCurrentJSContext()
{
    // Get JSContext from stack.
    return nsXPConnect::XPConnect()->GetCurrentJSContext();
}

JSContext *
nsScriptSecurityManager::GetSafeJSContext()
{
    // Get JSContext from stack.
    return nsXPConnect::XPConnect()->GetSafeJSContext();
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

uint16_t
nsScriptSecurityManager::AppStatusForPrincipal(nsIPrincipal *aPrin)
{
    uint32_t appId = aPrin->GetAppId();

    // After bug 1238160, the principal no longer knows how to answer "is this a
    // browser element", which is really what this code path wants. Currently,
    // desktop is the only platform where we intend to disable isolation on a
    // browser frame, so non-desktop should be able to assume that
    // inIsolatedMozBrowser is true for all mozbrowser frames.  Additionally,
    // apps are no longer used on desktop, so appId is always NO_APP_ID.  We use
    // a release assertion in nsFrameLoader::OwnerIsIsolatedMozBrowserFrame so
    // that platforms with apps can assume inIsolatedMozBrowser is true for all
    // mozbrowser frames.
    bool inIsolatedMozBrowser = aPrin->GetIsInIsolatedMozBrowserElement();

    NS_WARN_IF_FALSE(appId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
                     "Asking for app status on a principal with an unknown app id");
    // Installed apps have a valid app id (not NO_APP_ID or UNKNOWN_APP_ID)
    // and they are not inside a mozbrowser.
    if (appId == nsIScriptSecurityManager::NO_APP_ID ||
        appId == nsIScriptSecurityManager::UNKNOWN_APP_ID ||
        inIsolatedMozBrowser)
    {
        return nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    }

    nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(appsService, nsIPrincipal::APP_STATUS_NOT_INSTALLED);

    nsCOMPtr<mozIApplication> app;
    appsService->GetAppByLocalId(appId, getter_AddRefs(app));
    NS_ENSURE_TRUE(app, nsIPrincipal::APP_STATUS_NOT_INSTALLED);

    uint16_t status = nsIPrincipal::APP_STATUS_INSTALLED;
    NS_ENSURE_SUCCESS(app->GetAppStatus(&status),
                      nsIPrincipal::APP_STATUS_NOT_INSTALLED);

    nsString appOrigin;
    NS_ENSURE_SUCCESS(app->GetOrigin(appOrigin),
                      nsIPrincipal::APP_STATUS_NOT_INSTALLED);
    nsCOMPtr<nsIURI> appURI;
    NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(appURI), appOrigin),
                      nsIPrincipal::APP_STATUS_NOT_INSTALLED);

    // The app could contain a cross-origin iframe - make sure that the content
    // is actually same-origin with the app.
    MOZ_ASSERT(inIsolatedMozBrowser == false, "Checked this above");
    nsAutoCString suffix;
    PrincipalOriginAttributes attrs;
    NS_ENSURE_TRUE(attrs.PopulateFromOrigin(NS_ConvertUTF16toUTF8(appOrigin), suffix),
                   nsIPrincipal::APP_STATUS_NOT_INSTALLED);
    attrs.mAppId = appId;
    attrs.mInIsolatedMozBrowser = false;
    nsCOMPtr<nsIPrincipal> appPrin = BasePrincipal::CreateCodebasePrincipal(appURI, attrs);
    NS_ENSURE_TRUE(appPrin, nsIPrincipal::APP_STATUS_NOT_INSTALLED);
    return aPrin->Equals(appPrin) ? status
                                  : nsIPrincipal::APP_STATUS_NOT_INSTALLED;
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

nsresult
nsScriptSecurityManager::GetChannelResultPrincipal(nsIChannel* aChannel,
                                                   nsIPrincipal** aPrincipal,
                                                   bool aIgnoreSandboxing)
{
    NS_PRECONDITION(aChannel, "Must have channel!");
    nsCOMPtr<nsISupports> owner;
    aChannel->GetOwner(getter_AddRefs(owner));
    if (owner) {
        CallQueryInterface(owner, aPrincipal);
        if (*aPrincipal) {
            return NS_OK;
        }
    }

    // Check whether we have an nsILoadInfo that says what we should do.
    nsCOMPtr<nsILoadInfo> loadInfo;
    aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
    if (loadInfo) {
        if (!aIgnoreSandboxing && loadInfo->GetLoadingSandboxed()) {
            RefPtr<nsNullPrincipal> prin;
            if (loadInfo->LoadingPrincipal()) {
              prin =
                nsNullPrincipal::CreateWithInheritedAttributes(loadInfo->LoadingPrincipal());
            } else {
              NeckoOriginAttributes nAttrs;
              loadInfo->GetOriginAttributes(&nAttrs);
              PrincipalOriginAttributes pAttrs;
              pAttrs.InheritFromNecko(nAttrs);
              prin = nsNullPrincipal::Create(pAttrs);
            }
            prin.forget(aPrincipal);
            return NS_OK;
        }

        bool forceInterit = loadInfo->GetForceInheritPrincipal();
        if (aIgnoreSandboxing && !forceInterit) {
          // Check if SEC_FORCE_INHERIT_PRINCIPAL was dropped because of
          // sandboxing:
          if (loadInfo->GetLoadingSandboxed() &&
              (loadInfo->GetSecurityFlags() &
               nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL_WAS_DROPPED)) {
            forceInterit = true;
          }
        }
        if (forceInterit) {
            NS_ADDREF(*aPrincipal = loadInfo->TriggeringPrincipal());
            return NS_OK;
        }

        nsSecurityFlags securityFlags = loadInfo->GetSecurityMode();
        if (securityFlags == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS ||
            securityFlags == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS ||
            securityFlags == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {

            nsCOMPtr<nsIURI> uri;
            nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
            NS_ENSURE_SUCCESS(rv, rv);
            nsCOMPtr<nsIPrincipal> triggeringPrincipal = loadInfo->TriggeringPrincipal();
            bool inheritForAboutBlank = loadInfo->GetAboutBlankInherits();

            if (nsContentUtils::ChannelShouldInheritPrincipal(triggeringPrincipal,
                                                               uri,
                                                               inheritForAboutBlank,
                                                               false)) {
                triggeringPrincipal.forget(aPrincipal);
                return NS_OK;
            }
        }
    }
    return GetChannelURIPrincipal(aChannel, aPrincipal);
}

nsresult
nsScriptSecurityManager::MaybeSetAddonIdFromURI(PrincipalOriginAttributes& aAttrs, nsIURI* aURI)
{
  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);
  if (scheme.EqualsLiteral("moz-extension") && GetAddonPolicyService()) {
    rv = GetAddonPolicyService()->ExtensionURIToAddonId(aURI, aAttrs.mAddonId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
    NS_PRECONDITION(aChannel, "Must have channel!");

    // Get the principal from the URI.  Make sure this does the same thing
    // as nsDocument::Reset and XULDocument::StartDocumentLoad.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(aChannel, loadContext);

    nsCOMPtr<nsILoadInfo> loadInfo;
    aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
    nsContentPolicyType contentPolicyType = nsIContentPolicy::TYPE_INVALID;
    if (loadInfo) {
      contentPolicyType = loadInfo->GetExternalContentPolicyType();
    }

    PrincipalOriginAttributes attrs;
    if (nsIContentPolicy::TYPE_DOCUMENT == contentPolicyType ||
        nsIContentPolicy::TYPE_SUBDOCUMENT == contentPolicyType) {
      // If it's document or sub-document, inherit originAttributes from
      // the document.
      if (loadContext) {
        DocShellOriginAttributes docShellAttrs;
        loadContext->GetOriginAttributes(docShellAttrs);
        attrs.InheritFromDocShellToDoc(docShellAttrs, uri);
      }
    } else {
      // Inherit origin attributes from loading principal if any.
      nsCOMPtr<nsIPrincipal> loadingPrincipal;
      if (loadInfo) {
        loadInfo->GetLoadingPrincipal(getter_AddRefs(loadingPrincipal));
      }
      if (loadingPrincipal) {
        attrs = BasePrincipal::Cast(loadingPrincipal)->OriginAttributesRef();
      }
    }

    rv = MaybeSetAddonIdFromURI(attrs, uri);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
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
            ReportError(nullptr, NS_LITERAL_STRING("CheckSameOriginError"),
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

    // See if we're attempting to load a file: URI. If so, let a
    // UniversalXPConnect capability trump the above check.
    bool isFile = false;
    bool isRes = false;
    if (NS_FAILED(aURI->SchemeIs("file", &isFile)) ||
        NS_FAILED(aURI->SchemeIs("resource", &isRes)))
        return NS_ERROR_FAILURE;
    if (isFile || isRes)
    {
        if (nsContentUtils::IsCallerChrome())
            return NS_OK;
    }

    // Report error.
    nsAutoCString spec;
    if (NS_FAILED(aURI->GetAsciiSpec(spec)))
        return NS_ERROR_FAILURE;
    nsAutoCString msg("Access to '");
    msg.Append(spec);
    msg.AppendLiteral("' from script denied");
    SetPendingException(cx, msg.get());
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
    NS_PRECONDITION(aURI, "Must have URI!");

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
    // Make a clone of the incoming URI, because we're going to mutate it.
    nsCOMPtr<nsIURI> probe;
    nsresult rv = aProbeArg->Clone(getter_AddRefs(probe));
    NS_ENSURE_SUCCESS(rv, false);

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
        rv = probe->SetHost(newHost);
        NS_ENSURE_SUCCESS(rv, false);
    }
}

static bool
AllSchemesMatch(nsIURI* aURI, nsIURI* aOtherURI)
{
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(aURI);
    nsCOMPtr<nsINestedURI> nestedOtherURI = do_QueryInterface(aOtherURI);
    auto stringComparator = nsCaseInsensitiveCStringComparator();
    if (!nestedURI && !nestedOtherURI) {
        // Neither of the URIs is nested, compare their schemes directly:
        nsAutoCString scheme, otherScheme;
        aURI->GetScheme(scheme);
        aOtherURI->GetScheme(otherScheme);
        return scheme.Equals(otherScheme, stringComparator);
    }
    while (nestedURI && nestedOtherURI) {
        nsCOMPtr<nsIURI> currentURI = do_QueryInterface(nestedURI);
        nsCOMPtr<nsIURI> currentOtherURI = do_QueryInterface(nestedOtherURI);
        nsAutoCString scheme, otherScheme;
        currentURI->GetScheme(scheme);
        currentOtherURI->GetScheme(otherScheme);
        if (!scheme.Equals(otherScheme, stringComparator)) {
            return false;
        }

        nestedURI->GetInnerURI(getter_AddRefs(currentURI));
        nestedOtherURI->GetInnerURI(getter_AddRefs(currentOtherURI));
        nestedURI = do_QueryInterface(currentURI);
        nestedOtherURI = do_QueryInterface(currentOtherURI);
    }
    if (!!nestedURI != !!nestedOtherURI) {
        // If only one of the scheme chains runs out at one point, clearly the chains
        // aren't of the same length, so we bail:
        return false;
    }
    return true;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIWithPrincipal(nsIPrincipal* aPrincipal,
                                                   nsIURI *aTargetURI,
                                                   uint32_t aFlags)
{
    NS_PRECONDITION(aPrincipal, "CheckLoadURIWithPrincipal must have a principal");
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
        nsCOMPtr<nsIExpandedPrincipal> expanded = do_QueryInterface(aPrincipal);
        if (expanded) {
            nsTArray< nsCOMPtr<nsIPrincipal> > *whiteList;
            expanded->GetWhiteList(&whiteList);
            for (uint32_t i = 0; i < whiteList->Length(); ++i) {
                nsresult rv = CheckLoadURIWithPrincipal((*whiteList)[i],
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

    NS_NAMED_LITERAL_STRING(errorTag, "CheckLoadURIError");
    bool reportErrors = !(aFlags & nsIScriptSecurityManager::DONT_REPORT_ERRORS);

    // Check for uris that are only loadable by principals that subsume them
    bool hasFlags;
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasFlags) {
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
    else if (AllSchemesMatch(sourceURI, aTargetURI) ||
             (sViewSourceReachableFromInner &&
              sourceScheme.EqualsIgnoreCase(targetScheme.get()) &&
              NS_SUCCEEDED(aTargetURI->SchemeIs("view-source", &targetIsViewSource)) &&
              targetIsViewSource))
    {
        // every scheme can access another URI from the same scheme,
        // as long as they don't represent null principals...
        // Or they don't require an special permission to do so
        // See bug#773886
        rv = NS_URIChainHasFlags(targetBaseURI,
                                 nsIProtocolHandler::URI_CROSS_ORIGIN_NEEDS_WEBAPPS_PERM,
                                 &hasFlags);
        NS_ENSURE_SUCCESS(rv, rv);

        if (hasFlags) {
            // Let apps load the whitelisted theme resources even if they don't
            // have the webapps-manage permission but have the themeable one.
            // Resources from the theme origin are also allowed to load from
            // the theme origin (eg. stylesheets using images from the theme).
            auto themeOrigin = Preferences::GetCString("b2g.theme.origin");
            if (themeOrigin) {
                nsAutoCString targetOrigin;
                nsPrincipal::GetOriginForURI(targetBaseURI, targetOrigin);
                if (targetOrigin.Equals(themeOrigin)) {
                    nsAutoCString pOrigin;
                    aPrincipal->GetOrigin(pOrigin);
                    return nsContentUtils::IsExactSitePermAllow(aPrincipal, "themeable") ||
                           pOrigin.Equals(themeOrigin)
                        ? NS_OK : NS_ERROR_DOM_BAD_URI;
                }
            }
            // In this case, we allow opening only if the source and target URIS
            // are on the same domain, or the opening URI has the webapps
            // permision granted
            if (!SecurityCompareURIs(sourceBaseURI, targetBaseURI) &&
                !nsContentUtils::IsExactSitePermAllow(aPrincipal, WEBAPPS_PERM_NAME)) {
                return NS_ERROR_DOM_BAD_URI;
            }
        }
        return NS_OK;
    }

    // If the schemes don't match, the policy is specified by the protocol
    // flags on the target URI.  Note that the order of policy checks here is
    // very important!  We start from most restrictive and work our way down.
    // Note that since we're working with the innermost URI, we can just use
    // the methods that work on chains of nested URIs and they will only look
    // at the flags for our one URI.

    // Check for system target URI
    rv = DenyAccessIfURIHasFlags(aTargetURI,
                                 nsIProtocolHandler::URI_DANGEROUS_TO_LOAD);
    if (NS_FAILED(rv)) {
        // Deny access, since the origin principal is not system
        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
        }
        return rv;
    }

    // Check for chrome target URI
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) {
        if (aFlags & nsIScriptSecurityManager::ALLOW_CHROME) {

            // For now, don't change behavior for resource:// or moz-icon:// and
            // just allow them.
            if (!targetScheme.EqualsLiteral("chrome")) {
                return NS_OK;
            }

            // Allow a URI_IS_UI_RESOURCE source to link to a URI_IS_UI_RESOURCE
            // target if ALLOW_CHROME is set.
            //
            // ALLOW_CHROME is a flag that we pass on all loads _except_ docshell
            // loads (since docshell loads run the loaded content with its origin
            // principal). So we're effectively allowing resource://, chrome://,
            // and moz-icon:// source URIs to load resource://, chrome://, and
            // moz-icon:// files, so long as they're not loading it as a document.
            bool sourceIsUIResource;
            rv = NS_URIChainHasFlags(sourceBaseURI,
                                     nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                     &sourceIsUIResource);
            NS_ENSURE_SUCCESS(rv, rv);
            if (sourceIsUIResource) {
                return NS_OK;
            }

            // Allow the load only if the chrome package is whitelisted.
            nsCOMPtr<nsIXULChromeRegistry> reg(do_GetService(
                                                 NS_CHROMEREGISTRY_CONTRACTID));
            if (reg) {
                bool accessAllowed = false;
                reg->AllowContentToAccess(targetBaseURI, &accessAllowed);
                if (accessAllowed) {
                    return NS_OK;
                }
            }
        }

        // Special-case the hidden window: it's allowed to load
        // URI_IS_UI_RESOURCE no matter what.  Bug 1145470 tracks removing this.
        nsAutoCString sourceSpec;
        if (NS_SUCCEEDED(sourceBaseURI->GetSpec(sourceSpec)) &&
            sourceSpec.EqualsLiteral("resource://gre-resources/hiddenWindow.html")) {
            return NS_OK;
        }

        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
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
        for (size_t i = 0; i < mFileURIWhitelist.Length(); ++i) {
            if (EqualOrSubdomain(sourceURI, mFileURIWhitelist[i])) {
                return NS_OK;
            }
        }

        // Allow chrome://
        if (sourceScheme.EqualsLiteral("chrome")) {
            return NS_OK;
        }

        // Nothing else.
        if (reportErrors) {
            ReportError(nullptr, errorTag, sourceURI, aTargetURI);
        }
        return NS_ERROR_DOM_BAD_URI;
    }

    // OK, everyone is allowed to load this, since unflagged handlers are
    // deprecated but treated as URI_LOADABLE_BY_ANYONE.  But check whether we
    // need to warn.  At some point we'll want to make this warning into an
    // error and treat unflagged handlers as URI_DANGEROUS_TO_LOAD.
    rv = NS_URIChainHasFlags(targetBaseURI,
                             nsIProtocolHandler::URI_LOADABLE_BY_ANYONE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasFlags) {
        nsXPIDLString message;
        NS_ConvertASCIItoUTF16 ucsTargetScheme(targetScheme);
        const char16_t* formatStrings[] = { ucsTargetScheme.get() };
        rv = sStrBundle->
            FormatStringFromName(MOZ_UTF16("ProtocolFlagError"),
                                 formatStrings,
                                 ArrayLength(formatStrings),
                                 getter_Copies(message));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIConsoleService> console(
              do_GetService("@mozilla.org/consoleservice;1"));
            NS_ENSURE_TRUE(console, NS_ERROR_FAILURE);

            console->LogStringMessage(message.get());
        }
    }

    return NS_OK;
}

nsresult
nsScriptSecurityManager::ReportError(JSContext* cx, const nsAString& messageTag,
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

    // Localize the error message
    nsXPIDLString message;
    NS_ConvertASCIItoUTF16 ucsSourceSpec(sourceSpec);
    NS_ConvertASCIItoUTF16 ucsTargetSpec(targetSpec);
    const char16_t *formatStrings[] = { ucsSourceSpec.get(), ucsTargetSpec.get() };
    rv = sStrBundle->FormatStringFromName(PromiseFlatString(messageTag).get(),
                                          formatStrings,
                                          ArrayLength(formatStrings),
                                          getter_Copies(message));
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

bool
nsScriptSecurityManager::ScriptAllowed(JSObject *aGlobal)
{
    MOZ_ASSERT(aGlobal);
    MOZ_ASSERT(JS_IsGlobalObject(aGlobal) || js::IsWindowProxy(aGlobal));

    // Check the bits on the compartment private.
    return xpc::Scriptability::Get(aGlobal).Allowed();
}

///////////////// Principals ///////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::GetSystemPrincipal(nsIPrincipal **result)
{
    NS_ADDREF(*result = mSystemPrincipal);

    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSimpleCodebasePrincipal(nsIURI* aURI,
                                                    nsIPrincipal** aPrincipal)
{
  PrincipalOriginAttributes attrs(UNKNOWN_APP_ID, false);
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetNoAppCodebasePrincipal(nsIURI* aURI,
                                                   nsIPrincipal** aPrincipal)
{
  PrincipalOriginAttributes attrs(NO_APP_ID, false);
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI* aURI,
                                              nsIPrincipal** aPrincipal)
{
  return GetNoAppCodebasePrincipal(aURI, aPrincipal);
}

NS_IMETHODIMP
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI* aURI, JS::Handle<JS::Value> aOriginAttributes,
                                                 JSContext* aCx, nsIPrincipal** aPrincipal)
{
  PrincipalOriginAttributes attrs;
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
  PrincipalOriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIPrincipal> prin = nsNullPrincipal::Create(attrs);
  prin.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CreateExpandedPrincipal(nsIPrincipal** aPrincipalArray, uint32_t aLength,
                                                 nsIPrincipal** aResult)
{
  nsTArray<nsCOMPtr<nsIPrincipal>> principals;
  principals.SetCapacity(aLength);
  for (uint32_t i = 0; i < aLength; ++i) {
    principals.AppendElement(aPrincipalArray[i]);
  }

  nsCOMPtr<nsIPrincipal> p = new nsExpandedPrincipal(principals);
  p.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetAppCodebasePrincipal(nsIURI* aURI,
                                                 uint32_t aAppId,
                                                 bool aInIsolatedMozBrowser,
                                                 nsIPrincipal** aPrincipal)
{
  NS_ENSURE_TRUE(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
                 NS_ERROR_INVALID_ARG);

  PrincipalOriginAttributes attrs(aAppId, aInIsolatedMozBrowser);
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::
  GetLoadContextCodebasePrincipal(nsIURI* aURI,
                                  nsILoadContext* aLoadContext,
                                  nsIPrincipal** aPrincipal)
{
  DocShellOriginAttributes docShellAttrs;
  bool result = aLoadContext->GetOriginAttributes(docShellAttrs);;
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  PrincipalOriginAttributes attrs;
  attrs.InheritFromDocShellToDoc(docShellAttrs, aURI);

  nsresult rv = MaybeSetAddonIdFromURI(attrs, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetDocShellCodebasePrincipal(nsIURI* aURI,
                                                      nsIDocShell* aDocShell,
                                                      nsIPrincipal** aPrincipal)
{
  PrincipalOriginAttributes attrs;
  attrs.InheritFromDocShellToDoc(nsDocShell::Cast(aDocShell)->GetOriginAttributes(), aURI);

  nsresult rv = MaybeSetAddonIdFromURI(attrs, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> prin = BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
  prin.forget(aPrincipal);
  return *aPrincipal ? NS_OK : NS_ERROR_FAILURE;
}

// static
nsIPrincipal*
nsScriptSecurityManager::doGetObjectPrincipal(JSObject *aObj)
{
    JSCompartment *compartment = js::GetObjectCompartment(aObj);
    JSPrincipals *principals = JS_GetCompartmentPrincipals(compartment);
    return nsJSPrincipals::get(principals);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *cx,
                                          const nsIID &aIID,
                                          nsISupports *aObj,
                                          nsIClassInfo *aClassInfo)
{
// XXX Special case for nsIXPCException ?
    ClassInfoData objClassInfo = ClassInfoData(aClassInfo, nullptr);
    if (objClassInfo.IsDOMClass())
    {
        return NS_OK;
    }

    // We give remote-XUL whitelisted domains a free pass here. See bug 932906.
    if (!xpc::AllowContentXBLScope(js::GetContextCompartment(cx)))
    {
        return NS_OK;
    }

    if (nsContentUtils::IsCallerChrome())
    {
        return NS_OK;
    }

    //-- Access denied, report an error
    NS_ConvertUTF8toUTF16 strName("CreateWrapperDenied");
    nsAutoCString origin;
    nsIPrincipal* subjectPrincipal = nsContentUtils::SubjectPrincipal();
    GetPrincipalDomainOrigin(subjectPrincipal, origin);
    NS_ConvertUTF8toUTF16 originUnicode(origin);
    NS_ConvertUTF8toUTF16 classInfoName(objClassInfo.GetName());
    const char16_t* formatStrings[] = {
        classInfoName.get(),
        originUnicode.get()
    };
    uint32_t length = ArrayLength(formatStrings);
    if (originUnicode.IsEmpty()) {
        --length;
    } else {
        strName.AppendLiteral("ForOrigin");
    }
    nsXPIDLString errorMsg;
    nsresult rv = sStrBundle->FormatStringFromName(strName.get(),
                                                   formatStrings,
                                                   length,
                                                   getter_Copies(errorMsg));
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
    SetPendingException(cx, errorMsg.get());
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
    SetPendingException(cx, errorMsg.get());
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

    nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();
    if (!bundleService)
        return NS_ERROR_FAILURE;

    rv = bundleService->CreateBundle("chrome://global/locale/security/caps.properties", &sStrBundle);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create our system principal singleton
    RefPtr<nsSystemPrincipal> system = new nsSystemPrincipal();

    mSystemPrincipal = system;

    //-- Register security check callback in the JS engine
    //   Currently this is used to control access to function.caller
    sRuntime = xpc::GetJSRuntime();

    static const JSSecurityCallbacks securityCallbacks = {
        ContentSecurityPolicyPermitsJSAction,
        JSPrincipalsSubsume,
    };

    MOZ_ASSERT(!JS_GetSecurityCallbacks(sRuntime));
    JS_SetSecurityCallbacks(sRuntime, &securityCallbacks);
    JS_InitDestroyPrincipalsCallback(sRuntime, nsJSPrincipals::Destroy);

    JS_SetTrustedPrincipals(sRuntime, system);

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
    if (sRuntime) {
        JS_SetSecurityCallbacks(sRuntime, nullptr);
        JS_SetTrustedPrincipals(sRuntime, nullptr);
        sRuntime = nullptr;
    }

    NS_IF_RELEASE(sIOService);
    NS_IF_RELEASE(sStrBundle);
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
        MOZ_CRASH();
    }

    ClearOnShutdown(&gScriptSecMan);
    gScriptSecMan = ssManager;
}

// Currently this nsGenericFactory constructor is used only from FastLoad
// (XPCOM object deserialization) code, when "creating" the system principal
// singleton.
nsSystemPrincipal *
nsScriptSecurityManager::SystemPrincipalSingletonConstructor()
{
    nsIPrincipal *sysprin = nullptr;
    if (gScriptSecMan)
        NS_ADDREF(sysprin = gScriptSecMan->mSystemPrincipal);
    return static_cast<nsSystemPrincipal*>(sysprin);
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

    //
    // Rebuild the set of principals for which we allow file:// URI loads. This
    // implements a small subset of an old pref-based CAPS people that people
    // have come to depend on. See bug 995943.
    //

    mFileURIWhitelist.Clear();
    auto policies = mozilla::Preferences::GetCString("capability.policy.policynames");
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
        if (!Preferences::GetString(checkLoadURIPrefName.get()).LowerCaseEqualsLiteral("allaccess")) {
            continue;
        }

        // Grab the list of domains associated with this policy.
        nsCString domainPrefName = NS_LITERAL_CSTRING("capability.policy.") +
                                   policyName +
                                   NS_LITERAL_CSTRING(".sites");
        auto siteList = Preferences::GetCString(domainPrefName.get());
        AddSitesToFileURIWhitelist(siteList);
    }
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
            mFileURIWhitelist.AppendElement(uri);
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

    return NS_OK;
}

namespace mozilla {

void
GetJarPrefix(uint32_t aAppId, bool aInIsolatedMozBrowser, nsACString& aJarPrefix)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  if (aAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  aJarPrefix.Truncate();

  // Fallback.
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID && !aInIsolatedMozBrowser) {
    return;
  }

  // aJarPrefix = appId + "+" + { 't', 'f' } + "+";
  aJarPrefix.AppendInt(aAppId);
  aJarPrefix.Append('+');
  aJarPrefix.Append(aInIsolatedMozBrowser ? 't' : 'f');
  aJarPrefix.Append('+');

  return;
}

} // namespace mozilla

NS_IMETHODIMP
nsScriptSecurityManager::GetJarPrefix(uint32_t aAppId,
                                      bool aInIsolatedMozBrowser,
                                      nsACString& aJarPrefix)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  mozilla::GetJarPrefix(aAppId, aInIsolatedMozBrowser, aJarPrefix);
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
