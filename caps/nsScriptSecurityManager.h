/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptSecurityManager_h__
#define nsScriptSecurityManager_h__

#include "nsIScriptSecurityManager.h"

#include "nsIAddonPolicyService.h"
#include "mozilla/Maybe.h"
#include "nsIAddonPolicyService.h"
#include "nsIPrincipal.h"
#include "nsCOMPtr.h"
#include "nsIChannelEventSink.h"
#include "nsIObserver.h"
#include "nsServiceManagerUtils.h"
#include "plstr.h"
#include "js/TypeDecls.h"

#include <stdint.h>

class nsCString;
class nsIIOService;
class nsIStringBundle;
class nsSystemPrincipal;

namespace mozilla {
class OriginAttributes;
} // namespace mozilla

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////
#define NS_SCRIPTSECURITYMANAGER_CID \
{ 0x7ee2a4c0, 0x4b93, 0x17d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsScriptSecurityManager final : public nsIScriptSecurityManager,
                                      public nsIChannelEventSink,
                                      public nsIObserver
{
public:
    static void Shutdown();

    NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTSECURITYMANAGER
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIOBSERVER

    static nsScriptSecurityManager*
    GetScriptSecurityManager();

    // Invoked exactly once, by XPConnect.
    static void InitStatics();

    static nsSystemPrincipal*
    SystemPrincipalSingletonConstructor();

    JSContext* GetCurrentJSContext();

    JSContext* GetSafeJSContext();

    /**
     * Utility method for comparing two URIs.  For security purposes, two URIs
     * are equivalent if their schemes, hosts, and ports (if any) match.  This
     * method returns true if aSubjectURI and aObjectURI have the same origin,
     * false otherwise.
     */
    static bool SecurityCompareURIs(nsIURI* aSourceURI, nsIURI* aTargetURI);
    static uint32_t SecurityHashURI(nsIURI* aURI);

    static uint16_t AppStatusForPrincipal(nsIPrincipal *aPrin);

    static nsresult 
    ReportError(JSContext* cx, const nsAString& messageTag,
                nsIURI* aSource, nsIURI* aTarget);

    static uint32_t
    HashPrincipalByOrigin(nsIPrincipal* aPrincipal);

    static bool
    GetStrictFileOriginPolicy()
    {
        return sStrictFileOriginPolicy;
    }

    void DeactivateDomainPolicy();

private:

    // GetScriptSecurityManager is the only call that can make one
    nsScriptSecurityManager();
    virtual ~nsScriptSecurityManager();

    // Decides, based on CSP, whether or not eval() and stuff can be executed.
    static bool
    ContentSecurityPolicyPermitsJSAction(JSContext *cx);

    static bool
    JSPrincipalsSubsume(JSPrincipals *first, JSPrincipals *second);

    // Returns null if a principal cannot be found; generally callers
    // should error out at that point.
    static nsIPrincipal* doGetObjectPrincipal(JSObject* obj);

    nsresult
    Init();

    nsresult
    InitPrefs();

    inline void
    ScriptSecurityPrefChanged();

    inline void
    AddSitesToFileURIWhitelist(const nsCString& aSiteList);

    // If aURI is a moz-extension:// URI, set mAddonId to the associated addon.
    nsresult MaybeSetAddonIdFromURI(mozilla::OriginAttributes& aAttrs, nsIURI* aURI);

    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    bool mPrefInitialized;
    bool mIsJavaScriptEnabled;
    nsTArray<nsCOMPtr<nsIURI>> mFileURIWhitelist;

    // This machinery controls new-style domain policies. The old-style
    // policy machinery will be removed soon.
    nsCOMPtr<nsIDomainPolicy> mDomainPolicy;

    // Cached addon policy service. We can't generate this in Init() because
    // that's too early to get a service.
    mozilla::Maybe<nsCOMPtr<nsIAddonPolicyService>> mAddonPolicyService;
    nsIAddonPolicyService* GetAddonPolicyService()
    {
        if (mAddonPolicyService.isNothing()) {
            mAddonPolicyService.emplace(do_GetService("@mozilla.org/addons/policy-service;1"));
        }
        return mAddonPolicyService.ref();
    }

    static bool sStrictFileOriginPolicy;

    static nsIIOService    *sIOService;
    static nsIStringBundle *sStrBundle;
    static JSRuntime       *sRuntime;
};

namespace mozilla {

void
GetJarPrefix(uint32_t aAppid,
             bool aInMozBrowser,
             nsACString& aJarPrefix);

} // namespace mozilla

#endif // nsScriptSecurityManager_h__
