/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Instrumentation_h
#define vm_Instrumentation_h

#include "js/UniquePtr.h"
#include "vm/GlobalObject.h"

namespace js {

// Logic related to instrumentation which can be performed in a realm.

#define FOR_EACH_INSTRUMENTATION_KIND(MACRO)                                \
  /* The main entry point of a script. */                                   \
  MACRO(Main, "main", 1 << 0)                                               \
  /* Points other than the main entry point where a frame for the script */ \
  /* might start executing. */                                              \
  MACRO(Entry, "entry", 1 << 1)                                             \
  /* Points at which a script's frame will be popped or suspended. */       \
  MACRO(Exit, "exit", 1 << 2)                                               \
  /* Breakpoint sites. */                                                   \
  MACRO(Breakpoint, "breakpoint", 1 << 3)                                   \
  /* Property access operations. */                                         \
  MACRO(GetProperty, "getProperty", 1 << 4)                                 \
  MACRO(SetProperty, "setProperty", 1 << 5)                                 \
  MACRO(GetElement, "getElement", 1 << 6)                                   \
  MACRO(SetElement, "setElement", 1 << 7)

// Points at which instrumentation can be added on the scripts in a realm.
enum class InstrumentationKind {
#define DEFINE_INSTRUMENTATION_ENUM(Name, _1, Value) Name = Value,
  FOR_EACH_INSTRUMENTATION_KIND(DEFINE_INSTRUMENTATION_ENUM)
#undef DEFINE_INSTRUMENTATION_ENUM
};

class RealmInstrumentation {
  // Callback invoked on instrumentation operations.
  GCPtrObject callback;

  // Debugger with which the instrumentation is associated. This debugger's
  // Debugger.Script instances store instrumentation IDs for scripts in the
  // realm.
  GCPtrObject dbgObject;

  // Mask of the InstrumentationKind operations which should be instrumented.
  uint32_t kinds = 0;

  // Whether instrumentation is currently active in the realm. This is an
  // int32_t so it can be directly accessed from JIT code.
  int32_t active = 0;

 public:
  static bool install(JSContext* cx, Handle<GlobalObject*> global,
                      HandleObject callback, HandleObject dbgObject,
                      Handle<StringVector> kinds);

  static JSObject* getCallback(GlobalObject* global);

  // Get the mask of operation kinds which should be instrumented.
  static uint32_t getInstrumentationKinds(GlobalObject* global);

  // Get the string name of an instrumentation kind.
  static JSAtom* getInstrumentationKindName(JSContext* cx,
                                            InstrumentationKind kind);

  static bool getScriptId(JSContext* cx, Handle<GlobalObject*> global,
                          HandleScript script, int32_t* id);

  static bool setActive(JSContext* cx, Handle<GlobalObject*> global,
                        Debugger* dbg, bool active);

  static bool isActive(GlobalObject* global);

  static const int32_t* addressOfActive(GlobalObject* global);

  // This is public for js_new.
  RealmInstrumentation(Zone* zone, JSObject* callback, JSObject* dbgObject,
                       uint32_t kinds);

  void trace(JSTracer* trc);

  static void holderFinalize(JSFreeOp* fop, JSObject* obj);
  static void holderTrace(JSTracer* trc, JSObject* obj);
};

// For use in the frontend when an opcode may or may not need instrumentation.
enum class ShouldInstrument {
  No,
  Yes,
};

bool InstrumentationActiveOperation(JSContext* cx, MutableHandleValue rv);
JSObject* InstrumentationCallbackOperation(JSContext* cx);
bool InstrumentationScriptIdOperation(JSContext* cx, HandleScript script,
                                      MutableHandleValue rv);

}  // namespace js

namespace JS {

template <>
struct DeletePolicy<js::RealmInstrumentation>
    : public js::GCManagedDeletePolicy<js::RealmInstrumentation> {};

} /* namespace JS */

#endif /* vm_Instrumentation_h */
