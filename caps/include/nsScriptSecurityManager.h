/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptSecurityManager_h__
#define nsScriptSecurityManager_h__

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "nsIXPCSecurityManager.h"
#include "nsInterfaceHashtable.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsIChannelEventSink.h"
#include "nsIObserver.h"
#include "pldhash.h"
#include "plstr.h"
#include "nsIScriptExternalNameSet.h"

#include <stdint.h>

class nsIDocShell;
class nsString;
class nsIClassInfo;
class nsIIOService;
class nsIStringBundle;
class nsSystemPrincipal;
struct ClassPolicy;
class ClassInfoData;
class DomainPolicy;

/////////////////////
// PrincipalKey //
/////////////////////

class PrincipalKey : public PLDHashEntryHdr
{
public:
    typedef const nsIPrincipal* KeyType;
    typedef const nsIPrincipal* KeyTypePointer;

    PrincipalKey(const nsIPrincipal* key)
      : mKey(const_cast<nsIPrincipal*>(key))
    {
    }

    PrincipalKey(const PrincipalKey& toCopy)
      : mKey(toCopy.mKey)
    {
    } 

    ~PrincipalKey()
    {
    }

    KeyType GetKey() const
    {
        return mKey;
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
        bool eq;
        mKey->Equals(const_cast<nsIPrincipal*>(aKey),
                     &eq);
        return eq;
    }

    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
        return aKey;
    }

    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
        uint32_t hash;
        const_cast<nsIPrincipal*>(aKey)->GetHashValue(&hash);
        return PLDHashNumber(hash);
    }

    enum { ALLOW_MEMMOVE = true };

private:
    nsCOMPtr<nsIPrincipal> mKey;
};

////////////////////
// Policy Storage //
////////////////////

// Property Policy
union SecurityLevel
{
    intptr_t   level;
    char*      capability;
};

// Security levels
// These values all have the low bit set (except UNDEFINED_ACCESS)
// to distinguish them from pointer values, because no pointer
// to allocated memory ever has the low bit set. A SecurityLevel
// contains either one of these constants or a pointer to a string
// representing the name of a capability.

#define SCRIPT_SECURITY_UNDEFINED_ACCESS 0
#define SCRIPT_SECURITY_ACCESS_IS_SET_BIT 1
#define SCRIPT_SECURITY_NO_ACCESS \
  ((1 << 0) | SCRIPT_SECURITY_ACCESS_IS_SET_BIT)
#define SCRIPT_SECURITY_SAME_ORIGIN_ACCESS \
  ((1 << 1) | SCRIPT_SECURITY_ACCESS_IS_SET_BIT)
#define SCRIPT_SECURITY_ALL_ACCESS \
  ((1 << 2) | SCRIPT_SECURITY_ACCESS_IS_SET_BIT)

#define SECURITY_ACCESS_LEVEL_FLAG(_sl) \
           ((_sl.level == 0) || \
            (_sl.level & SCRIPT_SECURITY_ACCESS_IS_SET_BIT))


struct PropertyPolicy : public PLDHashEntryHdr
{
    JSString       *key;  // interned string
    SecurityLevel  mGet;
    SecurityLevel  mSet;
};

static bool
InitPropertyPolicyEntry(PLDHashTable *table,
                     PLDHashEntryHdr *entry,
                     const void *key)
{
    PropertyPolicy* pp = (PropertyPolicy*)entry;
    pp->key = (JSString *)key;
    pp->mGet.level = SCRIPT_SECURITY_UNDEFINED_ACCESS;
    pp->mSet.level = SCRIPT_SECURITY_UNDEFINED_ACCESS;
    return true;
}

static void
ClearPropertyPolicyEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    PropertyPolicy* pp = (PropertyPolicy*)entry;
    pp->key = nullptr;
}

// Class Policy
#define NO_POLICY_FOR_CLASS (ClassPolicy*)1

