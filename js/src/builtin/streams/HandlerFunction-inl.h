/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Stream handler for operations that act on a target object, and possibly upon
 * an extra value.
 */

#ifndef builtin_streams_HandlerFunction_inl_h
#define builtin_streams_HandlerFunction_inl_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include <stddef.h>  // size_t

#include "gc/AllocKind.h"    // js::gc::AllocKind
#include "js/CallArgs.h"     // JS::CallArgs
#include "js/RootingAPI.h"   // JS::Handle, JS::Rooted
#include "js/Value.h"        // JS::ObjectValue
#include "vm/JSContext.h"    // JSContext
#include "vm/JSFunction.h"   // JSFunction, js::Native, js::NewNativeFunction
#include "vm/JSObject.h"     // JSObject
#include "vm/ObjectGroup.h"  // js::GenericObject
#include "vm/StringType.h"   // js::PropertyName

#include "vm/JSContext-inl.h"  // JSContext::check

namespace js {

// Handler functions are extended functions, that close over a target object and
// (optionally) over an extra object, storing those objects in the function's
// extended slots.
constexpr size_t StreamHandlerFunctionSlot_Target = 0;
constexpr size_t StreamHandlerFunctionSlot_Extra = 1;

static_assert(StreamHandlerFunctionSlot_Extra <
                  FunctionExtended::NUM_EXTENDED_SLOTS,
              "handler function slots shouldn't exceed available extended "
              "slots");

inline MOZ_MUST_USE JSFunction* NewHandler(JSContext* cx, Native handler,
                                           JS::Handle<JSObject*> target) {
  cx->check(target);

  JS::Handle<PropertyName*> funName = cx->names().empty;
  JS::Rooted<JSFunction*> handlerFun(
      cx, NewNativeFunction(cx, handler, 0, funName,
                            gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
  if (!handlerFun) {
    return nullptr;
  }
  handlerFun->setExtendedSlot(StreamHandlerFunctionSlot_Target,
                              JS::ObjectValue(*target));
  return handlerFun;
}

inline MOZ_MUST_USE JSFunction* NewHandlerWithExtra(
    JSContext* cx, Native handler, JS::Handle<JSObject*> target,
    JS::Handle<JSObject*> extra) {
  cx->check(extra);
  JSFunction* handlerFun = NewHandler(cx, handler, target);
  if (handlerFun) {
    handlerFun->setExtendedSlot(StreamHandlerFunctionSlot_Extra,
                                JS::ObjectValue(*extra));
  }
  return handlerFun;
}

inline MOZ_MUST_USE JSFunction* NewHandlerWithExtraValue(
    JSContext* cx, Native handler, JS::Handle<JSObject*> target,
    JS::Handle<JS::Value> extra) {
  cx->check(extra);
  JSFunction* handlerFun = NewHandler(cx, handler, target);
  if (handlerFun) {
    handlerFun->setExtendedSlot(StreamHandlerFunctionSlot_Extra, extra);
  }
  return handlerFun;
}

/**
 * Within the call of a handler function that "closes over" a target value that
 * is always a |T*| object (and never a wrapper around one), return that |T*|.
 */
template <class T>
inline MOZ_MUST_USE T* TargetFromHandler(const JS::CallArgs& args) {
  JSFunction& func = args.callee().as<JSFunction>();
  return &func.getExtendedSlot(StreamHandlerFunctionSlot_Target)
              .toObject()
              .as<T>();
}

/**
 * Within the call of a handler function that "closes over" a target value and
 * an extra value, return that extra value.
 */
inline MOZ_MUST_USE JS::Value ExtraValueFromHandler(const JS::CallArgs& args) {
  JSFunction& func = args.callee().as<JSFunction>();
  return func.getExtendedSlot(StreamHandlerFunctionSlot_Extra);
}

/**
 * Within the call of a handler function that "closes over" a target value and
 * an extra value, where that extra value is always a |T*| object (and never a
 * wrapper around one), return that |T*|.
 */
template <class T>
inline MOZ_MUST_USE T* ExtraFromHandler(const JS::CallArgs& args) {
  return &ExtraValueFromHandler(args).toObject().as<T>();
}

}  // namespace js

#endif  // builtin_streams_HandlerFunction_inl_h
