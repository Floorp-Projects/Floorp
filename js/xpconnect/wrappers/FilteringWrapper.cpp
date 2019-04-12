/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilteringWrapper.h"
#include "AccessCheck.h"
#include "ChromeObjectWrapper.h"
#include "XrayWrapper.h"
#include "nsJSUtils.h"
#include "mozilla/ErrorResult.h"
#include "xpcpublic.h"

#include "jsapi.h"
#include "js/Symbol.h"

using namespace JS;
using namespace js;

namespace xpc {

static JS::SymbolCode sCrossOriginWhitelistedSymbolCodes[] = {
    JS::SymbolCode::toStringTag, JS::SymbolCode::hasInstance,
    JS::SymbolCode::isConcatSpreadable};

static bool IsCrossOriginWhitelistedSymbol(JSContext* cx, JS::HandleId id) {
  if (!JSID_IS_SYMBOL(id)) {
    return false;
  }

  JS::Symbol* symbol = JSID_TO_SYMBOL(id);
  for (auto code : sCrossOriginWhitelistedSymbolCodes) {
    if (symbol == JS::GetWellKnownSymbol(cx, code)) {
      return true;
    }
  }

  return false;
}

bool IsCrossOriginWhitelistedProp(JSContext* cx, JS::HandleId id) {
  return id == GetJSIDByIndex(cx, XPCJSContext::IDX_THEN) ||
         IsCrossOriginWhitelistedSymbol(cx, id);
}

bool AppendCrossOriginWhitelistedPropNames(JSContext* cx,
                                           JS::MutableHandleIdVector props) {
  // Add "then" if it's not already in the list.
  RootedIdVector thenProp(cx);
  if (!thenProp.append(GetJSIDByIndex(cx, XPCJSContext::IDX_THEN))) {
    return false;
  }

  if (!AppendUnique(cx, props, thenProp)) {
    return false;
  }

  // Now add the three symbol-named props cross-origin objects have.
#ifdef DEBUG
  for (size_t n = 0; n < props.length(); ++n) {
    MOZ_ASSERT(!JSID_IS_SYMBOL(props[n]),
               "Unexpected existing symbol-name prop");
  }
#endif
  if (!props.reserve(props.length() +
                     ArrayLength(sCrossOriginWhitelistedSymbolCodes))) {
    return false;
  }

  for (auto code : sCrossOriginWhitelistedSymbolCodes) {
    props.infallibleAppend(SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, code)));
  }

  return true;
}

template <typename Policy>
static bool Filter(JSContext* cx, HandleObject wrapper,
                   MutableHandleIdVector props) {
  size_t w = 0;
  RootedId id(cx);
  for (size_t n = 0; n < props.length(); ++n) {
    id = props[n];
    if (Policy::check(cx, wrapper, id, Wrapper::GET) ||
        Policy::check(cx, wrapper, id, Wrapper::SET)) {
      props[w++].set(id);
    } else if (JS_IsExceptionPending(cx)) {
      return false;
    }
  }
  if (!props.resize(w)) {
    return false;
  }

  return true;
}

template <typename Policy>
static bool FilterPropertyDescriptor(JSContext* cx, HandleObject wrapper,
                                     HandleId id,
                                     MutableHandle<PropertyDescriptor> desc) {
  MOZ_ASSERT(!JS_IsExceptionPending(cx));
  bool getAllowed = Policy::check(cx, wrapper, id, Wrapper::GET);
  if (JS_IsExceptionPending(cx)) {
    return false;
  }
  bool setAllowed = Policy::check(cx, wrapper, id, Wrapper::SET);
  if (JS_IsExceptionPending(cx)) {
    return false;
  }

  MOZ_ASSERT(
      getAllowed || setAllowed,
      "Filtering policy should not allow GET_PROPERTY_DESCRIPTOR in this case");

  if (!desc.hasGetterOrSetter()) {
    // Handle value properties.
    if (!getAllowed) {
      desc.value().setUndefined();
    }
  } else {
    // Handle accessor properties.
    MOZ_ASSERT(desc.value().isUndefined());
    if (!getAllowed) {
      desc.setGetter(nullptr);
    }
    if (!setAllowed) {
      desc.setSetter(nullptr);
    }
  }

  return true;
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(
    JSContext* cx, HandleObject wrapper, HandleId id,
    MutableHandle<PropertyDescriptor> desc) const {
  assertEnteredPolicy(cx, wrapper, id,
                      BaseProxyHandler::GET | BaseProxyHandler::SET |
                          BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
  if (!Base::getOwnPropertyDescriptor(cx, wrapper, id, desc)) {
    return false;
  }
  return FilterPropertyDescriptor<Policy>(cx, wrapper, id, desc);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::ownPropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
  return Base::ownPropertyKeys(cx, wrapper, props) &&
         Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::getOwnEnumerablePropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
  return Base::getOwnEnumerablePropertyKeys(cx, wrapper, props) &&
         Filter<Policy>(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::enumerate(
    JSContext* cx, HandleObject wrapper,
    JS::MutableHandleIdVector props) const {
  assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
  // Trigger the default proxy enumerate trap, which will use
  // js::GetPropertyKeys for the list of (censored) ids.
  return js::BaseProxyHandler::enumerate(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::call(JSContext* cx,
                                          JS::Handle<JSObject*> wrapper,
                                          const JS::CallArgs& args) const {
  if (!Policy::checkCall(cx, wrapper, args)) {
    return false;
  }
  return Base::call(cx, wrapper, args);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::construct(JSContext* cx,
                                               JS::Handle<JSObject*> wrapper,
                                               const JS::CallArgs& args) const {
  if (!Policy::checkCall(cx, wrapper, args)) {
    return false;
  }
  return Base::construct(cx, wrapper, args);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::nativeCall(
    JSContext* cx, JS::IsAcceptableThis test, JS::NativeImpl impl,
    const JS::CallArgs& args) const {
  if (Policy::allowNativeCall(cx, test, impl)) {
    return Base::Permissive::nativeCall(cx, test, impl, args);
  }
  return Base::Restrictive::nativeCall(cx, test, impl, args);
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::getPrototype(
    JSContext* cx, JS::HandleObject wrapper,
    JS::MutableHandleObject protop) const {
  // Filtering wrappers do not allow access to the prototype.
  protop.set(nullptr);
  return true;
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::enter(JSContext* cx, HandleObject wrapper,
                                           HandleId id, Wrapper::Action act,
                                           bool mayThrow, bool* bp) const {
  if (!Policy::check(cx, wrapper, id, act)) {
    *bp =
        JS_IsExceptionPending(cx) ? false : Policy::deny(cx, act, id, mayThrow);
    return false;
  }
  *bp = true;
  return true;
}

#define NNXOW FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>
#define NNXOWC FilteringWrapper<CrossCompartmentSecurityWrapper, OpaqueWithCall>

template <>
const NNXOW NNXOW::singleton(0);
template <>
const NNXOWC NNXOWC::singleton(0);

template class NNXOW;
template class NNXOWC;
template class ChromeObjectWrapperBase;
}  // namespace xpc
