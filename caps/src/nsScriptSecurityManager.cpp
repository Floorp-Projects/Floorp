/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nspr.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIJSContextStack.h"
#include "nsDOMError.h"
#include "xpcexception.h"
#include "nsDOMCID.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptExternalNameSet.h"
#include "jsdbgapi.h"
#include "nsDOMPropNames.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID, 
                     NS_SCRIPT_NAMESET_REGISTRY_CID);

enum {
    SCRIPT_SECURITY_UNDEFINED_ACCESS,
    SCRIPT_SECURITY_CAPABILITY_ONLY,
    SCRIPT_SECURITY_SAME_DOMAIN_ACCESS,
    SCRIPT_SECURITY_ALL_ACCESS,
    SCRIPT_SECURITY_NO_ACCESS
};

static JSContext *
GetCurrentContext() {
    // Get JSContext from stack.
    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, stack, "nsThreadJSContextStack", 
                    &rv);
    if (NS_FAILED(rv))
        return nsnull;
    JSContext *cx;
    if (NS_FAILED(stack->Peek(&cx)))
        return nsnull;
    return cx;
}

static nsDOMProp 
findDomProp(const char *propName, int n);

/////////////////////
// nsIPrincipalKey //
/////////////////////

class nsIPrincipalKey : public nsHashKey {
public:
    nsIPrincipalKey(nsIPrincipal* key) {
        mKey = key;
        NS_IF_ADDREF(mKey);
    }
    
    ~nsIPrincipalKey(void) {
        NS_IF_RELEASE(mKey);
    }
    
    PRUint32 HashValue(void) const {
        PRUint32 hash;
        mKey->HashValue(&hash);
        return hash;
    }
    
    PRBool Equals(const nsHashKey *aKey) const {
        PRBool eq;
        mKey->Equals(((nsIPrincipalKey *) aKey)->mKey, &eq);
        return eq;
    }
    
    nsHashKey *Clone(void) const {
        return new nsIPrincipalKey(mKey);
    }

protected:
    nsIPrincipal* mKey;
};


///////////////////////
// nsSecurityNameSet //
///////////////////////

class nsSecurityNameSet : public nsIScriptExternalNameSet 
{
public:
    nsSecurityNameSet();
    virtual ~nsSecurityNameSet();
    
    NS_DECL_ISUPPORTS
    NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
    NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

nsSecurityNameSet::nsSecurityNameSet()
{
    NS_INIT_REFCNT();
}

nsSecurityNameSet::~nsSecurityNameSet()
{
}

NS_IMPL_ISUPPORTS(nsSecurityNameSet, NS_GET_IID(nsIScriptExternalNameSet));

static char *
getStringArgument(JSContext *cx, JSObject *obj, uintN argc, jsval *argv)
{
    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(cx, "String argument expected");
        return nsnull;
    }
    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     */
    JSString *str = JSVAL_TO_STRING(argv[0]);
    if (!str)
        return nsnull;

    return JS_GetStringBytes(str);
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_isPrivilegeEnabled(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    JSBool result = JS_FALSE;
    char *cap = getStringArgument(cx, obj, argc, argv);
    if (cap) {
        nsresult rv;
        NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                        NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
        if (NS_SUCCEEDED(rv)) {
            NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
            rv = securityManager->IsCapabilityEnabled(cap, &result);
            if (NS_FAILED(rv)) 
                result = JS_FALSE;
        }
    }
    *rval = BOOLEAN_TO_JSVAL(result);
    return JS_TRUE;
}


PR_STATIC_CALLBACK(JSBool)
netscape_security_enablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                  jsval *argv, jsval *rval)
{
    char *cap = getStringArgument(cx, obj, argc, argv);
    if (!cap)
        return JS_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;
    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
    if (NS_FAILED(securityManager->EnableCapability(cap)))
        return JS_FALSE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_disablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                   jsval *argv, jsval *rval)
{
    char *cap = getStringArgument(cx, obj, argc, argv);
    if (!cap)
        return JS_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;
    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
    if (NS_FAILED(securityManager->DisableCapability(cap)))
        return JS_FALSE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_revertPrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                  jsval *argv, jsval *rval)
{
    char *cap = getStringArgument(cx, obj, argc, argv);
    if (!cap)
        return JS_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;
    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
    if (NS_FAILED(securityManager->RevertCapability(cap)))
        return JS_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { "isPrivilegeEnabled", netscape_security_isPrivilegeEnabled,   1},
    { "enablePrivilege",    netscape_security_enablePrivilege,      1},
    { "disablePrivilege",   netscape_security_disablePrivilege,     1},
    { "revertPrivilege",    netscape_security_revertPrivilege,      1},
    {0}
};

