/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsIPref.h"
#include "nsIURL.h"
#ifdef OJI
#include "jvmmgr.h"
#endif
#include "nspr.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIJSContextStack.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kIScriptSecurityManagerIID, NS_ISCRIPTSECURITYMANAGER_IID);
static NS_DEFINE_IID(kIXPCSecurityManagerIID, NS_IXPCSECURITYMANAGER_IID);

static const char accessErrorMessage[] = 
    "access disallowed from scripts at %s to documents at another domain";

enum {
    SCRIPT_SECURITY_SAME_DOMAIN_ACCESS,
    SCRIPT_SECURITY_ALL_ACCESS,
    SCRIPT_SECURITY_NO_ACCESS
};

////////////////////////////////////
// Methods implementing ISupports //
////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) 
      return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(kIScriptSecurityManagerIID)) {
      *aInstancePtr = (void*)(nsIScriptSecurityManager *)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  if (aIID.Equals(kIXPCSecurityManagerIID)) {
      *aInstancePtr = (void*)(nsIXPCSecurityManager *)this;
      NS_ADDREF_THIS();
      return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsScriptSecurityManager);
NS_IMPL_RELEASE(nsScriptSecurityManager);


///////////////////////////////////////////////////
// Methods implementing nsIScriptSecurityManager //
///////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CheckScriptAccess(nsIScriptContext *aContext, 
                                           void *aObj, const char *aProp, 
                                           PRBool isWrite, PRBool *aResult)
{
    *aResult = PR_FALSE;
    JSContext *cx = (JSContext *)aContext->GetNativeContext();
    PRInt32 secLevel = GetSecurityLevel(cx, (char *) aProp, nsnull);
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
      default:
        // Default is no access
        *aResult = PR_FALSE;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckURI(nsIScriptContext *aContext, 
                                  nsIURI *aURI,
                                  PRBool *aResult)
{
    // Temporary: only enforce if security.checkuri pref is enabled
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
                                 (nsISupports**) &mPrefs);
    PRBool enabled;
    if (NS_FAILED(mPrefs->GetBoolPref("security.checkuri", &enabled)) ||
        !enabled) 
    {
        *aResult = PR_TRUE;
        return NS_OK;
    }

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
        *aResult = PR_TRUE;
        return NS_OK;
    }
    if (nsCRT::strcmp(scheme, "about") == 0) {
        nsXPIDLCString spec;
        if (NS_FAILED(aURI->GetSpec(getter_Copies(spec))))
            return NS_ERROR_FAILURE;
        if (nsCRT::strcmp(spec, "about:blank") == 0) {
            *aResult = PR_TRUE;
            return NS_OK;
        }
    }
    JSContext *cx = (JSContext*) aContext->GetNativeContext();
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal)))) {
        return NS_ERROR_FAILURE;
    }
    if (nsCRT::strcmp(scheme, "file") == 0) {
        nsCOMPtr<nsICodebasePrincipal> codebase;
        if (NS_SUCCEEDED(principal->QueryInterface(
                 NS_GET_IID(nsICodebasePrincipal), 
                 (void **) getter_AddRefs(codebase))))
        {
            nsCOMPtr<nsIURI> uri;
            if (NS_SUCCEEDED(codebase->GetURI(getter_AddRefs(uri)))) {
                nsXPIDLCString scheme2;
                if (NS_SUCCEEDED(uri->GetScheme(getter_Copies(scheme2))) &&
                    nsCRT::strcmp(scheme2, "file") == 0)
                {
                    *aResult = PR_TRUE;
                    return NS_OK;
                }
            }
        }
        if (NS_FAILED(principal->CanAccess("UniversalFileRead", aResult)))
            return NS_ERROR_FAILURE;

        if (*aResult)
            return NS_OK;
    }

    // Only allowed for the system principal to create other URIs.
    if (NS_FAILED(principal->Equals(mSystemPrincipal, aResult)))
        return NS_ERROR_FAILURE;

    if (!*aResult) {
        // Report error.
        nsXPIDLCString spec;
        if (NS_FAILED(aURI->GetSpec(getter_Copies(spec))))
            return NS_ERROR_FAILURE;
	    JS_ReportError(cx, "illegal URL method '%s'", (const char *)spec);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(nsIPrincipal **result)
{
    // Get JSContext from stack.
    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, stack, "nsThreadJSContextStack", 
                    &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    JSContext *cx;
    if (NS_FAILED(stack->Peek(&cx)))
        return NS_ERROR_FAILURE;
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
nsScriptSecurityManager::CreateCodebasePrincipal(nsIURI *aURI, 
                                                 nsIPrincipal **result)
{
    nsCOMPtr<nsCodebasePrincipal> codebase = new nsCodebasePrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(codebase->Init(aURI)))
        return NS_ERROR_FAILURE;
    *result = codebase;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CanEnableCapability(nsIPrincipal *principal, 
                                             const char *capability, 
                                             PRBool *result)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::SetCanEnableCapability(nsIPrincipal *principal, 
                                                const char *capability, 
                                                PRBool canEnable)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::EnableCapability(nsIScriptContext *cx, 
                                          const char *capability)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::RevertCapability(nsIScriptContext *cx, 
                                          const char *capability)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::DisableCapability(nsIScriptContext *cx, 
                                           const char *capability)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}


////////////////////////////////////////////////
// Methods implementing nsIXPCSecurityManager //
////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptSecurityManager::CanCreateWrapper(JSContext *aJSContext, 
                                          const nsIID &aIID, 
                                          nsISupports *aObj)
{
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
    : mSystemPrincipal(nsnull)
{
    NS_INIT_REFCNT();
}

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
//  nsServiceManager::ReleaseService(kPrefServiceCID, mPrefs);  
} 

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    static nsScriptSecurityManager *ssecMan = NULL;
    if (!ssecMan) 
        ssecMan = new nsScriptSecurityManager();
    return ssecMan;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *aCx, 
                                             nsIPrincipal **result)
{
    // Get principals from innermost frame of JavaScript or Java.
    JSPrincipals *principals;
    JSStackFrame *fp;
    JSScript *script;
#ifdef OJI
    JSStackFrame *pFrameToStartLooking = 
        *JVM_GetStartJSFrameFromParallelStack();
    JSStackFrame *pFrameToEndLooking   = 
        JVM_GetEndJSFrameFromParallelStack(pFrameToStartLooking);
    if (pFrameToStartLooking == nsnull) {
        pFrameToStartLooking = JS_FrameIterator(aCx, &pFrameToStartLooking);
        if (pFrameToStartLooking == nsnull) {
            // There are no frames or scripts at this point.
            pFrameToEndLooking = nsnull;
        }
    }
#else
    fp = nsnull; // indicate to JS_FrameIterator to start from innermost frame
    JSStackFrame *pFrameToStartLooking = JS_FrameIterator(aCx, &fp);
    JSStackFrame *pFrameToEndLooking   = nsnull;
#endif
    fp = pFrameToStartLooking;
    while (fp != pFrameToEndLooking) {
        script = JS_GetFrameScript(aCx, fp);
        if (script) {
            principals = JS_GetScriptPrincipals(aCx, script);
            if (principals) {
                nsJSPrincipals *nsJSPrin = (nsJSPrincipals *) principals;
                *result = nsJSPrin->nsIPrincipalPtr;
                NS_ADDREF(*result);
                return NS_OK;
            } else {
                return NS_ERROR_FAILURE;
            }
        }
        fp = JS_FrameIterator(aCx, &fp);
    }
#ifdef OJI
    principals = JVM_GetJavaPrincipalsFromStack(pFrameToStartLooking);
    if (principals && principals->codebase) {
        // create new principals
        /*
        nsresult rv;
        NS_WITH_SERVICE(nsIPrincipalManager, prinMan, 
                        NS_PRINCIPALMANAGER_PROGID, &rv);
        // NB TODO: create nsIURI to pass in
        if (NS_SUCCEEDED(rv)) 
            rv = prinMan->CreateCodebasePrincipal(principals->codebase, 
                                                  nsnull, result);
        if (NS_SUCCEEDED(rv))
            return NS_OK;
        */
    }
#endif
    // Couldn't find principals: no mobile code on stack.
    // Use system principal.
    *result = mSystemPrincipal;
    NS_ADDREF(*result);
    return NS_OK;
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
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPermissions(JSContext *aCx, JSObject *aObj, 
                                          const char *aCapability,
                                          PRBool* aResult)
{
    // Temporary: only enforce if security.checkdomprops pref is enabled
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
                                 (nsISupports**) &mPrefs);
    PRBool enabled;
    if (NS_FAILED(mPrefs->GetBoolPref("security.checkdomprops", &enabled)) ||
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

    nsCOMPtr<nsIPrincipal> object;
    if (NS_FAILED(GetObjectPrincipal(aCx, aObj, getter_AddRefs(object))))
        return NS_ERROR_FAILURE;
    if (subject == object) {
        *aResult = PR_TRUE;
        return NS_OK;
    }
    nsCOMPtr<nsICodebasePrincipal> subjectCodebase;
    if (NS_SUCCEEDED(subject->QueryInterface(
                        NS_GET_IID(nsICodebasePrincipal),
	                (void **) getter_AddRefs(subjectCodebase))))
    {
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
    if (NS_FAILED(subject->CanAccess(aCapability, aResult)))
        return NS_ERROR_FAILURE;
    if (*aResult)
        return NS_OK;
    
    /*
    ** Access tests failed, so now report error.
    */
    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(subjectCodebase->GetURI(getter_AddRefs(uri)))) 
        return NS_ERROR_FAILURE;
    char *spec;
    if (NS_FAILED(uri->GetSpec(&spec)))
        return NS_ERROR_FAILURE;
    JS_ReportError(aCx, accessErrorMessage, spec);
    nsCRT::free(spec);
    *aResult = PR_FALSE;
    return NS_OK;
}


PRInt32 
nsScriptSecurityManager::GetSecurityLevel(JSContext *cx, char *prop_name, 
                                          int priv_code)
{
    if (prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    char *tmp_prop_name = AddSecPolicyPrefix(cx, prop_name);
    if (tmp_prop_name == nsnull) 
        return SCRIPT_SECURITY_NO_ACCESS;
    PRInt32 secLevel;
    char *secLevelString;
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
                                 (nsISupports**) &mPrefs);
    if (NS_SUCCEEDED(mPrefs->CopyCharPref(tmp_prop_name, &secLevelString)) &&
        secLevelString) 
    {
        PR_FREEIF(tmp_prop_name);
        if (PL_strcmp(secLevelString, "sameOrigin") == 0)
            secLevel = SCRIPT_SECURITY_SAME_DOMAIN_ACCESS;
        else if (PL_strcmp(secLevelString, "allAccess") == 0)
            secLevel = SCRIPT_SECURITY_ALL_ACCESS;
        else if (PL_strcmp(secLevelString, "noAccess") == 0)
            secLevel = SCRIPT_SECURITY_NO_ACCESS;
        else
            secLevel = SCRIPT_SECURITY_NO_ACCESS;
        // NB TODO: what about signed scripts?
        PR_Free(secLevelString);
        return secLevel;
    }

    // If no preference is defined for this property, allow access. 
    // This violates the rule of a safe default, but means we don't have
    // to specify the large majority of unchecked properties, only the
    // minority of checked ones.
    PR_FREEIF(tmp_prop_name);
    return SCRIPT_SECURITY_ALL_ACCESS;
}


char *
nsScriptSecurityManager::AddSecPolicyPrefix(JSContext *cx, char *pref_str)
{
    const char *subjectOrigin = "";//GetSubjectOriginURL(cx);
    char *policy_str, *retval = 0;
    if ((policy_str = GetSitePolicy(subjectOrigin)) == 0) {
        /* No site-specific policy.  Get global policy name. */
        nsIPref * mPrefs;
        nsServiceManager::GetService(kPrefServiceCID,NS_GET_IID(nsIPref), (nsISupports**)&mPrefs);
        if (NS_OK != mPrefs->CopyCharPref("javascript.security_policy", &policy_str))
            policy_str = PL_strdup("default");
    }
    if (policy_str) { //why can't this be default? && PL_strcasecmp(policy_str, "default") != 0) {
        retval = PR_sprintf_append(NULL, "security.policy.%s.%s", policy_str, pref_str);
        PR_Free(policy_str);
    }
    
    return retval;
}


char *
nsScriptSecurityManager::GetSitePolicy(const char *org)
{
    return nsnull;
}


NS_IMETHODIMP
nsScriptSecurityManager::CheckXPCPermissions(JSContext *aJSContext)
{
    // Temporary: only enforce if security.checkxpconnect pref is enabled
    nsIPref *mPrefs;
    nsServiceManager::GetService(kPrefServiceCID, NS_GET_IID(nsIPref), 
                                 (nsISupports**) &mPrefs);
    PRBool enabled;
    if (NS_FAILED(mPrefs->GetBoolPref("security.checkxpconnect", &enabled)) ||
        !enabled) 
    {
        return NS_OK;
    }
    
    nsCOMPtr<nsIPrincipal> subject;
    if (NS_FAILED(GetSubjectPrincipal(aJSContext, getter_AddRefs(subject))))
        return NS_ERROR_FAILURE;
    PRBool ok = PR_FALSE;
    if (NS_FAILED(subject->CanAccess("UniversalXPConnect", &ok)))
        return NS_ERROR_FAILURE;
    if (!ok) {
        JS_ReportError(aJSContext, "Access denied to XPConnect service.");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}


#if 0
static JSBool
callCapsCode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval, nsCapsFn fn, char *name)
{
    JSString *str;
    char *cstr;
    struct nsTarget *target;

    if (argc == 0 || !JSVAL_IS_STRING(argv[0])) {
        JS_ReportError(cx, "String argument expected for %s.", name);
        return JS_FALSE;
    }
    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     * XXX but then use of an object will cause errors here....
     */
    str = JSVAL_TO_STRING(argv[0]);
    if (!str)
        return JS_FALSE;

    cstr = JS_GetStringBytes(str);
    if (cstr == NULL)
        return JS_FALSE;

    target = nsCapsFindTarget(cstr);
    if (target == NULL)
        return JS_FALSE;
    /* stack depth of 1: first frame is for the native function called */
    if (!(*fn)(cx, target, 1)) {
        /* XXX report error, later, throw exception */
        return JS_FALSE;
    }
    return JS_TRUE;
}
 
PR_STATIC_CALLBACK(JSBool)
netscape_security_isPrivilegeEnabled(JSContext *cx, JSObject *obj, uintN argc,
                                        jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsIsPrivilegeEnabled,
                        isPrivilegeEnabledStr);
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_enablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsEnablePrivilege,
                        enablePrivilegeStr);
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_disablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                      jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsDisablePrivilege,
                        disablePrivilegeStr);
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_revertPrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    return callCapsCode(cx, obj, argc, argv, rval, nsCapsRevertPrivilege,
                        revertPrivilegeStr);
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { isPrivilegeEnabledStr, netscape_security_isPrivilegeEnabled,   1},
    { enablePrivilegeStr,    netscape_security_enablePrivilege,      1},
    { disablePrivilegeStr,   netscape_security_disablePrivilege,     1},
    { revertPrivilegeStr,    netscape_security_revertPrivilege,      1},
    {0}
};

