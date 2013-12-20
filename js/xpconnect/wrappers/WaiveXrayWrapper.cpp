/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaiveXrayWrapper.h"
#include "WrapperFactory.h"
#include "jsapi.h"

using namespace JS;

namespace xpc {

static bool
WaiveAccessors(JSContext *cx, JS::MutableHandle<JSPropertyDescriptor> desc)
{
    if (desc.hasGetterObject() && desc.getterObject()) {
        RootedValue v(cx, JS::ObjectValue(*desc.getterObject()));
        if (!WrapperFactory::WaiveXrayAndWrap(cx, &v))
            return false;
        desc.setGetterObject(&v.toObject());
    }

    if (desc.hasSetterObject() && desc.setterObject()) {
        RootedValue v(cx, JS::ObjectValue(*desc.setterObject()));
        if (!WrapperFactory::WaiveXrayAndWrap(cx, &v))
            return false;
        desc.setSetterObject(&v.toObject());
    }
    return true;
}

WaiveXrayWrapper::WaiveXrayWrapper(unsigned flags) : js::CrossCompartmentWrapper(flags)
{
}

WaiveXrayWrapper::~WaiveXrayWrapper()
{
}

bool
WaiveXrayWrapper::getPropertyDescriptor(JSContext *cx, HandleObject wrapper,
                                        HandleId id, JS::MutableHandle<JSPropertyDescriptor> desc,
                                        unsigned flags)
{
    return CrossCompartmentWrapper::getPropertyDescriptor(cx, wrapper, id, desc, flags) &&
           WrapperFactory::WaiveXrayAndWrap(cx, desc.value()) && WaiveAccessors(cx, desc);
}

bool
WaiveXrayWrapper::getOwnPropertyDescriptor(JSContext *cx, HandleObject wrapper,
                                           HandleId id, JS::MutableHandle<JSPropertyDescriptor> desc,
                                           unsigned flags)
{
    return CrossCompartmentWrapper::getOwnPropertyDescriptor(cx, wrapper, id, desc, flags) &&
           WrapperFactory::WaiveXrayAndWrap(cx, desc.value()) && WaiveAccessors(cx, desc);
}

bool
WaiveXrayWrapper::get(JSContext *cx, HandleObject wrapper,
                      HandleObject receiver, HandleId id,
                      MutableHandleValue vp)
{
    return CrossCompartmentWrapper::get(cx, wrapper, receiver, id, vp) &&
           WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

bool
WaiveXrayWrapper::call(JSContext *cx, HandleObject wrapper, const JS::CallArgs &args)
{
    return CrossCompartmentWrapper::call(cx, wrapper, args) &&
           WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool
WaiveXrayWrapper::construct(JSContext *cx, HandleObject wrapper, const JS::CallArgs &args)
{
    return CrossCompartmentWrapper::construct(cx, wrapper, args) &&
           WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

// NB: This is important as the other side of a handshake with FieldGetter. See
// nsXBLProtoImplField.cpp.
bool
WaiveXrayWrapper::nativeCall(JSContext *cx, JS::IsAcceptableThis test,
                             JS::NativeImpl impl, JS::CallArgs args)
{
    return CrossCompartmentWrapper::nativeCall(cx, test, impl, args) &&
           WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool
WaiveXrayWrapper::getPrototypeOf(JSContext *cx, HandleObject wrapper, MutableHandleObject protop)
{
    return CrossCompartmentWrapper::getPrototypeOf(cx, wrapper, protop) &&
           (!protop || WrapperFactory::WaiveXrayAndWrap(cx, protop));
}

}
