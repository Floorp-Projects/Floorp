/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID, 
                     NS_SCRIPT_NAMESET_REGISTRY_CID);

enum {
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
nsScriptSecurityManager::CheckScriptAccess(nsIScriptContext *aContext, 
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
    JSContext *cx = (JSContext *)aContext->GetNativeContext();
    nsXPIDLCString capability;
    PRInt32 secLevel = GetSecurityLevel(cx, domProp, isWrite,
                                        getter_Copies(capability));
    switch (secLevel) {
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
nsScriptSecurityManager::CheckLoadURIFromScript(nsIScriptContext *aContext, 
                                                nsIURI *aURI)
{
    // Get principal of currently executing script.
    JSContext *cx = (JSContext*) aContext->GetNativeContext();
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
    if (!principal) 
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
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
    if (NS_FAILED(prefs->GetBoolPref("javascript.enabled", result))) {
        // Default to enabled.
        *result = PR_TRUE;
        return NS_OK;
    }
    if (!*result) {
        // JavaScript is disabled, but we must still execute system JavaScript
        *result = (principal == mSystemPrincipal);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteFunction(void *jsFunc,
                                            PRBool *result)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
    if (NS_FAILED(prefs->GetBoolPref("javascript.enabled", result))) {
        // Default to enabled.
        *result = PR_TRUE;
        return NS_OK;
    }
    if (!*result) {
        // norris TODO: figure out JSContext strategy, replace nsnulls below
        // JavaScript is disabled, but we must still execute system JavaScript
        JSScript *script = JS_GetFunctionScript(nsnull, (JSFunction *) jsFunc);
        if (!script)
            return NS_ERROR_FAILURE;
        JSPrincipals *jsprin = JS_GetScriptPrincipals(nsnull, script);
        if (!jsprin)
            return NS_ERROR_FAILURE;
        nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) jsprin;
        *result = (nsJSPrin->nsIPrincipalPtr == mSystemPrincipal);
    }
    return NS_OK;
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
      mPrincipals(nsnull)
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
    // Temporary: only enforce if security.checkdomprops pref is enabled
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_FAILED(prefs->GetBoolPref("security.checkdomprops", &enabled)) ||
        !enabled) 
    {
        *aResult = PR_TRUE;
        return NS_OK;
    }

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
nsScriptSecurityManager::GetSecurityLevel(JSContext *cx, nsDOMProp domProp, 
                                          PRBool isWrite, char **capability)
{
    nsXPIDLCString prefName;
    if (NS_FAILED(GetPrefName(cx, domProp, getter_Copies(prefName))))
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

    // If no preference is defined for this property, allow access. 
    // This violates the rule of a safe default, but means we don't have
    // to specify the large majority of unchecked properties, only the
    // minority of checked ones.
    return SCRIPT_SECURITY_ALL_ACCESS;
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

static char *domPropNames[NS_DOM_PROP_MAX] = {
    "appcoresmanager.add",
    "appcoresmanager.find",
    "appcoresmanager.remove",
    "appcoresmanager.shutdown",
    "appcoresmanager.startup",
    "attr.name", 
    "attr.specified", 
    "attr.value", 
    "barprop.visible", 
    "baseappcore.id",
    "baseappcore.init",
    "baseappcore.setdocumentcharset",
    "characterdata.appenddata", 
    "characterdata.data", 
    "characterdata.deletedata", 
    "characterdata.insertdata", 
    "characterdata.length", 
    "characterdata.replacedata", 
    "characterdata.substringdata", 
    "css2properties.azimuth", 
    "css2properties.background", 
    "css2properties.backgroundattachment", 
    "css2properties.backgroundcolor", 
    "css2properties.backgroundimage", 
    "css2properties.backgroundposition", 
    "css2properties.backgroundrepeat", 
    "css2properties.border", 
    "css2properties.borderbottom", 
    "css2properties.borderbottomcolor", 
    "css2properties.borderbottomstyle", 
    "css2properties.borderbottomwidth", 
    "css2properties.bordercollapse", 
    "css2properties.bordercolor", 
    "css2properties.borderleft", 
    "css2properties.borderleftcolor", 
    "css2properties.borderleftstyle", 
    "css2properties.borderleftwidth", 
    "css2properties.borderright", 
    "css2properties.borderrightcolor", 
    "css2properties.borderrightstyle", 
    "css2properties.borderrightwidth", 
    "css2properties.borderspacing", 
    "css2properties.borderstyle", 
    "css2properties.bordertop", 
    "css2properties.bordertopcolor", 
    "css2properties.bordertopstyle", 
    "css2properties.bordertopwidth", 
    "css2properties.borderwidth", 
    "css2properties.bottom", 
    "css2properties.captionside", 
    "css2properties.clear", 
    "css2properties.clip", 
    "css2properties.color", 
    "css2properties.content", 
    "css2properties.counterincrement", 
    "css2properties.counterreset", 
    "css2properties.cssfloat", 
    "css2properties.cue", 
    "css2properties.cueafter", 
    "css2properties.cuebefore", 
    "css2properties.cursor", 
    "css2properties.direction", 
    "css2properties.display", 
    "css2properties.elevation", 
    "css2properties.emptycells", 
    "css2properties.font", 
    "css2properties.fontfamily", 
    "css2properties.fontsize", 
    "css2properties.fontsizeadjust", 
    "css2properties.fontstretch", 
    "css2properties.fontstyle", 
    "css2properties.fontvariant", 
    "css2properties.fontweight", 
    "css2properties.height", 
    "css2properties.left", 
    "css2properties.letterspacing", 
    "css2properties.lineheight", 
    "css2properties.liststyle", 
    "css2properties.liststyleimage", 
    "css2properties.liststyleposition", 
    "css2properties.liststyletype", 
    "css2properties.margin", 
    "css2properties.marginbottom", 
    "css2properties.marginleft", 
    "css2properties.marginright", 
    "css2properties.margintop", 
    "css2properties.markeroffset", 
    "css2properties.marks", 
    "css2properties.maxheight", 
    "css2properties.maxwidth", 
    "css2properties.minheight", 
    "css2properties.minwidth", 
    "css2properties.opacity", 
    "css2properties.orphans", 
    "css2properties.outline", 
    "css2properties.outlinecolor", 
    "css2properties.outlinestyle", 
    "css2properties.outlinewidth", 
    "css2properties.overflow", 
    "css2properties.padding", 
    "css2properties.paddingbottom", 
    "css2properties.paddingleft", 
    "css2properties.paddingright", 
    "css2properties.paddingtop", 
    "css2properties.page", 
    "css2properties.pagebreakafter", 
    "css2properties.pagebreakbefore", 
    "css2properties.pagebreakinside", 
    "css2properties.pause", 
    "css2properties.pauseafter", 
    "css2properties.pausebefore", 
    "css2properties.pitch", 
    "css2properties.pitchrange", 
    "css2properties.playduring", 
    "css2properties.position", 
    "css2properties.quotes", 
    "css2properties.richness", 
    "css2properties.right", 
    "css2properties.size", 
    "css2properties.speak", 
    "css2properties.speakheader", 
    "css2properties.speaknumeral", 
    "css2properties.speakpunctuation", 
    "css2properties.speechrate", 
    "css2properties.stress", 
    "css2properties.tablelayout", 
    "css2properties.textalign", 
    "css2properties.textdecoration", 
    "css2properties.textindent", 
    "css2properties.textshadow", 
    "css2properties.texttransform", 
    "css2properties.top", 
    "css2properties.unicodebidi", 
    "css2properties.verticalalign", 
    "css2properties.visibility", 
    "css2properties.voicefamily", 
    "css2properties.volume", 
    "css2properties.whitespace", 
    "css2properties.widows", 
    "css2properties.width", 
    "css2properties.wordspacing", 
    "css2properties.zindex", 
    "cssfontfacerule.style", 
    "cssimportrule.href", 
    "cssimportrule.media", 
    "cssimportrule.stylesheet", 
    "cssmediarule.cssrules", 
    "cssmediarule.deleterule", 
    "cssmediarule.insertrule", 
    "cssmediarule.mediatypes", 
    "csspagerule.name", 
    "csspagerule.style", 
    "cssrule.csstext", 
    "cssrule.sheet", 
    "cssrule.type", 
    "cssstyledeclaration.csstext", 
    "cssstyledeclaration.getpropertypriority", 
    "cssstyledeclaration.getpropertyvalue", 
    "cssstyledeclaration.item", 
    "cssstyledeclaration.length", 
    "cssstyledeclaration.setproperty", 
    "cssstylerule.selectortext", 
    "cssstylerule.style", 
    "cssstylerulecollection.item", 
    "cssstylerulecollection.length", 
    "cssstylesheet.cssrules", 
    "cssstylesheet.deleterule", 
    "cssstylesheet.href", 
    "cssstylesheet.insertrule", 
    "cssstylesheet.media", 
    "cssstylesheet.owningnode", 
    "cssstylesheet.parentstylesheet", 
    "cssstylesheet.title", 
    "document.createattribute", 
    "document.createcdatasection", 
    "document.createcomment", 
    "document.createdocumentfragment", 
    "document.createelement", 
    "document.createentityreference", 
    "document.createprocessinginstruction", 
    "document.createtextnode", 
    "document.doctype", 
    "document.documentelement", 
    "document.getelementsbytagname", 
    "document.implementation", 
    "documenttype.entities", 
    "documenttype.name", 
    "documenttype.notations", 
    "domexception.code", 
    "domexception.message", 
    "domexception.name", 
    "domexception.result", 
    "domexception.tostring", 
    "domimplementation.hasfeature", 
    "element.getattribute", 
    "element.getattributenode", 
    "element.getelementsbytagname", 
    "element.normalize", 
    "element.removeattribute", 
    "element.removeattributenode", 
    "element.setattribute", 
    "element.setattributenode", 
    "element.tagname", 
    "entity.notationname", 
    "entity.publicid", 
    "entity.systemid", 
    "event.bubbles", 
    "event.cancelable", 
    "event.currentnode", 
    "event.eventphase", 
    "event.initevent", 
    "event.preventbubble", 
    "event.preventcapture", 
    "event.preventdefault", 
    "event.target", 
    "event.type", 
    "eventtarget.addeventlistener", 
    "eventtarget.removeeventlistener", 
    "history.back", 
    "history.current", 
    "history.forward", 
    "history.go", 
    "history.length", 
    "history.next", 
    "history.previous", 
    "htmlanchorelement.accesskey", 
    "htmlanchorelement.blur", 
    "htmlanchorelement.charset", 
    "htmlanchorelement.coords", 
    "htmlanchorelement.focus", 
    "htmlanchorelement.href", 
    "htmlanchorelement.hreflang", 
    "htmlanchorelement.name", 
    "htmlanchorelement.rel", 
    "htmlanchorelement.rev", 
    "htmlanchorelement.shape", 
    "htmlanchorelement.tabindex", 
    "htmlanchorelement.target", 
    "htmlanchorelement.type", 
    "htmlappletelement.align", 
    "htmlappletelement.alt", 
    "htmlappletelement.archive", 
    "htmlappletelement.code", 
    "htmlappletelement.codebase", 
    "htmlappletelement.height", 
    "htmlappletelement.hspace", 
    "htmlappletelement.name", 
    "htmlappletelement.object", 
    "htmlappletelement.vspace", 
    "htmlappletelement.width", 
    "htmlareaelement.accesskey", 
    "htmlareaelement.alt", 
    "htmlareaelement.coords", 
    "htmlareaelement.href", 
    "htmlareaelement.nohref", 
    "htmlareaelement.shape", 
    "htmlareaelement.tabindex", 
    "htmlareaelement.target", 
    "htmlbaseelement.href", 
    "htmlbaseelement.target", 
    "htmlbasefontelement.color", 
    "htmlbasefontelement.face", 
    "htmlbasefontelement.size", 
    "htmlblockquoteelement.cite", 
    "htmlbodyelement.alink", 
    "htmlbodyelement.background", 
    "htmlbodyelement.bgcolor", 
    "htmlbodyelement.link", 
    "htmlbodyelement.text", 
    "htmlbodyelement.vlink", 
    "htmlbrelement.clear", 
    "htmlbuttonelement.accesskey", 
    "htmlbuttonelement.disabled", 
    "htmlbuttonelement.form", 
    "htmlbuttonelement.name", 
    "htmlbuttonelement.tabindex", 
    "htmlbuttonelement.type", 
    "htmlbuttonelement.value", 
    "htmlcollection.item", 
    "htmlcollection.length", 
    "htmlcollection.nameditem", 
    "htmldirectoryelement.compact", 
    "htmldivelement.align", 
    "htmldlistelement.compact", 
    "htmldocument.anchors", 
    "htmldocument.applets", 
    "htmldocument.body", 
    "htmldocument.close", 
    "htmldocument.cookie", 
    "htmldocument.domain", 
    "htmldocument.forms", 
    "htmldocument.getelementbyid", 
    "htmldocument.getelementsbyname", 
    "htmldocument.images", 
    "htmldocument.links", 
    "htmldocument.referrer", 
    "htmldocument.title", 
    "htmldocument.url", 
    "htmlelement.classname", 
    "htmlelement.dir", 
    "htmlelement.id", 
    "htmlelement.lang", 
    "htmlelement.style", 
    "htmlelement.title", 
    "htmlembedelement.align", 
    "htmlembedelement.height", 
    "htmlembedelement.name", 
    "htmlembedelement.src", 
    "htmlembedelement.type", 
    "htmlembedelement.width", 
    "htmlfieldsetelement.form", 
    "htmlfontelement.color", 
    "htmlfontelement.face", 
    "htmlfontelement.size", 
    "htmlformelement.acceptcharset", 
    "htmlformelement.action", 
    "htmlformelement.elements", 
    "htmlformelement.enctype", 
    "htmlformelement.length", 
    "htmlformelement.method", 
    "htmlformelement.name", 
    "htmlformelement.reset", 
    "htmlformelement.submit", 
    "htmlformelement.target", 
    "htmlframeelement.frameborder", 
    "htmlframeelement.longdesc", 
    "htmlframeelement.marginheight", 
    "htmlframeelement.marginwidth", 
    "htmlframeelement.name", 
    "htmlframeelement.noresize", 
    "htmlframeelement.scrolling", 
    "htmlframeelement.src", 
    "htmlframesetelement.cols", 
    "htmlframesetelement.rows", 
    "htmlheadelement.profile", 
    "htmlheadingelement.align", 
    "htmlhrelement.align", 
    "htmlhrelement.noshade", 
    "htmlhrelement.size", 
    "htmlhrelement.width", 
    "htmlhtmlelement.version", 
    "htmliframeelement.align", 
    "htmliframeelement.frameborder", 
    "htmliframeelement.height", 
    "htmliframeelement.longdesc", 
    "htmliframeelement.marginheight", 
    "htmliframeelement.marginwidth", 
    "htmliframeelement.name", 
    "htmliframeelement.scrolling", 
    "htmliframeelement.src", 
    "htmliframeelement.width", 
    "htmlimageelement.align", 
    "htmlimageelement.alt", 
    "htmlimageelement.border", 
    "htmlimageelement.height", 
    "htmlimageelement.hspace", 
    "htmlimageelement.ismap", 
    "htmlimageelement.longdesc", 
    "htmlimageelement.lowsrc", 
    "htmlimageelement.name", 
    "htmlimageelement.src", 
    "htmlimageelement.usemap", 
    "htmlimageelement.vspace", 
    "htmlimageelement.width", 
    "htmlinputelement.accept", 
    "htmlinputelement.accesskey", 
    "htmlinputelement.align", 
    "htmlinputelement.alt", 
    "htmlinputelement.autocomplete", 
    "htmlinputelement.blur", 
    "htmlinputelement.checked", 
    "htmlinputelement.click", 
    "htmlinputelement.controllers",
    "htmlinputelement.defaultchecked", 
    "htmlinputelement.defaultvalue", 
    "htmlinputelement.disabled", 
    "htmlinputelement.focus", 
    "htmlinputelement.form", 
    "htmlinputelement.maxlength", 
    "htmlinputelement.name", 
    "htmlinputelement.readonly", 
    "htmlinputelement.select", 
    "htmlinputelement.size", 
    "htmlinputelement.src", 
    "htmlinputelement.tabindex", 
    "htmlinputelement.type", 
    "htmlinputelement.usemap", 
    "htmlinputelement.value", 
    "htmlisindexelement.form", 
    "htmlisindexelement.prompt", 
    "htmllabelelement.accesskey", 
    "htmllabelelement.form", 
    "htmllabelelement.htmlfor", 
    "htmllayerelement.background", 
    "htmllayerelement.bgcolor", 
    "htmllayerelement.document", 
    "htmllayerelement.left", 
    "htmllayerelement.name", 
    "htmllayerelement.top", 
    "htmllayerelement.visibility", 
    "htmllayerelement.zindex", 
    "htmllegendelement.accesskey", 
    "htmllegendelement.align", 
    "htmllegendelement.form", 
    "htmllielement.type", 
    "htmllielement.value", 
    "htmllinkelement.charset", 
    "htmllinkelement.disabled", 
    "htmllinkelement.href", 
    "htmllinkelement.hreflang", 
    "htmllinkelement.media", 
    "htmllinkelement.rel", 
    "htmllinkelement.rev", 
    "htmllinkelement.target", 
    "htmllinkelement.type", 
    "htmlmapelement.areas", 
    "htmlmapelement.name", 
    "htmlmenuelement.compact", 
    "htmlmetaelement.content", 
    "htmlmetaelement.httpequiv", 
    "htmlmetaelement.name", 
    "htmlmetaelement.scheme", 
    "htmlmodelement.cite", 
    "htmlmodelement.datetime", 
    "htmlobjectelement.align", 
    "htmlobjectelement.archive", 
    "htmlobjectelement.border", 
    "htmlobjectelement.code", 
    "htmlobjectelement.codebase", 
    "htmlobjectelement.codetype", 
    "htmlobjectelement.data", 
    "htmlobjectelement.declare", 
    "htmlobjectelement.form", 
    "htmlobjectelement.height", 
    "htmlobjectelement.hspace", 
    "htmlobjectelement.name", 
    "htmlobjectelement.standby", 
    "htmlobjectelement.tabindex", 
    "htmlobjectelement.type", 
    "htmlobjectelement.usemap", 
    "htmlobjectelement.vspace", 
    "htmlobjectelement.width", 
    "htmlolistelement.compact", 
    "htmlolistelement.start", 
    "htmlolistelement.type", 
    "htmloptgroupelement.disabled", 
    "htmloptgroupelement.label", 
    "htmloptionelement.defaultselected", 
    "htmloptionelement.disabled", 
    "htmloptionelement.form", 
    "htmloptionelement.index", 
    "htmloptionelement.label", 
    "htmloptionelement.selected", 
    "htmloptionelement.text", 
    "htmloptionelement.value", 
    "htmlparagraphelement.align", 
    "htmlparamelement.name", 
    "htmlparamelement.type", 
    "htmlparamelement.value", 
    "htmlparamelement.valuetype", 
    "htmlpreelement.width", 
    "htmlquoteelement.cite", 
    "htmlscriptelement.charset", 
    "htmlscriptelement.defer", 
    "htmlscriptelement.event", 
    "htmlscriptelement.htmlfor", 
    "htmlscriptelement.src", 
    "htmlscriptelement.text", 
    "htmlscriptelement.type", 
    "htmlselectelement.add", 
    "htmlselectelement.blur", 
    "htmlselectelement.disabled", 
    "htmlselectelement.focus", 
    "htmlselectelement.form", 
    "htmlselectelement.length", 
    "htmlselectelement.multiple", 
    "htmlselectelement.name", 
    "htmlselectelement.options", 
    "htmlselectelement.remove", 
    "htmlselectelement.selectedindex", 
    "htmlselectelement.size", 
    "htmlselectelement.tabindex", 
    "htmlselectelement.type", 
    "htmlselectelement.value", 
    "htmlstyleelement.disabled", 
    "htmlstyleelement.media", 
    "htmlstyleelement.type", 
    "htmltablecaptionelement.align", 
    "htmltablecellelement.abbr", 
    "htmltablecellelement.align", 
    "htmltablecellelement.axis", 
    "htmltablecellelement.bgcolor", 
    "htmltablecellelement.cellindex", 
    "htmltablecellelement.ch", 
    "htmltablecellelement.choff", 
    "htmltablecellelement.colspan", 
    "htmltablecellelement.headers", 
    "htmltablecellelement.height", 
    "htmltablecellelement.nowrap", 
    "htmltablecellelement.rowspan", 
    "htmltablecellelement.scope", 
    "htmltablecellelement.valign", 
    "htmltablecellelement.width", 
    "htmltablecolelement.align", 
    "htmltablecolelement.ch", 
    "htmltablecolelement.choff", 
    "htmltablecolelement.span", 
    "htmltablecolelement.valign", 
    "htmltablecolelement.width", 
    "htmltableelement.align", 
    "htmltableelement.bgcolor", 
    "htmltableelement.border", 
    "htmltableelement.caption", 
    "htmltableelement.cellpadding", 
    "htmltableelement.cellspacing", 
    "htmltableelement.createcaption", 
    "htmltableelement.createtfoot", 
    "htmltableelement.createthead", 
    "htmltableelement.deletecaption", 
    "htmltableelement.deleterow", 
    "htmltableelement.deletetfoot", 
    "htmltableelement.deletethead", 
    "htmltableelement.frame", 
    "htmltableelement.insertrow", 
    "htmltableelement.rows", 
    "htmltableelement.rules", 
    "htmltableelement.summary", 
    "htmltableelement.tbodies", 
    "htmltableelement.tfoot", 
    "htmltableelement.thead", 
    "htmltableelement.width", 
    "htmltablerowelement.align", 
    "htmltablerowelement.bgcolor", 
    "htmltablerowelement.cells", 
    "htmltablerowelement.ch", 
    "htmltablerowelement.choff", 
    "htmltablerowelement.deletecell", 
    "htmltablerowelement.insertcell", 
    "htmltablerowelement.rowindex", 
    "htmltablerowelement.sectionrowindex", 
    "htmltablerowelement.valign", 
    "htmltablesectionelement.align", 
    "htmltablesectionelement.ch", 
    "htmltablesectionelement.choff", 
    "htmltablesectionelement.deleterow", 
    "htmltablesectionelement.insertrow", 
    "htmltablesectionelement.rows", 
    "htmltablesectionelement.valign", 
    "htmltextareaelement.accesskey", 
    "htmltextareaelement.blur", 
    "htmltextareaelement.cols", 
    "htmltextareaelement.controllers", 
    "htmltextareaelement.defaultvalue", 
    "htmltextareaelement.disabled", 
    "htmltextareaelement.focus", 
    "htmltextareaelement.form", 
    "htmltextareaelement.name", 
    "htmltextareaelement.readonly", 
    "htmltextareaelement.rows", 
    "htmltextareaelement.select", 
    "htmltextareaelement.tabindex", 
    "htmltextareaelement.type", 
    "htmltextareaelement.value", 
    "htmltitleelement.text", 
    "htmlulistelement.compact", 
    "htmlulistelement.type", 
    "keyevent.altkey", 
    "keyevent.charcode", 
    "keyevent.ctrlkey", 
    "keyevent.initkeyevent", 
    "keyevent.keycode", 
    "keyevent.metakey", 
    "keyevent.shiftkey", 
    "location.hash", 
    "location.host", 
    "location.hostname", 
    "location.pathname", 
    "location.port", 
    "location.protocol", 
    "location.search", 
    "location.tostring", 
    "mimetype.description", 
    "mimetype.enabledplugin", 
    "mimetype.suffixes", 
    "mimetype.type", 
    "mimetypearray.item", 
    "mimetypearray.length", 
    "mimetypearray.nameditem", 
    "mouseevent.button", 
    "mouseevent.clickcount", 
    "mouseevent.clientx", 
    "mouseevent.clienty", 
    "mouseevent.initmouseevent", 
    "mouseevent.relatednode", 
    "mouseevent.screenx", 
    "mouseevent.screeny", 
    "namednodemap.getnameditem", 
    "namednodemap.item", 
    "namednodemap.length", 
    "namednodemap.removenameditem", 
    "namednodemap.setnameditem", 
    "navigator.appcodename", 
    "navigator.appname", 
    "navigator.appversion", 
    "navigator.javaenabled", 
    "navigator.language", 
    "navigator.mimetypes", 
    "navigator.platform", 
    "navigator.plugins", 
    "navigator.preference", 
    "navigator.securitypolicy", 
    "navigator.taintenabled", 
    "navigator.useragent", 
    "node.appendchild", 
    "node.attributes", 
    "node.childnodes", 
    "node.clonenode", 
    "node.firstchild", 
    "node.haschildnodes", 
    "node.insertbefore", 
    "node.lastchild", 
    "node.nextsibling", 
    "node.nodename", 
    "node.nodetype", 
    "node.nodevalue", 
    "node.ownerdocument", 
    "node.parentnode", 
    "node.previoussibling", 
    "node.removechild", 
    "node.replacechild", 
    "nodelist.item", 
    "nodelist.length", 
    "notation.publicid", 
    "notation.systemid", 
    "nsdocument.createelementwithnamespace", 
    "nsdocument.createrange", 
    "nsdocument.height", 
    "nsdocument.stylesheets", 
    "nsdocument.width", 
    "nshtmlbuttonelement.blur", 
    "nshtmlbuttonelement.focus", 
    "nshtmldocument.alinkcolor", 
    "nshtmldocument.bgcolor", 
    "nshtmldocument.captureevents", 
    "nshtmldocument.embeds", 
    "nshtmldocument.fgcolor", 
    "nshtmldocument.getselection", 
    "nshtmldocument.lastmodified", 
    "nshtmldocument.layers", 
    "nshtmldocument.linkcolor", 
    "nshtmldocument.nameditem", 
    "nshtmldocument.open", 
    "nshtmldocument.plugins", 
    "nshtmldocument.releaseevents", 
    "nshtmldocument.routeevent", 
    "nshtmldocument.vlinkcolor", 
    "nshtmldocument.write", 
    "nshtmldocument.writeln", 
    "nshtmlformelement.encoding", 
    "nshtmlformelement.item", 
    "nshtmlformelement.nameditem", 
    "nshtmlselectelement.item", 
    "nslocation.reload", 
    "nslocation.replace", 
    "nsrange.createcontextualfragment", 
    "nsrange.isvalidfragment", 
    "nsuievent.cancelbubble", 
    "nsuievent.ischar", 
    "nsuievent.layerx", 
    "nsuievent.layery", 
    "nsuievent.pagex", 
    "nsuievent.pagey", 
    "nsuievent.rangeoffset", 
    "nsuievent.rangeparent", 
    "nsuievent.which", 
    "plugin.description", 
    "plugin.filename", 
    "plugin.item", 
    "plugin.length", 
    "plugin.name", 
    "plugin.nameditem", 
    "pluginarray.item", 
    "pluginarray.length", 
    "pluginarray.nameditem", 
    "pluginarray.refresh", 
    "processinginstruction.data", 
    "processinginstruction.target", 
    "range.clone", 
    "range.clonecontents", 
    "range.collapse", 
    "range.commonparent", 
    "range.compareendpoints", 
    "range.deletecontents", 
    "range.endoffset", 
    "range.endparent", 
    "range.extractcontents", 
    "range.insertnode", 
    "range.iscollapsed", 
    "range.selectnode", 
    "range.selectnodecontents", 
    "range.setend", 
    "range.setendafter", 
    "range.setendbefore", 
    "range.setstart", 
    "range.setstartafter", 
    "range.setstartbefore", 
    "range.startoffset", 
    "range.startparent", 
    "range.surroundcontents", 
    "range.tostring", 
    "screen.availheight", 
    "screen.availleft", 
    "screen.availtop", 
    "screen.availwidth", 
    "screen.colordepth", 
    "screen.height", 
    "screen.pixeldepth", 
    "screen.width", 
    "selection.addrange", 
    "selection.addselectionlistener", 
    "selection.anchornode", 
    "selection.anchoroffset", 
    "selection.clearselection", 
    "selection.collapse", 
    "selection.collapsetoend", 
    "selection.collapsetostart", 
    "selection.containsnode", 
    "selection.deletefromdocument", 
    "selection.endbatchchanges", 
    "selection.extend", 
    "selection.focusnode", 
    "selection.focusoffset", 
    "selection.getrangeat", 
    "selection.iscollapsed", 
    "selection.rangecount", 
    "selection.removeselectionlistener", 
    "selection.startbatchchanges", 
    "selection.tostring", 
    "selectionlistener.notifyselectionchanged", 
    "stylesheet.disabled", 
    "stylesheet.readonly", 
    "stylesheet.type", 
    "stylesheetcollection.item", 
    "stylesheetcollection.length", 
    "text.splittext", 
    "textrange.rangeend", 
    "textrange.rangestart", 
    "textrange.rangetype", 
    "textrangelist.item", 
    "textrangelist.length", 
    "toolkitcore.closewindow",
    "toolkitcore.showdialog",
    "toolkitcore.showmodaldialog",
    "toolkitcore.showwindow",
    "toolkitcore.showwindowwithargs",
    "uievent.detail", 
    "uievent.inituievent", 
    "uievent.view", 
    "window.alert", 
    "window.back", 
    "window.blur", 
    "window.captureevents", 
    "window.clearinterval", 
    "window.cleartimeout", 
    "window.close", 
    "window.closed", 
    "window.confirm", 
    "window.content", 
    "window.controllers", 
    "window.createpopup", 
    "window.defaultstatus", 
    "window.directories", 
    "window.disableexternalcapture", 
    "window.document", 
    "window.dump", 
    "window.enableexternalcapture", 
    "window.focus", 
    "window.forward", 
    "window.frames", 
    "window.history", 
    "window.home", 
    "window.innerheight", 
    "window.innerwidth", 
    "window.locationbar", 
    "window.menubar", 
    "window.moveby", 
    "window.moveto", 
    "window.name", 
    "window.navigator", 
    "window.open", 
    "window.opendialog", 
    "window.opener", 
    "window.outerheight", 
    "window.outerwidth", 
    "window.pagexoffset", 
    "window.pageyoffset", 
    "window.parent", 
    "window.personalbar", 
    "window.print", 
    "window.prompt", 
    "window.releaseevents", 
    "window.resizeby", 
    "window.resizeto", 
    "window.routeevent", 
    "window.screen", 
    "window.screenx", 
    "window.screeny", 
    "window.scrollbars", 
    "window.scrollby", 
    "window.scrollto", 
    "window.scrollx", 
    "window.scrolly", 
    "window.self", 
    "window.setinterval", 
    "window.settimeout", 
    "window.sizetocontent", 
    "window.status", 
    "window.statusbar", 
    "window.stop", 
    "window.toolbar", 
    "window.top", 
    "window.window", 
    "windowcollection.item", 
    "windowcollection.length", 
    "windowcollection.nameditem", 
    "xulcommanddispatcher.addcommandupdater",
    "xulcommanddispatcher.focusednode",
    "xulcommanddispatcher.getcontrollerforcommand",
    "xulcommanddispatcher.getcontrollers",
    "xulcommanddispatcher.removecommandupdater",
    "xulcommanddispatcher.updatecommands",
    "xuldocument.commanddispatcher",
    "xuldocument.getelementbyid",
    "xuldocument.getelementsbyattribute",
    "xuldocument.persist",
    "xuldocument.popupnode",
    "xuldocument.tooltipnode",
    "xuleditorelement.editorshell",
    "xulelement.addbroadcastlistener",
    "xulelement.classname",
    "xulelement.controllers",
    "xulelement.database",
    "xulelement.docommand",
    "xulelement.getelementsbyattribute",
    "xulelement.id",
    "xulelement.removebroadcastlistener",
    "xulelement.resource",
    "xulelement.style",
    "xultreeelement.addcelltoselection",
    "xultreeelement.additemtoselection",
    "xultreeelement.clearcellselection",
    "xultreeelement.clearitemselection",
    "xultreeelement.invertselection",
    "xultreeelement.removecellfromselection",
    "xultreeelement.removeitemfromselection",
    "xultreeelement.selectall",
    "xultreeelement.selectcell",
    "xultreeelement.selectcellrange",
    "xultreeelement.selectedcells",
    "xultreeelement.selecteditems",
    "xultreeelement.selectitem",
    "xultreeelement.selectitemrange",
    "xultreeelement.togglecellselection",
    "xultreeelement.toggleitemselection",
};


NS_IMETHODIMP
nsScriptSecurityManager::GetPrefName(JSContext *cx, nsDOMProp domProp, 
                                     char **result)
{
    nsresult rv;
    static const char *defaultStr = "default";
    nsAutoString s = "security.policy.";
    if (!GetBit(hasDomainPolicyVector, domProp)) {
        s += defaultStr;
    } else {
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal)))) {
            return NS_ERROR_FAILURE;
        }
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

NS_IMETHODIMP
nsScriptSecurityManager::InitFromPrefs()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    PolicyEnumeratorInfo info;
    info.prefs = prefs;
    info.secMan = this;
    prefs->EnumerateChildren("security.policy", 
                             nsScriptSecurityManager::enumeratePolicyCallback,
                             (void *) &info);
    return NS_OK;
}