JSBool
lm_InitSecurity(MochaDecoder *decoder)
{
    JSContext  *cx;
    JSObject   *obj;
    JSObject   *proto;
    JSClass    *objectClass;
    jsval      v;
    JSObject   *securityObj;

    /*
     * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
     * et. al. so that code that worked with 4.0 can still work.
     */

    /*
     * Find Object.prototype's class by walking up the window object's
     * prototype chain.
     */
    cx = decoder->js_context;
    obj = decoder->window_object;
    while (proto = JS_GetPrototype(cx, obj))
        obj = proto;
    objectClass = JS_GetClass(cx, obj);

    if (!JS_GetProperty(cx, decoder->window_object, "netscape", &v))
        return JS_FALSE;
    if (JSVAL_IS_OBJECT(v)) {
        /*
         * "netscape" property of window object exists; must be LiveConnect
         * package. Get the "security" property.
         */
        obj = JSVAL_TO_OBJECT(v);
        if (!JS_GetProperty(cx, obj, "security", &v) || !JSVAL_IS_OBJECT(v))
            return JS_FALSE;
        securityObj = JSVAL_TO_OBJECT(v);
    } else {
        /* define netscape.security object */
        obj = JS_DefineObject(cx, decoder->window_object, "netscape",
                              objectClass, NULL, 0);
        if (obj == NULL)
            return JS_FALSE;
        securityObj = JS_DefineObject(cx, obj, "security", objectClass,
                                      NULL, 0);
        if (securityObj == NULL)
            return JS_FALSE;
    }

    /* Define PrivilegeManager object with the necessary "static" methods. */
    obj = JS_DefineObject(cx, securityObj, "PrivilegeManager", objectClass,
                          NULL, 0);
    if (obj == NULL)
        return JS_FALSE;

    return JS_DefineFunctions(cx, obj, PrivilegeManager_static_methods);
}

