/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Mitch Stoltz
 * Steve Morse
 */
#include "nsScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectOwner.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsIJARURI.h"
#include "nspr.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsJSPrincipals.h"
#include "nsSystemPrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsAggregatePrincipal.h"
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
#include "nsIXPConnect.h"
#include "nsIXPCSecurityManager.h"
#include "nsTextFormatter.h"
#include "nsIIOService.h"
#include "nsIStringBundle.h"
#include "nsINetSupportDialogService.h"
#include "nsNetUtil.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIFile.h"
#include "nsIZipReader.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID, 
                     NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

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

static JSContext *
GetSafeContext() {
    // Get the "safe" JSContext: our JSContext of last resort
    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, stack, "nsThreadJSContextStack", 
                    &rv);
    if (NS_FAILED(rv))
        return nsnull;
    nsCOMPtr<nsIThreadJSContextStack> tcs = do_QueryInterface(stack);
    JSContext *cx;
    if (NS_FAILED(tcs->GetSafeJSContext(&cx)))
        return nsnull;
    return cx;
}



static nsDOMProp 
findDomProp(const char *propName, int n);

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
getStringArgument(JSContext *cx, JSObject *obj, PRUint16 argNum, uintN argc, jsval *argv)
{
    if (argc <= argNum || !JSVAL_IS_STRING(argv[argNum])) {
        JS_ReportError(cx, "String argument expected");
        return nsnull;
    }
    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     */
    JSString *str = JSVAL_TO_STRING(argv[argNum]);
    if (!str)
        return nsnull;

    return JS_GetStringBytes(str);
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_isPrivilegeEnabled(JSContext *cx, JSObject *obj, uintN argc,
                                     jsval *argv, jsval *rval)
{
    JSBool result = JS_FALSE;
    char *cap = getStringArgument(cx, obj, 0, argc, argv);
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
    char *cap = getStringArgument(cx, obj, 0, argc, argv);
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
    char *cap = getStringArgument(cx, obj, 0, argc, argv);
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
    char *cap = getStringArgument(cx, obj, 0, argc, argv);
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

PR_STATIC_CALLBACK(JSBool)
netscape_security_setCanEnablePrivilege(JSContext *cx, JSObject *obj, uintN argc,
                                        jsval *argv, jsval *rval)
{
    if (argc < 2) return JS_FALSE;
    char *principalID = getStringArgument(cx, obj, 0, argc, argv);
    char *cap = getStringArgument(cx, obj, 1, argc, argv);
    if (!principalID || !cap)
        return JS_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;
    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
    if (NS_FAILED(securityManager->SetCanEnableCapability(principalID, cap, 
                                                          nsIPrincipal::ENABLE_GRANTED)))
        return JS_FALSE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
netscape_security_invalidate(JSContext *cx, JSObject *obj, uintN argc,
                             jsval *argv, jsval *rval)
{
    char *principalID = getStringArgument(cx, obj, 0, argc, argv);
    if (!principalID)
        return JS_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;
    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");
    if (NS_FAILED(securityManager->SetCanEnableCapability(principalID, 
                                                          nsBasePrincipal::Invalid,
                                                          nsIPrincipal::ENABLE_GRANTED)))
        return JS_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { "isPrivilegeEnabled", netscape_security_isPrivilegeEnabled,   1},
    { "enablePrivilege",    netscape_security_enablePrivilege,      1},
    { "disablePrivilege",   netscape_security_disablePrivilege,     1},
    { "revertPrivilege",    netscape_security_revertPrivilege,      1},
    //-- System Cert Functions
    { "setCanEnablePrivilege", netscape_security_setCanEnablePrivilege,   2},
    { "invalidate",            netscape_security_invalidate,              1},
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

NS_IMPL_THREADSAFE_ISUPPORTS2(nsScriptSecurityManager,
                   nsIScriptSecurityManager,
                   nsIXPCSecurityManager)

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
                                           PRBool isWrite)
{
    nsDOMProp domProp = (nsDOMProp) domPropInt;
    if (!GetBit(hasPolicyVector, domPropInt)) {
        // No policy for this DOM property, so just allow access.
        return NS_OK;
    }
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(GetSubjectPrincipal(cx, getter_AddRefs(principal)))) {
        return NS_ERROR_FAILURE;
    }
    PRBool equals;
    if (!principal || 
        NS_SUCCEEDED(principal->Equals(mSystemPrincipal, &equals)) && equals) 
    {
        // We have native code or the system principal: just allow access
        return NS_OK;
    }
    nsCAutoString capability;
    PRInt32 secLevel = GetSecurityLevel(principal, domProp, isWrite,
                                        capability);
    switch (secLevel) {
      case SCRIPT_SECURITY_UNDEFINED_ACCESS:
        // If no preference is defined for this property, allow access. 
        // This violates the rule of a safe default, but means we don't have
        // to specify the large majority of unchecked properties, only the
        // minority of checked ones.
      case SCRIPT_SECURITY_ALL_ACCESS:
        return NS_OK;
      case SCRIPT_SECURITY_SAME_DOMAIN_ACCESS: {
        const char *cap = isWrite  
                          ? "UniversalBrowserWrite" 
                          : "UniversalBrowserRead";
        return CheckPermissions(cx, (JSObject *) aObj, cap);
      }
      case SCRIPT_SECURITY_CAPABILITY_ONLY: {
        PRBool capabilityEnabled = PR_FALSE;
        nsresult rv = IsCapabilityEnabled(capability, &capabilityEnabled);
        if (NS_FAILED(rv) || !capabilityEnabled)
            return NS_ERROR_DOM_SECURITY_ERR;
        return NS_OK;
      }
      default:
        // Default is no access
        return NS_ERROR_DOM_SECURITY_ERR;
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
    if (NS_SUCCEEDED(CheckLoadURI(uri, aURI, PR_FALSE)))
        return NS_OK;

    // See if we're attempting to load a file: URI. If so, let a 
    // UniversalFileRead capability trump the above check.
    nsXPIDLCString scheme;
    if (NS_FAILED(aURI->GetScheme(getter_Copies(scheme))))
        return NS_ERROR_FAILURE;
    if (nsCRT::strcasecmp(scheme, "file") == 0 ||
        nsCRT::strcasecmp(scheme, "resource") == 0) 
    {
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
nsScriptSecurityManager::CheckLoadURI(nsIURI *aFromURI, nsIURI *aURI,
                                      PRBool aDisallowFromMail)
{
    nsCOMPtr<nsIJARURI> jarURI;
    nsCOMPtr<nsIURI> uri = aFromURI;
    while(uri && NS_SUCCEEDED(uri->QueryInterface(NS_GET_IID(nsIJARURI),
                                                  getter_AddRefs(jarURI))))
        jarURI->GetJARFile(getter_AddRefs(uri));
    if (!uri) return NS_ERROR_FAILURE;

    nsXPIDLCString fromScheme;
    if (NS_FAILED(uri->GetScheme(getter_Copies(fromScheme))))
        return NS_ERROR_FAILURE;

    if (aDisallowFromMail && 
        (nsCRT::strcasecmp(fromScheme, "mailbox")  == 0 ||
         nsCRT::strcasecmp(fromScheme, "imap")     == 0 ||
         nsCRT::strcasecmp(fromScheme, "news")     == 0))
    {
        return NS_ERROR_DOM_BAD_URI;
    }

    uri = aURI;
    while(uri && NS_SUCCEEDED(uri->QueryInterface(NS_GET_IID(nsIJARURI),
                                                  getter_AddRefs(jarURI))))
        jarURI->GetJARFile(getter_AddRefs(uri));
    if (!uri) return NS_ERROR_FAILURE;

    nsXPIDLCString scheme;
    if (NS_FAILED(uri->GetScheme(getter_Copies(scheme))))
        return NS_ERROR_FAILURE;
    
    if (nsCRT::strcasecmp(scheme, fromScheme) == 0)
    {
        // every scheme can access another URI from the same scheme
        return NS_OK;
    }

    enum Action { AllowProtocol, DenyProtocol };
    struct { 
        const char *name;
        Action action;
    } protocolList[] = {
        { "about",           AllowProtocol },
        { "data",            AllowProtocol },
        { "file",            DenyProtocol  },
        { "ftp",             AllowProtocol },
        { "http",            AllowProtocol },
        { "https",           AllowProtocol },
        { "keyword",         DenyProtocol  },
        { "res",             DenyProtocol  },
        { "resource",        DenyProtocol  },
        { "datetime",        DenyProtocol  },
        { "finger",          AllowProtocol },
        { "chrome",          DenyProtocol  },
        { "javascript",      AllowProtocol },
        { "mailto",          AllowProtocol },
        { "imap",            DenyProtocol  },
        { "mailbox",         DenyProtocol  },
        { "pop3",            DenyProtocol  },
        { "pop",             AllowProtocol },
        { "news",            AllowProtocol },
    };

    for (unsigned i=0; i < sizeof(protocolList)/sizeof(protocolList[0]); i++) {
        if (nsCRT::strcasecmp(scheme, protocolList[i].name) == 0) {
            switch (protocolList[i].action) {
            case AllowProtocol:
                // everyone can access these schemes.
                return NS_OK;
            case DenyProtocol:
                // Deny access
                return NS_ERROR_DOM_BAD_URI;
            }
        }
    }

    // If we reach here, we have an unknown protocol. Warn, but allow.
    // This is risky from a security standpoint, but allows flexibility
    // in installing new protocol handlers after initial ship.
    NS_WARN_IF_FALSE(PR_FALSE, "unknown protocol");

    return NS_OK;
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
nsScriptSecurityManager::GetCertificatePrincipal(const char* aCertID,
                                                 nsIPrincipal **result)
{
    nsresult rv;
    //-- Create a certificate principal
    nsCertificatePrincipal *certificate = new nsCertificatePrincipal();
    if (!certificate)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(certificate);
    if (NS_FAILED(certificate->Init(aCertID)))
    {
        NS_RELEASE(certificate);
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrincipal> principal = do_QueryInterface((nsBasePrincipal*)certificate, &rv);
    NS_RELEASE(certificate);
    if (NS_FAILED(rv)) return rv;

    if (mPrincipals) {
        // Check to see if we already have this principal.
        nsIPrincipalKey key(principal);
        nsCOMPtr<nsIPrincipal> fromTable = (nsIPrincipal *) mPrincipals->Get(&key);
        if (fromTable) 
            principal = fromTable;
    }

    //-- Bundle this certificate principal into an aggregate principal
    nsAggregatePrincipal* agg = new nsAggregatePrincipal();
    if (!agg) return NS_ERROR_OUT_OF_MEMORY;
    rv = agg->SetCertificate(principal);
    if (NS_FAILED(rv)) return rv;
    principal = do_QueryInterface((nsBasePrincipal*)agg, &rv);
    if (NS_FAILED(rv)) return rv;

    *result = principal;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsScriptSecurityManager::GetCodebasePrincipal(nsIURI *aURI, 
                                              nsIPrincipal **result)
{
    nsresult rv;
    nsCodebasePrincipal *codebase = new nsCodebasePrincipal();
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(codebase);
    if (NS_FAILED(codebase->Init(aURI))) {
        NS_RELEASE(codebase);
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrincipal> principal = 
      do_QueryInterface((nsBasePrincipal*)codebase, &rv);
    NS_RELEASE(codebase);
    if (NS_FAILED(rv)) return rv;

    if (mPrincipals) {
        // Check to see if we already have this principal.
        nsIPrincipalKey key(principal);
        nsCOMPtr<nsIPrincipal> fromTable = (nsIPrincipal *) mPrincipals->Get(&key);
        if (fromTable) 
            principal = fromTable;
    }

    //-- Bundle this codebase principal into an aggregate principal
    nsAggregatePrincipal* agg = new nsAggregatePrincipal();
    if (!agg) return NS_ERROR_OUT_OF_MEMORY;
    rv = agg->SetCodebase(principal);
    if (NS_FAILED(rv)) return rv;
    principal = do_QueryInterface((nsBasePrincipal*)agg, &rv);
    if (NS_FAILED(rv)) return rv;

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
        nsCAutoString capability;
        PRInt32 secLevel = GetSecurityLevel(principal, 
                                            NS_DOM_PROP_JAVASCRIPT_ENABLED, 
                                            PR_FALSE, capability);
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
        if (nsCRT::strcasecmp(scheme, "imap") == 0 || 
            nsCRT::strcasecmp(scheme, "mailbox") == 0 ||
            nsCRT::strcasecmp(scheme, "news") == 0) 
        {
            *result = mIsMailJavaScriptEnabled;
            return NS_OK;
        }
    }
    *result = mIsJavaScriptEnabled;
    return NS_OK;
}


NS_IMETHODIMP
nsScriptSecurityManager::CanExecuteFunction(void *jsFuncObj,
                                            PRBool *result)
{
    JSContext *cx = GetCurrentContext();
    if (!cx) {
        cx = GetSafeContext();
        if (!cx) {
            return NS_ERROR_UNEXPECTED;
        }
    }
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = GetFunctionObjectPrincipal(cx, (JSObject *) jsFuncObj, 
                                             getter_AddRefs(principal));
    if (NS_FAILED(rv))
        return rv;
    if (!principal) {
        *result = PR_FALSE;
        return NS_OK;
    }
    return CanExecuteScripts(principal, result);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetScriptPrincipal(JSContext *cx, 
                                            JSScript *script,
                                            nsIPrincipal **result) 
{
    if (!script) {
        *result = nsnull;
        return NS_OK;
    }
    JSPrincipals *jsp = JS_GetScriptPrincipals(cx, script);
    if (!jsp) {
        // Script didn't have principals -- shouldn't happen.
        return NS_ERROR_FAILURE;
    }
    nsJSPrincipals *nsJSPrin = NS_STATIC_CAST(nsJSPrincipals *, jsp);
    *result = nsJSPrin->nsIPrincipalPtr;
    if (!result)
        return NS_ERROR_FAILURE;
    NS_ADDREF(*result);
    return NS_OK;

}

NS_IMETHODIMP
nsScriptSecurityManager::GetFunctionObjectPrincipal(JSContext *cx, 
                                                    JSObject *obj,
                                                    nsIPrincipal **result) 
{
    JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, obj);
    if (JS_GetFunctionObject(fun) != obj) {
        // Function has been cloned; get principals from scope
        return GetObjectPrincipal(cx, obj, result);
    }
    JSScript *script = JS_GetFunctionScript(cx, fun);
    return GetScriptPrincipal(cx, script, result);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetFramePrincipal(JSContext *cx, 
                                           JSStackFrame *fp,
                                           nsIPrincipal **result) 
{
    JSObject *obj = JS_GetFrameFunctionObject(cx, fp);
    if (!obj) {
        // Must be in a top-level script. Get principal from the script.
        JSScript *script = JS_GetFrameScript(cx, fp);
        return GetScriptPrincipal(cx, script, result);
    } 
    return GetFunctionObjectPrincipal(cx, obj, result);
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
    // If this capability check is being called as a result of the security
    // manager accessing a security preference, allow execution.
    if (mIsAccessingPrefs && 
        (PL_strcmp(capability, "SecurityPreferencesAccess") == 0))
    {
        *result = PR_TRUE;
        return NS_OK;
    }
    do {
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_FAILED(GetFramePrincipal(cx, fp, getter_AddRefs(principal)))) {
            return NS_ERROR_FAILURE;
        }
        if (!principal)
            continue;

        // First check if the principal is even able to enable the 
        // given capability. If not, don't look any further.
        PRInt16 canEnable;
        rv = principal->CanEnableCapability(capability, &canEnable);
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
        rv = principal->IsCapabilityEnabled(capability, annotation, 
                                            result);
        if (NS_FAILED(rv))
            return rv;
        if (*result)
            return NS_OK;
    } while ((fp = JS_FrameIterator(cx, &fp)) != nsnull);
    *result = PR_FALSE;
    return NS_OK;
}

#define PROPERTIES_URL "chrome://communicator/locale/security/security.properties"

nsresult
Localize(char *genericString, nsString &result) 
{
    nsresult ret;
    
    /* create a URL for the string resource file */
    nsIIOService *pNetService = nsnull;
    ret = nsServiceManager::GetService(kIOServiceCID, kIIOServiceIID,
                                       (nsISupports**) &pNetService);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot get net service\n");
        return ret;
    }
    nsIURI *uri = nsnull;
    ret = pNetService->NewURI(PROPERTIES_URL, nsnull, &uri);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot create URI\n");
        nsServiceManager::ReleaseService(kIOServiceCID, pNetService);
        return ret;
    }
    
    nsIURI *url = nsnull;
    ret = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
    nsServiceManager::ReleaseService(kIOServiceCID, pNetService);
    
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot create URL\n");
        return ret;
    }
    
    /* create a bundle for the localization */
    nsIStringBundleService *pStringService = nsnull;
    ret = nsServiceManager::GetService(kStringBundleServiceCID,
        kIStringBundleServiceIID, (nsISupports**) &pStringService);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot get string service\n");
        return ret;
    }
    char *spec = nsnull;
    ret = url->GetSpec(&spec);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot get url spec\n");
        nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
        nsCRT::free(spec);
        return ret;
    }
    nsILocale *locale = nsnull;
    nsIStringBundle *bundle = nsnull;
    ret = pStringService->CreateBundle(spec, locale, &bundle);
    nsCRT::free(spec);
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot create instance\n");
        return ret;
    }
    
    /* localize the given string */
    nsAutoString strtmp;
    strtmp.AssignWithConversion(genericString);

    PRUnichar *ptrv = nsnull;
    ret = bundle->GetStringFromName(strtmp.GetUnicode(), &ptrv);
    NS_RELEASE(bundle);
    if (NS_FAILED(ret)) {
        NS_WARNING("cannot get string from name\n");
    }
    result = ptrv;
    nsCRT::free(ptrv);
    return ret;
}

static PRBool
CheckConfirmDialog(const PRUnichar *szMessage, const PRUnichar *szCheckMessage,
                   PRBool *checkValue) 
{
    nsresult res;  
    NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
    if (NS_FAILED(res)) {
        *checkValue = 0;
        return PR_FALSE;
    }
    
    PRInt32 buttonPressed = 1; /* in case user exits dialog by clicking X */
    nsAutoString yes, no, titleline;
    if (NS_FAILED(res = Localize("Yes", yes)))
        return PR_FALSE;
    if (NS_FAILED(res = Localize("No", no)))
        return PR_FALSE;
    if (NS_FAILED(res = Localize("Titleline", titleline)))
        return PR_FALSE;
    
    res = dialog->UniversalDialog(
        nsnull, /* title message */
        titleline.GetUnicode(), /* title text in top line of window */
        szMessage, /* this is the main message */
        szCheckMessage, /* This is the checkbox message */
        yes.GetUnicode(), /* first button text */
        no.GetUnicode(), /* second button text */
        nsnull, /* third button text */
        nsnull, /* fourth button text */
        nsnull, /* first edit field label */
        nsnull, /* second edit field label */
        nsnull, /* first edit field initial and final value */
        nsnull, /* second edit field initial and final value */
        nsnull,  /* icon: question mark by default */
        checkValue, /* initial and final value of checkbox */
        2, /* number of buttons */
        0, /* number of edit fields */
        0, /* is first edit field a password field */
        &buttonPressed);
    
    if (NS_FAILED(res)) {
        *checkValue = 0;
    }
    if (*checkValue != 0 && *checkValue != 1) {
        *checkValue = 0; /* this should never happen but it is happening!!! */
    }
    return (buttonPressed == 0);
}

NS_IMETHODIMP
nsScriptSecurityManager::GetPrincipalAndFrame(JSContext *cx, 
                                              nsIPrincipal **result, 
                                              JSStackFrame **frameResult) 
{
    // Get principals from innermost frame of JavaScript or Java.
    JSStackFrame *fp = nsnull; // tell JS_FrameIterator to start at innermost
    for (fp = JS_FrameIterator(cx, &fp); fp; fp = JS_FrameIterator(cx, &fp)) {
        if (NS_FAILED(GetFramePrincipal(cx, fp, result))) {
            return NS_ERROR_FAILURE;
        }
        if (*result) {
            *frameResult = fp;
            return NS_OK;
        }
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
        // Prompt user for permission to enable capability.
        static PRBool remember = PR_TRUE;
        nsAutoString query, check;
        if (NS_FAILED(Localize("EnableCapabilityQuery", query)))
            return NS_ERROR_FAILURE;
        if (NS_FAILED(Localize("CheckMessage", check)))
            return NS_ERROR_FAILURE;
        char *source;
        if (NS_FAILED(principal->ToUserVisibleString(&source)))
            return NS_ERROR_FAILURE;
        PRUnichar *message = nsTextFormatter::smprintf(query.GetUnicode(), 
                                                       source);
        Recycle(source);
        if (CheckConfirmDialog(message, check.GetUnicode(), &remember))
            canEnable = nsIPrincipal::ENABLE_GRANTED;
        else
            canEnable = nsIPrincipal::ENABLE_DENIED;
        PR_FREEIF(message);
        if (remember) {
            //-- Save principal to prefs and to mPrincipals
            if (NS_FAILED(principal->SetCanEnableCapability(capability, canEnable)))
                return NS_ERROR_FAILURE;
            if (NS_FAILED(SavePrincipal(principal)))
                return NS_ERROR_FAILURE;
        }
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

NS_IMETHODIMP
nsScriptSecurityManager::SetCanEnableCapability(const char* certificateID, 
                                                const char* capability,
                                                PRInt16 canEnable)
{
    nsresult rv;
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    rv = GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    //-- Get the system certificate
    if (!mSystemCertificate)
    {
        nsCOMPtr<nsIFile> systemCertFile;
        rv = NS_GetSpecialDirectory("xpcom.currentProcess.componentDirectory",
                                    getter_AddRefs(systemCertFile));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        systemCertFile->Append("systemSignature.jar");
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        nsCOMPtr<nsIZipReader> systemCertJar;
        rv = nsComponentManager::CreateInstance(kZipReaderCID, nsnull, 
                                                NS_GET_IID(nsIZipReader),
                                                getter_AddRefs(systemCertJar));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        systemCertJar->Init(systemCertFile);
        rv = systemCertJar->Open();
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        rv = systemCertJar->GetCertificatePrincipal(nsnull, 
                                                    getter_AddRefs(mSystemCertificate));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }

    //-- Make sure the caller's principal is the system certificate
    PRBool isEqual = PR_FALSE;
    if (mSystemCertificate)
    {
        rv = mSystemCertificate->Equals(subjectPrincipal, &isEqual);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }
    if (!isEqual)
    {
        JSContext* cx = GetCurrentContext();
        if (!cx) return NS_ERROR_FAILURE;
		static const char msg1[] = "Only code signed by the system certificate may call SetCanEnableCapability or Invalidate";
        static const char msg2[] = "Attempt to call SetCanEnableCapability or Invalidate when no system certificate has been established";
            JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, 
                                   mSystemCertificate ? msg1 : msg2)));
        return NS_ERROR_FAILURE;
    }

    //-- Get the target principal
    nsCOMPtr<nsIPrincipal> objectPrincipal;
    rv =  GetCertificatePrincipal(certificateID, getter_AddRefs(objectPrincipal));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    rv = objectPrincipal->SetCanEnableCapability(capability, canEnable);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    return SavePrincipal(objectPrincipal);
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
    : mOriginToPolicyMap(nsnull), mPrefs(nsnull), 
      mSystemPrincipal(nsnull), mPrincipals(nsnull), 
      mIsJavaScriptEnabled(PR_FALSE),
      mIsMailJavaScriptEnabled(PR_FALSE),
      mIsAccessingPrefs(PR_FALSE)
{
    NS_INIT_REFCNT();
    memset(hasPolicyVector, 0, sizeof(hasPolicyVector));
    memset(hasDomainPolicyVector, 0, sizeof(hasDomainPolicyVector));
    InitFromPrefs();
}

nsScriptSecurityManager::~nsScriptSecurityManager(void)
{
    delete mOriginToPolicyMap;
    NS_IF_RELEASE(mPrefs);
    NS_IF_RELEASE(mSystemPrincipal);
    delete mPrincipals;
} 

nsScriptSecurityManager *
nsScriptSecurityManager::GetScriptSecurityManager()
{
    static nsScriptSecurityManager *ssecMan = NULL;
    if (!ssecMan) {
        ssecMan = new nsScriptSecurityManager();
        if (!ssecMan)
            return NULL;
	    nsresult rv;
	    NS_WITH_SERVICE(nsIScriptNameSetRegistry, registry, 
                        kCScriptNameSetRegistryCID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsSecurityNameSet* nameSet = new nsSecurityNameSet();
            registry->AddExternalNameSet(nameSet);
        }

        NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
        if (NS_SUCCEEDED(rv) && xpc) {
            rv = xpc->SetDefaultSecurityManager(
                            NS_STATIC_CAST(nsIXPCSecurityManager*, ssecMan), 
                            nsIXPCSecurityManager::HOOK_ALL);
            if (NS_FAILED(rv)) {
                NS_WARNING("failed to install xpconnect security manager!");    
            } 
#ifdef DEBUG_jband
            else {
                printf("!!!!! xpc security manager registered\n");
            }
#endif
        }
        else {
            NS_WARNING("can't get xpconnect to install security manager!");    
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
    JSObject *parent = aObj;
    do {
        JSClass *jsClass = JS_GetClass(aCx, parent);
        const uint32 privateNsISupports = JSCLASS_HAS_PRIVATE | 
                                          JSCLASS_PRIVATE_IS_NSISUPPORTS;
        if (jsClass && (jsClass->flags & (privateNsISupports)) == 
                            privateNsISupports)
        {
            nsISupports *supports = (nsISupports *) JS_GetPrivate(aCx, parent);
            nsCOMPtr<nsIScriptObjectPrincipal> objPrin = 
                do_QueryInterface(supports);
            if (objPrin && NS_SUCCEEDED(objPrin->GetPrincipal(result)))
                return NS_OK;
        }
        parent = JS_GetParent(aCx, parent);
    } while (parent);

    // Couldn't find a principal for this object.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScriptSecurityManager::SavePrincipal(nsIPrincipal* aToSave)
{
    NS_ASSERTION(mPrefs, "nsScriptSecurityManager::mPrefs not initialized");
    nsresult rv;
    nsCOMPtr<nsIPrincipal> persistent = aToSave;
    nsCOMPtr<nsIAggregatePrincipal> aggregate = do_QueryInterface(aToSave, &rv);
    if (NS_SUCCEEDED(rv))
        if (NS_FAILED(aggregate->GetPrimaryChild(getter_AddRefs(persistent))))
            return NS_ERROR_FAILURE;

    //-- Save to mPrincipals
    if (!mPrincipals) 
    {
        mPrincipals = new nsSupportsHashtable(31);
        if (!mPrincipals)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    nsIPrincipalKey key(persistent);
    mPrincipals->Put(&key, persistent);

    //-- Save to prefs
    nsXPIDLCString prefName;
    nsXPIDLCString prefData;
    rv = persistent->ToStreamableForm(getter_Copies(prefName),
                                      getter_Copies(prefData));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    mIsAccessingPrefs = PR_TRUE;
    rv = mPrefs->SetCharPref(prefName, prefData);
    mIsAccessingPrefs = PR_FALSE;
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    return mPrefs->SavePrefFile();
}

NS_IMETHODIMP
nsScriptSecurityManager::CheckPermissions(JSContext *aCx, JSObject *aObj, 
                                          const char *aCapability)
{
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
        return NS_OK;
    }
    nsCOMPtr<nsICodebasePrincipal> subjectCodebase = do_QueryInterface(subject);
    if (subjectCodebase) {
        PRBool isSameOrigin = PR_FALSE;
        if (NS_FAILED(subjectCodebase->SameOrigin(object, &isSameOrigin)))
            return NS_ERROR_FAILURE;

        if (isSameOrigin)
            return NS_OK;
    }

    // Allow access to about:blank
    nsCOMPtr<nsICodebasePrincipal> objectCodebase = do_QueryInterface(object);
    if (objectCodebase) {
        nsXPIDLCString origin;
        if (NS_FAILED(objectCodebase->GetOrigin(getter_Copies(origin))))
            return NS_ERROR_FAILURE;
        if (nsCRT::strcasecmp(origin, "about:blank") == 0) {
            return NS_OK;
        }
    }

    /*
    ** If we failed the origin tests it still might be the case that we
    ** are a signed script and have permissions to do this operation.
    ** Check for that here
    */
    PRBool capabilityEnabled = PR_FALSE;
    if (NS_FAILED(IsCapabilityEnabled(aCapability, &capabilityEnabled)))
        return NS_ERROR_FAILURE;
    if (capabilityEnabled)
        return NS_OK;
    
    /*
    ** Access tests failed, so now report error.
    */
    char *str;
    if (NS_FAILED(subject->ToUserVisibleString(&str)))
        return NS_ERROR_FAILURE;
    JS_ReportError(aCx, "access disallowed from scripts at %s to documents "
                        "at another domain", str);
    nsCRT::free(str);
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

PRInt32 
nsScriptSecurityManager::GetSecurityLevel(nsIPrincipal *principal, 
                                          nsDOMProp domProp, 
                                          PRBool isWrite, 
                                          nsCString &capability)
{
    nsCAutoString prefName;
    if (NS_FAILED(GetPrefName(principal, domProp, prefName)))
        return SCRIPT_SECURITY_NO_ACCESS;
    PRInt32 secLevel;
    char *secLevelString;
	nsresult rv;
    mIsAccessingPrefs = PR_TRUE;
    rv = mPrefs->CopyCharPref(prefName, &secLevelString);
    mIsAccessingPrefs = PR_FALSE;
    if (NS_FAILED(rv)) {
        prefName += (isWrite ? ".write" : ".read");
        rv = mPrefs->CopyCharPref(prefName, &secLevelString);
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
            capability = secLevelString;
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
        // T E M P O R A R Y
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
        // T E M P O R A R Y
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

struct nsDomainEntry {
    nsDomainEntry(const char *anOrigin, const char *aPolicy, 
                  int aPolicyLength) 
        : mNext(nsnull), mOrigin(anOrigin), mPolicy(aPolicy, aPolicyLength)
    { }
    PRBool Matches(const char *anOrigin) {
        int len = nsCRT::strlen(anOrigin);
        int thisLen = mOrigin.Length();
        if (len < thisLen)
            return PR_FALSE;
        if (mOrigin != (anOrigin + (len - thisLen)))
            return PR_FALSE;
        if (len == thisLen)
            return PR_TRUE;
        char charBefore = anOrigin[len-thisLen-1];
        return (charBefore == '.' || charBefore == ':' || charBefore == '/');
    }
    nsDomainEntry *mNext;
    nsCString mOrigin;
    nsCString mPolicy;
};

NS_IMETHODIMP
nsScriptSecurityManager::GetPrefName(nsIPrincipal *principal, 
                                     nsDOMProp domProp, nsCString &result)
{
    static const char *defaultStr = "default";
    result = "security.policy.";
    if (!GetBit(hasDomainPolicyVector, domProp)) {
        result += defaultStr;
    } else {
        PRBool equals = PR_TRUE;
        if (principal && NS_FAILED(principal->Equals(mSystemPrincipal, &equals)))
            return NS_ERROR_FAILURE;
        if (equals) {
            result += defaultStr;
        } else {
            nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
            if (!codebase)
                return NS_ERROR_FAILURE;
            nsresult rv;
            nsXPIDLCString origin;
            if (NS_FAILED(rv = codebase->GetOrigin(getter_Copies(origin))))
                return rv;
            nsCString *policy = nsnull;
            if (mOriginToPolicyMap) {
                const char *s = origin;
                const char *nextToLastDot = nsnull;
                const char *lastDot = nsnull;
                const char *p = s;
                while (*p) {
                    if (*p == '.') {
                        nextToLastDot = lastDot;
                        lastDot = p;
                    }
                    p++;
                }
                nsStringKey key(nextToLastDot ? nextToLastDot+1 : s);
                nsDomainEntry *de = (nsDomainEntry *) mOriginToPolicyMap->Get(&key);
                while (de) {
                    if (de->Matches(s)) {
                        policy = &de->mPolicy;
                        break;
                    }
                    de = de->mNext;
                }
            }
            if (policy)
                result += *policy;
            else
                result += defaultStr;
        }
    }
    result += '.';
    result += domPropNames[domProp];
    return NS_OK;
}

static nsDOMProp 
findDomProp(const char *propName, int n) 
{
    int hi = sizeof(domPropNames)/sizeof(domPropNames[0]) - 1;
    int lo = 0;
    do {
        int mid = (hi + lo) / 2;
        int cmp = PL_strncmp(propName, domPropNames[mid], n);
        if (cmp == 0) {
            if (domPropNames[mid][n] == '\0')
                return (nsDOMProp) mid;
            cmp = -1;
        }
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    } while (hi > lo);
    if (PL_strncmp(propName, domPropNames[lo], n) == 0 &&
        domPropNames[lo][n] == '\0')
    {
        return (nsDOMProp) lo;
    }
    return NS_DOM_PROP_MAX;
}

PR_STATIC_CALLBACK(PRBool)
DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsDomainEntry *entry = (nsDomainEntry *) aData;
    do {
        nsDomainEntry *next = entry->mNext;
        delete entry;
        entry = next;
    } while (entry);
    return PR_TRUE;
}

void 
nsScriptSecurityManager::EnumeratePolicyCallback(const char *prefName, 
                                                 void *data)
{
    if (!prefName || !*prefName)
        return;
    nsScriptSecurityManager *mgr = (nsScriptSecurityManager *) data;
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
            if (!mgr->mOriginToPolicyMap) {
                mgr->mOriginToPolicyMap = 
                    new nsObjectHashtable(nsnull, nsnull, DeleteEntry, nsnull);
                if (!mgr->mOriginToPolicyMap)
                    return;
            }
            char *s;
            if (NS_FAILED(mgr->mPrefs->CopyCharPref(prefName, &s)))
                return;
            char *q=s;
            char *r=s;
            char *lastDot = nsnull;
            char *nextToLastDot = nsnull;
            PRBool working = PR_TRUE;
            while (working) {
                if (*r == ' ' || *r == '\0') {
                    working = (*r != '\0');
                    *r = '\0';
                    nsStringKey key(nextToLastDot ? nextToLastDot+1 : q);
                    nsDomainEntry *value = new nsDomainEntry(q, policyName, 
                                                             policyLength);
                    if (!value)
                        break;
                    nsDomainEntry *de = (nsDomainEntry *) 
                        mgr->mOriginToPolicyMap->Get(&key);
                    if (!de) {
                        mgr->mOriginToPolicyMap->Put(&key, value);
                    } else {
                        if (de->Matches(q)) {
                            value->mNext = de;
                            mgr->mOriginToPolicyMap->Put(&key, value);
                        } else {
                            while (de->mNext) {
                                if (de->mNext->Matches(q)) {
                                    value->mNext = de->mNext;
                                    de->mNext = value;
                                    break;
                                }
                                de = de->mNext;
                            }
                            if (!de->mNext) {
                                de->mNext = value;
                            }
                        }
                    }
                    q = r + 1;
                    lastDot = nextToLastDot = nsnull;
                } else if (*r == '.') {
                    nextToLastDot = lastDot;
                    lastDot = r;
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
            SetBit(mgr->hasPolicyVector, domProp);
            if (!isDefault)
                SetBit(mgr->hasDomainPolicyVector, domProp);
            return;
        }
    } 
    NS_ASSERTION(PR_FALSE, "DOM property name invalid or not found");
}

struct EnumeratePrincipalsInfo {
    // this struct doesn't own these objects; consider them parameters on
    // the stack
    nsSupportsHashtable *ht;
    nsIPref *prefs; 
};

void
nsScriptSecurityManager::EnumeratePrincipalsCallback(const char *prefName, 
                                                     void *voidParam)
{
    EnumeratePrincipalsInfo *info = (EnumeratePrincipalsInfo *) voidParam;
    
    char* data;
    if (NS_FAILED(info->prefs->CopyCharPref(prefName, &data))) 
        return;
    
    static const char certificateName[] = "security.principal.certificate";
    static const char codebaseName[] = "security.principal.codebase";
    nsCOMPtr<nsIPrincipal> principal;
    if (PL_strncmp(prefName, certificateName, 
                   sizeof(certificateName)-1) == 0) 
    {
        nsCertificatePrincipal *certificate = new nsCertificatePrincipal();
        if (certificate) {
            NS_ADDREF(certificate);
            if (NS_SUCCEEDED(certificate->InitFromPersistent(prefName, data))) 
                principal = do_QueryInterface((nsBasePrincipal*)certificate);
            NS_RELEASE(certificate);
        }
    } else if(PL_strncmp(prefName, codebaseName, 
                   sizeof(codebaseName)-1) == 0) 
    {
        nsCodebasePrincipal *codebase = new nsCodebasePrincipal();
        if (codebase) {
            NS_ADDREF(codebase);
            if (NS_SUCCEEDED(codebase->InitFromPersistent(prefName, data))) 
                principal = do_QueryInterface((nsBasePrincipal*)codebase);
            NS_RELEASE(codebase);
        }
    }
    nsCRT::free(data);
    if (principal) {
        nsIPrincipalKey key(principal);
        info->ht->Put(&key, principal);
    }
}

static const char jsEnabledPrefName[] = "javascript.enabled";
static const char jsMailEnabledPrefName[] = "javascript.allow.mailnews";

int
nsScriptSecurityManager::JSEnabledPrefChanged(const char *pref, void *data)
{
    nsScriptSecurityManager *secMgr = (nsScriptSecurityManager *) data;

    if (NS_FAILED(secMgr->mPrefs->GetBoolPref(jsEnabledPrefName, 
                                              &secMgr->mIsJavaScriptEnabled)))
    {
        // Default to enabled.
        secMgr->mIsJavaScriptEnabled = PR_TRUE;
    }

    if (NS_FAILED(secMgr->mPrefs->GetBoolPref(jsMailEnabledPrefName, 
                                              &secMgr->mIsMailJavaScriptEnabled))) 
    {
        // Default to enabled.
        secMgr->mIsMailJavaScriptEnabled = PR_TRUE;
    }

    return 0;
}

int
nsScriptSecurityManager::PrincipalPrefChanged(const char *pref, void *data)
{
    nsScriptSecurityManager *secMgr = (nsScriptSecurityManager *) data;
    if (secMgr->mIsAccessingPrefs)
        return 0;
    EnumeratePrincipalsInfo info;
    info.ht = secMgr->mPrincipals;
    info.prefs = secMgr->mPrefs;
    EnumeratePrincipalsCallback(pref, &info);
    return 0;
}


NS_IMETHODIMP
nsScriptSecurityManager::InitFromPrefs()
{
    // The DOM property enums and names better be in sync
    NS_ASSERTION(NS_DOM_PROP_MAX == sizeof(domPropNames)/sizeof(domPropNames[0]), 
                 "mismatch in property name count");

    // The DOM property names had better be sorted for binary search to work
#ifdef DEBUG
    for (unsigned i=1; i < sizeof(domPropNames)/sizeof(domPropNames[0]); i++) {
        NS_ASSERTION(strcmp(domPropNames[i-1], domPropNames[i]) < 0,
                     "DOM properties are not properly sorted");
    }
#endif 

    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    mPrefs = prefs;
    NS_ADDREF(mPrefs);

    // Set the initial value of the "javascript.enabled" pref
    JSEnabledPrefChanged(jsEnabledPrefName, this);

    // set callbacks in case the value of the pref changes
    prefs->RegisterCallback(jsEnabledPrefName, JSEnabledPrefChanged, this);
    prefs->RegisterCallback(jsMailEnabledPrefName, JSEnabledPrefChanged, this);
    prefs->EnumerateChildren("security.policy", 
                             nsScriptSecurityManager::EnumeratePolicyCallback,
                             (void *) this);

    if (!mPrincipals) {
        mPrincipals = new nsSupportsHashtable(31);
        if (!mPrincipals)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    EnumeratePrincipalsInfo info;
    info.ht = mPrincipals;
    info.prefs = mPrefs;
    prefs->EnumerateChildren("security.principal", 
                             nsScriptSecurityManager::EnumeratePrincipalsCallback,
                             (void *) &info);

    prefs->RegisterCallback("security.principal", PrincipalPrefChanged, this);

    return NS_OK;
}
