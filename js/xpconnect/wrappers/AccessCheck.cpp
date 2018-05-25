/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"

#include "nsJSPrincipals.h"
#include "BasePrincipal.h"
#include "nsDOMWindowList.h"
#include "nsGlobalWindow.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "FilteringWrapper.h"

#include "jsfriendapi.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

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
    return BasePrincipal::Cast(aprin)->FastSubsumes(bprin);
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
    MOZ_ASSERT(OriginAttributes::IsRestrictOpenerAccessForFPI());
    nsIPrincipal* aprin = GetCompartmentPrincipal(a);
    nsIPrincipal* bprin = GetCompartmentPrincipal(b);
    return BasePrincipal::Cast(aprin)->FastSubsumesConsideringDomain(bprin);
}

bool
AccessCheck::subsumesConsideringDomainIgnoringFPD(JSCompartment* a,
                                                  JSCompartment* b)
{
    MOZ_ASSERT(!OriginAttributes::IsRestrictOpenerAccessForFPI());
    nsIPrincipal* aprin = GetCompartmentPrincipal(a);
    nsIPrincipal* bprin = GetCompartmentPrincipal(b);
    return BasePrincipal::Cast(aprin)->FastSubsumesConsideringDomainIgnoringFPD(bprin);
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
    nsIPrincipal* principal = GetCompartmentPrincipal(compartment);
    return nsXPConnect::SystemPrincipal() == principal;
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

    nsGlobalWindowInner* win = WindowOrNull(obj);
    if (!win) {
        return false;
    }

    nsDOMWindowList* col = win->GetFrames();
    if (!col) {
        return false;
    }

    nsCOMPtr<mozIDOMWindowProxy> domwin;
    if (JSID_IS_INT(id)) {
        domwin = col->IndexedGetter(JSID_TO_INT(id));
    } else if (JSID_IS_STRING(id)) {
        nsAutoJSString idAsString;
        if (!idAsString.init(cx, JSID_TO_STRING(id))) {
            return false;
        }
        domwin = col->NamedItem(idAsString);
    }

    return domwin != nullptr;
}

CrossOriginObjectType
IdentifyCrossOriginObject(JSObject* obj)
{
    obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
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

    RootedObject obj(cx, js::UncheckedUnwrap(wrapper, /* stopAtWindowProxy = */ false));
    CrossOriginObjectType type = IdentifyCrossOriginObject(obj);
    if (JSID_IS_STRING(id)) {
        if (IsPermitted(type, JSID_TO_FLAT_STRING(id), act == Wrapper::SET))
            return true;
    }

    if (type != CrossOriginOpaque &&
        IsCrossOriginWhitelistedProp(cx, id)) {
        // We always allow access to "then", @@toStringTag, @@hasInstance, and
        // @@isConcatSpreadable.  But then we nerf them to be a value descriptor
        // with value undefined in CrossOriginXrayWrapper.
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

    // Same-origin wrappers are fine.
    if (AccessCheck::wrapperSubsumes(obj))
        return true;

    // Badness.
    JS_ReportErrorASCII(cx, "Permission denied to pass object to privileged code");
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

void
AccessCheck::reportCrossOriginDenial(JSContext* cx, JS::HandleId id,
                                     const nsACString& accessType)
{
    // This function exists because we want to report DOM SecurityErrors, not JS
    // Errors, when denying access on cross-origin DOM objects.  It's
    // conceptually pretty similar to
    // AutoEnterPolicy::reportErrorIfExceptionIsNotPending.
    if (JS_IsExceptionPending(cx)) {
        return;
    }

    nsAutoCString message;
    if (JSID_IS_VOID(id)) {
        message = NS_LITERAL_CSTRING("Permission denied to access object");
    } else {
        // We want to use JS_ValueToSource here, because that most closely
        // matches what AutoEnterPolicy::reportErrorIfExceptionIsNotPending
        // does.
        JS::RootedValue idVal(cx, js::IdToValue(id));
        nsAutoJSString propName;
        JS::RootedString idStr(cx, JS_ValueToSource(cx, idVal));
        if (!idStr || !propName.init(cx, idStr)) {
            return;
        }
        message = NS_LITERAL_CSTRING("Permission denied to ") +
                  accessType +
                  NS_LITERAL_CSTRING(" property ") +
                  NS_ConvertUTF16toUTF8(propName) +
                  NS_LITERAL_CSTRING(" on cross-origin object");
    }
    ErrorResult rv;
    rv.ThrowDOMException(NS_ERROR_DOM_SECURITY_ERR, message);
    MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(cx));
}

bool
OpaqueWithSilentFailing::deny(JSContext* cx, js::Wrapper::Action act, HandleId id,
                              bool mayThrow)
{
    // Fail silently for GET, ENUMERATE, and GET_PROPERTY_DESCRIPTOR.
    if (act == js::Wrapper::GET || act == js::Wrapper::ENUMERATE ||
        act == js::Wrapper::GET_PROPERTY_DESCRIPTOR)
    {
        // Note that ReportWrapperDenial doesn't do any _exception_ reporting,
        // so we want to do this regardless of the value of mayThrow.
        return ReportWrapperDenial(cx, id, WrapperDenialForCOW,
                                   "Access to privileged JS object not permitted");
    }

    return false;
}

} // namespace xpc