JSBool lm_CheckSetParentSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *newParent;

    if (!JSVAL_IS_OBJECT(*vp))
        return JS_TRUE;
    newParent = JSVAL_TO_OBJECT(*vp);
    if (newParent) {
        const char *oldOrigin = lm_GetObjectOriginURL(cx, obj);
        const char *newOrigin = lm_GetObjectOriginURL(cx, newParent);
        if (!sameOrigins(cx, oldOrigin, newOrigin))
            return JS_TRUE;
    } else {
        if (!JS_InstanceOf(cx, obj, &lm_layer_class, 0) &&
            !JS_InstanceOf(cx, obj, &lm_window_class, 0))
        {
            return JS_TRUE;
        }
        if (lm_GetContainerPrincipals(cx, obj) == NULL) {
            JSPrincipals *principals;
            principals = lm_GetInnermostPrincipals(cx, obj, NULL);
            if (principals == NULL)
                return JS_FALSE;
            lm_SetContainerPrincipals(cx, obj, principals);
        }
    }
    return JS_TRUE;
}

752 JSBool win_check_access(JSContext *cx, JSObject *obj, jsval id,
753                         JSAccessMode mode, jsval *vp)
754 {
755     if(mode == JSACC_PARENT)  {
756         return lm_CheckSetParentSlot(cx, obj, id, vp);
757     }
758     return JS_TRUE;
759 }
760 
761 JSClass lm_window_class = {
762     "Window", JSCLASS_HAS_PRIVATE,
763     JS_PropertyStub, JS_PropertyStub, win_getProperty, win_setProperty,
764     win_list_properties, win_resolve_name, JS_ConvertStub, win_finalize,
765     NULL, win_check_access
766 };
#endif
