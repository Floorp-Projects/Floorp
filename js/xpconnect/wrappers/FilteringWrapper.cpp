/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "WaiveXrayWrapper.h"
#include "ChromeObjectWrapper.h"
#include "XrayWrapper.h"
#include "WrapperFactory.h"

#include "XPCWrapper.h"

#include "jsapi.h"

using namespace js;

namespace xpc {

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::FilteringWrapper(unsigned flags) : Base(flags)
{
}

template <typename Base, typename Policy>
FilteringWrapper<Base, Policy>::~FilteringWrapper()
{
}

template <typename Policy>
static bool
Filter(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    size_t w = 0;
    for (size_t n = 0; n < props.length(); ++n) {
        jsid id = props[n];
        if (Policy::check(cx, wrapper, id, Wrapper::GET))
            props[w++] = id;
        else if (JS_IsExceptionPending(cx))
            return false;
    }
    props.resize(w);
    return true;
}

template <typename Policy>
static bool
FilterSetter(JSContext *cx, JSObject *wrapper, jsid id, js::PropertyDescriptor *desc)
{
    bool setAllowed = Policy::check(cx, wrapper, id, Wrapper::SET);
    if (!setAllowed) {
        if (JS_IsExceptionPending(cx))
            return false;
        desc->setter = nullptr;
    }
    return true;
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                      js::PropertyDescriptor *desc, unsigned flags)
{
    if (!Base::getPropertyDescriptor(cx, wrapper, id, desc, flags))
        return false;
    return FilterSetter<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                         js::PropertyDescriptor *desc,
                                                         unsigned flags)
{
    if (!Base::getOwnPropertyDescriptor(cx, wrapper, id, desc, flags))
        return false;
    return FilterSetter<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::getOwnPropertyNames(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::enumerate(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    return Base::keys(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp)
{
    // We refuse to trigger the iterator hook across chrome wrappers because
    // we don't know how to censor custom iterator objects. Instead we trigger
    // the default proxy iterate trap, which will ask enumerate() for the list
    // of (censored) ids.
    return js::BaseProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::nativeCall(JSContext *cx, JS::IsAcceptableThis test,
                                           JS::NativeImpl impl, JS::CallArgs args)
{
    if (Policy::allowNativeCall(cx, test, impl))
        return Base::Permissive::nativeCall(cx, test, impl, args);
    return Base::Restrictive::nativeCall(cx, test, impl, args);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enter(JSContext *cx, JSObject *wrapper, jsid id,
                                      Wrapper::Action act, bool *bp)
{
    if (!Policy::check(cx, wrapper, id, act)) {
        if (JS_IsExceptionPending(cx)) {
            *bp = false;
            return false;
        }
        JSAutoCompartment ac(cx, wrapper);
        *bp = Policy::deny(cx, id, act);
        return false;
    }
    *bp = true;
    return true;
}

#define SOW FilteringWrapper<CrossCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define SCSOW FilteringWrapper<SameCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define XOW FilteringWrapper<SecurityXrayXPCWN, CrossOriginAccessiblePropertiesOnly>
#define DXOW   FilteringWrapper<SecurityXrayDOM, CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, CrossOriginAccessiblePropertiesOnly>
#define CW FilteringWrapper<SameCompartmentSecurityWrapper, ComponentsObjectPolicy>
#define XCW FilteringWrapper<CrossCompartmentSecurityWrapper, ComponentsObjectPolicy>
template<> SOW SOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                              WrapperFactory::SOW_FLAG);
template<> SCSOW SCSOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                  WrapperFactory::SOW_FLAG);
template<> XOW XOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG);
template<> DXOW DXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG);
template<> NNXOW NNXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG);

template<> CW CW::singleton(0);
template<> XCW XCW::singleton(0);

template class SOW;
template class XOW;
template class DXOW;
template class NNXOW;
template class ChromeObjectWrapperBase;
}