/*
 * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
 * et. al. so that code that worked with 4.0 can still work.
 */
NS_IMETHODIMP 
nsSecurityNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
    JSContext *cx = (JSContext *) aScriptContext->GetNativeContext();
    JSObject *global = JS_GetGlobalObject(cx);

    /*
     * Find Object.prototype's class by walking up the global object's
     * prototype chain.
     */
    JSObject *obj = global;
    JSObject *proto;
    while ((proto = JS_GetPrototype(cx, obj)) != nsnull)
        obj = proto;
    JSClass *objectClass = JS_GetClass(cx, obj);

    jsval v;
    if (!JS_GetProperty(cx, global, "netscape", &v))
        return NS_ERROR_FAILURE;
    JSObject *securityObj;
    if (JSVAL_IS_OBJECT(v)) {
        /*
         * "netscape" property of window object exists; must be LiveConnect
         * package. Get the "security" property.
         */
        obj = JSVAL_TO_OBJECT(v);
        if (!JS_GetProperty(cx, obj, "security", &v) || !JSVAL_IS_OBJECT(v))
            return NS_ERROR_FAILURE;
        securityObj = JSVAL_TO_OBJECT(v);
    } else {
        /* define netscape.security object */
        obj = JS_DefineObject(cx, global, "netscape", objectClass, nsnull, 0);
        if (obj == nsnull)
            return NS_ERROR_FAILURE;
        securityObj = JS_DefineObject(cx, obj, "security", objectClass,
                                      nsnull, 0);
        if (securityObj == nsnull)
            return NS_ERROR_FAILURE;
    }

    /* Define PrivilegeManager object with the necessary "static" methods. */
    obj = JS_DefineObject(cx, securityObj, "PrivilegeManager", objectClass,
                          nsnull, 0);
    if (obj == nsnull)
        return NS_ERROR_FAILURE;

    return JS_DefineFunctions(cx, obj, PrivilegeManager_static_methods)
           ? NS_OK
           : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsSecurityNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    return NS_OK;
}



/////////////////////////////
// nsScriptSecurityManager //
/////////////////////////////

////////////////////////////////////
// Methods implementing ISupports //
////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) 
      return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(NS_GET_IID(nsIScriptSecurityManager))) {
      *aInstancePtr = (void*)(nsIScriptSecurityManager *)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIXPCSecurityManager))) {
      *aInstancePtr = (void*)(nsIXPCSecurityManager *)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsScriptSecurityManager);
NS_IMPL_RELEASE(nsScriptSecurityManager);

inline PRBool
GetBit(unsigned char *bitVector, PRInt32 index) 
{
    unsigned char c = bitVector[index >> 3];
    c &= (1 << (index & 7));
    return c != 0;
}

inline void
SetBit(unsigned char *bitVector, PRInt32 index) 
{
    bitVector[index >> 3] |= (1 << (index & 7));
}


