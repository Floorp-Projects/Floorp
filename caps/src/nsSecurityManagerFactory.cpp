/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*Factory for internal browser security resource managers*/

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsAggregatePrincipal.h"
#include "nsCertificatePrincipal.h"
#include "nsCodebasePrincipal.h"
#include "nsSystemPrincipal.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptContext.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

///////////////////////
// nsSecurityNameSet //
///////////////////////

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

nsSecurityNameSet::nsSecurityNameSet()
{
    NS_INIT_REFCNT();
}

nsSecurityNameSet::~nsSecurityNameSet()
{
}

NS_IMPL_ISUPPORTS1(nsSecurityNameSet, nsIScriptExternalNameSet)

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
        nsCOMPtr<nsIScriptSecurityManager> securityManager = 
                 do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            //            NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

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
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->EnableCapability(cap);
    if (NS_FAILED(rv))
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
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->DisableCapability(cap);
    if (NS_FAILED(rv))
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
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->RevertCapability(cap);
    if (NS_FAILED(rv))
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
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->SetCanEnableCapability(principalID, cap, 
                                                 nsIPrincipal::ENABLE_GRANTED);
    if (NS_FAILED(rv))
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
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->SetCanEnableCapability(principalID, 
                                                 nsBasePrincipal::Invalid,
                                                 nsIPrincipal::ENABLE_GRANTED);
    if (NS_FAILED(rv))
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
nsSecurityNameSet::InitializeNameSet(nsIScriptContext* aScriptContext)
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



NS_GENERIC_FACTORY_CONSTRUCTOR(nsAggregatePrincipal)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCertificatePrincipal)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCodebasePrincipal)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecurityNameSet)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSystemPrincipal,
    nsScriptSecurityManager::SystemPrincipalSingletonConstructor)


NS_DECL_CLASSINFO(nsAggregatePrincipal)
NS_DECL_CLASSINFO(nsCertificatePrincipal)
NS_DECL_CLASSINFO(nsCodebasePrincipal)
NS_DECL_CLASSINFO(nsSystemPrincipal)


static NS_IMETHODIMP
Construct_nsIScriptSecurityManager(nsISupports *aOuter, REFNSIID aIID, 
                                   void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;
    *aResult = nsnull;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsScriptSecurityManager *obj = nsScriptSecurityManager::GetScriptSecurityManager();
    if (!obj) 
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(obj->QueryInterface(aIID, aResult)))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

static NS_METHOD 
RegisterSecurityNameSet(nsIComponentManager *aCompMgr,
                        nsIFile *aPath,
                        const char *registryLocation,
                        const char *componentType,
                        const nsModuleComponentInfo *info)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);

    if (NS_FAILED(rv))
        return rv;

    nsXPIDLCString previous;
    rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY,
                                  "PrivilegeManager",
                                  NS_SECURITYNAMESET_CONTRACTID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
    NS_ENSURE_SUCCESS(rv, rv);

    return rv;
}


static nsModuleComponentInfo components[] =
{
    { NS_SCRIPTSECURITYMANAGER_CLASSNAME, 
      NS_SCRIPTSECURITYMANAGER_CID, 
      NS_SCRIPTSECURITYMANAGER_CONTRACTID,
      Construct_nsIScriptSecurityManager,
      RegisterSecurityNameSet,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsIClassInfo::MAIN_THREAD_ONLY
    },

    { NS_AGGREGATEPRINCIPAL_CLASSNAME, 
      NS_AGGREGATEPRINCIPAL_CID, 
      NS_AGGREGATEPRINCIPAL_CONTRACTID,
      nsAggregatePrincipalConstructor,
      nsnull,
      nsnull,
      nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsAggregatePrincipal),
      nsnull,
      &NS_CLASSINFO_NAME(nsAggregatePrincipal),
      nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::EAGER_CLASSINFO
    },

    { NS_CERTIFICATEPRINCIPAL_CLASSNAME, 
      NS_CERTIFICATEPRINCIPAL_CID, 
      NS_CERTIFICATEPRINCIPAL_CONTRACTID,
      nsCertificatePrincipalConstructor,
      nsnull,
      nsnull,
      nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsCertificatePrincipal),
      nsnull,
      &NS_CLASSINFO_NAME(nsCertificatePrincipal),
      nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::EAGER_CLASSINFO
    },

    { NS_CODEBASEPRINCIPAL_CLASSNAME, 
      NS_CODEBASEPRINCIPAL_CID, 
      NS_CODEBASEPRINCIPAL_CONTRACTID,
      nsCodebasePrincipalConstructor,
      nsnull,
      nsnull,
      nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsCodebasePrincipal),
      nsnull,
      &NS_CLASSINFO_NAME(nsCodebasePrincipal),
      nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::EAGER_CLASSINFO
    },

    { NS_SYSTEMPRINCIPAL_CLASSNAME, 
      NS_SYSTEMPRINCIPAL_CID, 
      NS_SYSTEMPRINCIPAL_CONTRACTID,
      nsSystemPrincipalConstructor,
      nsnull,
      nsnull,
      nsnull,
      NS_CI_INTERFACE_GETTER_NAME(nsSystemPrincipal),
      nsnull,
      &NS_CLASSINFO_NAME(nsSystemPrincipal),
      nsIClassInfo::SINGLETON | nsIClassInfo::MAIN_THREAD_ONLY |
      nsIClassInfo::EAGER_CLASSINFO
    },

    { "Security Script Name Set",
      NS_SECURITYNAMESET_CID,
      NS_SECURITYNAMESET_CONTRACTID,
      nsSecurityNameSetConstructor,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsIClassInfo::MAIN_THREAD_ONLY
    }
};


NS_IMPL_NSGETMODULE(nsSecurityManagerModule, components);

