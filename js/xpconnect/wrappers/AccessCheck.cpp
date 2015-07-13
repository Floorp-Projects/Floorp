/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"

#include "nsJSPrincipals.h"
#include "nsGlobalWindow.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"

#include "jsfriendapi.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsIDOMWindowCollection.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace JS;
using namespace js;

namespace xpc {

nsIPrincipal*
GetCompartmentPrincipal(JSCompartment* compartment)
{
    return nsJSPrincipals::get(JS_GetCompartmentPrincipals(compartment));
}

nsIPrincipal*
GetObjectPrincipal(JSObject* obj)
{
    return GetCompartmentPrincipal(js::GetObjectCompartment(obj));
}

// Does the principal of compartment a subsume the principal of compartment b?
bool
AccessCheck::subsumes(JSCompartment* a, JSCompartment* b)
{
    nsIPrincipal* aprin = GetCompartmentPrincipal(a);
    nsIPrincipal* bprin = GetCompartmentPrincipal(b);
    return aprin->Subsumes(bprin);
}

bool
AccessCheck::subsumes(JSObject* a, JSObject* b)
{
    return subsumes(js::GetObjectCompartment(a), js::GetObjectCompartment(b));
}

// Same as above, but considering document.domain.
bool
AccessCheck::subsumesConsideringDomain(JSCompartment* a, JSCompartment* b)
{
    nsIPrincipal* aprin = GetCompartmentPrincipal(a);
    nsIPrincipal* bprin = GetCompartmentPrincipal(b);
    return aprin->SubsumesConsideringDomain(bprin);
}

// Does the compartment of the wrapper subsumes the compartment of the wrappee?
bool
AccessCheck::wrapperSubsumes(JSObject* wrapper)
{
    MOZ_ASSERT(js::IsWrapper(wrapper));
    JSObject* wrapped = js::UncheckedUnwrap(wrapper);
    return AccessCheck::subsumes(js::GetObjectCompartment(wrapper),
                                 js::GetObjectCompartment(wrapped));
}

bool
AccessCheck::isChrome(JSCompartment* compartment)
{
    bool privileged;
    nsIPrincipal* principal = GetCompartmentPrincipal(compartment);
    return NS_SUCCEEDED(nsXPConnect::SecurityManager()->IsSystemPrincipal(principal, &privileged)) && privileged;
}

bool
AccessCheck::isChrome(JSObject* obj)
{
    return isChrome(js::GetObjectCompartment(obj));
}

nsIPrincipal*
AccessCheck::getPrincipal(JSCompartment* compartment)
{
    return GetCompartmentPrincipal(compartment);
}

// Hardcoded policy for cross origin property access. See the HTML5 Spec.
static bool
IsPermitted(CrossOriginObjectType type, JSFlatString* prop, bool set)
{
    size_t propLength = JS_GetStringLength(JS_FORGET_STRING_FLATNESS(prop));
    if (!propLength)
        return false;

    char16_t propChar0 = JS_GetFlatStringCharAt(prop, 0);
    if (type == CrossOriginLocation)
        return dom::LocationBinding::IsPermitted(prop, propChar0, set);
    if (type == CrossOriginWindow)
        return dom::WindowBinding::IsPermitted(prop, propChar0, set);

    return false;
}

static bool
IsFrameId(JSContext* cx, JSObject* obj, jsid idArg)
{
    MOZ_ASSERT(!js::IsWrapper(obj));
    RootedId id(cx, idArg);

    nsGlobalWindow* win = WindowOrNull(obj);
    if (!win) {
        return false;
    }

    nsCOMPtr<nsIDOMWindowCollection> col;
    win->GetFrames(getter_AddRefs(col));
    if (!col) {
        return false;
    }

    nsCOMPtr<nsIDOMWindow> domwin;
    if (JSID_IS_INT(id)) {
        col->Item(JSID_TO_INT(id), getter_AddRefs(domwin));
    } else if (JSID_IS_STRING(id)) {
        nsAutoJSString idAsString;
        if (!idAsString.init(cx, JSID_TO_STRING(id))) {
            return false;
        }
        col->NamedItem(idAsString, getter_AddRefs(domwin));
    }

    return domwin != nullptr;
}

CrossOriginObjectType
IdentifyCrossOriginObject(JSObject* obj)
{
    obj = js::UncheckedUnwrap(obj, /* stopAtOuter = */ false);
    const js::Class* clasp = js::GetObjectClass(obj);
    MOZ_ASSERT(!XrayUtils::IsXPCWNHolderClass(Jsvalify(clasp)), "shouldn't have a holder here");

    if (clasp->name[0] == 'L' && !strcmp(clasp->name, "Location"))
        return CrossOriginLocation;
    if (clasp->name[0] == 'W' && !strcmp(clasp->name, "Window"))
        return CrossOriginWindow;

    return CrossOriginOpaque;
}

bool
AccessCheck::isCrossOriginAccessPermitted(JSContext* cx, HandleObject wrapper, HandleId id,
                                          Wrapper::Action act)
{
    if (act == Wrapper::CALL)
        return false;

    if (act == Wrapper::ENUMERATE)
        return true;

    // For the case of getting a property descriptor, we allow if either GET or SET
    // is allowed, and rely on FilteringWrapper to filter out any disallowed accessors.
    if (act == Wrapper::GET_PROPERTY_DESCRIPTOR) {
        return isCrossOriginAccessPermitted(cx, wrapper, id, Wrapper::GET) ||
               isCrossOriginAccessPermitted(cx, wrapper, id, Wrapper::SET);
    }

    RootedObject obj(cx, js::UncheckedUnwrap(wrapper, /* stopAtOuter = */ false));
    CrossOriginObjectType type = IdentifyCrossOriginObject(obj);
    if (JSID_IS_STRING(id)) {
        if (IsPermitted(type, JSID_TO_FLAT_STRING(id), act == Wrapper::SET))
            return true;
    }

    if (act != Wrapper::GET)
        return false;

    // Check for frame IDs. If we're resolving named frames, make sure to only
    // resolve ones that don't shadow native properties. See bug 860494.
    if (type == CrossOriginWindow) {
        if (JSID_IS_STRING(id)) {
            bool wouldShadow = false;
            if (!XrayUtils::HasNativeProperty(cx, wrapper, id, &wouldShadow) ||
                wouldShadow)
            {
                // If the named subframe matches the name of a DOM constructor,
                // the global resolve triggered by the HasNativeProperty call
                // above will try to perform a CheckedUnwrap on |wrapper|, and
                // throw a security error if it fails. That exception isn't
                // really useful for our callers, so we silence it and just
                // deny access to the property (since it matched a builtin).
                //
                // Note that this would be a problem if the resolve code ever
                // tried to CheckedUnwrap the wrapper _before_ concluding that
                // the name corresponds to a builtin global property, since it
                // would mean that we'd never permit cross-origin named subframe
                // access (something we regrettably need to support).
                JS_ClearPendingException(cx);
                return false;
            }
        }
        return IsFrameId(cx, obj, id);
    }
    return false;
}

bool
AccessCheck::checkPassToPrivilegedCode(JSContext* cx, HandleObject wrapper, HandleValue v)
{
    // Primitives are fine.
    if (!v.isObject())
        return true;
    RootedObject obj(cx, &v.toObject());

    // Non-wrappers are fine.
    if (!js::IsWrapper(obj))
        return true;

    // CPOWs use COWs (in the unprivileged junk scope) for all child->parent
    // references. Without this test, the child process wouldn't be able to
    // pass any objects at all to CPOWs.
    if (mozilla::jsipc::IsWrappedCPOW(obj) &&
        js::GetObjectCompartment(wrapper) == js::GetObjectCompartment(xpc::UnprivilegedJunkScope()) &&
        XRE_IsParentProcess())
    {
        return true;
    }

    // COWs are fine to pass to chrome if and only if they have __exposedProps__,
    // since presumably content should never have a reason to pass an opaque
    // object back to chrome.
    if (AccessCheck::isChrome(js::UncheckedUnwrap(wrapper)) && WrapperFactory::IsCOW(obj)) {
        RootedObject target(cx, js::UncheckedUnwrap(obj));
        JSAutoCompartment ac(cx, target);
        RootedId id(cx, GetRTIdByIndex(cx, XPCJSRuntime::IDX_EXPOSEDPROPS));
        bool found = false;
        if (!JS_HasPropertyById(cx, target, id, &found))
            return false;
        if (found)
            return true;
    }

    // Same-origin wrappers are fine.
    if (AccessCheck::wrapperSubsumes(obj))
        return true;

    // Badness.
    JS_ReportError(cx, "Permission denied to pass object to privileged code");
    return false;
}

bool
AccessCheck::checkPassToPrivilegedCode(JSContext* cx, HandleObject wrapper, const CallArgs& args)
{
    if (!checkPassToPrivilegedCode(cx, wrapper, args.thisv()))
        return false;
    for (size_t i = 0; i < args.length(); ++i) {
        if (!checkPassToPrivilegedCode(cx, wrapper, args[i]))
            return false;
    }
    return true;
}

enum Access { READ = (1<<0), WRITE = (1<<1), NO_ACCESS = 0 };

static void
EnterAndThrow(JSContext* cx, JSObject* wrapper, const char* msg)
{
    JSAutoCompartment ac(cx, wrapper);
    JS_ReportError(cx, msg);
}

bool
ExposedPropertiesOnly::check(JSContext* cx, HandleObject wrapper, HandleId id, Wrapper::Action act)
{
    RootedObject wrappedObject(cx, Wrapper::wrappedObject(wrapper));

    if (act == Wrapper::CALL)
        return false;

    // For the case of getting a property descriptor, we allow if either GET or SET
    // is allowed, and rely on FilteringWrapper to filter out any disallowed accessors.
    if (act == Wrapper::GET_PROPERTY_DESCRIPTOR) {
        return check(cx, wrapper, id, Wrapper::GET) ||
               check(cx, wrapper, id, Wrapper::SET);
    }

    RootedId exposedPropsId(cx, GetRTIdByIndex(cx, XPCJSRuntime::IDX_EXPOSEDPROPS));

    // We need to enter the wrappee's compartment to look at __exposedProps__,
    // but we want to be in the wrapper's compartment if we call Deny().
    //
    // Unfortunately, |cx| can be in either compartment when we call ::check. :-(
    JSAutoCompartment ac(cx, wrappedObject);

    bool found = false;
    if (!JS_HasPropertyById(cx, wrappedObject, exposedPropsId, &found))
        return false;

    // If no __exposedProps__ existed, deny access.
    if (!found) {
        // Previously we automatically granted access to indexed properties and
        // .length for Array COWs. We're not doing that anymore, so make sure to
        // let people know what's going on.
        bool isArray = JS_IsArrayObject(cx, wrappedObject) || JS_IsTypedArrayObject(wrappedObject);
        bool isIndexedAccessOnArray = isArray && JSID_IS_INT(id) && JSID_TO_INT(id) >= 0;
        bool isLengthAccessOnArray = isArray && JSID_IS_STRING(id) &&
                                     JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(id), "length");
        if (isIndexedAccessOnArray || isLengthAccessOnArray) {
            JSAutoCompartment ac2(cx, wrapper);
            ReportWrapperDenial(cx, id, WrapperDenialForCOW,
                                "Access to elements and length of privileged Array not permitted");
        }

        return false;
    }

