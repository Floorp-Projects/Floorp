/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "AccessCheck.h"

#include "nsJSPrincipals.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "FilteringWrapper.h"

#include "jsfriendapi.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace js;

namespace xpc {

nsIPrincipal *
GetCompartmentPrincipal(JSCompartment *compartment)
{
    return nsJSPrincipals::get(JS_GetCompartmentPrincipals(compartment));
}

// Does the principal of compartment a subsume the principal of compartment b?
bool
AccessCheck::subsumes(JSCompartment *a, JSCompartment *b)
{
    nsIPrincipal *aprin = GetCompartmentPrincipal(a);
    nsIPrincipal *bprin = GetCompartmentPrincipal(b);

    // If either a or b doesn't have principals, we don't have enough
    // information to tell. Seeing as how this is Gecko, we are default-unsafe
    // in this case.
    if (!aprin || !bprin)
        return true;

    bool subsumes;
    nsresult rv = aprin->Subsumes(bprin, &subsumes);
    NS_ENSURE_SUCCESS(rv, false);

    return subsumes;
}

bool
AccessCheck::subsumes(JSObject *a, JSObject *b)
{
    return subsumes(js::GetObjectCompartment(a), js::GetObjectCompartment(b));
}

// Same as above, but ignoring document.domain.
bool
AccessCheck::subsumesIgnoringDomain(JSCompartment *a, JSCompartment *b)
{
    nsIPrincipal *aprin = GetCompartmentPrincipal(a);
    nsIPrincipal *bprin = GetCompartmentPrincipal(b);

    if (!aprin || !bprin)
        return false;

    bool subsumes;
    nsresult rv = aprin->SubsumesIgnoringDomain(bprin, &subsumes);
    NS_ENSURE_SUCCESS(rv, false);

    return subsumes;
}

// Does the compartment of the wrapper subsumes the compartment of the wrappee?
bool
AccessCheck::wrapperSubsumes(JSObject *wrapper)
{
    MOZ_ASSERT(js::IsWrapper(wrapper));
    JSObject *wrapped = js::UnwrapObject(wrapper);
    return AccessCheck::subsumes(js::GetObjectCompartment(wrapper),
                                 js::GetObjectCompartment(wrapped));
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

bool
AccessCheck::isChrome(JSObject *obj)
{
    return isChrome(js::GetObjectCompartment(obj));
}

bool
AccessCheck::callerIsChrome()
{
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (!ssm)
        return false;
    bool subjectIsSystem;
    nsresult rv = ssm->SubjectPrincipalIsSystem(&subjectIsSystem);
    return NS_SUCCEEDED(rv) && subjectIsSystem;
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
    const jschar *propChars =
        JS_GetInternedStringCharsAndLength(JS_FORGET_STRING_FLATNESS(prop), &propLength);
    if (!propLength)
        return false;
    switch (name[0]) {
        NAME('L', "Location",
             PROP('h', W("hash") W("href"))
             PROP('r', R("replace")))
        NAME('W', "Window",
             PROP('b', R("blur"))
             PROP('c', R("close") R("closed"))
             PROP('f', R("focus") R("frames"))
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
    } else if (JSID_IS_STRING(id)) {
        nsAutoString str(JS_GetInternedStringChars(JSID_TO_STRING(id)));
        col->NamedItem(str, getter_AddRefs(domwin));
    } else {
        return false;
    }

    return domwin != nullptr;
}

static bool
IsWindow(const char *name)
{
    return name[0] == 'W' && !strcmp(name, "Window");
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

    if (JSID_IS_STRING(id)) {
        if (IsPermitted(name, JSID_TO_FLAT_STRING(id), act == Wrapper::SET))
            return true;
    }

    return IsWindow(name) && IsFrameId(cx, obj, id);
}

bool
AccessCheck::isSystemOnlyAccessPermitted(JSContext *cx)
{
    MOZ_ASSERT(cx == nsContentUtils::GetCurrentJSContext());
    return nsContentUtils::CanAccessNativeAnon();
}

bool
AccessCheck::needsSystemOnlyWrapper(JSObject *obj)
{
    JSObject* wrapper = obj;
    if (dom::GetSameCompartmentWrapperForDOMBinding(wrapper))
        return wrapper != obj;

    if (!IS_WN_WRAPPER(obj))
        return false;

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(js::GetObjectPrivate(obj));
    return wn->NeedsSOW();
}

bool
AccessCheck::isScriptAccessOnly(JSContext *cx, JSObject *wrapper)
{
    MOZ_ASSERT(js::IsWrapper(wrapper));

    unsigned flags;
    (void) js::UnwrapObject(wrapper, true, &flags);

    // If the wrapper indicates script-only access, we are done.
    if (flags & WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG) {
        if (flags & WrapperFactory::SOW_FLAG)
            return !isSystemOnlyAccessPermitted(cx);
        return true;
    }

    return false;
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
IsInSandbox(JSContext *cx, JSObject *obj)
{
    JSAutoCompartment ac(cx, obj);
    JSObject *global = JS_GetGlobalForObject(cx, obj);
    return !strcmp(js::GetObjectJSClass(global)->name, "Sandbox");
}

static void
EnterAndThrow(JSContext *cx, JSObject *wrapper, const char *msg)
{
    JSAutoCompartment ac(cx, wrapper);
    JS_ReportError(cx, msg);
}

bool
ExposedPropertiesOnly::check(JSContext *cx, JSObject *wrapper, jsid id, Wrapper::Action act)
{
    JSObject *wrappedObject = Wrapper::wrappedObject(wrapper);

    if (act == Wrapper::CALL)
        return true;

    jsid exposedPropsId = GetRTIdByIndex(cx, XPCJSRuntime::IDX_EXPOSEDPROPS);

    // We need to enter the wrappee's compartment to look at __exposedProps__,
    // but we want to be in the wrapper's compartment if we call Deny().
    //
    // Unfortunately, |cx| can be in either compartment when we call ::check. :-(
    JSAutoCompartment ac(cx, wrappedObject);

    JSBool found = false;
    if (!JS_HasPropertyById(cx, wrappedObject, exposedPropsId, &found))
        return false;

    // Always permit access to "length" and indexed properties of arrays.
    if ((JS_IsArrayObject(cx, wrappedObject) ||
         JS_IsTypedArrayObject(wrappedObject)) &&
        ((JSID_IS_INT(id) && JSID_TO_INT(id) >= 0) ||
         (JSID_IS_STRING(id) && JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(id), "length")))) {
        return true; // Allow
    }

    // If no __exposedProps__ existed, deny access.
    if (!found) {
        // Everything below here needs to be done in the wrapper's compartment.
        JSAutoCompartment wrapperAC(cx, wrapper);
        // Make a temporary exception for objects in a chrome sandbox to help
        // out jetpack. See bug 784233.
        if (!JS_ObjectIsFunction(cx, wrappedObject) &&
            IsInSandbox(cx, wrappedObject))
        {
            // This little loop hole will go away soon! See bug 553102.
            nsCOMPtr<nsPIDOMWindow> win =
                do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(wrapper));
            if (win) {
                nsCOMPtr<nsIDocument> doc =
                    do_QueryInterface(win->GetExtantDocument());
                if (doc) {
                    doc->WarnOnceAbout(nsIDocument::eNoExposedProps,
                                       /* asError = */ true);
                }
            }

            return true;
        }
        return false;
    }

    if (id == JSID_VOID)
        return true;

    JS::Value exposedProps;
    if (!JS_LookupPropertyById(cx, wrappedObject, exposedPropsId, &exposedProps))
        return false;

    if (exposedProps.isNullOrUndefined())
        return false;

    if (!exposedProps.isObject()) {
        EnterAndThrow(cx, wrapper, "__exposedProps__ must be undefined, null, or an Object");
        return false;
    }

    JSObject *hallpass = &exposedProps.toObject();

    if (!AccessCheck::subsumes(js::UnwrapObject(hallpass), wrappedObject)) {
        EnterAndThrow(cx, wrapper, "Invalid __exposedProps__");
        return false;
    }

    Access access = NO_ACCESS;

    JSPropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, hallpass, id, 0, &desc)) {
        return false; // Error
    }
    if (!desc.obj || !(desc.attrs & JSPROP_ENUMERATE))
        return false;