struct ClassPolicy : public PLDHashEntryHdr
{
    char* key;
    PLDHashTable* mPolicy;

    // Note: the DomainPolicy owns us, so if if dies we will too.  Hence no
    // need to refcount it here (and in fact, we'd probably leak if we tried).
    DomainPolicy* mDomainWeAreWildcardFor;
};

static void
ClearClassPolicyEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
    ClassPolicy* cp = (ClassPolicy *)entry;
    if (cp->key)
    {
        PL_strfree(cp->key);
        cp->key = nullptr;
    }
    PL_DHashTableDestroy(cp->mPolicy);
}

// Note: actual impl is going to be after the DomainPolicy class definition,
// since we need to access members of DomainPolicy in the impl
static void
MoveClassPolicyEntry(PLDHashTable *table,
                     const PLDHashEntryHdr *from,
                     PLDHashEntryHdr *to);

static bool
InitClassPolicyEntry(PLDHashTable *table,
                     PLDHashEntryHdr *entry,
                     const void *key)
{
    static PLDHashTableOps classPolicyOps =
    {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        PL_DHashVoidPtrKeyStub,
        PL_DHashMatchEntryStub,
        PL_DHashMoveEntryStub,
        ClearPropertyPolicyEntry,
        PL_DHashFinalizeStub,
        InitPropertyPolicyEntry
    };

    ClassPolicy* cp = (ClassPolicy*)entry;
    cp->mDomainWeAreWildcardFor = nullptr;
    cp->key = PL_strdup((const char*)key);
    if (!cp->key)
        return false;
    cp->mPolicy = PL_NewDHashTable(&classPolicyOps, nullptr,
                                   sizeof(PropertyPolicy), 16);
    if (!cp->mPolicy) {
        PL_strfree(cp->key);
        cp->key = nullptr;
        return false;
    }
    return true;
}

// Domain Policy
class DomainPolicy : public PLDHashTable
{
public:
    DomainPolicy() : mWildcardPolicy(nullptr),
                     mRefCount(0)
    {
        mGeneration = sGeneration;
    }

    bool Init()
    {
        static const PLDHashTableOps domainPolicyOps =
        {
            PL_DHashAllocTable,
            PL_DHashFreeTable,
            PL_DHashStringKey,
            PL_DHashMatchStringKey,
            MoveClassPolicyEntry,
            ClearClassPolicyEntry,
            PL_DHashFinalizeStub,
            InitClassPolicyEntry
        };

        return PL_DHashTableInit(this, &domainPolicyOps, nullptr,
                                 sizeof(ClassPolicy), 16);
    }

    ~DomainPolicy()
    {
        PL_DHashTableFinish(this);
        NS_ASSERTION(mRefCount == 0, "Wrong refcount in DomainPolicy dtor");
    }

    void Hold()
    {
        mRefCount++;
    }

    void Drop()
    {
        if (--mRefCount == 0)
            delete this;
    }
    
    static void InvalidateAll()
    {
        sGeneration++;
    }
    
    bool IsInvalid()
    {
        return mGeneration != sGeneration; 
    }
    
    ClassPolicy* mWildcardPolicy;

private:
    uint32_t mRefCount;
    uint32_t mGeneration;
    static uint32_t sGeneration;
};

static void
MoveClassPolicyEntry(PLDHashTable *table,
                     const PLDHashEntryHdr *from,
                     PLDHashEntryHdr *to)
{
    memcpy(to, from, table->entrySize);

    // Now update the mDefaultPolicy pointer that points to us, if any.
    ClassPolicy* cp = static_cast<ClassPolicy*>(to);
    if (cp->mDomainWeAreWildcardFor) {
        NS_ASSERTION(cp->mDomainWeAreWildcardFor->mWildcardPolicy ==
                     static_cast<const ClassPolicy*>(from),
                     "Unexpected wildcard policy on mDomainWeAreWildcardFor");
        cp->mDomainWeAreWildcardFor->mWildcardPolicy = cp;
    }
}

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

    // Returns null if a principal cannot be found; generally callers
    // should error out at that point.
    static nsIPrincipal* doGetObjectPrincipal(JS::Handle<JSObject*> obj);