    if (id == JSID_VOID)
        return true;

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, wrappedObject, exposedPropsId, &desc))
        return false;

    if (!desc.object())
        return false;

    if (desc.hasGetterOrSetter()) {
        EnterAndThrow(cx, wrapper, "__exposedProps__ must be a value property");
        return false;
    }

    RootedValue exposedProps(cx, desc.value());
    if (exposedProps.isNullOrUndefined())
        return false;

    if (!exposedProps.isObject()) {
        EnterAndThrow(cx, wrapper, "__exposedProps__ must be undefined, null, or an Object");
        return false;
    }

    RootedObject hallpass(cx, &exposedProps.toObject());

    if (!AccessCheck::subsumes(js::UncheckedUnwrap(hallpass), wrappedObject)) {
        EnterAndThrow(cx, wrapper, "Invalid __exposedProps__");
        return false;
    }

    Access access = NO_ACCESS;

    if (!JS_GetPropertyDescriptorById(cx, hallpass, id, &desc)) {
        return false; // Error
    }
    if (!desc.object() || !desc.enumerable())
        return false;

    if (!desc.value().isString()) {
        EnterAndThrow(cx, wrapper, "property must be a string");
        return false;
    }

    JSFlatString* flat = JS_FlattenString(cx, desc.value().toString());
    if (!flat)
        return false;

    size_t length = JS_GetStringLength(JS_FORGET_STRING_FLATNESS(flat));

    for (size_t i = 0; i < length; ++i) {
        char16_t ch = JS_GetFlatStringCharAt(flat, i);
        switch (ch) {
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

    // Inspect the property on the underlying object to check for red flags.
    bool skipCallableChecks = CompartmentPrivate::Get(wrappedObject)->skipCOWCallableChecks;
    if (!JS_GetPropertyDescriptorById(cx, wrappedObject, id, &desc))
        return false;

    // Reject accessor properties.
    if (!skipCallableChecks && desc.hasGetterOrSetter()) {
        EnterAndThrow(cx, wrapper, "Exposing privileged accessor properties is prohibited");
        return false;
    }

    // Reject privileged or cross-origin callables.
    if (!skipCallableChecks && desc.value().isObject()) {
        RootedObject maybeCallable(cx, js::UncheckedUnwrap(&desc.value().toObject()));
        if (JS::IsCallable(maybeCallable) && !AccessCheck::subsumes(wrapper, maybeCallable)) {
            EnterAndThrow(cx, wrapper, "Exposing privileged or cross-origin callable is prohibited");
            return false;
        }
    }

    return true;
}

bool
ExposedPropertiesOnly::deny(js::Wrapper::Action act, HandleId id)
{
    // Fail silently for GET, ENUMERATE, and GET_PROPERTY_DESCRIPTOR.
    if (act == js::Wrapper::GET || act == js::Wrapper::ENUMERATE ||
        act == js::Wrapper::GET_PROPERTY_DESCRIPTOR)
    {
        AutoJSContext cx;
        return ReportWrapperDenial(cx, id, WrapperDenialForCOW,
                                   "Access to privileged JS object not permitted");
    }

    return false;
}

} // namespace xpc
