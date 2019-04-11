/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Tracer.h"

#include "mozilla/DebugOnly.h"

#include "jsutil.h"
#include "NamespaceImports.h"

#include "gc/GCInternals.h"
#include "gc/PublicIterators.h"
#include "gc/Zone.h"
#include "util/Text.h"
#include "vm/BigIntType.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Shape.h"
#include "vm/SymbolType.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/ObjectGroup-inl.h"
#include "vm/Realm-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::gc;
using mozilla::DebugOnly;

namespace js {
template <typename T>
void CheckTracedThing(JSTracer* trc, T thing);
}  // namespace js

/*** Callback Tracer Dispatch ***********************************************/

template <typename T>
T* DoCallback(JS::CallbackTracer* trc, T** thingp, const char* name) {
  CheckTracedThing(trc, *thingp);
  JS::AutoTracingName ctx(trc, name);
  trc->dispatchToOnEdge(thingp);
  return *thingp;
}
#define INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS(name, type, _, _1) \
  template type* DoCallback<type>(JS::CallbackTracer*, type**, const char*);
JS_FOR_EACH_TRACEKIND(INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS);
#undef INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS

template <typename T>
T DoCallback(JS::CallbackTracer* trc, T* thingp, const char* name) {
  auto thing = MapGCThingTyped(*thingp, [trc, name](auto t) {
    return TaggedPtr<T>::wrap(DoCallback(trc, &t, name));
  });
  // Only update *thingp if the value changed, to avoid TSan false positives for
  // template objects when using DumpHeapTracer or UbiNode tracers while Ion
  // compiling off-thread.
  if (thing.isSome() && thing.value() != *thingp) {
    *thingp = thing.value();
  }
  return *thingp;
}
template JS::Value DoCallback<JS::Value>(JS::CallbackTracer*, JS::Value*,
                                         const char*);
template JS::PropertyKey DoCallback<JS::PropertyKey>(JS::CallbackTracer*,
                                                     JS::PropertyKey*,
                                                     const char*);
template TaggedProto DoCallback<TaggedProto>(JS::CallbackTracer*, TaggedProto*,
                                             const char*);

void JS::CallbackTracer::getTracingEdgeName(char* buffer, size_t bufferSize) {
  MOZ_ASSERT(bufferSize > 0);
  if (contextFunctor_) {
    (*contextFunctor_)(this, buffer, bufferSize);
    return;
  }
  if (contextIndex_ != InvalidIndex) {
    snprintf(buffer, bufferSize, "%s[%zu]", contextName_, contextIndex_);
    return;
  }
  snprintf(buffer, bufferSize, "%s", contextName_);
}

/*** Public Tracing API *****************************************************/

JS_PUBLIC_API void JS::TraceChildren(JSTracer* trc, GCCellPtr thing) {
  js::TraceChildren(trc, thing.asCell(), thing.kind());
}

void js::TraceChildren(JSTracer* trc, void* thing, JS::TraceKind kind) {
  MOZ_ASSERT(thing);
  ApplyGCThingTyped(thing, kind, [trc](auto t) {
    MOZ_ASSERT_IF(t->runtimeFromAnyThread() != trc->runtime(),
                  ThingIsPermanentAtomOrWellKnownSymbol(t) ||
                      t->zoneFromAnyThread()->isSelfHostingZone());
    t->traceChildren(trc);
  });
}

JS_PUBLIC_API void JS::TraceIncomingCCWs(
    JSTracer* trc, const JS::CompartmentSet& compartments) {
  for (js::CompartmentsIter comp(trc->runtime()); !comp.done(); comp.next()) {
    if (compartments.has(comp)) {
      continue;
    }

    for (Compartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
      mozilla::DebugOnly<const CrossCompartmentKey> prior = e.front().key();
      e.front().mutableKey().applyToWrapped([trc, &compartments](auto tp) {
        Compartment* comp = (*tp)->maybeCompartment();
        if (comp && compartments.has(comp)) {
          TraceManuallyBarrieredEdge(trc, tp, "cross-compartment wrapper");
        }
      });
      MOZ_ASSERT(e.front().key() == prior);
    }
  }
}

/*** Cycle Collector Helpers ************************************************/

// This function is used by the Cycle Collector (CC) to trace through -- or in
// CC parlance, traverse -- a Shape tree. The CC does not care about Shapes or
// BaseShapes, only the JSObjects held live by them. Thus, we walk the Shape
// lineage, but only report non-Shape things. This effectively makes the entire
// shape lineage into a single node in the CC, saving tremendous amounts of
// space and time in its algorithms.
//
// The algorithm implemented here uses only bounded stack space. This would be
// possible to implement outside the engine, but would require much extra
// infrastructure and many, many more slow GOT lookups. We have implemented it
// inside SpiderMonkey, despite the lack of general applicability, for the
// simplicity and performance of FireFox's embedding of this engine.
void gc::TraceCycleCollectorChildren(JS::CallbackTracer* trc, Shape* shape) {
  do {
    MOZ_ASSERT(shape->base());
    shape->base()->assertConsistency();

    // Don't trace the propid because the CC doesn't care about jsid.

    if (shape->hasGetterObject()) {
      JSObject* tmp = shape->getterObject();
      DoCallback(trc, &tmp, "getter");
      MOZ_ASSERT(tmp == shape->getterObject());
    }

    if (shape->hasSetterObject()) {
      JSObject* tmp = shape->setterObject();
      DoCallback(trc, &tmp, "setter");
      MOZ_ASSERT(tmp == shape->setterObject());
    }

    shape = shape->previous();
  } while (shape);
}

void gc::TraceCycleCollectorChildren(JS::CallbackTracer* trc,
                                     ObjectGroup* group) {
  MOZ_ASSERT(trc->isCallbackTracer());

  group->traceChildren(trc);
}

