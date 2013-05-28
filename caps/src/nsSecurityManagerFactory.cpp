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
#include "xpcprivate.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

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

static JSBool
netscape_security_enablePrivilege(JSContext *cx, unsigned argc, JS::Value *vp)
{
    Telemetry::Accumulate(Telemetry::ENABLE_PRIVILEGE_EVER_CALLED, true);
    return xpc::EnableUniversalXPConnect(cx);
}

static const JSFunctionSpec PrivilegeManager_static_methods[] = {
    JS_FS("enablePrivilege", netscape_security_enablePrivilege, 1, 0),
    JS_FS_END
};

/*
 * "Steal" calls to netscape.security.PrivilegeManager.enablePrivilege,
 * et al. so that code that worked with 4.0 can still work.
 */
NS_IMETHODIMP
nsSecurityNameSet::InitializeNameSet(nsIScriptContext* aScriptContext)
{
    AutoJSContext cx;
    JS::Rooted<JSObject*> global(cx, aScriptContext->GetNativeGlobal());
    JSAutoCompartment ac(cx, global);

    /*
     * Find Object.prototype's class by walking up the global object's
     * prototype chain.
     */
    JS::Rooted<JSObject*> obj(cx, global);
    JS::Rooted<JSObject*> proto(cx);
    for (;;) {
        MOZ_ALWAYS_TRUE(JS_GetPrototype(cx, obj, proto.address()));
        if (!proto)
            break;
        obj = proto;
    }
    JSClass *objectClass = JS_GetClass(obj);

    JS::Rooted<JS::Value> v(cx);
    if (!JS_GetProperty(cx, global, "netscape", v.address()))
        return NS_ERROR_FAILURE;

    JS::Rooted<JSObject*> securityObj(cx);
    if (v.isObject()) {
        /*
         * "netscape" property of window object exists; get the
         * "security" property.
         */
        obj = &v.toObject();
        if (!JS_GetProperty(cx, obj, "security", v.address()) || !v.isObject())
            return NS_ERROR_FAILURE;
        securityObj = &v.toObject();
    } else {
        /* define netscape.security object */
        obj = JS_DefineObject(cx, global, "netscape", objectClass, nullptr, 0);
        if (obj == nullptr)
            return NS_ERROR_FAILURE;
        securityObj = JS_DefineObject(cx, obj, "security", objectClass,
                                      nullptr, 0);
        if (securityObj == nullptr)
            return NS_ERROR_FAILURE;
    }

    // We hide enablePrivilege behind a pref because it has been altered in a
    // way that makes it fundamentally insecure to use in production. Mozilla
    // uses this pref during automated testing to support legacy test code that
    // uses enablePrivilege. If you're not doing test automation, you _must_ not
    // flip this pref, or you will be exposing all your users to security
    // vulnerabilities.
    if (!Preferences::GetBool("security.turn_off_all_security_so_that_viruses_can_take_over_this_computer"))
        return NS_OK;

    /* Define PrivilegeManager object with the necessary "static" methods. */
    obj = JS_DefineObject(cx, securityObj, "PrivilegeManager", objectClass,
                          nullptr, 0);
    if (obj == nullptr)
        return NS_ERROR_FAILURE;

    return JS_DefineFunctions(cx, obj, PrivilegeManager_static_methods)
           ? NS_OK
           : NS_ERROR_FAILURE;
}
