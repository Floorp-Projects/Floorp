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

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::isSafeToUnwrap()
{
    return Policy::isSafeToUnwrap();
}

template <typename Policy>
static bool
Filter(JSContext *cx, JS::Handle<JSObject*> wrapper, AutoIdVector &props)
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
FilteringWrapper<Base, Policy>::getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                                      JS::Handle<jsid> id,
                                                      js::PropertyDescriptor *desc, unsigned flags)
{
    assertEnteredPolicy(cx, wrapper, id);
    if (!Base::getPropertyDescriptor(cx, wrapper, id, desc, flags))
        return false;
    return FilterSetter<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                                         JS::Handle<jsid> id,
                                                         js::PropertyDescriptor *desc,
                                                         unsigned flags)
{
    assertEnteredPolicy(cx, wrapper, id);
    if (!Base::getOwnPropertyDescriptor(cx, wrapper, id, desc, flags))
        return false;
    return FilterSetter<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                                    AutoIdVector &props)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID);
    return Base::getOwnPropertyNames(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                          AutoIdVector &props)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID);
    return Base::enumerate(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::keys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                     AutoIdVector &props)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID);
    return Base::keys(cx, wrapper, props) &&
           Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::iterate(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                        unsigned flags, JS::MutableHandle<JS::Value> vp)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID);
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
FilteringWrapper<Base, Policy>::enter(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                      JS::Handle<jsid> id, Wrapper::Action act, bool *bp)
{
    // This is a super ugly hacky to get around Xray Resolve wonkiness.
    //
    // Basically, XPCWN Xrays sometimes call into the Resolve hook of the
    // scriptable helper, and pass the wrapper itself as the object upon which
    // the resolve is happening. Then, special handling happens in
    // XrayWrapper::defineProperty to detect the resolve and redefine the
    // property on the holder. Really, we should just pass the holder itself to
    // NewResolve, but there's too much code in nsDOMClassInfo that assumes this
    // isn't the case (in particular, code expects to be able to look up
    // properties on the object, which doesn't work for the holder). Given that
    // these hooks are going away eventually with the new DOM bindings, let's
    // just hack around this for now.
    if (XrayUtils::IsXrayResolving(cx, wrapper, id)) {
        *bp = true;
        return true;
    }
    if (!Policy::check(cx, wrapper, id, act)) {
        *bp = JS_IsExceptionPending(cx) ? false : Policy::deny(act);
        return false;
    }
    *bp = true;
    return true;
}

#define SOW FilteringWrapper<CrossCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define SCSOW FilteringWrapper<SameCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define XOW FilteringWrapper<SecurityXrayXPCWN, CrossOriginAccessiblePropertiesOnly>
#define DXOW   FilteringWrapper<SecurityXrayDOM, CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>
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