///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CheckScriptAccess(JSContext *cx, 
                                           void *aObj, PRInt32 domPropInt, 
                                           PRBool isWrite, PRBool *aResult)
{
    nsDOMProp domProp = (nsDOMProp) domPropInt;
    *aResult = PR_FALSE;
    if (!GetBit(hasPolicyVector, domPropInt)) {
        // No policy for this DOM property, so just allow access.
        *aResult = PR_TRUE;
        return NS_OK;
    }
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal)))) {
        return NS_ERROR_FAILURE;
    }
    nsXPIDLCString capability;
    PRInt32 secLevel = GetSecurityLevel(principal, domProp, isWrite,
                                        getter_Copies(capability));
    switch (secLevel) {
      case SCRIPT_SECURITY_UNDEFINED_ACCESS:
        // If no preference is defined for this property, allow access. 
        // This violates the rule of a safe default, but means we don't have
        // to specify the large majority of unchecked properties, only the
        // minority of checked ones.
      case SCRIPT_SECURITY_ALL_ACCESS:
        *aResult = PR_TRUE;
        return NS_OK;
      case SCRIPT_SECURITY_SAME_DOMAIN_ACCESS: {
        const char *cap = isWrite  
                          ? "UniversalBrowserWrite" 
                          : "UniversalBrowserRead";
        return CheckPermissions(cx, (JSObject *) aObj, cap, aResult);
      }
      case SCRIPT_SECURITY_CAPABILITY_ONLY: 
        return IsCapabilityEnabled(capability, aResult);
      default:
        // Default is no access
        *aResult = PR_FALSE;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURIFromScript(JSContext *cx, 
                                                nsIURI *aURI)
{
    // Get principal of currently executing script.
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal)))) {
        return NS_ERROR_FAILURE;
    }

    // Native code can load all URIs.
    if (!principal) 
        return NS_OK;

    // The system principal can load all URIs.
    PRBool equals = PR_FALSE;
    if (NS_FAILED(principal->Equals(mSystemPrincipal, &equals)))
        return NS_ERROR_FAILURE;
    if (equals)
        return NS_OK;

    // Otherwise, principal should have a codebase that we can use to
    // do the remaining tests.
    nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
    if (!codebase) 
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(codebase->GetURI(getter_AddRefs(uri)))) 
        return NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(CheckLoadURI(uri, aURI)))
        return NS_OK;

    // See if we're attempting to load a file: URI. If so, let a 
    // UniversalFileRead capability trump the above check.
    nsXPIDLCString scheme;
    if (NS_FAILED(aURI->GetScheme(getter_Copies(scheme))))
        return NS_ERROR_FAILURE;
    if (nsCRT::strcmp(scheme, "file") == 0) {
        PRBool enabled;
        if (NS_FAILED(IsCapabilityEnabled("UniversalFileRead", &enabled)))
            return NS_ERROR_FAILURE;
        if (enabled)
            return NS_OK;
    }

    // Report error.
    nsXPIDLCString spec;
    if (NS_FAILED(aURI->GetSpec(getter_Copies(spec))))
        return NS_ERROR_FAILURE;
	JS_ReportError(cx, "illegal URL method '%s'", (const char *)spec);
    return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckLoadURI(nsIURI *aFromURI,
                                      nsIURI *aURI)
{
    nsXPIDLCString scheme;
    if (NS_FAILED(aURI->GetScheme(getter_Copies(scheme))))
        return NS_ERROR_FAILURE;
    if (nsCRT::strcmp(scheme, "http")         == 0 ||
        nsCRT::strcmp(scheme, "https")        == 0 ||
        nsCRT::strcmp(scheme, "javascript")   == 0 ||
        nsCRT::strcmp(scheme, "ftp")          == 0 ||
        nsCRT::strcmp(scheme, "mailto")       == 0 ||
        nsCRT::strcmp(scheme, "news")         == 0)
    {
        return NS_OK;
    }
    if (nsCRT::strcmp(scheme, "about") == 0) {
        nsXPIDLCString spec;
        if (NS_FAILED(aURI->GetSpec(getter_Copies(spec))))
            return NS_ERROR_FAILURE;
        if (nsCRT::strcmp(spec, "about:blank") == 0) {
            return NS_OK;
        }
    }
    if (nsCRT::strcmp(scheme, "file") == 0) {
        nsXPIDLCString scheme2;
        if (NS_SUCCEEDED(aFromURI->GetScheme(getter_Copies(scheme2))) &&
            nsCRT::strcmp(scheme2, "file") == 0)
        {
            return NS_OK;
        }
    }

    // Temporary: allow a preference to disable this check
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_SUCCEEDED(prefs->GetBoolPref("security.checkuri", &enabled)) &&
        !enabled) 
    {
        return NS_OK;
    }

    return NS_ERROR_DOM_BAD_URI;
}


NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(nsIPrincipal **result)
{
    JSContext *cx = GetCurrentContext();
    if (!cx) {
        *result = nsnull;
        return NS_OK;
    }
    return GetSubjectPrincipal(cx, result);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSystemPrincipal(nsIPrincipal **result)
{
    if (!mSystemPrincipal) {
        mSystemPrincipal = new nsSystemPrincipal();
        if (!mSystemPrincipal)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mSystemPrincipal);
    }
    *result = mSystemPrincipal;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI *aURI, 
                                              nsIPrincipal **result)
{
    nsresult rv;
    nsCodebasePrincipal *codebase = new nsCodebasePrincipal();
    NS_ADDREF(codebase);
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(codebase->Init(aURI))) {
        NS_RELEASE(codebase);
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrincipal> principal;
    rv = codebase->QueryInterface(NS_GET_IID(nsIPrincipal), 
                                  (void **) getter_AddRefs(principal));
    NS_RELEASE(codebase);
    if (NS_FAILED(rv))
        return rv;

    if (mPrincipals) {
        // Check to see if we already have this principal.
        nsIPrincipalKey key(principal);
        nsCOMPtr<nsIPrincipal> p2 = (nsIPrincipal *) mPrincipals->Get(&key);
        if (p2) 
            principal = p2;
    }
    *result = principal;
    NS_ADDREF(*result);
    return NS_OK;
}


NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteScripts(nsIPrincipal *principal,
                                           PRBool *result)
{
    if (principal == mSystemPrincipal) {
         // Even if JavaScript is disabled, we must still execute system scripts
        *result = PR_TRUE;
        return NS_OK;
    }
    
    if (GetBit(hasDomainPolicyVector, NS_DOM_PROP_JAVASCRIPT_ENABLED)) {
        // We may have a per-domain security policy for JavaScript execution
        nsXPIDLCString capability;
        PRInt32 secLevel = GetSecurityLevel(principal, 
                                            NS_DOM_PROP_JAVASCRIPT_ENABLED, 
                                            PR_FALSE, getter_Copies(capability));
        if (secLevel != SCRIPT_SECURITY_UNDEFINED_ACCESS) {
            *result = (secLevel == SCRIPT_SECURITY_ALL_ACCESS);
            return NS_OK;
        }
    }
    
    if (mIsJavaScriptEnabled != mIsMailJavaScriptEnabled) {
        // Is this script running from mail?
        nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
        if (!codebase) 
            return NS_ERROR_FAILURE;
        nsCOMPtr<nsIURI> uri;
        if (NS_FAILED(codebase->GetURI(getter_AddRefs(uri)))) 
            return NS_ERROR_FAILURE;
        nsXPIDLCString scheme;
        if (NS_FAILED(uri->GetScheme(getter_Copies(scheme))))
            return NS_ERROR_FAILURE;
        if (nsCRT::strcmp(scheme, "imap") == 0 || 
            nsCRT::strcmp(scheme, "mailbox") == 0) 
        {
            *result = mIsMailJavaScriptEnabled;
            return NS_OK;
        }
    }
    *result = mIsJavaScriptEnabled;
    return NS_OK;
}


NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteFunction(void *jsFunc,
                                            PRBool *result)
{
    // norris TODO: figure out JSContext strategy, replace nsnulls below
    // JavaScript is disabled, but we must still execute system JavaScript
    JSScript *script = JS_GetFunctionScript(nsnull, (JSFunction *) jsFunc);
    if (!script)
        return NS_ERROR_FAILURE;
    JSPrincipals *jsprin = JS_GetScriptPrincipals(nsnull, script);
    if (!jsprin)
        return NS_ERROR_FAILURE;
    nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) jsprin;

    return CanExecuteScripts(nsJSPrin->nsIPrincipalPtr, result);
}


NS_IMETHODIMP
nsScriptSecurityManager::IsCapabilityEnabled(const char *capability,
                                             PRBool *result)
{
    nsresult rv;
    JSStackFrame *fp = nsnull;
    JSContext *cx = GetCurrentContext();
    fp = cx ? JS_FrameIterator(cx, &fp) : nsnull;
    if (!fp) {
        // No script code on stack. Allow execution.
        *result = PR_TRUE;
        return NS_OK;
    }
    while (fp) {
        JSScript *script = JS_GetFrameScript(cx, fp);
        if (script) {
            JSPrincipals *principals = JS_GetScriptPrincipals(cx, script);
            if (!principals) {
                // Script didn't have principals!
                return NS_ERROR_FAILURE;
            }

            // First check if the principal is even able to enable the 
            // given capability. If not, don't look any further.
            nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) principals;
            PRInt16 canEnable;
            rv = nsJSPrin->nsIPrincipalPtr->CanEnableCapability(capability, 
                                                                &canEnable);
            if (NS_FAILED(rv))
                return rv;
            if (canEnable != nsIPrincipal::ENABLE_GRANTED &&
                canEnable != nsIPrincipal::ENABLE_WITH_USER_PERMISSION) 
            {
                *result = PR_FALSE;
                return NS_OK;
            }

            // Now see if the capability is enabled.
            void *annotation = JS_GetFrameAnnotation(cx, fp);
            rv = nsJSPrin->nsIPrincipalPtr->IsCapabilityEnabled(capability, 
                                                                annotation, 
                                                                result);
            if (NS_FAILED(rv))
                return rv;
            if (*result)
                return NS_OK;
        }
        fp = JS_FrameIterator(cx, &fp);
    }
    *result = PR_FALSE;
    return NS_OK;
}

