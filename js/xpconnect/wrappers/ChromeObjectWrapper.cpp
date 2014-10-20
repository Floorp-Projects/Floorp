/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeObjectWrapper.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "xpcprivate.h"
#include "jsapi.h"
#include "jswrapper.h"
#include "nsXULAppAPI.h"

using namespace JS;

namespace xpc {

// When creating wrappers for chrome objects in content, we detect if the
// prototype of the wrapped chrome object is a prototype for a standard class
// (like Array.prototype). If it is, we use the corresponding standard prototype
// from the wrapper's scope, rather than the wrapped standard prototype
// from the wrappee's scope.
//
// One of the reasons for doing this is to allow standard operations like
// chromeArray.forEach(..) to Just Work without explicitly listing them in
// __exposedProps__. Since proxies don't automatically inherit behavior from
// their prototype, we have to instrument the traps to do this manually.
const ChromeObjectWrapper ChromeObjectWrapper::singleton;

using js::assertEnteredPolicy;

static bool
AllowedByBase(JSContext *cx, HandleObject wrapper, HandleId id,
              js::Wrapper::Action act)
{
    MOZ_ASSERT(js::Wrapper::wrapperHandler(wrapper) ==
               &ChromeObjectWrapper::singleton);
    bool bp;
    const ChromeObjectWrapper *handler = &ChromeObjectWrapper::singleton;
    return handler->ChromeObjectWrapperBase::enter(cx, wrapper, id, act, &bp);
}

static bool
PropIsFromStandardPrototype(JSContext *cx, JS::MutableHandle<JSPropertyDescriptor> desc)
{
    MOZ_ASSERT(desc.object());
    RootedObject unwrapped(cx, js::UncheckedUnwrap(desc.object()));
    JSAutoCompartment ac(cx, unwrapped);
    return IdentifyStandardPrototype(unwrapped) != JSProto_Null;
}

// Note that we're past the policy enforcement stage, here, so we can query
// CrossCompartmentSecurityWrapper (our grand-parent wrapper) and get an
// unfiltered view of the underlying object. This lets us determine whether
// the property we would have found (given a transparent wrapper) would
// have come off a standard prototype.
static bool
PropIsFromStandardPrototype(JSContext *cx, HandleObject wrapper,
                            HandleId id)
{
    MOZ_ASSERT(js::Wrapper::wrapperHandler(wrapper) ==
               &ChromeObjectWrapper::singleton);
    Rooted<JSPropertyDescriptor> desc(cx);
    const ChromeObjectWrapper *handler = &ChromeObjectWrapper::singleton;
    if (!handler->js::CrossCompartmentSecurityWrapper::getPropertyDescriptor(cx, wrapper, id,
                                                                             &desc) ||
        !desc.object())
    {
        return false;
    }
    return PropIsFromStandardPrototype(cx, &desc);
}

bool
ChromeObjectWrapper::getPropertyDescriptor(JSContext *cx,
                                           HandleObject wrapper,
                                           HandleId id,
                                           JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, wrapper, id, GET | SET | GET_PROPERTY_DESCRIPTOR);
    // First, try a lookup on the base wrapper if permitted.
    desc.object().set(nullptr);
    if (AllowedByBase(cx, wrapper, id, Wrapper::GET) &&
        !ChromeObjectWrapperBase::getPropertyDescriptor(cx, wrapper, id,
                                                        desc)) {
        return false;
    }

    // If the property is something that can be found on a standard prototype,
    // prefer the one we'll get via the prototype chain in the content
    // compartment.
    if (desc.object() && PropIsFromStandardPrototype(cx, desc))
        desc.object().set(nullptr);

    // If we found something or have no proto, we're done.
    RootedObject wrapperProto(cx);
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
      return false;
    if (desc.object() || !wrapperProto)
        return true;

    // If not, try doing the lookup on the prototype.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    return JS_GetPropertyDescriptorById(cx, wrapperProto, id, desc);
}

