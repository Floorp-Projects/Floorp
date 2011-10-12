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
#include "FilteringWrapper.h"
#include "WrapperFactory.h"

#include "jsfriendapi.h"

using namespace js;

namespace xpc {

nsIPrincipal *
GetCompartmentPrincipal(JSCompartment *compartment)
{
    JSPrincipals *prin = JS_GetCompartmentPrincipals(compartment);
    return prin ? static_cast<nsJSPrincipals *>(prin)->nsIPrincipalPtr : nsnull;
}

bool
AccessCheck::isSameOrigin(JSCompartment *a, JSCompartment *b)
{
    nsIPrincipal *aprin = GetCompartmentPrincipal(a);
    nsIPrincipal *bprin = GetCompartmentPrincipal(b);

    // If either a or b doesn't have principals, we don't have enough
    // information to tell. Seeing as how this is Gecko, we are default-unsafe
    // in this case.
    if (!aprin || !bprin)
        return true;

    bool equals;
    nsresult rv = aprin->EqualsIgnoringDomain(bprin, &equals);
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to ask about equality");
        return false;
    }

    return equals;
}

bool
AccessCheck::isLocationObjectSameOrigin(JSContext *cx, JSObject *wrapper)
{
    JSObject *obj = js::GetObjectParent(js::UnwrapObject(wrapper));
    if (!js::GetObjectClass(obj)->ext.innerObject) {
        obj = js::UnwrapObject(obj);
        JS_ASSERT(js::GetObjectClass(obj)->ext.innerObject);
    }
    obj = JS_ObjectToInnerObject(cx, obj);
    return obj &&
           (isSameOrigin(js::GetObjectCompartment(wrapper),
                         js::GetObjectCompartment(obj)) ||
            documentDomainMakesSameOrigin(cx, obj));
}

bool
AccessCheck::isChrome(JSCompartment *compartment)
{
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm) {
        return false;
    }

    bool privileged;
    nsIPrincipal *principal = GetCompartmentPrincipal(compartment);
    return NS_SUCCEEDED(ssm->IsSystemPrincipal(principal, &privileged)) && privileged;
}

nsIPrincipal *
AccessCheck::getPrincipal(JSCompartment *compartment)
{
    return GetCompartmentPrincipal(compartment);
}

#define NAME(ch, str, cases)                                                  \
    case ch: if (!strcmp(name, str)) switch (propChars[0]) { cases }; break;
#define PROP(ch, actions) case ch: { actions }; break;
#define RW(str) if (JS_FlatStringEqualsAscii(prop, str)) return true;
#define R(str) if (!set && JS_FlatStringEqualsAscii(prop, str)) return true;
#define W(str) if (set && JS_FlatStringEqualsAscii(prop, str)) return true;

