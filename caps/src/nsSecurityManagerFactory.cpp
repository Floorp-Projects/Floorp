/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "jsfriendapi.h"

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

static JSString *
getStringArgument(JSContext *cx, JSObject *obj, PRUint16 argNum, unsigned argc, jsval *argv)
{
    if (argc <= argNum || !JSVAL_IS_STRING(argv[argNum])) {
        JS_ReportError(cx, "String argument expected");
        return nsnull;
    }

    /*
     * We don't want to use JS_ValueToString because we want to be able
     * to have an object to represent a target in subsequent versions.
     */
    return JSVAL_TO_STRING(argv[argNum]);
}

static bool
getBytesArgument(JSContext *cx, JSObject *obj, PRUint16 argNum, unsigned argc, jsval *argv,
                 JSAutoByteString *bytes)
{
    JSString *str = getStringArgument(cx, obj, argNum, argc, argv);
    return str && bytes->encode(cx, str);
}

static JSBool
netscape_security_enablePrivilege(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    JSAutoByteString cap;
    if (!getBytesArgument(cx, obj, 0, argc, JS_ARGV(cx, vp), &cap))
        return JS_FALSE;

    // Can't use nsContentUtils::GetDocumentFromCaller because that
    // depends on various XPConnect stuff that's not set up here.
    {
        JSAutoEnterCompartment ac;
        if (ac.enter(cx, obj)) {
            nsCOMPtr<nsPIDOMWindow> win =
                do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(cx, obj));
            if (win) {
                nsCOMPtr<nsIDocument> doc =
                    do_QueryInterface(win->GetExtantDocument());
                if (doc) {
                    doc->WarnOnceAbout(nsIDocument::eEnablePrivilege);
                }
            }
        }
    }

    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return JS_FALSE;

    //    NS_ASSERTION(cx == GetCurrentContext(), "unexpected context");

    rv = securityManager->EnableCapability(cap.ptr());
    if (NS_FAILED(rv))
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec PrivilegeManager_static_methods[] = {
    { "enablePrivilege",    netscape_security_enablePrivilege,      1,0},
    {nsnull,nsnull,0,0}
};

/*
 * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
 * et al. so that code that worked with 4.0 can still work.
 */
NS_IMETHODIMP 
nsSecurityNameSet::InitializeNameSet(nsIScriptContext* aScriptContext)
{
    JSContext* cx = aScriptContext->GetNativeContext();
    JSObject *global = JS_ObjectToInnerObject(cx, JS_GetGlobalObject(cx));

    /*
     * Find Object.prototype's class by walking up the global object's
     * prototype chain.
     */
    JSObject *obj = global;
    JSObject *proto;
    JSAutoRequest ar(cx);
    while ((proto = JS_GetPrototype(obj)) != nsnull)
        obj = proto;
    JSClass *objectClass = JS_GetClass(obj);

    JS::Value v;
    if (!JS_GetProperty(cx, global, "netscape", &v))
        return NS_ERROR_FAILURE;

    JSObject *securityObj;
    if (v.isObject()) {
        /*
         * "netscape" property of window object exists; get the
         * "security" property.
         */
        obj = &v.toObject();
        if (!JS_GetProperty(cx, obj, "security", &v) || !v.isObject())
            return NS_ERROR_FAILURE;
        securityObj = &v.toObject();
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
