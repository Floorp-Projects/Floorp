/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptSecurityManager_h__
#define nsScriptSecurityManager_h__

#include "nsIScriptSecurityManager.h"

#include "mozilla/Maybe.h"
#include "nsIPrincipal.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFwd.h"
#include "plstr.h"
#include "js/TypeDecls.h"

#include <stdint.h>

class nsIIOService;
class nsIStringBundle;

namespace mozilla {
class OriginAttributes;
class SystemPrincipal;
} // namespace mozilla

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////
#define NS_SCRIPTSECURITYMANAGER_CID \
{ 0x7ee2a4c0, 0x4b93, 0x17d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsScriptSecurityManager final : public nsIScriptSecurityManager
{
public:
    static void Shutdown();

    NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTSECURITYMANAGER

    static nsScriptSecurityManager*
    GetScriptSecurityManager();

    // Invoked exactly once, by XPConnect.
    static void InitStatics();

    static already_AddRefed<mozilla::SystemPrincipal>
    SystemPrincipalSingletonConstructor();

    /**
     * Utility method for comparing two URIs.  For security purposes, two URIs
     * are equivalent if their schemes, hosts, and ports (if any) match.  This
     * method returns true if aSubjectURI and aObjectURI have the same origin,
     * false otherwise.
     */
    static bool SecurityCompareURIs(nsIURI* aSourceURI, nsIURI* aTargetURI);
    static uint32_t SecurityHashURI(nsIURI* aURI);

    static nsresult
    ReportError(JSContext* cx, const char* aMessageTag,
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
    ContentSecurityPolicyPermitsJSAction(JSContext *cx, JS::HandleValue aValue);

    static bool
    JSPrincipalsSubsume(JSPrincipals *first, JSPrincipals *second);

    nsresult
    Init();

    nsresult
    InitPrefs();

    void
    ScriptSecurityPrefChanged(const char* aPref = nullptr);

    inline void
    AddSitesToFileURIWhitelist(const nsCString& aSiteList);

    nsresult GetChannelResultPrincipal(nsIChannel* aChannel,
                                       nsIPrincipal** aPrincipal,
                                       bool aIgnoreSandboxing);

    nsresult
    CheckLoadURIFlags(nsIURI* aSourceURI, nsIURI* aTargetURI, nsIURI* aSourceBaseURI,
                      nsIURI* aTargetBaseURI, uint32_t aFlags);

    // Returns the file URI whitelist, initializing it if it has not been
    // initialized.
    const nsTArray<nsCOMPtr<nsIURI>>& EnsureFileURIWhitelist();

    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    bool mPrefInitialized;
    bool mIsJavaScriptEnabled;

    // List of URIs whose domains and sub-domains are whitelisted to allow
    // access to file: URIs.  Lazily initialized; isNothing() when not yet
    // initialized.
    mozilla::Maybe<nsTArray<nsCOMPtr<nsIURI>>> mFileURIWhitelist;

    // This machinery controls new-style domain policies. The old-style
    // policy machinery will be removed soon.
    nsCOMPtr<nsIDomainPolicy> mDomainPolicy;

    static bool sStrictFileOriginPolicy;

    static nsIIOService    *sIOService;
    static nsIStringBundle *sStrBundle;
    static JSContext       *sContext;
};

#endif // nsScriptSecurityManager_h__
