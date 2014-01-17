/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptSecurityManager_h__
#define nsScriptSecurityManager_h__

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIXPCSecurityManager.h"
#include "nsInterfaceHashtable.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsIChannelEventSink.h"
#include "nsIObserver.h"
#include "pldhash.h"
#include "plstr.h"
#include "nsIScriptExternalNameSet.h"
#include "js/TypeDecls.h"

#include <stdint.h>

class nsIDocShell;
class nsString;
class nsIClassInfo;
class nsIIOService;
class nsIStringBundle;
class nsSystemPrincipal;
class ClassInfoData;

/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////
#define NS_SCRIPTSECURITYMANAGER_CID \
{ 0x7ee2a4c0, 0x4b93, 0x17d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsScriptSecurityManager : public nsIScriptSecurityManager,
                                public nsIChannelEventSink,
                                public nsIObserver
{
public:
    static void Shutdown();
    
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)
        
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTSECURITYMANAGER
    NS_DECL_NSIXPCSECURITYMANAGER
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIOBSERVER

    static nsScriptSecurityManager*
    GetScriptSecurityManager();

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

    static nsresult 
    ReportError(JSContext* cx, const nsAString& messageTag,
                nsIURI* aSource, nsIURI* aTarget);

    static nsresult
    CheckSameOriginPrincipal(nsIPrincipal* aSubject,
                             nsIPrincipal* aObject);
    static uint32_t
    HashPrincipalByOrigin(nsIPrincipal* aPrincipal);

    static bool
    GetStrictFileOriginPolicy()
    {
        return sStrictFileOriginPolicy;
    }

    /**
     * Returns true if the two principals share the same app attributes.
     *
     * App attributes are appId and the inBrowserElement flag.
     * Two principals have the same app attributes if those information are
     * equals.
     * This method helps keeping principals from different apps isolated from
     * each other. Also, it helps making sure mozbrowser (web views) and their
     * parent are isolated from each other. All those entities do not share the
     * same data (cookies, IndexedDB, localStorage, etc.) so we shouldn't allow
     * violating that principle.
     */
    static bool
    AppAttributesEqual(nsIPrincipal* aFirst,
                       nsIPrincipal* aSecond);

    void DeactivateDomainPolicy();

private:

    // GetScriptSecurityManager is the only call that can make one
    nsScriptSecurityManager();
    virtual ~nsScriptSecurityManager();

    bool SubjectIsPrivileged();

    static bool
    CheckObjectAccess(JSContext *cx, JS::Handle<JSObject*> obj,
                      JS::Handle<jsid> id, JSAccessMode mode,
                      JS::MutableHandle<JS::Value> vp);

    // Decides, based on CSP, whether or not eval() and stuff can be executed.
    static bool
    ContentSecurityPolicyPermitsJSAction(JSContext *cx);

    static bool
    JSPrincipalsSubsume(JSPrincipals *first, JSPrincipals *second);

    // Returns null if a principal cannot be found; generally callers
    // should error out at that point.
    static nsIPrincipal* doGetObjectPrincipal(JSObject* obj);

    // Returns null if a principal cannot be found.  Note that rv can be NS_OK
    // when this happens -- this means that there was no JS running.
    nsIPrincipal*
    doGetSubjectPrincipal(nsresult* rv);
    
    nsresult
    CheckPropertyAccessImpl(uint32_t aAction,
                            nsAXPCNativeCallContext* aCallContext,
                            JSContext* cx, JSObject* aJSObject,
                            nsISupports* aObj,
                            nsIClassInfo* aClassInfo,
                            const char* aClassName, jsid aProperty);

    nsresult
    CheckSameOriginDOMProp(nsIPrincipal* aSubject, 
                           nsIPrincipal* aObject,
                           uint32_t aAction);

    nsresult
    GetCodebasePrincipalInternal(nsIURI* aURI, uint32_t aAppId,
                                 bool aInMozBrowser,
                                 nsIPrincipal** result);

    nsresult
    CreateCodebasePrincipal(nsIURI* aURI, uint32_t aAppId, bool aInMozBrowser,
                            nsIPrincipal** result);

    // Returns null if a principal cannot be found.  Note that rv can be NS_OK
    // when this happens -- this means that there was no script for the
    // context.  Callers MUST pass in a non-null rv here.
    nsIPrincipal*
    GetSubjectPrincipal(JSContext* cx, nsresult* rv);

    nsresult
    Init();

    nsresult
    InitPrefs();

    inline void
    ScriptSecurityPrefChanged();

    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    bool mPrefInitialized;
    bool mIsJavaScriptEnabled;

    // This machinery controls new-style domain policies. The old-style
    // policy machinery will be removed soon.
    nsCOMPtr<nsIDomainPolicy> mDomainPolicy;

    static bool sStrictFileOriginPolicy;

    static nsIIOService    *sIOService;
    static nsIStringBundle *sStrBundle;
    static JSRuntime       *sRuntime;
};

#define NS_SECURITYNAMESET_CID \
 { 0x7c02eadc, 0x76, 0x4d03, \
 { 0x99, 0x8d, 0x80, 0xd7, 0x79, 0xc4, 0x85, 0x89 } }
#define NS_SECURITYNAMESET_CONTRACTID "@mozilla.org/security/script/nameset;1"

class nsSecurityNameSet : public nsIScriptExternalNameSet 
{
public:
    nsSecurityNameSet();
    virtual ~nsSecurityNameSet();
    
    NS_DECL_ISUPPORTS

    NS_IMETHOD InitializeNameSet(nsIScriptContext* aScriptContext);
};

namespace mozilla {

void
GetJarPrefix(uint32_t aAppid,
             bool aInMozBrowser,
             nsACString& aJarPrefix);

} // namespace mozilla

#endif // nsScriptSecurityManager_h__