/*** Traced Edge Printer ****************************************************/

static size_t CountDecimalDigits(size_t num) {
  size_t numDigits = 0;
  do {
    num /= 10;
    numDigits++;
  } while (num > 0);

  return numDigits;
}

static const char* StringKindHeader(JSString* str) {
  MOZ_ASSERT(str->isLinear());

  if (str->isAtom()) {
    if (str->isPermanentAtom()) {
      return "permanent atom: ";
    }
    return "atom: ";
  }

  if (str->isFlat()) {
    if (str->isExtensible()) {
      return "extensible: ";
    }
    if (str->isUndepended()) {
      return "undepended: ";
    }
    if (str->isInline()) {
      if (str->isFatInline()) {
        return "fat inline: ";
      }
      return "inline: ";
    }
    return "flat: ";
  }

  if (str->isDependent()) {
    return "dependent: ";
  }
  if (str->isExternal()) {
    return "external: ";
  }
  return "linear: ";
}

JS_PUBLIC_API void JS_GetTraceThingInfo(char* buf, size_t bufsize,
                                        JSTracer* trc, void* thing,
                                        JS::TraceKind kind, bool details) {
  const char* name = nullptr; /* silence uninitialized warning */
  size_t n;

  if (bufsize == 0) {
    return;
  }

  switch (kind) {
    case JS::TraceKind::BaseShape:
      name = "base_shape";
      break;

    case JS::TraceKind::JitCode:
      name = "jitcode";
      break;

    case JS::TraceKind::LazyScript:
      name = "lazyscript";
      break;

    case JS::TraceKind::Null:
      name = "null_pointer";
      break;

    case JS::TraceKind::Object: {
      name = static_cast<JSObject*>(thing)->getClass()->name;
      break;
    }

    case JS::TraceKind::ObjectGroup:
      name = "object_group";
      break;

    case JS::TraceKind::RegExpShared:
      name = "reg_exp_shared";
      break;

    case JS::TraceKind::Scope:
      name = "scope";
      break;

    case JS::TraceKind::Script:
      name = "script";
      break;

    case JS::TraceKind::Shape:
      name = "shape";
      break;

    case JS::TraceKind::String:
      name = ((JSString*)thing)->isDependent() ? "substring" : "string";
      break;

    case JS::TraceKind::Symbol:
      name = "symbol";
      break;

    case JS::TraceKind::BigInt:
      name = "BigInt";
      break;

    default:
      name = "INVALID";
      break;
  }

  n = strlen(name);
  if (n > bufsize - 1) {
    n = bufsize - 1;
  }
  js_memcpy(buf, name, n + 1);
  buf += n;
  bufsize -= n;
  *buf = '\0';

  if (details && bufsize > 2) {
    switch (kind) {
      case JS::TraceKind::Object: {
        JSObject* obj = (JSObject*)thing;
        if (obj->is<JSFunction>()) {
          JSFunction* fun = &obj->as<JSFunction>();
          if (fun->displayAtom()) {
            *buf++ = ' ';
            bufsize--;
            PutEscapedString(buf, bufsize, fun->displayAtom(), 0);
          }
        } else if (obj->getClass()->flags & JSCLASS_HAS_PRIVATE) {
          snprintf(buf, bufsize, " %p", obj->as<NativeObject>().getPrivate());
        } else {
          snprintf(buf, bufsize, " <no private>");
        }
        break;
      }

      case JS::TraceKind::Script: {
        JSScript* script = static_cast<JSScript*>(thing);
        snprintf(buf, bufsize, " %s:%u", script->filename(), script->lineno());
        break;
      }

      case JS::TraceKind::LazyScript: {
        LazyScript* script = static_cast<LazyScript*>(thing);
        snprintf(buf, bufsize, " %s:%u", script->filename(), script->lineno());
        break;
      }

      case JS::TraceKind::String: {
        *buf++ = ' ';
        bufsize--;
        JSString* str = (JSString*)thing;

        if (str->isLinear()) {
          const char* header = StringKindHeader(str);
          bool willFit = str->length() + strlen("<length > ") + strlen(header) +
                             CountDecimalDigits(str->length()) <
                         bufsize;

          n = snprintf(buf, bufsize, "<%slength %zu%s> ", header, str->length(),
                       willFit ? "" : " (truncated)");
          buf += n;
          bufsize -= n;

          PutEscapedString(buf, bufsize, &str->asLinear(), 0);
        } else {
          snprintf(buf, bufsize, "<rope: length %zu>", str->length());
        }
        break;
      }

      case JS::TraceKind::Symbol: {
        JS::Symbol* sym = static_cast<JS::Symbol*>(thing);
        if (JSString* desc = sym->description()) {
          if (desc->isLinear()) {
            *buf++ = ' ';
            bufsize--;
            PutEscapedString(buf, bufsize, &desc->asLinear(), 0);
          } else {
            snprintf(buf, bufsize, "<nonlinear desc>");
          }
        } else {
          snprintf(buf, bufsize, "<null>");
        }
        break;
      }

      case JS::TraceKind::Scope: {
        js::Scope* scope = static_cast<js::Scope*>(thing);
        snprintf(buf, bufsize, " %s", js::ScopeKindString(scope->kind()));
        break;
      }

      default:
        break;
    }
  }
  buf[bufsize - 1] = '\0';
}

JS::CallbackTracer::CallbackTracer(JSContext* cx,
                                   WeakMapTraceKind weakTraceKind)
    : CallbackTracer(cx->runtime(), weakTraceKind) {}

uint32_t JSTracer::gcNumberForMarking() const {
  MOZ_ASSERT(isMarkingTracer());
  return runtime()->gc.gcNumber();
}
