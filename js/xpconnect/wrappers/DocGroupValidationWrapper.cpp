/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocGroupValidationWrapper.h"
#include "AddonWrapper.h"
#include "WaiveXrayWrapper.h"

#include "mozilla/dom/DocGroup.h"
#include "nsGlobalWindow.h"

using namespace xpc;

static bool
AccessAllowed(JSContext* cx, JS::Handle<JSObject*> wrapper)
{
    RootedObject unwrapped(cx, UncheckedUnwrap(wrapper));
    RefPtr<nsGlobalWindow> window = WindowGlobalOrNull(unwrapped);
    if (!window) {
        // Access to non-windows is always kosher.
        return true;
    }

    if (window->GetDocGroup()->AccessAllowed())
        return true;

    static bool sThrowOnMismatch;
    static bool sPrefInitialized;
    if (!sPrefInitialized) {
        sPrefInitialized = true;
        Preferences::AddBoolVarCache(&sThrowOnMismatch,
                                     "extensions.throw_on_docgroup_mismatch.enabled");
    }

    // If DocGroup validation is disabled, don't throw.
    if (!sThrowOnMismatch)
        return true;

    JS_ReportErrorASCII(cx, "accessing object in wrong DocGroup");
    return false;
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                                          MutableHandle<PropertyDescriptor> desc) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::getOwnPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                                                Handle<PropertyDescriptor> desc,
                                                ObjectOpResult& result) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::defineProperty(cx, wrapper, id, desc, result);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::ownPropertyKeys(JSContext* cx, HandleObject wrapper,
                                                 AutoIdVector& props) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::ownPropertyKeys(cx, wrapper, props);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::delete_(JSContext* cx, HandleObject wrapper, HandleId id,
                                         ObjectOpResult& result) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::delete_(cx, wrapper, id, result);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::enumerate(JSContext* cx, HandleObject wrapper, MutableHandleObject objp) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::enumerate(cx, wrapper, objp);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::getPrototype(JSContext* cx, HandleObject proxy,
                                              MutableHandleObject protop) const
{
    return AccessAllowed(cx, proxy) &&
           Base::getPrototype(cx, proxy, protop);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::setPrototype(JSContext* cx, HandleObject proxy, HandleObject proto,
                                              ObjectOpResult& result) const
{
    return AccessAllowed(cx, proxy) &&
           Base::setPrototype(cx, proxy, proto, result);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                                        MutableHandleObject protop) const
{
    return AccessAllowed(cx, proxy) &&
           Base::getPrototypeIfOrdinary(cx, proxy, isOrdinary, protop);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::setImmutablePrototype(JSContext* cx, HandleObject proxy,
                                                       bool* succeeded) const
{
    return AccessAllowed(cx, proxy) &&
           Base::setImmutablePrototype(cx, proxy, succeeded);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::preventExtensions(JSContext* cx, HandleObject wrapper,
                                                   ObjectOpResult& result) const
{
    return AccessAllowed(cx, wrapper) &&
        Base::preventExtensions(cx, wrapper, result);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::isExtensible(JSContext* cx, HandleObject wrapper, bool* extensible) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::isExtensible(cx, wrapper, extensible);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::has(JSContext* cx, HandleObject wrapper, HandleId id, bool* bp) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::has(cx, wrapper, id, bp);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::get(JSContext* cx, HandleObject wrapper, HandleValue receiver,
                                     HandleId id, MutableHandleValue vp) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::get(cx, wrapper, receiver, id, vp);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::set(JSContext* cx, HandleObject wrapper, HandleId id, HandleValue v,
                                     HandleValue receiver, ObjectOpResult& result) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::set(cx, wrapper, id, v, receiver, result);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::call(JSContext* cx, HandleObject wrapper, const CallArgs& args) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::call(cx, wrapper, args);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::construct(JSContext* cx, HandleObject wrapper, const CallArgs& args) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::construct(cx, wrapper, args);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::getPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                                       MutableHandle<PropertyDescriptor> desc) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::getPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::hasOwn(JSContext* cx, HandleObject wrapper, HandleId id, bool* bp) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::hasOwn(cx, wrapper, id, bp);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject wrapper,
                                                              AutoIdVector& props) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::getOwnEnumerablePropertyKeys(cx, wrapper, props);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::hasInstance(JSContext* cx, HandleObject wrapper, MutableHandleValue v,
                                             bool* bp) const
{
    return AccessAllowed(cx, wrapper) &&
           Base::hasInstance(cx, wrapper, v, bp);
}

template<typename Base>
const char*
DocGroupValidationWrapper<Base>::className(JSContext* cx, HandleObject proxy) const
{
    return AccessAllowed(cx, proxy)
         ? Base::className(cx, proxy)
         : "forbidden";
}

template<typename Base>
JSString*
DocGroupValidationWrapper<Base>::fun_toString(JSContext* cx, HandleObject wrapper,
                                              unsigned indent) const
{
    return AccessAllowed(cx, wrapper)
           ? Base::fun_toString(cx, wrapper, indent)
           : nullptr;
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::regexp_toShared(JSContext* cx, HandleObject proxy, MutableHandle<RegExpShared*> g) const
{
    return AccessAllowed(cx, proxy) &&
           Base::regexp_toShared(cx, proxy, g);
}

template<typename Base>
bool
DocGroupValidationWrapper<Base>::boxedValue_unbox(JSContext* cx, HandleObject proxy, MutableHandleValue vp) const
{
    return AccessAllowed(cx, proxy) &&
           Base::boxedValue_unbox(cx, proxy, vp);
}

namespace xpc {

template<typename Base>
const DocGroupValidationWrapper<Base> DocGroupValidationWrapper<Base>::singleton(0);

#define DEFINE_SINGLETON(Base)                          \
    template class DocGroupValidationWrapper<Base>

DEFINE_SINGLETON(CrossCompartmentWrapper);
DEFINE_SINGLETON(PermissiveXrayXPCWN);
DEFINE_SINGLETON(PermissiveXrayDOM);
DEFINE_SINGLETON(PermissiveXrayJS);

DEFINE_SINGLETON(AddonWrapper<CrossCompartmentWrapper>);
DEFINE_SINGLETON(AddonWrapper<PermissiveXrayXPCWN>);
DEFINE_SINGLETON(AddonWrapper<PermissiveXrayDOM>);

DEFINE_SINGLETON(WaiveXrayWrapper);

} // namespace xpc