bool
ChromeObjectWrapper::defineProperty(JSContext *cx, HandleObject wrapper,
                                    HandleId id,
                                    MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!AccessCheck::checkPassToPrivilegedCode(cx, wrapper, desc.value()))
        return false;
    return ChromeObjectWrapperBase::defineProperty(cx, wrapper, id, desc);
}

bool
ChromeObjectWrapper::set(JSContext *cx, HandleObject wrapper,
                         HandleObject receiver, HandleId id,
                         bool strict, MutableHandleValue vp) const
{
    if (!AccessCheck::checkPassToPrivilegedCode(cx, wrapper, vp))
        return false;
    return ChromeObjectWrapperBase::set(cx, wrapper, receiver, id, strict, vp);
}



bool
ChromeObjectWrapper::has(JSContext *cx, HandleObject wrapper,
                         HandleId id, bool *bp) const
{
    assertEnteredPolicy(cx, wrapper, id, GET);
    // Try the lookup on the base wrapper if permitted.
    if (AllowedByBase(cx, wrapper, id, js::Wrapper::GET) &&
        !ChromeObjectWrapperBase::has(cx, wrapper, id, bp))
    {
        return false;
    }

    // If we found something or have no prototype, we're done.
    RootedObject wrapperProto(cx);
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
        return false;
    if (*bp || !wrapperProto)
        return true;

    // Try the prototype if that failed.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, wrapperProto, id, &desc))
        return false;
    *bp = !!desc.object();
    return true;
}

bool
ChromeObjectWrapper::get(JSContext *cx, HandleObject wrapper,
                         HandleObject receiver, HandleId id,
                         MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, wrapper, id, GET);
    vp.setUndefined();
    // Only call through to the get trap on the underlying object if we're
    // allowed to see the property, and if what we'll find is not on a standard
    // prototype.
    if (AllowedByBase(cx, wrapper, id, js::Wrapper::GET) &&
        !PropIsFromStandardPrototype(cx, wrapper, id))
    {
        // Call the get trap.
        if (!ChromeObjectWrapperBase::get(cx, wrapper, receiver, id, vp))
            return false;
        // If we found something, we're done.
        if (!vp.isUndefined())
            return true;
    }

    // If we have no proto, we're done.
    RootedObject wrapperProto(cx);
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
        return false;
    if (!wrapperProto)
        return true;

    // Try the prototype.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    return js::GetGeneric(cx, wrapperProto, receiver, id, vp.address());
}

// SecurityWrapper categorically returns false for objectClassIs, but the
// contacts API depends on Array.isArray returning true for COW-implemented
// contacts. This isn't really ideal, but make it work for now.
bool
ChromeObjectWrapper::objectClassIs(HandleObject obj, js::ESClassValue classValue,
                                   JSContext *cx) const
{
  return CrossCompartmentWrapper::objectClassIs(obj, classValue, cx);
}

// This mechanism isn't ideal because we end up calling enter() on the base class
// twice (once during enter() here and once during the trap itself), and policy
// enforcement or COWs isn't cheap. But it results in the cleanest code, and this
// whole proto remapping thing for COWs is going to be phased out anyway.
bool
ChromeObjectWrapper::enter(JSContext *cx, HandleObject wrapper,
                           HandleId id, js::Wrapper::Action act, bool *bp) const
{
    if (AllowedByBase(cx, wrapper, id, act))
        return true;
    // COWs fail silently for GETs, and that also happens to be the only case
    // where we might want to redirect the lookup to the home prototype chain.
    *bp = (act == Wrapper::GET || act == Wrapper::ENUMERATE ||
           act == Wrapper::GET_PROPERTY_DESCRIPTOR) && !JS_IsExceptionPending(cx);
    if (!*bp || id == JSID_VOID)
        return false;

    // Note that PropIsFromStandardPrototype needs to invoke getPropertyDescriptor
    // before we've fully entered the policy. Waive our policy.
    js::AutoWaivePolicy policy(cx, wrapper, id, act);
    return PropIsFromStandardPrototype(cx, wrapper, id);
}

}