static nsresult
GetPrincipalAndFrame(JSContext *cx, nsIPrincipal **result, 
                     JSStackFrame **frameResult) 
{
    // Get principals from innermost frame of JavaScript or Java.
    JSStackFrame *fp = nsnull; // tell JS_FrameIterator to start at innermost
    fp = JS_FrameIterator(cx, &fp);
    while (fp) {
        JSScript *script = JS_GetFrameScript(cx, fp);
        if (script) {
            JSPrincipals *principals = JS_GetScriptPrincipals(cx, script);
            if (!principals) {
                // Script didn't have principals!
                return NS_ERROR_FAILURE;
            }
            nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) principals;
            *result = nsJSPrin->nsIPrincipalPtr;
            NS_ADDREF(*result);
            *frameResult = fp;
            return NS_OK;
        }
        fp = JS_FrameIterator(cx, &fp);
    }
    *result = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::EnableCapability(const char *capability)
{
    JSContext *cx = GetCurrentContext();
    JSStackFrame *fp;
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), 
                                       &fp)))
    {
        return NS_ERROR_FAILURE;
    }
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    PRBool enabled;
    if (NS_FAILED(principal->IsCapabilityEnabled(capability, annotation, 
                                                 &enabled)))
    {
        return NS_ERROR_FAILURE;
    }
    if (enabled)
        return NS_OK;
    PRInt16 canEnable;
    if (NS_FAILED(principal->CanEnableCapability(capability, &canEnable)))
        return NS_ERROR_FAILURE;
    if (canEnable == nsIPrincipal::ENABLE_WITH_USER_PERMISSION) {
        // XXX ask user!
        canEnable = nsIPrincipal::ENABLE_GRANTED;
        if (NS_FAILED(principal->SetCanEnableCapability(capability, canEnable)))
            return NS_ERROR_FAILURE;
        nsIPrincipalKey key(principal);
        if (!mPrincipals) {
            mPrincipals = new nsSupportsHashtable(31);
            if (!mPrincipals)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        // This is a little sneaky. "supports" below is a void *, which won't 
        // be refcounted, but is matched with a key that is the same object,
        // which will be refcounted.
        mPrincipals->Put(&key, principal);
    }
    if (canEnable != nsIPrincipal::ENABLE_GRANTED) {
		static const char msg[] = "enablePrivilege not granted";
		JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, msg)));
        return NS_ERROR_FAILURE; // XXX better error code?
    }
    if (NS_FAILED(principal->EnableCapability(capability, &annotation))) 
        return NS_ERROR_FAILURE;
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::RevertCapability(const char *capability)
{
    JSContext *cx = GetCurrentContext();
    JSStackFrame *fp;
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), 
                                       &fp)))
    {
        return NS_ERROR_FAILURE;
    }
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    principal->RevertCapability(capability, &annotation);
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::DisableCapability(const char *capability)
{
    JSContext *cx = GetCurrentContext();
    JSStackFrame *fp;
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetPrincipalAndFrame(cx, getter_AddRefs(principal), 
                                       &fp)))
    {
        return NS_ERROR_FAILURE;
    }
    void *annotation = JS_GetFrameAnnotation(cx, fp);
    principal->DisableCapability(capability, &annotation);
    JS_SetFrameAnnotation(cx, fp, annotation);
    return NS_OK;
}


