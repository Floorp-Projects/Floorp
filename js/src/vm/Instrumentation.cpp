/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Instrumentation.h"

#include "jsapi.h"

#include "debugger/DebugAPI.h"
#include "proxy/DeadObjectProxy.h"

#include "vm/JSObject-inl.h"

namespace js {

RealmInstrumentation::RealmInstrumentation(Zone* zone, JSObject* callback,
                                           JSObject* dbgObject, uint32_t kinds)
    : callback(callback), dbgObject(dbgObject), kinds(kinds) {}

void RealmInstrumentation::trace(JSTracer* trc) {
  TraceEdge(trc, &callback, "RealmInstrumentation::callback");
  TraceEdge(trc, &dbgObject, "RealmInstrumentation::dbgObject");
}

enum InstrumentationHolderSlots {
  RealmInstrumentationSlot,
  ReservedSlotCount,
};

static RealmInstrumentation* GetInstrumentation(JSObject* obj) {
  Value v = JS_GetReservedSlot(obj, RealmInstrumentationSlot);
  return static_cast<RealmInstrumentation*>(v.toPrivate());
}

/* static */
void RealmInstrumentation::holderFinalize(FreeOp* fop, JSObject* obj) {
  RealmInstrumentation* instrumentation = GetInstrumentation(obj);
  fop->delete_(obj, instrumentation, MemoryUse::RealmInstrumentation);
}

/* static */
void RealmInstrumentation::holderTrace(JSTracer* trc, JSObject* obj) {
  RealmInstrumentation* instrumentation = GetInstrumentation(obj);
  instrumentation->trace(trc);
}

static const ClassOps InstrumentationHolderClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    RealmInstrumentation::holderFinalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    RealmInstrumentation::holderTrace,
};

static const Class InstrumentationHolderClass = {
    "Instrumentation Holder",
    JSCLASS_HAS_RESERVED_SLOTS(ReservedSlotCount) | JSCLASS_FOREGROUND_FINALIZE,
    &InstrumentationHolderClassOps, JS_NULL_CLASS_SPEC, JS_NULL_CLASS_EXT};

static const char* instrumentationNames[] = {
#define DEFINE_INSTRUMENTATION_STRING(_1, String, _2) String,
    FOR_EACH_INSTRUMENTATION_KIND(DEFINE_INSTRUMENTATION_STRING)
#undef DEFINE_INSTRUMENTATION_STRING
};

static bool StringToInstrumentationKind(JSContext* cx, HandleString str,
                                        InstrumentationKind* result) {
  for (size_t i = 0; i < mozilla::ArrayLength(instrumentationNames); i++) {
    bool match;
    if (!JS_StringEqualsAscii(cx, str, instrumentationNames[i], &match)) {
      return false;
    }
    if (match) {
      *result = (InstrumentationKind)(1 << i);
      return true;
    }
  }

  JS_ReportErrorASCII(cx, "Unknown instrumentation kind");
  return false;
}

/* static */
JSAtom* RealmInstrumentation::getInstrumentationKindName(
    JSContext* cx, InstrumentationKind kind) {
  for (size_t i = 0; i < mozilla::ArrayLength(instrumentationNames); i++) {
    if (kind == (InstrumentationKind)(1 << i)) {
      JSString* str = JS_AtomizeString(cx, instrumentationNames[i]);
      if (!str) {
        return nullptr;
      }
      return &str->asAtom();
    }
  }
  MOZ_CRASH("Unexpected instrumentation kind");
}

/* static */
bool RealmInstrumentation::install(JSContext* cx, Handle<GlobalObject*> global,
                                   HandleObject callbackArg,
                                   HandleObject dbgObjectArg,
                                   Handle<StringVector> kindStrings) {
  MOZ_ASSERT(global == cx->global());

  if (global->getInstrumentationHolder()) {
    JS_ReportErrorASCII(cx, "Global already has instrumentation specified");
    return false;
  }

  RootedObject callback(cx, callbackArg);
  if (!cx->compartment()->wrap(cx, &callback)) {
    return false;
  }

  RootedObject dbgObject(cx, dbgObjectArg);
  if (!cx->compartment()->wrap(cx, &dbgObject)) {
    return false;
  }

  uint32_t kinds = 0;
  for (size_t i = 0; i < kindStrings.length(); i++) {
    HandleString str = kindStrings[i];
    InstrumentationKind kind;
    if (!StringToInstrumentationKind(cx, str, &kind)) {
      return false;
    }
    kinds |= (uint32_t)kind;
  }

  Rooted<UniquePtr<RealmInstrumentation>> instrumentation(
      cx,
      MakeUnique<RealmInstrumentation>(cx->zone(), callback, dbgObject, kinds));
  if (!instrumentation) {
    ReportOutOfMemory(cx);
    return false;
  }

  JSObject* holder = NewBuiltinClassInstance(cx, &InstrumentationHolderClass);
  if (!holder) {
    return false;
  }

  InitReservedSlot(&holder->as<NativeObject>(), RealmInstrumentationSlot,
                   instrumentation.release(), MemoryUse::RealmInstrumentation);

  global->setInstrumentationHolder(holder);
  return true;
}

