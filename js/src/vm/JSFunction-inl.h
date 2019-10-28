/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSFunction_inl_h
#define vm_JSFunction_inl_h

#include "vm/JSFunction.h"

#include "gc/Allocator.h"
#include "gc/GCTrace.h"
#include "js/CharacterEncoding.h"
#include "vm/EnvironmentObject.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

inline const char* GetFunctionNameBytes(JSContext* cx, JSFunction* fun,
                                        UniqueChars* bytes) {
  if (JSAtom* name = fun->explicitName()) {
    *bytes = StringToNewUTF8CharsZ(cx, *name);
    return bytes->get();
  }
  return js_anonymous_str;
}

inline bool CanReuseFunctionForClone(JSContext* cx, HandleFunction fun) {
  if (!fun->isSingleton()) {
    return false;
  }
  if (fun->isInterpretedLazy()) {
    LazyScript* lazy = fun->lazyScript();
    if (lazy->hasBeenCloned()) {
      return false;
    }
    lazy->setHasBeenCloned();
  } else {
    JSScript* script = fun->nonLazyScript();
    if (script->hasBeenCloned()) {
      return false;
    }
    script->setHasBeenCloned();
    if (LazyScript* lazy = script->maybeLazyScript()) {
      lazy->setHasBeenCloned();
    }
  }
  return true;
}

inline JSFunction* CloneFunctionObjectIfNotSingleton(
    JSContext* cx, HandleFunction fun, HandleObject parent,
    HandleObject proto = nullptr, NewObjectKind newKind = GenericObject) {
  /*
   * For attempts to clone functions at a function definition opcode,
   * try to avoid the the clone if the function has singleton type. This
   * was called pessimistically, and we need to preserve the type's
   * property that if it is singleton there is only a single object
   * with its type in existence.
   *
   * For functions inner to run once lambda, it may be possible that
   * the lambda runs multiple times and we repeatedly clone it. In these
   * cases, fall through to CloneFunctionObject, which will deep clone
   * the function's script.
   */
  if (CanReuseFunctionForClone(cx, fun)) {
    ObjectOpResult succeeded;
    if (proto && !SetPrototype(cx, fun, proto, succeeded)) {
      return nullptr;
    }
    MOZ_ASSERT(!proto || succeeded);
    fun->setEnvironment(parent);
    return fun;
  }

  // These intermediate variables are needed to avoid link errors on some
  // platforms.  Sigh.
  gc::AllocKind finalizeKind = gc::AllocKind::FUNCTION;
  gc::AllocKind extendedFinalizeKind = gc::AllocKind::FUNCTION_EXTENDED;
  gc::AllocKind kind = fun->isExtended() ? extendedFinalizeKind : finalizeKind;

  if (CanReuseScriptForClone(cx->realm(), fun, parent)) {
    return CloneFunctionReuseScript(cx, fun, parent, kind, newKind, proto);
  }

  RootedScript script(cx, JSFunction::getOrCreateScript(cx, fun));
  if (!script) {
    return nullptr;
  }
  RootedScope enclosingScope(cx, script->enclosingScope());
  Rooted<ScriptSourceObject*> sourceObject(cx, script->sourceObject());
  return CloneFunctionAndScript(cx, fun, parent, enclosingScope, sourceObject,
                                kind, proto);
}

} /* namespace js */

/* static */ inline JS::Result<JSFunction*, JS::OOM&> JSFunction::create(
    JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
    js::HandleShape shape, js::HandleObjectGroup group) {
  MOZ_ASSERT(kind == js::gc::AllocKind::FUNCTION ||
             kind == js::gc::AllocKind::FUNCTION_EXTENDED);

  debugCheckNewObject(group, shape, kind, heap);

  const JSClass* clasp = group->clasp();
  MOZ_ASSERT(clasp->isJSFunction());

  static constexpr size_t NumDynamicSlots = 0;
  MOZ_ASSERT(dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(),
                               clasp) == NumDynamicSlots);

  JSObject* obj = js::AllocateObject(cx, kind, NumDynamicSlots, heap, clasp);
  if (!obj) {
    return cx->alreadyReportedOOM();
  }

  NativeObject* nobj = static_cast<NativeObject*>(obj);
  nobj->initGroup(group);
  nobj->initShape(shape);

  nobj->initSlots(nullptr);
  nobj->setEmptyElements();

  MOZ_ASSERT(!clasp->hasPrivate());
  MOZ_ASSERT(shape->slotSpan() == 0);

  JSFunction* fun = static_cast<JSFunction*>(nobj);
  fun->nargs_ = 0;

  // This must be overwritten by some ultimate caller: there's no default
  // value to which we could sensibly initialize this.
  MOZ_MAKE_MEM_UNDEFINED(&fun->u, sizeof(u));

  // Safe: we're initializing for the very first time.
  fun->atom_.unsafeSet(nullptr);

  if (kind == js::gc::AllocKind::FUNCTION_EXTENDED) {
    fun->setFlags(FunctionFlags::EXTENDED);
    for (js::GCPtrValue& extendedSlot : fun->toExtended()->extendedSlots) {
      extendedSlot.unsafeSet(JS::UndefinedValue());
    }
  } else {
    fun->setFlags(0);
  }

  MOZ_ASSERT(!clasp->shouldDelayMetadataBuilder(),
             "Function has no extra data hanging off it, that wouldn't be "
             "allocated at this point, that would require delaying the "
             "building of metadata for it");
  fun = SetNewObjectMetadata(cx, fun);

  js::gc::gcTracer.traceCreateObject(fun);

  return fun;
}

#endif /* vm_JSFunction_inl_h */
