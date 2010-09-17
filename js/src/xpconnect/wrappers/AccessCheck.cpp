/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code, released
 * June 24, 2010.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
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

#include "AccessCheck.h"

#include "nsJSPrincipals.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsContentUtils.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"

namespace xpc {

static nsIPrincipal *
GetCompartmentPrincipal(JSCompartment *compartment)
{
    return static_cast<nsJSPrincipals *>(compartment->principals)->nsIPrincipalPtr;
}

bool
AccessCheck::isSameOrigin(JSCompartment *a, JSCompartment *b)
{
    PRBool cond;
    return NS_SUCCEEDED(GetCompartmentPrincipal(a)->Equals(GetCompartmentPrincipal(b), &cond)) &&
           cond;
}

bool
AccessCheck::isChrome(JSCompartment *compartment)
{
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm) {
        return false;
    }

    PRBool privileged;
    nsIPrincipal *principal = GetCompartmentPrincipal(compartment);
    return NS_SUCCEEDED(ssm->IsSystemPrincipal(principal, &privileged)) && privileged;
}

#define NAME(ch, str, cases) case ch: if (!strcmp(name, str)) switch (prop[0]) { cases }; break;
#define PROP(ch, actions) case ch: { actions }; break;
#define RW(str) if (!strcmp(prop, str)) return true;
#define R(str) if (!set && !strcmp(prop, str)) return true;
#define W(str) if (set && !strcmp(prop, str)) return true;

// Hardcoded policy for cross origin property access. This was culled from the
// preferences file (all.js). We don't want users to overwrite highly sensitive
// security policies.
static bool
IsPermitted(const char *name, const char* prop, bool set)
{
    switch(name[0]) {
        NAME('D', "DOMException",
             PROP('c', RW("code"))
             PROP('m', RW("message"))
             PROP('n', RW("name"))
             PROP('r', RW("result"))
             PROP('t', R("toString")))
        NAME('H', "History",
             PROP('b', R("back"))
             PROP('f', R("forward"))
             PROP('g', R("go")))
        NAME('L', "Location",
             PROP('h', W("hash") W("href"))
             PROP('r', R("replace")))
        NAME('N', "Navigator",
             PROP('p', RW("preference")))
        NAME('W', "Window",
             PROP('b', R("blur"))
             PROP('c', R("close") R("closed"))
             PROP('f', R("focus") R("frames"))
             PROP('h', R("history"))
             PROP('l', RW("location") R("length"))
             PROP('o', R("opener"))
             PROP('p', R("parent") R("postMessage"))
             PROP('s', R("self"))
             PROP('t', R("top"))
             PROP('w', R("window")))
    }
    return false;
}

#undef NAME
#undef RW
#undef R
#undef W

static bool
IsFrameId(JSContext *cx, JSObject *obj, jsid id)
{
    XPCWrappedNative *wn = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    if (!wn) {
        return false;
    }

    nsCOMPtr<nsIDOMWindow> domwin(do_QueryWrappedNative(wn));
    if (!domwin) {
        return false;
    }

    nsCOMPtr<nsIDOMWindowCollection> col;
    domwin->GetFrames(getter_AddRefs(col));
    if (!col) {
        return false;
    }

    if (JSID_IS_INT(id)) {
        col->Item(JSID_TO_INT(id), getter_AddRefs(domwin));
    } else if (JSID_IS_ATOM(id)) {
        nsAutoString str(reinterpret_cast<PRUnichar *>
                         (JS_GetStringChars(ATOM_TO_STRING(JSID_TO_ATOM(id)))));
        col->NamedItem(str, getter_AddRefs(domwin));
    } else {
        return false;
    }

    return domwin != nsnull;
}

static bool
IsWindow(const char *name)
{
    return name[0] == 'W' && !strcmp(name, "Window");
}

bool
AccessCheck::isCrossOriginAccessPermitted(JSContext *cx, JSObject *wrapper, jsid id,
                                          JSWrapper::Action act)
{
    if (!XPCWrapper::GetSecurityManager())
        return true;

    if (act == JSWrapper::CALL)
        return true;

    JSObject *obj = JSWrapper::wrappedObject(wrapper);

    const char *name;
    js::Class *clasp = obj->getClass();
    if (clasp->ext.innerObject)
        name = "Window";
    else if (Jsvalify(clasp) == &HolderClass)
        name = XrayWrapper<JSCrossCompartmentWrapper>::unwrapHolder(cx, obj)->getClass()->name;
    else
        name = clasp->name;

    if (JSID_IS_ATOM(id)) {
        JSString *str = ATOM_TO_STRING(JSID_TO_ATOM(id));
        const char *prop = JS_GetStringBytes(str);
        if (IsPermitted(name, prop, act == JSWrapper::SET))
            return true;
    }

    if (IsWindow(name) && IsFrameId(cx, obj, id))
        return true;

    return (act == JSWrapper::SET)
           ? nsContentUtils::IsCallerTrustedForWrite()
           : nsContentUtils::IsCallerTrustedForRead();
}