/* static */
bool RealmInstrumentation::setActive(JSContext* cx,
                                     Handle<GlobalObject*> global,
                                     Debugger* dbg, bool active) {
  MOZ_ASSERT(global == cx->global());

  RootedObject holder(cx, global->getInstrumentationHolder());
  if (!holder) {
    JS_ReportErrorASCII(cx, "Global does not have instrumentation specified");
    return false;
  }

  RealmInstrumentation* instrumentation = GetInstrumentation(holder);
  if (active != instrumentation->active) {
    instrumentation->active = active;

    // For simplicity, discard all Ion code in the entire zone when
    // instrumentation activity changes.
    js::CancelOffThreadIonCompile(cx->runtime());
    cx->zone()->setPreservingCode(false);
    cx->zone()->discardJitCode(cx->runtime()->defaultFreeOp(),
                               Zone::KeepBaselineCode);
  }

  return true;
}

/* static */
bool RealmInstrumentation::isActive(GlobalObject* global) {
  JSObject* holder = global->getInstrumentationHolder();
  MOZ_ASSERT(holder);

  RealmInstrumentation* instrumentation = GetInstrumentation(holder);
  return instrumentation->active;
}

/* static */
const int32_t* RealmInstrumentation::addressOfActive(GlobalObject* global) {
  JSObject* holder = global->getInstrumentationHolder();
  MOZ_ASSERT(holder);

  RealmInstrumentation* instrumentation = GetInstrumentation(holder);
  return &instrumentation->active;
}

/* static */
JSObject* RealmInstrumentation::getCallback(GlobalObject* global) {
  JSObject* holder = global->getInstrumentationHolder();
  MOZ_ASSERT(holder);

  RealmInstrumentation* instrumentation = GetInstrumentation(holder);
  return instrumentation->callback;
}

/* static */
uint32_t RealmInstrumentation::getInstrumentationKinds(GlobalObject* global) {
  JSObject* holder = global->getInstrumentationHolder();
  if (!holder) {
    return 0;
  }

  RealmInstrumentation* instrumentation = GetInstrumentation(holder);
  return instrumentation->kinds;
}

/* static */
bool RealmInstrumentation::getScriptId(JSContext* cx,
                                       Handle<GlobalObject*> global,
                                       HandleScript script, int32_t* id) {
  MOZ_ASSERT(global == cx->global());
  RootedObject holder(cx, global->getInstrumentationHolder());
  RealmInstrumentation* instrumentation = GetInstrumentation(holder);

  RootedObject dbgObject(cx, UncheckedUnwrap(instrumentation->dbgObject));

  if (IsDeadProxyObject(dbgObject)) {
    JS_ReportErrorASCII(cx, "Instrumentation debugger object is dead");
    return false;
  }

  AutoRealm ar(cx, dbgObject);

  RootedValue idValue(cx);
  if (!DebugAPI::getScriptInstrumentationId(cx, dbgObject, script, &idValue)) {
    return false;
  }

  if (!idValue.isNumber()) {
    JS_ReportErrorASCII(cx, "Instrumentation ID not set for script");
    return false;
  }

  *id = idValue.toNumber();
  return true;
}

bool InstrumentationActiveOperation(JSContext* cx, MutableHandleValue rv) {
  rv.setBoolean(RealmInstrumentation::isActive(cx->global()));
  return true;
}

JSObject* InstrumentationCallbackOperation(JSContext* cx) {
  return RealmInstrumentation::getCallback(cx->global());
}

bool InstrumentationScriptIdOperation(JSContext* cx, HandleScript script,
                                      MutableHandleValue rv) {
  int32_t id;
  if (!RealmInstrumentation::getScriptId(cx, cx->global(), script, &id)) {
    return false;
  }
  rv.setInt32(id);
  return true;
}

bool GlobalHasInstrumentation(JSObject* global) {
  return global->is<js::GlobalObject>() &&
         global->as<js::GlobalObject>().getInstrumentationHolder();
}

}  // namespace js
