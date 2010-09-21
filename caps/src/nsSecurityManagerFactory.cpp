/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*Factory for internal browser security resource managers*/

#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsPrincipal.h"
#include "nsSystemPrincipal.h"
#include "nsNullPrincipal.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "nsIClassInfoImpl.h"

///////////////////////
// nsSecurityNameSet //
///////////////////////

nsSecurityNameSet::nsSecurityNameSet()
{
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

static void
getUTF8StringArgument(JSContext *cx, JSObject *obj, PRUint16 argNum,
                      uintN argc, jsval *argv, nsCString& aRetval)
{
    if (argc <= argNum || !JSVAL_IS_STRING(argv[argNum])) {
        JS_ReportError(cx, "String argument expected");
        aRetval.Truncate();
        return;
    }

    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     */
    JSString *str = JSVAL_TO_STRING(argv[argNum]);
    if (!str) {
        aRetval.Truncate();
        return;
    }

    PRUnichar *data = (PRUnichar*)JS_GetStringChars(str);
    CopyUTF16toUTF8(data, aRetval);
}

static JSBool
netscape_security_isPrivilegeEnabled(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    JSBool result = JS_FALSE;
    char *cap = getStringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp));
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
    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(result));
    return JS_TRUE;
}


static JSBool
netscape_security_enablePrivilege(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    char *cap = getStringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp));
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
netscape_security_disablePrivilege(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    char *cap = getStringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp));
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
netscape_security_revertPrivilege(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    char *cap = getStringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp));
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
netscape_security_setCanEnablePrivilege(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    if (argc < 2) return JS_FALSE;
    nsCAutoString principalFingerprint;
    getUTF8StringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp), principalFingerprint);
    char *cap = getStringArgument(cx, obj, 1, argc, JS_ARGV(cx, vp));
    if (principalFingerprint.IsEmpty() || !cap)
        return JS_FALSE;

    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->SetCanEnableCapability(principalFingerprint, cap, 
                                                 nsIPrincipal::ENABLE_GRANTED);
    if (NS_FAILED(rv))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
netscape_security_invalidate(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsCAutoString principalFingerprint;
    getUTF8StringArgument(cx, obj, 0, argc, JS_ARGV(cx, vp), principalFingerprint);
    if (principalFingerprint.IsEmpty())
        return JS_FALSE;

    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->SetCanEnableCapability(principalFingerprint,
                                                 nsPrincipal::sInvalid,
                                                 nsIPrincipal::ENABLE_GRANTED);
    if (NS_FAILED(rv))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { "isPrivilegeEnabled", netscape_security_isPrivilegeEnabled,   1,0},
    { "enablePrivilege",    netscape_security_enablePrivilege,      1,0},
    { "disablePrivilege",   netscape_security_disablePrivilege,     1,0},
    { "revertPrivilege",    netscape_security_revertPrivilege,      1,0},
    //-- System Cert Functions
    { "setCanEnablePrivilege", netscape_security_setCanEnablePrivilege,
                                                                    2,0},
    { "invalidate",            netscape_security_invalidate,        1,0},
    {nsnull,nsnull,0,0}
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
    JSAutoRequest ar(cx);
    while ((proto = JS_GetPrototype(cx, obj)) != nsnull)
        obj = proto;
    JSClass *objectClass = JS_GET_CLASS(cx, obj);

    jsval v;
    if (!JS_GetProperty(cx, global, "netscape", &v))
        return NS_ERROR_FAILURE;
    JSObject *securityObj;
    if (JSVAL_IS_OBJECT(v)) {
        /*
         * "netscape" property of window object exists; get the
         * "security" property.
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