// Hardcoded policy for cross origin property access. This was culled from the
// preferences file (all.js). We don't want users to overwrite highly sensitive
// security policies.
static bool
IsPermitted(const char *name, JSFlatString *prop, bool set)
{
    size_t propLength;
    const jschar *propChars = JS_GetInternedStringCharsAndLength(prop, &propLength);
    if (!propLength)
        return false;
    switch(name[0]) {
        NAME('D', "DOMException",
             PROP('c', RW("code"))
             PROP('m', RW("message"))
             PROP('n', RW("name"))
             PROP('r', RW("result"))
             PROP('t', R("toString")))
        NAME('E', "Error",
             PROP('m', R("message")))
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
        nsAutoString str(JS_GetInternedStringChars(JSID_TO_STRING(id)));
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

static bool
IsLocation(const char *name)
{
    return name[0] == 'L' && !strcmp(name, "Location");
}

static nsIPrincipal *
GetPrincipal(JSObject *obj)
{
    NS_ASSERTION(!IS_SLIM_WRAPPER(obj), "global object is a slim wrapper?");
    if (!IS_WN_WRAPPER(obj)) {
        NS_ASSERTION(!(~js::GetObjectClass(obj)->flags &
                       (JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_HAS_PRIVATE)),
                     "bad object");
        nsCOMPtr<nsIScriptObjectPrincipal> objPrin =
            do_QueryInterface((nsISupports*)xpc_GetJSPrivate(obj));
        NS_ASSERTION(objPrin, "global isn't nsIScriptObjectPrincipal?");
        return objPrin->GetPrincipal();
    }

    nsIXPConnect *xpc = nsXPConnect::GetRuntimeInstance()->GetXPConnect();
    return xpc->GetPrincipal(obj, PR_TRUE);
}

bool
AccessCheck::documentDomainMakesSameOrigin(JSContext *cx, JSObject *obj)
{
    JSObject *scope = nsnull;
    JSStackFrame *fp = nsnull;
    JS_FrameIterator(cx, &fp);
    if (fp) {
        while (!JS_IsScriptFrame(cx, fp)) {
            if (!JS_FrameIterator(cx, &fp))
                break;
        }

        if (fp)
            scope = JS_GetGlobalForFrame(fp);
    }

    if (!scope)
        scope = JS_GetGlobalForScopeChain(cx);

    nsIPrincipal *subject;
    nsIPrincipal *object;

    {
        JSAutoEnterCompartment ac;

        if (!ac.enter(cx, scope))
            return false;

        subject = GetPrincipal(scope);
    }

    if (!subject)
        return false;

    {
        JSAutoEnterCompartment ac;

        if (!ac.enter(cx, obj))
            return false;

        object = GetPrincipal(JS_GetGlobalForObject(cx, obj));
    }

    bool subsumes;
    return NS_SUCCEEDED(subject->Subsumes(object, &subsumes)) && subsumes;
}

bool
AccessCheck::isCrossOriginAccessPermitted(JSContext *cx, JSObject *wrapper, jsid id,
                                          Wrapper::Action act)
{
    if (!XPCWrapper::GetSecurityManager())
        return true;

    if (act == Wrapper::CALL)
        return true;

    JSObject *obj = Wrapper::wrappedObject(wrapper);

    const char *name;
    js::Class *clasp = js::GetObjectClass(obj);
    NS_ASSERTION(Jsvalify(clasp) != &XrayUtils::HolderClass, "shouldn't have a holder here");
    if (clasp->ext.innerObject)
        name = "Window";
    else
        name = clasp->name;

    if (JSID_IS_ATOM(id)) {
        if (IsPermitted(name, JSID_TO_FLAT_STRING(id), act == Wrapper::SET))
            return true;
    }

    if (IsWindow(name) && IsFrameId(cx, obj, id))
        return true;

    // We only reach this point for cross origin location objects (see
    // SameOriginOrCrossOriginAccessiblePropertiesOnly::check).
    if (!IsLocation(name) && documentDomainMakesSameOrigin(cx, obj))
        return true;

    return (act == Wrapper::SET)
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

    bool privileged;
    if (NS_SUCCEEDED(ssm->IsSystemPrincipal(principal, &privileged)) &&
        privileged) {
        return true;
    }

    // Allow any code loaded from chrome://global/ to touch us, even if it was
    // cloned into a less privileged context.
    static const char prefix[] = "chrome://global/";
    const char *filename;
    if (fp &&
        (filename = JS_GetScriptFilename(cx, JS_GetFrameScript(cx, fp))) &&
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

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(js::GetObjectPrivate(obj));
    return wn->NeedsSOW();
}

bool
AccessCheck::isScriptAccessOnly(JSContext *cx, JSObject *wrapper)
{
    JS_ASSERT(js::IsWrapper(wrapper));

    uintN flags;
    JSObject *obj = js::UnwrapObject(wrapper, &flags);

    // If the wrapper indicates script-only access, we are done.
    if (flags & WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG) {
        if (flags & WrapperFactory::SOW_FLAG)
            return !isSystemOnlyAccessPermitted(cx);

        if (flags & WrapperFactory::PARTIALLY_TRANSPARENT)
            return !XrayUtils::IsTransparent(cx, wrapper);

        nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
        if (!ssm)
            return true;

        // Bypass script-only status if UniversalXPConnect is enabled.
        bool privileged;
        return !NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged)) ||
               !privileged;
    }

    // In addition, chrome objects can explicitly opt-in by setting .scriptOnly to true.
    if (js::GetProxyHandler(wrapper) ==
        &FilteringWrapper<CrossCompartmentSecurityWrapper,
                          CrossOriginAccessiblePropertiesOnly>::singleton) {
        jsid scriptOnlyId = GetRTIdByIndex(cx, XPCJSRuntime::IDX_SCRIPTONLY);
        jsval scriptOnly;
        if (JS_LookupPropertyById(cx, obj, scriptOnlyId, &scriptOnly) &&
            scriptOnly == JSVAL_TRUE)
            return true; // script-only
    }

    // Allow non-script access to same-origin location objects and any other
    // objects.
    return WrapperFactory::IsLocationObject(obj) && !isLocationObjectSameOrigin(cx, wrapper);
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
        const jschar *chars = JS_GetStringCharsZ(cx, str);
        if (chars)
            JS_ReportError(cx, "Permission denied to access property '%hs'", chars);
    }
}

