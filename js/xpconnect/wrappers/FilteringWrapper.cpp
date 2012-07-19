/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "WaiveXrayWrapper.h"
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

typedef Wrapper::Permission Permission;

static const Permission PermitObjectAccess = Wrapper::PermitObjectAccess;
static const Permission PermitPropertyAccess = Wrapper::PermitPropertyAccess;
static const Permission DenyAccess = Wrapper::DenyAccess;

template <typename Policy>
static bool
Filter(JSContext *cx, JSObject *wrapper, AutoIdVector &props)
{
    size_t w = 0;
    for (size_t n = 0; n < props.length(); ++n) {
        jsid id = props[n];
        Permission perm;
        if (!Policy::check(cx, wrapper, id, Wrapper::GET, perm))
            return false; // Error
        if (perm != DenyAccess)
            props[w++] = id;
    }
    props.resize(w);
    return true;
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
    // of (consored) ids.
    return js::BaseProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
bool
FilteringWrapper<Base, Policy>::enter(JSContext *cx, JSObject *wrapper, jsid id,
                                      Wrapper::Action act, bool *bp)
{
    Permission perm;
    if (!Policy::check(cx, wrapper, id, act, perm)) {
        *bp = false;
        return false;
    }
    *bp = true;
    if (perm == DenyAccess)
        return false;
    return Base::enter(cx, wrapper, id, act, bp);
}

#define SOW FilteringWrapper<CrossCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define SCSOW FilteringWrapper<SameCompartmentSecurityWrapper, OnlyIfSubjectIsSystem>
#define COW FilteringWrapper<CrossCompartmentSecurityWrapper, ExposedPropertiesOnly>
#define XOW FilteringWrapper<XrayWrapper<CrossCompartmentSecurityWrapper>, \
                             CrossOriginAccessiblePropertiesOnly>
#define PXOW   FilteringWrapper<XrayProxy, \
                                CrossOriginAccessiblePropertiesOnly>
#define DXOW   FilteringWrapper<XrayDOM, \
                                CrossOriginAccessiblePropertiesOnly>
#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, \
                               CrossOriginAccessiblePropertiesOnly>
#define LW    FilteringWrapper<XrayWrapper<SameCompartmentSecurityWrapper>, \
                               LocationPolicy>
#define XLW   FilteringWrapper<XrayWrapper<CrossCompartmentSecurityWrapper>, \
                               LocationPolicy>
#define CW FilteringWrapper<SameCompartmentSecurityWrapper, \
                            ComponentsObjectPolicy>
#define XCW FilteringWrapper<CrossCompartmentSecurityWrapper, \
                            ComponentsObjectPolicy>
template<> SOW SOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                              WrapperFactory::SOW_FLAG);
template<> SCSOW SCSOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                  WrapperFactory::SOW_FLAG);
template<> COW COW::singleton(0);
template<> XOW XOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                              WrapperFactory::PARTIALLY_TRANSPARENT);
template<> PXOW PXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                WrapperFactory::PARTIALLY_TRANSPARENT);
template<> DXOW DXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                WrapperFactory::PARTIALLY_TRANSPARENT);
template<> NNXOW NNXOW::singleton(WrapperFactory::SCRIPT_ACCESS_ONLY_FLAG |
                                  WrapperFactory::PARTIALLY_TRANSPARENT);
template<> LW  LW::singleton(WrapperFactory::SHADOWING_FORBIDDEN);
template<> XLW XLW::singleton(WrapperFactory::SHADOWING_FORBIDDEN);

template<> CW CW::singleton(0);
template<> XCW XCW::singleton(0);

template class SOW;
template class COW;
template class XOW;
template class PXOW;
template class DXOW;
template class NNXOW;
template class LW;
template class XLW;

}
