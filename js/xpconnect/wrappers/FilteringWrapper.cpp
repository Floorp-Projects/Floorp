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

// Note: Previously, FilteringWrapper supported complex access policies where
// certain properties on an object were accessible and others weren't. Today,
// the only supported policies are Opaque and OpaqueWithCall, none of which need
// that. So we just stub out the unreachable paths.
template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::getOwnPropertyDescriptor(
    JSContext* cx, HandleObject wrapper, HandleId id,
    MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc) const {
  MOZ_CRASH("FilteringWrappers are now always opaque");
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::ownPropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  MOZ_CRASH("FilteringWrappers are now always opaque");
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::getOwnEnumerablePropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  MOZ_CRASH("FilteringWrappers are now always opaque");
}

template <typename Base, typename Policy>
bool FilteringWrapper<Base, Policy>::enumerate(
    JSContext* cx, HandleObject wrapper,
    JS::MutableHandleIdVector props) const {
  MOZ_CRASH("FilteringWrappers are now always opaque");
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
