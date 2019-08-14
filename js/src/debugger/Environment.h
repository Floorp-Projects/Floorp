/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Environment_h
#define debugger_Environment_h

#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT
#include "mozilla/Attributes.h"  // for MOZ_MUST_USE
#include "mozilla/Maybe.h"       // for Maybe

#include "NamespaceImports.h"   // for Value, HandleId, HandleObject
#include "debugger/Debugger.h"  // for Env
#include "gc/Rooting.h"         // for HandleDebuggerEnvironment
#include "js/PropertySpec.h"    // for JSFunctionSpec, JSPropertySpec
#include "js/RootingAPI.h"      // for Handle, MutableHandle
#include "vm/NativeObject.h"    // for NativeObject
#include "vm/Scope.h"           // for ScopeKind

class JSObject;
class JSTracer;
struct JSContext;

namespace js {

class GlobalObject;
struct Class;
struct ClassOps;

enum class DebuggerEnvironmentType { Declarative, With, Object };

class DebuggerEnvironment : public NativeObject {
 public:
  enum { OWNER_SLOT };

  static const unsigned RESERVED_SLOTS = 1;

  static const Class class_;

  static NativeObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                                 HandleObject dbgCtor);
  static DebuggerEnvironment* create(JSContext* cx, HandleObject proto,
                                     HandleObject referent,
                                     HandleNativeObject debugger);

  void trace(JSTracer* trc);

  DebuggerEnvironmentType type() const;
  mozilla::Maybe<ScopeKind> scopeKind() const;
  MOZ_MUST_USE bool getParent(JSContext* cx,
                              MutableHandleDebuggerEnvironment result) const;
  MOZ_MUST_USE bool getObject(JSContext* cx,
                              MutableHandleDebuggerObject result) const;
  MOZ_MUST_USE bool getCallee(JSContext* cx,
                              MutableHandleDebuggerObject result) const;
  bool isDebuggee() const;
  bool isOptimized() const;

  static MOZ_MUST_USE bool getNames(JSContext* cx,
                                    HandleDebuggerEnvironment environment,
                                    MutableHandle<IdVector> result);
  static MOZ_MUST_USE bool find(JSContext* cx,
                                HandleDebuggerEnvironment environment,
                                HandleId id,
                                MutableHandleDebuggerEnvironment result);
  static MOZ_MUST_USE bool getVariable(JSContext* cx,
                                       HandleDebuggerEnvironment environment,
                                       HandleId id, MutableHandleValue result);
  static MOZ_MUST_USE bool setVariable(JSContext* cx,
                                       HandleDebuggerEnvironment environment,
                                       HandleId id, HandleValue value);

 private:
  static const ClassOps classOps_;

  static const JSPropertySpec properties_[];
  static const JSFunctionSpec methods_[];

  Env* referent() const {
    Env* env = static_cast<Env*>(getPrivate());
    MOZ_ASSERT(env);
    return env;
  }

  Debugger* owner() const;

  bool requireDebuggee(JSContext* cx) const;

  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  static MOZ_MUST_USE bool typeGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool scopeKindGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool parentGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool objectGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool calleeGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool inspectableGetter(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool optimizedOutGetter(JSContext* cx, unsigned argc,
                                              Value* vp);

  static MOZ_MUST_USE bool namesMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool findMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool getVariableMethod(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool setVariableMethod(JSContext* cx, unsigned argc,
                                             Value* vp);
};

} /* namespace js */

#endif /* debugger_Environment_h */