#ifdef DEBUG
    static nsIPrincipal*
    old_doGetObjectPrincipal(JS::Handle<JSObject*> obj,
                             bool aAllowShortCircuit = true);
#endif

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
                            const char* aClassName, jsid aProperty,
                            void** aCachedClassPolicy);

    nsresult
    CheckSameOriginDOMProp(nsIPrincipal* aSubject, 
                           nsIPrincipal* aObject,
                           uint32_t aAction);

    nsresult
    LookupPolicy(nsIPrincipal* principal,
                 ClassInfoData& aClassData, jsid aProperty,
                 uint32_t aAction,
                 ClassPolicy** aCachedClassPolicy,
                 SecurityLevel* result);

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

    /**
     * Check capability levels for an |aObj| that implements
     * nsISecurityCheckedComponent.
     *
     * NB: This function also checks to see if aObj is a plugin and the user
     * has set the "security.xpconnect.plugin.unrestricted" pref to allow
     * anybody to script plugin objects from anywhere.
     *
     * @param cx The context we're running on.
     *           NB: If null, "sameOrigin" does not have any effect.
     * @param aObj The nsISupports representation of the object in question
     *             object, possibly null.
     * @param aJSObject The JSObject representation of the object in question
     *                  if |cx| is non-null and |aObjectSecurityLevel| is
     *                  "sameOrigin". If null will be calculated from aObj (if
     *                  non-null) if and only if aObj is an XPCWrappedJS. The
     *                  rationale behind this is that if we're creating a JS
     *                  wrapper for an XPCWrappedJS, this object definitely
     *                  expects to be exposed to JS.
     * @param aSubjectPrincipal The nominal subject principal used when
     *                          aObjectSecurityLevel is "sameOrigin". If null,
     *                          this is calculated if it's needed.
     * @param aObjectSecurityLevel Can be one of three values:
     *                  - allAccess: Allow access no matter what.
     *                  - noAccess: Deny access no matter what.
     *                  - sameOrigin: If |cx| is null, behave like noAccess.
     *                                Otherwise, possibly compute a subject
     *                                and object principal and return true if
     *                                and only if the subject has greater than
     *                                or equal privileges to the object.
     */
    nsresult
    CheckXPCPermissions(JSContext* cx,
                        nsISupports* aObj, JSObject* aJSObject,
                        nsIPrincipal* aSubjectPrincipal,
                        const char* aObjectSecurityLevel);

    /**
     * Helper for CanExecuteScripts that allows the caller to specify
     * whether execution should be allowed if cx has no
     * nsIScriptContext.
     */
    nsresult
    CanExecuteScripts(JSContext* cx, nsIPrincipal *aPrincipal,
                      bool aAllowIfNoScriptContext, bool *result);

    nsresult
    Init();

    nsresult
    InitPrefs();

    nsresult
    InitPolicies();

    nsresult
    InitDomainPolicy(JSContext* cx, const char* aPolicyName,
                     DomainPolicy* aDomainPolicy);

    inline void
    ScriptSecurityPrefChanged();

    nsObjectHashtable* mOriginToPolicyMap;
    DomainPolicy* mDefaultPolicy;
    nsObjectHashtable* mCapabilities;

    nsCOMPtr<nsIPrincipal> mSystemPrincipal;
    bool mPrefInitialized;
    bool mIsJavaScriptEnabled;
    bool mPolicyPrefsChanged;

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
GetExtendedOrigin(nsIURI* aURI, uint32_t aAppid,
                  bool aInMozBrowser,
                  nsACString& aExtendedOrigin);

} // namespace mozilla

#endif // nsScriptSecurityManager_h__