enum Access { READ = (1<<0), WRITE = (1<<1), NO_ACCESS = 0 };

static bool
Deny(JSContext *cx, jsid id, Wrapper::Action act)
{
    // Refuse to perform the action and just return the default value.
    if (act == Wrapper::GET)
        return true;
    // If its a set, deny it and throw an exception.
    AccessCheck::deny(cx, id);
    return false;
}

bool
PermitIfUniversalXPConnect(JSContext *cx, jsid id, Wrapper::Action act,
                           ExposedPropertiesOnly::Permission &perm)
{
    // If UniversalXPConnect is enabled, allow access even if __exposedProps__ doesn't
    // exists.
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm) {
        return false;
    }
    bool privileged;
    if (NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged)) &&
        privileged) {
        perm = ExposedPropertiesOnly::PermitPropertyAccess;
        return true; // Allow
    }

    // Deny
    return Deny(cx, id, act);
}

bool
ExposedPropertiesOnly::check(JSContext *cx, JSObject *wrapper, jsid id, Wrapper::Action act,
                             Permission &perm)
{
    JSObject *wrappedObject = Wrapper::wrappedObject(wrapper);

    if (act == Wrapper::CALL) {
        perm = PermitObjectAccess;
        return true;
    }

    perm = DenyAccess;

    jsid exposedPropsId = GetRTIdByIndex(cx, XPCJSRuntime::IDX_EXPOSEDPROPS);

    JSBool found = JS_FALSE;
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, wrappedObject) ||
        !JS_HasPropertyById(cx, wrappedObject, exposedPropsId, &found))
        return false;

    // Always permit access to "length" and indexed properties of arrays.
    if (JS_IsArrayObject(cx, wrappedObject) &&
        ((JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) ||
         (JSID_IS_ATOM(id) && JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(id), "length")))) {
        perm = PermitPropertyAccess;
        return true; // Allow
    }

    // If no __exposedProps__ existed, deny access.
    if (!found) {
        // For now, only do this on functions.
        if (!JS_ObjectIsFunction(cx, wrappedObject)) {
            perm = PermitPropertyAccess;
            return true;
        }
        return PermitIfUniversalXPConnect(cx, id, act, perm); // Deny
    }

    if (id == JSID_VOID) {
        // This will force the caller to call us back for individual property accesses.
        perm = PermitPropertyAccess;
        return true;
    }

    jsval exposedProps;
    if (!JS_LookupPropertyById(cx, wrappedObject, exposedPropsId, &exposedProps))
        return false;

    if (JSVAL_IS_VOID(exposedProps) || JSVAL_IS_NULL(exposedProps)) {
        return PermitIfUniversalXPConnect(cx, id, act, perm); // Deny
    }

    if (!JSVAL_IS_OBJECT(exposedProps)) {
        JS_ReportError(cx, "__exposedProps__ must be undefined, null, or an Object");
        return false;
    }

    JSObject *hallpass = JSVAL_TO_OBJECT(exposedProps);

    Access access = NO_ACCESS;

    JSPropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, hallpass, id, JSRESOLVE_QUALIFIED, &desc)) {
        return false; // Error
    }
    if (desc.obj == NULL || !(desc.attrs & JSPROP_ENUMERATE)) {
        return PermitIfUniversalXPConnect(cx, id, act, perm); // Deny
    }

    if (!JSVAL_IS_STRING(desc.value)) {
        JS_ReportError(cx, "property must be a string");
        return false;
    }

    JSString *str = JSVAL_TO_STRING(desc.value);
    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

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

    if ((act == Wrapper::SET && !(access & WRITE)) ||
        (act != Wrapper::SET && !(access & READ))) {
        return PermitIfUniversalXPConnect(cx, id, act, perm); // Deny
    }

    perm = PermitPropertyAccess;
    return true; // Allow
}

}