bool
AccessCheck::isSystemOnlyAccessPermitted(JSContext *cx)
{
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm) {
        return true;
    }

    JSStackFrame *fp;
    nsIPrincipal *principal = ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);
    if (!principal) {
        return false;
    }

    if (!fp) {
        if (!JS_FrameIterator(cx, &fp)) {
            // No code at all is running. So we must be arriving here as the result
            // of C++ code asking us to do something. Allow access.
            return true;
        }

        // Some code is running, we can't make the assumption, as above, but we
        // can't use a native frame, so clear fp.
        fp = NULL;
    } else if (!JS_IsScriptFrame(cx, fp)) {
        fp = NULL;
    }

    PRBool privileged;
    if (NS_SUCCEEDED(ssm->IsSystemPrincipal(principal, &privileged)) &&
        privileged) {
        return true;
    }

    // Allow any code loaded from chrome://global/ to touch us, even if it was
    // cloned into a less privileged context.
    static const char prefix[] = "chrome://global/";
    const char *filename;
    if (fp &&
        (filename = JS_GetFrameScript(cx, fp)->filename) &&
        !strncmp(filename, prefix, NS_ARRAY_LENGTH(prefix) - 1)) {
        return true;
    }

    return NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged)) && privileged;
}

bool
AccessCheck::needsSystemOnlyWrapper(JSObject *obj)
{
    if (!IS_WN_WRAPPER(obj))
        return false;

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(obj->getPrivate());
    return wn->NeedsSOW();
}

void
AccessCheck::deny(JSContext *cx, jsid id)
{
    if (id == JSID_VOID) {
        JS_ReportError(cx, "Permission denied to access object");
    } else {
        jsval idval;
        if (!JS_IdToValue(cx, id, &idval))
            return;
        JSString *str = JS_ValueToString(cx, idval);
        if (!str)
            return;
        JS_ReportError(cx, "Permission denied to access property '%hs'", JS_GetStringChars(str));
    }
}

typedef enum { READ = (1<<0), WRITE = (1<<1), NO_ACCESS = 0 } Access;

bool
ExposedPropertiesOnly::check(JSContext *cx, JSObject *wrapper, jsid id, JSWrapper::Action act,
                             Permission &perm)
{
    JSObject *holder = JSWrapper::wrappedObject(wrapper);

    perm = DenyAccess;

    jsid exposedPropsId = GetRTIdByIndex(cx, XPCJSRuntime::IDX_EXPOSEDPROPS);

    JSBool found = JS_FALSE;
    if (!JS_HasPropertyById(cx, holder, exposedPropsId, &found))
        return false;
    if (!found) {
        perm = PermitObjectAccess;
        return true; // Allow
    }

    if (id == JSID_VOID) {
        // This will force the caller to call us back for individual property accesses.
        perm = PermitPropertyAccess;
        return true;
    }

    jsval exposedProps;
    if (!JS_LookupPropertyById(cx, holder, exposedPropsId, &exposedProps))
        return false;

    if (JSVAL_IS_VOID(exposedProps) || JSVAL_IS_NULL(exposedProps)) {
        return true; // Deny
    }

    if (!JSVAL_IS_OBJECT(exposedProps)) {
        JS_ReportError(cx, "__exposedProps__ must be undefined, null, or an Object");
        return false;
    }

    JSObject *hallpass = JSVAL_TO_OBJECT(exposedProps);

    Access access = NO_ACCESS;

    jsval v;
    if (!JS_LookupPropertyById(cx, hallpass, id, &v)) {
        return false; // Error
    }

    if (!JSVAL_IS_STRING(v)) {
        JS_ReportError(cx, "property must be a string");
        return false;
    }

    JSString *str = JSVAL_TO_STRING(v);
    const jschar *chars = JS_GetStringChars(str);
    size_t length = JS_GetStringLength(str);
    for (size_t i = 0; i < length; ++i) {
        switch (chars[i]) {
        case 'r':
            if (access & READ) {
                JS_ReportError(cx, "duplicate 'readable' property flag");
                return false;
            }
            access = Access(access | READ);
            break;

        case 'w':
            if (access & WRITE) {
                JS_ReportError(cx, "duplicate 'writable' property flag");
                return false;
            }
            access = Access(access | WRITE);
            break;

        default:
            JS_ReportError(cx, "properties can only be readable or read and writable");
            return false;
        }
    }

    if (access == NO_ACCESS) {
        JS_ReportError(cx, "specified properties must have a permission bit set");
        return false;
    }

    if ((act == JSWrapper::SET && !(access & WRITE)) ||
        (act != JSWrapper::SET && !(access & READ))) {
        return true; // Deny
    }

    perm = PermitPropertyAccess;
    return true; // Allow
}

}
