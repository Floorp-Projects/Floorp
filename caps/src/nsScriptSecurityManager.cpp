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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID, 
                     NS_SCRIPT_NAMESET_REGISTRY_CID);

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
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
	PRBool enabled;
    if (NS_FAILED(prefs->GetBoolPref("security.checkuri", &enabled)) ||
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
        return NS_ERROR_DOM_BAD_URI;
    }
    return NS_OK;
}

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

NS_IMETHODIMP
nsScriptSecurityManager::HasSubjectPrincipal(PRBool *result)
{
    *result = GetCurrentContext() != nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetSubjectPrincipal(nsIPrincipal **result)
{
    JSContext *cx = GetCurrentContext();
    if (!cx)
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
nsScriptSecurityManager::IsCapabilityEnabled(const char *capability,
                                             PRBool *result)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::EnableCapability(const char *capability)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::RevertCapability(const char *capability)
{
    return NS_ERROR_FAILURE;    // not yet implemented
}

NS_IMETHODIMP
nsScriptSecurityManager::DisableCapability(const char *capability)
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
    : mSystemPrincipal(nsnull)
{
    NS_INIT_REFCNT();
}

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
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
nsScriptSecurityManager::GetSubjectPrincipal(JSContext *aCx, 
                                             nsIPrincipal **result)
{
    // Get principals from innermost frame of JavaScript or Java.
    JSPrincipals *principals;
    JSStackFrame *fp;
    JSScript *script;
    fp = nsnull; // indicate to JS_FrameIterator to start from innermost frame
    JSStackFrame *pFrameToStartLooking = JS_FrameIterator(aCx, &fp);
    JSStackFrame *pFrameToEndLooking   = nsnull;
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
    // Couldn't find principals: no mobile code on stack.
    // Use system principal.
    return GetSystemPrincipal(result);
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
    JS_ReportError(aCx, "access disallowed from scripts at %s to documents "
                        "at another domain", spec);
    nsCRT::free(spec);
    *aResult = PR_FALSE;
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
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
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv))
		return NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(prefs->CopyCharPref(tmp_prop_name, &secLevelString)) &&
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
		nsresult rv;
		NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
		if (NS_FAILED(rv) ||
            NS_FAILED(prefs->CopyCharPref("javascript.security_policy", &policy_str)))
		{
            policy_str = PL_strdup("default");
		}
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
    nsCOMPtr<nsIPrincipal> subject;
    PRBool ok = PR_FALSE;
    if (NS_SUCCEEDED(GetSubjectPrincipal(aJSContext, getter_AddRefs(subject))))
        ok = PR_TRUE;
    if (!ok || NS_FAILED(subject->CanAccess("UniversalXPConnect", &ok)))
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


///////////////////////
// nsSecurityNameSet //
///////////////////////

nsSecurityNameSet::nsSecurityNameSet()
{
  NS_INIT_REFCNT();
}

nsSecurityNameSet::~nsSecurityNameSet()
{
}

NS_IMPL_ISUPPORTS(nsSecurityNameSet, NS_GET_IID(nsIScriptExternalNameSet));

NS_IMETHODIMP 
nsSecurityNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

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
nsSecurityNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    JSContext *cx = (JSContext *) aScriptContext->GetNativeContext();
    JSObject *global = JS_GetGlobalObject(cx);

    /*
     * Find Object.prototype's class by walking up the global object's
     * prototype chain.
     */
    JSObject *obj = global;
    JSObject *proto;
    while (proto = JS_GetPrototype(cx, obj))
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

