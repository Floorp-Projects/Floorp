/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "ChromeObjectWrapper.h"
#include "XrayWrapper.h"

#include "jsapi.h"

using namespace JS;
using namespace js;

namespace xpc {

template <typename Policy>
static bool
Filter(JSContext* cx, HandleObject wrapper, AutoIdVector& props)
{
    size_t w = 0;
    RootedId id(cx);
    for (size_t n = 0; n < props.length(); ++n) {
        id = props[n];
        if (Policy::check(cx, wrapper, id, Wrapper::GET) || Policy::check(cx, wrapper, id, Wrapper::SET))
            props[w++].set(id);
        else if (JS_IsExceptionPending(cx))
            return false;
    }
    props.resize(w);
    return true;
}

template <typename Policy>
static bool
FilterPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id, JS::MutableHandle<JSPropertyDescriptor> desc)
{
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    bool getAllowed = Policy::check(cx, wrapper, id, Wrapper::GET);
    if (JS_IsExceptionPending(cx))
        return false;
    bool setAllowed = Policy::check(cx, wrapper, id, Wrapper::SET);
    if (JS_IsExceptionPending(cx))
        return false;

    MOZ_ASSERT(getAllowed || setAllowed,
               "Filtering policy should not allow GET_PROPERTY_DESCRIPTOR in this case");

    if (!desc.hasGetterOrSetter()) {
        // Handle value properties.
        if (!getAllowed)
            desc.value().setUndefined();
    } else {
        // Handle accessor properties.
        MOZ_ASSERT(desc.value().isUndefined());
        if (!getAllowed)
            desc.setGetter(nullptr);
        if (!setAllowed)
            desc.setSetter(nullptr);
    }

    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getPropertyDescriptor(JSContext* cx, HandleObject wrapper,
                                                      HandleId id,
                                                      JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET |
                                         BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
    if (!Base::getPropertyDescriptor(cx, wrapper, id, desc))
        return false;
    return FilterPropertyDescriptor<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper,
                                                         HandleId id,
                                                         JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET |
                                         BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
    if (!Base::getOwnPropertyDescriptor(cx, wrapper, id, desc))
        return false;
    return FilterPropertyDescriptor<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::ownPropertyKeys(JSContext* cx, HandleObject wrapper,
                                                AutoIdVector& props) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    return Base::ownPropertyKeys(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnEnumerablePropertyKeys(JSContext* cx,
                                                             HandleObject wrapper,
                                                             AutoIdVector& props) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    return Base::getOwnEnumerablePropertyKeys(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerate(JSContext* cx, HandleObject wrapper,
                                          MutableHandleObject objp) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    // We refuse to trigger the enumerate hook across chrome wrappers because
    // we don't know how to censor custom iterator objects. Instead we trigger
    // the default proxy enumerate trap, which will use js::GetPropertyKeys
    // for the list of (censored) ids.
    return js::BaseProxyHandler::enumerate(cx, wrapper, objp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                    const JS::CallArgs& args) const
{
    if (!Policy::checkCall(cx, wrapper, args))
        return false;
    return Base::call(cx, wrapper, args);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::construct(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                          const JS::CallArgs& args) const
{
    if (!Policy::checkCall(cx, wrapper, args))
        return false;
    return Base::construct(cx, wrapper, args);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::nativeCall(JSContext* cx, JS::IsAcceptableThis test,
                                           JS::NativeImpl impl, JS::CallArgs args) const
{
    if (Policy::allowNativeCall(cx, test, impl))
        return Base::Permissive::nativeCall(cx, test, impl, args);
    return Base::Restrictive::nativeCall(cx, test, impl, args);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::defaultValue(JSContext* cx, HandleObject obj,
                                             JSType hint, MutableHandleValue vp) const
{
    return Base::defaultValue(cx, obj, hint, vp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getPrototype(JSContext* cx, JS::HandleObject wrapper,
                                             JS::MutableHandleObject protop) const
{
    // Filtering wrappers do not allow access to the prototype.
    protop.set(nullptr);
    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enter(JSContext* cx, HandleObject wrapper,
                                      HandleId id, Wrapper::Action act, bool* bp) const
{
    if (!Policy::check(cx, wrapper, id, act)) {
        *bp = JS_IsExceptionPending(cx) ? false : Policy::deny(act, id);
        return false;
    }
    *bp = true;
    return true;
}

CrossOriginXrayWrapper::CrossOriginXrayWrapper(unsigned flags) : SecurityXrayDOM(flags)
{
}

bool
CrossOriginXrayWrapper::getPropertyDescriptor(JSContext* cx,
                                              JS::Handle<JSObject*> wrapper,
                                              JS::Handle<jsid> id,
                                              JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!SecurityXrayDOM::getPropertyDescriptor(cx, wrapper, id, desc))
        return false;
    if (desc.object()) {
        // All properties on cross-origin DOM objects are |own|.
        desc.object().set(wrapper);

        // All properties on cross-origin DOM objects are non-enumerable and
        // "configurable". Any value attributes are read-only.
        desc.attributesRef() &= ~JSPROP_ENUMERATE;
        desc.attributesRef() &= ~JSPROP_PERMANENT;
        if (!desc.getter() && !desc.setter())
            desc.attributesRef() |= JSPROP_READONLY;
    }
    return true;
}

bool
CrossOriginXrayWrapper::getOwnPropertyDescriptor(JSContext* cx,
                                                 JS::Handle<JSObject*> wrapper,
                                                 JS::Handle<jsid> id,
                                                 JS::MutableHandle<JSPropertyDescriptor> desc) const
{
    // All properties on cross-origin DOM objects are |own|.
    return getPropertyDescriptor(cx, wrapper, id, desc);
}

bool
CrossOriginXrayWrapper::ownPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                        JS::AutoIdVector& props) const
{
    // All properties on cross-origin objects are supposed |own|, despite what
    // the underlying native object may report. Override the inherited trap to
    // avoid passing JSITER_OWNONLY as a flag.
    return SecurityXrayDOM::getPropertyKeys(cx, wrapper, JSITER_HIDDEN, props);
}

bool
CrossOriginXrayWrapper::defineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::Handle<JSPropertyDescriptor> desc,
                                       JS::ObjectOpResult& result) const
{
    JS_ReportError(cx, "Permission denied to define property on cross-origin object");
    return false;
}

bool
CrossOriginXrayWrapper::delete_(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                JS::Handle<jsid> id, JS::ObjectOpResult& result) const
{
    JS_ReportError(cx, "Permission denied to delete property on cross-origin object");
    return false;
}

#define XOW FilteringWrapper<CrossOriginXrayWrapper, CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>
#define NNXOWC FilteringWrapper<CrossCompartmentSecurityWrapper, OpaqueWithCall>

template<> const XOW XOW::singleton(0);
template<> const NNXOW NNXOW::singleton(0);
template<> const NNXOWC NNXOWC::singleton(0);

template class XOW;
template class NNXOW;
template class NNXOWC;
template class ChromeObjectWrapperBase;
} // namespace xpc
