/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Stream handler for operations bound to a provided target. */

#ifndef builtin_streams_HandlerFunction_inl_h
#define builtin_streams_HandlerFunction_inl_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include <stddef.h>  // size_t

#include "gc/AllocKind.h"    // js::gc::AllocKind
#include "js/CallArgs.h"     // JS::CallArgs
#include "js/RootingAPI.h"   // JS::Handle, JS::Rooted
#include "js/Value.h"        // JS::ObjectValue
#include "vm/JSContext.h"    // JSContext
#include "vm/JSFunction.h"   // js::NewNativeFunction
#include "vm/JSObject.h"     // JSObject
#include "vm/ObjectGroup.h"  // js::GenericObject
#include "vm/StringType.h"   // js::PropertyName

#include "vm/JSContext-inl.h"  // JSContext::check

namespace js {

constexpr size_t StreamHandlerFunctionSlot_Target = 0;

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

/**
 * Helper for handler functions that "close over" a value that is always a
 * direct reference to an object of class T, never a wrapper.
 */
template <class T>
inline MOZ_MUST_USE T* TargetFromHandler(const JS::CallArgs& args) {
  JSFunction& func = args.callee().as<JSFunction>();
  return &func.getExtendedSlot(StreamHandlerFunctionSlot_Target)
              .toObject()
              .as<T>();
}

}  // namespace js

#endif  // builtin_streams_HandlerFunction_inl_h
