/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Tracer.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/SizePrintfMacros.h"

#include "jsapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsutil.h"
#include "NamespaceImports.h"

#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "gc/Zone.h"

#include "vm/Shape.h"
#include "vm/Symbol.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"

#include "vm/ObjectGroup-inl.h"

using namespace js;
using namespace js::gc;
using mozilla::DebugOnly;

namespace js {
template<typename T>
void
CheckTracedThing(JSTracer* trc, T thing);
} // namespace js


/*** Callback Tracer Dispatch ********************************************************************/

template <typename T>
T
DoCallback(JS::CallbackTracer* trc, T* thingp, const char* name)
{
    CheckTracedThing(trc, *thingp);
    JS::AutoTracingName ctx(trc, name);
    trc->dispatchToOnEdge(thingp);
    return *thingp;
}
#define INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS(name, type, _) \
    template type* DoCallback<type*>(JS::CallbackTracer*, type**, const char*);
JS_FOR_EACH_TRACEKIND(INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS);
#undef INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS

template <typename S>
struct DoCallbackFunctor : public IdentityDefaultAdaptor<S> {
    template <typename T> S operator()(T* t, JS::CallbackTracer* trc, const char* name) {
        return js::gc::RewrapTaggedPointer<S, T>::wrap(DoCallback(trc, &t, name));
    }
};

template <>
Value
DoCallback<Value>(JS::CallbackTracer* trc, Value* vp, const char* name)
{
    *vp = DispatchTyped(DoCallbackFunctor<Value>(), *vp, trc, name);
    return *vp;
}

template <>
jsid
DoCallback<jsid>(JS::CallbackTracer* trc, jsid* idp, const char* name)
{
    *idp = DispatchTyped(DoCallbackFunctor<jsid>(), *idp, trc, name);
    return *idp;
}

template <>
TaggedProto
DoCallback<TaggedProto>(JS::CallbackTracer* trc, TaggedProto* protop, const char* name)
{
    *protop = DispatchTyped(DoCallbackFunctor<TaggedProto>(), *protop, trc, name);
    return *protop;
}

void
JS::CallbackTracer::getTracingEdgeName(char* buffer, size_t bufferSize)
{
    MOZ_ASSERT(bufferSize > 0);
    if (contextFunctor_) {
        (*contextFunctor_)(this, buffer, bufferSize);
        return;
    }
    if (contextIndex_ != InvalidIndex) {
        JS_snprintf(buffer, bufferSize, "%s[%lu]", contextName_, contextIndex_);
        return;
    }
    JS_snprintf(buffer, bufferSize, "%s", contextName_);
}


/*** Public Tracing API **************************************************************************/

JS_PUBLIC_API(void)
JS::TraceChildren(JSTracer* trc, GCCellPtr thing)
{
    js::TraceChildren(trc, thing.asCell(), thing.kind());
}

struct TraceChildrenFunctor {
    template <typename T>
    void operator()(JSTracer* trc, void* thing) {
        static_cast<T*>(thing)->traceChildren(trc);
    }
};

void
js::TraceChildren(JSTracer* trc, void* thing, JS::TraceKind kind)
{
    MOZ_ASSERT(thing);
    TraceChildrenFunctor f;
    DispatchTraceKindTyped(f, kind, trc, thing);
}

namespace {
struct TraceIncomingFunctor {
    JSTracer* trc_;
    const JS::CompartmentSet& compartments_;
    TraceIncomingFunctor(JSTracer* trc, const JS::CompartmentSet& compartments)
      : trc_(trc), compartments_(compartments)
    {}
    using ReturnType = void;
    template <typename T>
    ReturnType operator()(T tp) {
        if (!compartments_.has((*tp)->compartment()))
            return;
        TraceManuallyBarrieredEdge(trc_, tp, "cross-compartment wrapper");
    }
    // StringWrappers are just used to avoid copying strings
    // across zones multiple times, and don't hold a strong
    // reference.
    ReturnType operator()(JSString** tp) {}
};
} // namespace (anonymous)