////////////////////////////////////////////////
// Methods implementing nsIXPCSecurityManager //
////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *aJSContext, 
                                          const nsIID &aIID, 
                                          nsISupports *aObj)
{
	if (aIID.Equals(NS_GET_IID(nsIXPCException)))
		return NS_OK;		
    return CheckXPCPermissions(aJSContext);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateInstance(JSContext *aJSContext, 
                                           const nsCID &aCID)
{
    return CheckXPCPermissions(aJSContext);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetService(JSContext *aJSContext, 
                                       const nsCID &aCID)
{
    return CheckXPCPermissions(aJSContext);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanCallMethod(JSContext *aJSContext, 
                                       const nsIID &aIID, 
                                       nsISupports *aObj, 
                                       nsIInterfaceInfo *aInterfaceInfo, 
                                       PRUint16 aMethodIndex, 
                                       const jsid aName)
{
    return CheckXPCPermissions(aJSContext);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanGetProperty(JSContext *aJSContext, 
                                        const nsIID &aIID, 
                                        nsISupports *aObj, 
                                        nsIInterfaceInfo *aInterfaceInfo, 
                                        PRUint16 aMethodIndex, 
                                        const jsid aName)
{
    return CheckXPCPermissions(aJSContext);
}

NS_IMETHODIMP
nsScriptSecurityManager::CanSetProperty(JSContext *aJSContext, 
                                        const nsIID &aIID, 
                                        nsISupports *aObj, 
                                        nsIInterfaceInfo *aInterfaceInfo, 
                                        PRUint16 aMethodIndex, 
                                        const jsid aName)
{
    return CheckXPCPermissions(aJSContext);
}

///////////////////
// Other methods //
///////////////////

nsScriptSecurityManager::nsScriptSecurityManager(void)
    : mOriginToPolicyMap(nsnull), mSystemPrincipal(nsnull), 
      mPrincipals(nsnull), mIsJavaScriptEnabled(PR_FALSE),
      mIsMailJavaScriptEnabled(PR_FALSE)
{
    NS_INIT_REFCNT();
    memset(hasPolicyVector, 0, sizeof(hasPolicyVector));
    memset(hasDomainPolicyVector, 0, sizeof(hasDomainPolicyVector));
    InitFromPrefs();
}

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    delete mOriginToPolicyMap;
    NS_IF_RELEASE(mSystemPrincipal);
    delete mPrincipals;
} 

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    static nsScriptSecurityManager *ssecMan = NULL;
    if (!ssecMan) {
        ssecMan = new nsScriptSecurityManager();
	    nsresult rv;
	    NS_WITH_SERVICE(nsIScriptNameSetRegistry, registry, 
                        kCScriptNameSetRegistryCID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsSecurityNameSet* nameSet = new nsSecurityNameSet();
            registry->AddExternalNameSet(nameSet);
        }
    }
    return ssecMan;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *cx, 
                                             nsIPrincipal **result)
{
    JSStackFrame *fp;
    return GetPrincipalAndFrame(cx, result, &fp);
}


NS_IMETHODIMP
nsScriptSecurityManager::GetObjectPrincipal(JSContext *aCx, JSObject *aObj, 
                                            nsIPrincipal **result)
{
    JSObject *parent;
    while ((parent = JS_GetParent(aCx, aObj)) != nsnull) 
        aObj = parent;
    
    nsISupports *supports = (nsISupports *) JS_GetPrivate(aCx, aObj);
    nsCOMPtr<nsIScriptGlobalObjectData> globalData;
    if (!supports || NS_FAILED(supports->QueryInterface(
                                     NS_GET_IID(nsIScriptGlobalObjectData), 
                                     (void **) getter_AddRefs(globalData))))
    {
        return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(globalData->GetPrincipal(result))) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPermissions(JSContext *aCx, JSObject *aObj, 
                                          const char *aCapability,
                                          PRBool* aResult)
{
    /*
    ** Get origin of subject and object and compare.
    */
    nsCOMPtr<nsIPrincipal> subject;
    if (NS_FAILED(GetSubjectPrincipal(aCx, getter_AddRefs(subject))))
        return NS_ERROR_FAILURE;

    // If native code or system principal, allow access
    PRBool equals;
    if (!subject || 
        (NS_SUCCEEDED(subject->Equals(mSystemPrincipal, &equals)) && equals))
    {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> object;
    if (NS_FAILED(GetObjectPrincipal(aCx, aObj, getter_AddRefs(object))))
        return NS_ERROR_FAILURE;
    if (subject == object) {
        *aResult = PR_TRUE;
        return NS_OK;
    }
    nsCOMPtr<nsICodebasePrincipal> subjectCodebase = do_QueryInterface(subject);
    if (subjectCodebase) {
        if (NS_FAILED(subjectCodebase->SameOrigin(object, aResult)))
            return NS_ERROR_FAILURE;

        if (*aResult)
            return NS_OK;
    }

    /*
    ** If we failed the origin tests it still might be the case that we
    ** are a signed script and have permissions to do this operation.
    ** Check for that here
    */
    if (NS_FAILED(IsCapabilityEnabled(aCapability, aResult)))
        return NS_ERROR_FAILURE;
    if (*aResult)
        return NS_OK;
    
    // Temporary: only enforce if security.checkdomprops pref not disabled
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_SUCCEEDED(prefs->GetBoolPref("security.checkdomprops", &enabled)) &&
        enabled) 
    {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    /*
    ** Access tests failed, so now report error.
    */
    char *str;
    if (NS_FAILED(subject->ToString(&str)))
        return NS_ERROR_FAILURE;
    JS_ReportError(aCx, "access disallowed from scripts at %s to documents "
                        "at another domain", str);
    nsCRT::free(str);
    *aResult = PR_FALSE;
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}


PRInt32 
nsScriptSecurityManager::GetSecurityLevel(nsIPrincipal *principal, 
                                          nsDOMProp domProp, 
                                          PRBool isWrite, char **capability)
{
    nsXPIDLCString prefName;
    if (NS_FAILED(GetPrefName(principal, domProp, getter_Copies(prefName))))
        return SCRIPT_SECURITY_NO_ACCESS;
    PRInt32 secLevel;
    char *secLevelString;
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
    rv = prefs->CopyCharPref(prefName, &secLevelString);
    if (NS_FAILED(rv)) {
        nsAutoString s = (const char *) prefName;
        s += (isWrite ? ".write" : ".read");
        char *cp = s.ToNewCString();
        if (!cp) 
            return SCRIPT_SECURITY_NO_ACCESS;
        rv = prefs->CopyCharPref(cp, &secLevelString);
        Recycle(cp);
    }
    if (NS_SUCCEEDED(rv) && secLevelString) {
        if (PL_strcmp(secLevelString, "sameOrigin") == 0)
            secLevel = SCRIPT_SECURITY_SAME_DOMAIN_ACCESS;
        else if (PL_strcmp(secLevelString, "allAccess") == 0)
            secLevel = SCRIPT_SECURITY_ALL_ACCESS;
        else if (PL_strcmp(secLevelString, "noAccess") == 0)
            secLevel = SCRIPT_SECURITY_NO_ACCESS;
        else {
            // string should be the name of a capability
            *capability = secLevelString;
            secLevelString = nsnull;
            secLevel = SCRIPT_SECURITY_CAPABILITY_ONLY;
        }
        if (secLevelString)
            PR_Free(secLevelString);
        return secLevel;
    }
    return SCRIPT_SECURITY_UNDEFINED_ACCESS;
}


NS_IMETHODIMP
nsScriptSecurityManager::CheckXPCPermissions(JSContext *aJSContext)
{
    PRBool ok = PR_FALSE;
    if (NS_FAILED(IsCapabilityEnabled("UniversalXPConnect", &ok)))
        ok = PR_FALSE;
    if (!ok) {
        // Check the pref "security.checkxpconnect". If it exists and is
        // set to false, don't report an error.
	    nsresult rv;
	    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	    if (NS_SUCCEEDED(rv)) {
		    PRBool enabled;
		    if (NS_SUCCEEDED(prefs->GetBoolPref("security.checkxpconnect",
                                                &enabled)) &&
			    !enabled) 
		    {
			    return NS_OK;
		    }
	    }
		static const char msg[] = "Access denied to XPConnect service.";
		JS_SetPendingException(aJSContext, 
			                   STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext, msg)));
        return NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED;
    }
    return NS_OK;
}

static char *domPropNames[] = {
    NS_DOM_PROP_NAMES
};


NS_IMETHODIMP
nsScriptSecurityManager::GetPrefName(nsIPrincipal *principal, nsDOMProp domProp, 
                                     char **result)
{
    nsresult rv;
    static const char *defaultStr = "default";
    nsAutoString s = "security.policy.";
    if (!GetBit(hasDomainPolicyVector, domProp)) {
        s += defaultStr;
    } else {
        PRBool equals = PR_TRUE;
        if (principal && NS_FAILED(principal->Equals(mSystemPrincipal, &equals)))
            return NS_ERROR_FAILURE;
        if (equals) {
            s += defaultStr;
        } else {
            nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal, &rv);
            if (NS_FAILED(rv))
                return rv;
            nsXPIDLCString origin;
            if (NS_FAILED(rv = codebase->GetOrigin(getter_Copies(origin))))
                return rv;
            nsCString *policy = nsnull;
            if (mOriginToPolicyMap) {
                nsStringKey key(origin);
                policy = (nsCString *) mOriginToPolicyMap->Get(&key);
            }
            if (policy)
                s += *policy;
            else
                s += defaultStr;
        }
    }
    s += '.';
    s += domPropNames[domProp];
    *result = s.ToNewCString();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

static nsDOMProp 
findDomProp(const char *propName, int n) 
{
    int hi = sizeof(domPropNames)/sizeof(domPropNames[0]) - 1;
    int lo = 0;
    do {
        int mid = (hi + lo) / 2;
        int cmp = PL_strncmp(propName, domPropNames[mid], n);
        if (cmp == 0)
            return (nsDOMProp) mid;
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    } while (hi > lo);
    if (PL_strncmp(propName, domPropNames[lo], n) == 0)
        return (nsDOMProp) lo;
    return NS_DOM_PROP_MAX;
}

PR_STATIC_CALLBACK(PRBool)
DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsCString* entry = (nsCString*) aData;
    delete entry;
    return PR_TRUE;
}

struct PolicyEnumeratorInfo {
    nsIPref *prefs;
    nsScriptSecurityManager *secMan;
};

void 
nsScriptSecurityManager::enumeratePolicyCallback(const char *prefName, 
                                                 void *data)
{
    if (!prefName || !*prefName)
        return;
    PolicyEnumeratorInfo *info = (PolicyEnumeratorInfo *) data;
    unsigned count = 0;
    const char *dots[5];
    const char *p;
    for (p=prefName; *p; p++) {
        if (*p == '.') {
            dots[count++] = p;
            if (count == sizeof(dots)/sizeof(dots[0]))
                break;
        }
    }
    if (count < sizeof(dots)/sizeof(dots[0]))
        dots[count] = p;
    if (count < 3)
        return;
    const char *policyName = dots[1] + 1;
    int policyLength = dots[2] - policyName;
    PRBool isDefault = PL_strncmp("default", policyName, policyLength) == 0;
    if (!isDefault && count == 3) {
        // security.policy.<policyname>.sites
        const char *sitesName = dots[2] + 1;
        int sitesLength = dots[3] - sitesName;
        if (PL_strncmp("sites", sitesName, sitesLength) == 0) {
            if (!info->secMan->mOriginToPolicyMap) {
                info->secMan->mOriginToPolicyMap = 
                    new nsObjectHashtable(nsnull, nsnull, DeleteEntry, nsnull);
                if (!info->secMan->mOriginToPolicyMap)
                    return;
            }
            char *s;
            if (NS_FAILED(info->prefs->CopyCharPref(prefName, &s)))
                return;
            char *q=s;
            char *r=s;
            PRBool working = PR_TRUE;
            while (working) {
                if (*r == ' ' || *r == '\0') {
                    working = (*r != '\0');
                    *r = '\0';
                    nsStringKey key(q);
                    nsCString *value = new nsCString(policyName, policyLength);
                    if (!value)
                        break;
                    info->secMan->mOriginToPolicyMap->Put(&key, value);
                    q = r + 1;
                }
                r++;
            }
            PR_Free(s);
            return;
        }
    } else if (count >= 4) {
        // security.policy.<policyname>.<object>.<property>[.read|.write]
        const char *domPropName = dots[2] + 1;
        int domPropLength = dots[4] - domPropName;
        nsDOMProp domProp = findDomProp(domPropName, domPropLength);
        if (domProp < NS_DOM_PROP_MAX) {
            SetBit(info->secMan->hasPolicyVector, domProp);
            if (!isDefault)
                SetBit(info->secMan->hasDomainPolicyVector, domProp);
            return;
        }
    } 
    NS_ASSERTION(PR_FALSE, "DOM property name invalid or not found");
}

static const char jsEnabledPrefName[] = "javascript.enabled";
static const char jsMailEnabledPrefName[] = "javascript.allow.mailnews";

int
nsScriptSecurityManager::JSEnabledPrefChanged(const char *pref, void *data)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsScriptSecurityManager *secMgr = (nsScriptSecurityManager *) data;

    if (NS_FAILED(prefs->GetBoolPref(jsEnabledPrefName, 
                                     &secMgr->mIsJavaScriptEnabled))) 
    {
        // Default to enabled.
        secMgr->mIsJavaScriptEnabled = PR_TRUE;
    }

    if (NS_FAILED(prefs->GetBoolPref(jsMailEnabledPrefName, 
                                     &secMgr->mIsMailJavaScriptEnabled))) 
    {
        // Default to enabled.
        secMgr->mIsMailJavaScriptEnabled = PR_TRUE;
    }

    return 0;
}


NS_IMETHODIMP
nsScriptSecurityManager::InitFromPrefs()
{
    // The DOM property enums and names better be in sync
    NS_ASSERTION(NS_DOM_PROP_MAX == sizeof(domPropNames)/sizeof(domPropNames[0]), 
                 "mismatch in property name count");

    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // Set the initial value of the "javascript.enabled" pref
    JSEnabledPrefChanged(jsEnabledPrefName, this);

    // set callbacks in case the value of the pref changes
    prefs->RegisterCallback(jsEnabledPrefName, JSEnabledPrefChanged, this);
    prefs->RegisterCallback(jsMailEnabledPrefName, JSEnabledPrefChanged, this);

    PolicyEnumeratorInfo info;
    info.prefs = prefs;
    info.secMan = this;
    prefs->EnumerateChildren("security.policy", 
                             nsScriptSecurityManager::enumeratePolicyCallback,
                             (void *) &info);
    return NS_OK;
}