    if (!JSVAL_IS_STRING(desc.value)) {
        EnterAndThrow(cx, wrapper, "property must be a string");
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
                EnterAndThrow(cx, wrapper, "duplicate 'readable' property flag");
                return false;
            }
            access = Access(access | READ);
            break;

        case 'w':
            if (access & WRITE) {
                EnterAndThrow(cx, wrapper, "duplicate 'writable' property flag");
                return false;
            }
            access = Access(access | WRITE);
            break;

        default:
            EnterAndThrow(cx, wrapper, "properties can only be readable or read and writable");
            return false;
        }
    }

    if (access == NO_ACCESS) {
        EnterAndThrow(cx, wrapper, "specified properties must have a permission bit set");
        return false;
    }

    if ((act == Wrapper::SET && !(access & WRITE)) ||
        (act != Wrapper::SET && !(access & READ))) {
        return false;
    }

    return true;
}

bool
ExposedPropertiesOnly::allowNativeCall(JSContext *cx, JS::IsAcceptableThis test,
                                       JS::NativeImpl impl)
{
    return js::IsReadOnlyDateMethod(test, impl) || js::IsTypedArrayThisCheck(test);
}

bool
ComponentsObjectPolicy::check(JSContext *cx, JSObject *wrapper, jsid id, Wrapper::Action act)
{
    JSAutoCompartment ac(cx, wrapper);

    if (JSID_IS_STRING(id) && act == Wrapper::GET) {
        JSFlatString *flatId = JSID_TO_FLAT_STRING(id);
        if (JS_FlatStringEqualsAscii(flatId, "isSuccessCode") ||
            JS_FlatStringEqualsAscii(flatId, "lookupMethod") ||
            JS_FlatStringEqualsAscii(flatId, "interfaces") ||
            JS_FlatStringEqualsAscii(flatId, "interfacesByID") ||
            JS_FlatStringEqualsAscii(flatId, "results"))
        {
            return true;
        }
    }

    // We don't have any way to recompute same-compartment Components wrappers,
    // so we need this dynamic check. This can go away when we expose Components
    // as SpecialPowers.wrap(Components) during automation.
    if (xpc::IsUniversalXPConnectEnabled(cx)) {
        return true;
    }

    return false;
}

}
