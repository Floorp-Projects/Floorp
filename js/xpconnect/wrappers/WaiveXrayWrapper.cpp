/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaiveXrayWrapper.h"
#include "WrapperFactory.h"
#include "jsapi.h"

using namespace JS;

namespace xpc {

static bool WaiveAccessors(JSContext* cx,
                           MutableHandle<PropertyDescriptor> desc) {
  if (desc.hasGetterObject() && desc.getterObject()) {
    RootedValue v(cx, JS::ObjectValue(*desc.getterObject()));
    if (!WrapperFactory::WaiveXrayAndWrap(cx, &v)) {
      return false;
    }
    desc.setGetterObject(&v.toObject());
  }

  if (desc.hasSetterObject() && desc.setterObject()) {
    RootedValue v(cx, JS::ObjectValue(*desc.setterObject()));
    if (!WrapperFactory::WaiveXrayAndWrap(cx, &v)) {
      return false;
    }
    desc.setSetterObject(&v.toObject());
  }
  return true;
}

bool WaiveXrayWrapper::getOwnPropertyDescriptor(
    JSContext* cx, HandleObject wrapper, HandleId id,
    MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) const {
  Rooted<mozilla::Maybe<PropertyDescriptor>> desc_(cx);
  if (!CrossCompartmentWrapper::getOwnPropertyDescriptor(cx, wrapper, id,
                                                         &desc_)) {
    return false;
  }

  if (desc_.isNothing()) {
    desc.reset();
    return true;
  }

  Rooted<PropertyDescriptor> pd(cx, *desc_);
  if (!WrapperFactory::WaiveXrayAndWrap(cx, pd.value()) ||
      !WaiveAccessors(cx, &pd)) {
    return false;
  }

  desc.set(mozilla::Some(pd.get()));
  return true;
}

bool WaiveXrayWrapper::get(JSContext* cx, HandleObject wrapper,
                           HandleValue receiver, HandleId id,
                           MutableHandleValue vp) const {
  return CrossCompartmentWrapper::get(cx, wrapper, receiver, id, vp) &&
         WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

bool WaiveXrayWrapper::call(JSContext* cx, HandleObject wrapper,
                            const JS::CallArgs& args) const {
  return CrossCompartmentWrapper::call(cx, wrapper, args) &&
         WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool WaiveXrayWrapper::construct(JSContext* cx, HandleObject wrapper,
                                 const JS::CallArgs& args) const {
  return CrossCompartmentWrapper::construct(cx, wrapper, args) &&
         WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

// NB: This is important as the other side of a handshake with FieldGetter. See
// nsXBLProtoImplField.cpp.
bool WaiveXrayWrapper::nativeCall(JSContext* cx, JS::IsAcceptableThis test,
                                  JS::NativeImpl impl,
                                  const JS::CallArgs& args) const {
  return CrossCompartmentWrapper::nativeCall(cx, test, impl, args) &&
         WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool WaiveXrayWrapper::hasInstance(JSContext* cx, HandleObject wrapper,
                                   MutableHandleValue v, bool* bp) const {
  if (v.isObject() && WrapperFactory::IsXrayWrapper(&v.toObject())) {
    // If |v| is a XrayWrapper and in the same compartment as the value
    // wrapped by |wrapper|, then the Xrays of |v| would be waived upon
    // calling CrossCompartmentWrapper::hasInstance. This may trigger
    // getters and proxy traps of unwrapped |v|. To prevent that from
    // happening, we exit early.

    // |wrapper| is the right operand of "instanceof", and must either be
    // a function or an object with a @@hasInstance method. We are not going
    // to call @@hasInstance, so only check whether it is a function.
    // This check is here for consistency with usual "instanceof" behavior,
    // which throws if the right operand is not a function. Without this
    // check, the "instanceof" operator would return false and potentially
    // hide errors in the code that uses the "instanceof" operator.
    if (!JS::IsCallable(wrapper)) {
      RootedValue wrapperv(cx, JS::ObjectValue(*wrapper));
      js::ReportIsNotFunction(cx, wrapperv);
      return false;
    }

    *bp = false;
    return true;
  }

  // Both |wrapper| and |v| have no Xrays here.
  return CrossCompartmentWrapper::hasInstance(cx, wrapper, v, bp);
}

bool WaiveXrayWrapper::getPrototype(JSContext* cx, HandleObject wrapper,
                                    MutableHandleObject protop) const {
  return CrossCompartmentWrapper::getPrototype(cx, wrapper, protop) &&
         (!protop || WrapperFactory::WaiveXrayAndWrap(cx, protop));
}

bool WaiveXrayWrapper::getPrototypeIfOrdinary(
    JSContext* cx, HandleObject wrapper, bool* isOrdinary,
    MutableHandleObject protop) const {
  return CrossCompartmentWrapper::getPrototypeIfOrdinary(cx, wrapper,
                                                         isOrdinary, protop) &&
         (!protop || WrapperFactory::WaiveXrayAndWrap(cx, protop));
}

}  // namespace xpc