JS_PUBLIC_API(void)
JS::TraceIncomingCCWs(JSTracer* trc, const JS::CompartmentSet& compartments)
{
    for (js::CompartmentsIter comp(trc->runtime(), SkipAtoms); !comp.done(); comp.next()) {
        if (compartments.has(comp))
            continue;

        for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
            mozilla::DebugOnly<const CrossCompartmentKey> prior = e.front().key();
            e.front().mutableKey().applyToWrapped(TraceIncomingFunctor(trc, compartments));
            MOZ_ASSERT(e.front().key() == prior);
        }
    }
}


/*** Cycle Collector Helpers **********************************************************************/

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
void
gc::TraceCycleCollectorChildren(JS::CallbackTracer* trc, Shape* shape)
{
    // We need to mark the global, but it's OK to only do this once instead of
    // doing it for every Shape in our lineage, since it's always the same
    // global.
    JSObject* global = shape->compartment()->unsafeUnbarrieredMaybeGlobal();
    MOZ_ASSERT(global);
    DoCallback(trc, &global, "global");

    do {
        MOZ_ASSERT(global == shape->compartment()->unsafeUnbarrieredMaybeGlobal());

        MOZ_ASSERT(shape->base());
        shape->base()->assertConsistency();

        TraceEdge(trc, &shape->propidRef(), "propid");

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

// Object groups can point to other object groups via an UnboxedLayout or the
// the original unboxed group link. There can potentially be deep or cyclic
// chains of such groups to trace through without going through a thing that
// participates in cycle collection. These need to be handled iteratively to
// avoid blowing the stack when running the cycle collector's callback tracer.
struct ObjectGroupCycleCollectorTracer : public JS::CallbackTracer
{
    explicit ObjectGroupCycleCollectorTracer(JS::CallbackTracer* innerTracer)
        : JS::CallbackTracer(innerTracer->runtime(), DoNotTraceWeakMaps),
          innerTracer(innerTracer)
    {}

    void onChild(const JS::GCCellPtr& thing) override;

    JS::CallbackTracer* innerTracer;
    Vector<ObjectGroup*, 4, SystemAllocPolicy> seen, worklist;
};

void
ObjectGroupCycleCollectorTracer::onChild(const JS::GCCellPtr& thing)
{
    if (thing.is<BaseShape>()) {
        // The CC does not care about BaseShapes, and no additional GC things
        // will be reached by following this edge.
        return;
    }

    if (thing.is<JSObject>() || thing.is<JSScript>()) {
        // Invoke the inner cycle collector callback on this child. It will not
        // recurse back into TraceChildren.
        innerTracer->onChild(thing);
        return;
    }

    if (thing.is<ObjectGroup>()) {
        // If this group is required to be in an ObjectGroup chain, trace it
        // via the provided worklist rather than continuing to recurse.
        ObjectGroup& group = thing.as<ObjectGroup>();
        if (group.maybeUnboxedLayout()) {
            for (size_t i = 0; i < seen.length(); i++) {
                if (seen[i] == &group)
                    return;
            }
            if (seen.append(&group) && worklist.append(&group)) {
                return;
            } else {
                // If append fails, keep tracing normally. The worst that will
                // happen is we end up overrecursing.
            }
        }
    }

    TraceChildren(this, thing.asCell(), thing.kind());
}

void
gc::TraceCycleCollectorChildren(JS::CallbackTracer* trc, ObjectGroup* group)
{
    MOZ_ASSERT(trc->isCallbackTracer());

    // Early return if this group is not required to be in an ObjectGroup chain.
    if (!group->maybeUnboxedLayout())
        return group->traceChildren(trc);

    ObjectGroupCycleCollectorTracer groupTracer(trc->asCallbackTracer());
    group->traceChildren(&groupTracer);

    while (!groupTracer.worklist.empty()) {
        ObjectGroup* innerGroup = groupTracer.worklist.popCopy();
        innerGroup->traceChildren(&groupTracer);
    }
}


/*** Traced Edge Printer *************************************************************************/

static size_t
CountDecimalDigits(size_t num)
{
    size_t numDigits = 0;
    do {
        num /= 10;
        numDigits++;
    } while (num > 0);

    return numDigits;
}

JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char* buf, size_t bufsize, JSTracer* trc, void* thing,
                     JS::TraceKind kind, bool details)
{
    const char* name = nullptr; /* silence uninitialized warning */
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JS::TraceKind::Object:
      {
        name = static_cast<JSObject*>(thing)->getClass()->name;
        break;
      }

      case JS::TraceKind::Script:
        name = "script";
        break;

      case JS::TraceKind::String:
        name = ((JSString*)thing)->isDependent()
               ? "substring"
               : "string";
        break;

      case JS::TraceKind::Symbol:
        name = "symbol";
        break;

      case JS::TraceKind::BaseShape:
        name = "base_shape";
        break;

      case JS::TraceKind::JitCode:
        name = "jitcode";
        break;

      case JS::TraceKind::LazyScript:
        name = "lazyscript";
        break;

      case JS::TraceKind::Shape:
        name = "shape";
        break;

      case JS::TraceKind::ObjectGroup:
        name = "object_group";
        break;

      default:
        name = "INVALID";
        break;
    }

    n = strlen(name);
    if (n > bufsize - 1)
        n = bufsize - 1;
    js_memcpy(buf, name, n + 1);
    buf += n;
    bufsize -= n;
    *buf = '\0';

    if (details && bufsize > 2) {
        switch (kind) {
          case JS::TraceKind::Object:
          {
            JSObject* obj = (JSObject*)thing;
            if (obj->is<JSFunction>()) {
                JSFunction* fun = &obj->as<JSFunction>();
                if (fun->displayAtom()) {
                    *buf++ = ' ';
                    bufsize--;
                    PutEscapedString(buf, bufsize, fun->displayAtom(), 0);
                }
            } else if (obj->getClass()->flags & JSCLASS_HAS_PRIVATE) {
                JS_snprintf(buf, bufsize, " %p", obj->as<NativeObject>().getPrivate());
            } else {
                JS_snprintf(buf, bufsize, " <no private>");
            }
            break;
          }

          case JS::TraceKind::Script:
          {
            JSScript* script = static_cast<JSScript*>(thing);
            JS_snprintf(buf, bufsize, " %s:%" PRIuSIZE, script->filename(), script->lineno());
            break;
          }

          case JS::TraceKind::String:
          {
            *buf++ = ' ';
            bufsize--;
            JSString* str = (JSString*)thing;

            if (str->isLinear()) {
                bool willFit = str->length() + strlen("<length > ") +
                               CountDecimalDigits(str->length()) < bufsize;

                n = JS_snprintf(buf, bufsize, "<length %" PRIuSIZE "%s> ",
                                str->length(),
                                willFit ? "" : " (truncated)");
                buf += n;
                bufsize -= n;

                PutEscapedString(buf, bufsize, &str->asLinear(), 0);
            } else {
                JS_snprintf(buf, bufsize, "<rope: length %" PRIuSIZE ">", str->length());
            }
            break;
          }

          case JS::TraceKind::Symbol:
          {
            JS::Symbol* sym = static_cast<JS::Symbol*>(thing);
            if (JSString* desc = sym->description()) {
                if (desc->isLinear()) {
                    *buf++ = ' ';
                    bufsize--;
                    PutEscapedString(buf, bufsize, &desc->asLinear(), 0);
                } else {
                    JS_snprintf(buf, bufsize, "<nonlinear desc>");
                }
            } else {
                JS_snprintf(buf, bufsize, "<null>");
            }
            break;
          }

          default:
            break;
        }
    }
    buf[bufsize - 1] = '\0';
}
