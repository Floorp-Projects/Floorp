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

#include "vm/Symbol.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;
using mozilla::DebugOnly;

template <typename T>
void
DoCallback(JS::CallbackTracer* trc, T* thingp, const char* name)
{
    JSGCTraceKind kind = MapTypeToTraceKind<typename mozilla::RemovePointer<T>::Type>::kind;
    JS::AutoTracingName ctx(trc, name);
    trc->invoke((void**)thingp, kind);
}
#define INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS(name, type) \
    template void DoCallback<type*>(JS::CallbackTracer*, type**, const char*);
FOR_EACH_GC_LAYOUT(INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS);
#undef INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS

template <>
void
DoCallback<Value>(JS::CallbackTracer* trc, Value* vp, const char* name)
{
    if (vp->isObject()) {
        JSObject* prior = &vp->toObject();
        JSObject* obj = prior;
        DoCallback(trc, &obj, name);
        if (obj != prior)
            vp->setObjectOrNull(obj);
    } else if (vp->isString()) {
        JSString* prior = vp->toString();
        JSString* str = prior;
        DoCallback(trc, &str, name);
        if (str != prior)
            vp->setString(str);
    } else if (vp->isSymbol()) {
        JS::Symbol* prior = vp->toSymbol();
        JS::Symbol* sym = prior;
        DoCallback(trc, &sym, name);
        if (sym != prior)
            vp->setSymbol(sym);
    }
}

template <>
void
DoCallback<jsid>(JS::CallbackTracer* trc, jsid* idp, const char* name)
{
    if (JSID_IS_STRING(*idp)) {
        JSString* prior = JSID_TO_STRING(*idp);
        JSString* str = prior;
        DoCallback(trc, &str, name);
        if (str != prior)
            *idp = NON_INTEGER_ATOM_TO_JSID(reinterpret_cast<JSAtom*>(str));
    } else if (JSID_IS_SYMBOL(*idp)) {
        JS::Symbol* prior = JSID_TO_SYMBOL(*idp);
        JS::Symbol* sym = prior;
        DoCallback(trc, &sym, name);
        if (sym != prior)
            *idp = SYMBOL_TO_JSID(sym);
    }
}

JS_PUBLIC_API(void)
JS_CallUnbarrieredValueTracer(JSTracer* trc, Value* valuep, const char* name)
{
    TraceManuallyBarrieredEdge(trc, valuep, name);
}

JS_PUBLIC_API(void)
JS_CallUnbarrieredIdTracer(JSTracer* trc, jsid* idp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, idp, name);
}

JS_PUBLIC_API(void)
JS_CallUnbarrieredObjectTracer(JSTracer* trc, JSObject** objp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, objp, name);
}

JS_PUBLIC_API(void)
JS_CallUnbarrieredStringTracer(JSTracer* trc, JSString** strp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, strp, name);
}

JS_PUBLIC_API(void)
JS_CallUnbarrieredScriptTracer(JSTracer* trc, JSScript** scriptp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, scriptp, name);
}

JS_PUBLIC_API(void)
JS_CallValueTracer(JSTracer* trc, JS::Heap<JS::Value>* valuep, const char* name)
{
    TraceManuallyBarrieredEdge(trc, valuep->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallIdTracer(JSTracer* trc, JS::Heap<jsid>* idp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, idp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallObjectTracer(JSTracer* trc, JS::Heap<JSObject*>* objp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, objp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallStringTracer(JSTracer* trc, JS::Heap<JSString*>* strp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, strp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallScriptTracer(JSTracer* trc, JS::Heap<JSScript*>* scriptp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, scriptp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallFunctionTracer(JSTracer* trc, JS::Heap<JSFunction*>* funp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, funp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallTenuredObjectTracer(JSTracer* trc, JS::TenuredHeap<JSObject*>* objp, const char* name)
{
    JSObject* obj = objp->getPtr();
    if (!obj)
        return;

    JS::AutoOriginalTraceLocation reloc(trc, (void**)objp);
    TraceManuallyBarrieredEdge(trc, &obj, name);

    objp->setPtr(obj);
}

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind)
{
    js::TraceChildren(trc, thing, kind);
}

JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer* trc)
{
    AssertHeapIsIdle(trc->runtime());
    TraceRuntime(trc);
}

JS_PUBLIC_API(void)
JS_TraceIncomingCCWs(JSTracer* trc, const JS::ZoneSet& zones)
{
    for (js::ZonesIter z(trc->runtime(), SkipAtoms); !z.done(); z.next()) {
        Zone* zone = z.get();
        if (!zone || zones.has(zone))
            continue;

        for (js::CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
            JSCompartment* comp = c.get();
            if (!comp)
                continue;

            for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
                const CrossCompartmentKey& key = e.front().key();
                JSObject* obj;
                JSScript* script;

                switch (key.kind) {
                  case CrossCompartmentKey::StringWrapper:
                    // StringWrappers are just used to avoid copying strings
                    // across zones multiple times, and don't hold a strong
                    // reference.
                    continue;

                  case CrossCompartmentKey::ObjectWrapper:
                  case CrossCompartmentKey::DebuggerObject:
                  case CrossCompartmentKey::DebuggerSource:
                  case CrossCompartmentKey::DebuggerEnvironment:
                    obj = static_cast<JSObject*>(key.wrapped);
                    // Ignore CCWs whose wrapped value doesn't live in our given
                    // set of zones.
                    if (!zones.has(obj->zone()))
                        continue;

                    TraceManuallyBarrieredEdge(trc, &obj, "cross-compartment wrapper");
                    MOZ_ASSERT(obj == key.wrapped);
                    break;

                  case CrossCompartmentKey::DebuggerScript:
                    script = static_cast<JSScript*>(key.wrapped);
                    // Ignore CCWs whose wrapped value doesn't live in our given
                    // set of zones.
                    if (!zones.has(script->zone()))
                        continue;
                    TraceManuallyBarrieredEdge(trc, &script, "cross-compartment wrapper");
                    MOZ_ASSERT(script == key.wrapped);
                    break;
                }
            }
        }
    }
}

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
                     JSGCTraceKind kind, bool details)
{
    const char* name = nullptr; /* silence uninitialized warning */
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JSTRACE_OBJECT:
      {
        name = static_cast<JSObject*>(thing)->getClass()->name;
        break;
      }

      case JSTRACE_SCRIPT:
        name = "script";
        break;

      case JSTRACE_STRING:
        name = ((JSString*)thing)->isDependent()
               ? "substring"
               : "string";
        break;

      case JSTRACE_SYMBOL:
        name = "symbol";
        break;

      case JSTRACE_BASE_SHAPE:
        name = "base_shape";
        break;

      case JSTRACE_JITCODE:
        name = "jitcode";
        break;

      case JSTRACE_LAZY_SCRIPT:
        name = "lazyscript";
        break;

      case JSTRACE_SHAPE:
        name = "shape";
        break;

      case JSTRACE_OBJECT_GROUP:
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
          case JSTRACE_OBJECT:
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

          case JSTRACE_SCRIPT:
          {
            JSScript* script = static_cast<JSScript*>(thing);
            JS_snprintf(buf, bufsize, " %s:%" PRIuSIZE, script->filename(), script->lineno());
            break;
          }

          case JSTRACE_STRING:
          {
            *buf++ = ' ';
            bufsize--;
            JSString* str = (JSString*)thing;

            if (str->isLinear()) {
                bool willFit = str->length() + strlen("<length > ") +
                               CountDecimalDigits(str->length()) < bufsize;

                n = JS_snprintf(buf, bufsize, "<length %d%s> ",
                                (int)str->length(),
                                willFit ? "" : " (truncated)");
                buf += n;
                bufsize -= n;

                PutEscapedString(buf, bufsize, &str->asLinear(), 0);
            } else {
                JS_snprintf(buf, bufsize, "<rope: length %d>", (int)str->length());
            }
            break;
          }

          case JSTRACE_SYMBOL:
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

JSTracer::JSTracer(JSRuntime* rt, TracerKindTag kindTag,
                   WeakMapTraceKind weakTraceKind /* = TraceWeakMapValues */)
  : runtime_(rt)
  , tag(kindTag)
  , eagerlyTraceWeakMaps_(weakTraceKind)
{
}

const char*
JS::CallbackTracer::getTracingEdgeName(char* buffer, size_t bufferSize)
{
    if (contextFunctor_) {
        (*contextFunctor_)(this, buffer, bufferSize);
        return buffer;
    }
    if (contextIndex_ != InvalidIndex) {
        JS_snprintf(buffer, bufferSize, "%s[%lu]", contextName_, contextIndex_);
        return buffer;
    }
    return contextName_;
}

void
JS::CallbackTracer::setTraceCallback(JSTraceCallback traceCallback)
{
    callback = traceCallback;
}

bool
MarkStack::init(JSGCMode gcMode)
{
    setBaseCapacity(gcMode);

    MOZ_ASSERT(!stack_);
    uintptr_t* newStack = js_pod_malloc<uintptr_t>(baseCapacity_);
    if (!newStack)
        return false;

    setStack(newStack, 0, baseCapacity_);
    return true;
}

void
MarkStack::setBaseCapacity(JSGCMode mode)
{
    switch (mode) {
      case JSGC_MODE_GLOBAL:
      case JSGC_MODE_COMPARTMENT:
        baseCapacity_ = NON_INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      case JSGC_MODE_INCREMENTAL:
        baseCapacity_ = INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      default:
        MOZ_CRASH("bad gc mode");
    }

    if (baseCapacity_ > maxCapacity_)
        baseCapacity_ = maxCapacity_;
}

void
MarkStack::setMaxCapacity(size_t maxCapacity)
{
    MOZ_ASSERT(isEmpty());
    maxCapacity_ = maxCapacity;
    if (baseCapacity_ > maxCapacity_)
        baseCapacity_ = maxCapacity_;

    reset();
}

void
MarkStack::reset()
{
    if (capacity() == baseCapacity_) {
        // No size change; keep the current stack.
        setStack(stack_, 0, baseCapacity_);
        return;
    }

    uintptr_t* newStack = (uintptr_t*)js_realloc(stack_, sizeof(uintptr_t) * baseCapacity_);
    if (!newStack) {
        // If the realloc fails, just keep using the existing stack; it's
        // not ideal but better than failing.
        newStack = stack_;
        baseCapacity_ = capacity();
    }
    setStack(newStack, 0, baseCapacity_);
}

bool
MarkStack::enlarge(unsigned count)
{
    size_t newCapacity = Min(maxCapacity_, capacity() * 2);
    if (newCapacity < capacity() + count)
        return false;

    size_t tosIndex = position();

    uintptr_t* newStack = (uintptr_t*)js_realloc(stack_, sizeof(uintptr_t) * newCapacity);
    if (!newStack)
        return false;

    setStack(newStack, tosIndex, newCapacity);
    return true;
}

void
MarkStack::setGCMode(JSGCMode gcMode)
{
    // The mark stack won't be resized until the next call to reset(), but
    // that will happen at the end of the next GC.
    setBaseCapacity(gcMode);
}

size_t
MarkStack::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(stack_);
}

/*
 * DoNotTraceWeakMaps: the GC is recomputing the liveness of WeakMap entries,
 * so we delay visting entries.
 */
GCMarker::GCMarker(JSRuntime* rt)
  : JSTracer(rt, JSTracer::MarkingTracer, DoNotTraceWeakMaps),
    stack(size_t(-1)),
    color(BLACK),
    unmarkedArenaStackTop(nullptr),
    markLaterArenas(0),
    started(false),
    strictCompartmentChecking(false)
{
}

bool
GCMarker::init(JSGCMode gcMode)
{
    return stack.init(gcMode);
}

void
GCMarker::start()
{
    MOZ_ASSERT(!started);
    started = true;
    color = BLACK;

    MOZ_ASSERT(!unmarkedArenaStackTop);
    MOZ_ASSERT(markLaterArenas == 0);

}

void
GCMarker::stop()
{
    MOZ_ASSERT(isDrained());

    MOZ_ASSERT(started);
    started = false;

    MOZ_ASSERT(!unmarkedArenaStackTop);
    MOZ_ASSERT(markLaterArenas == 0);

    /* Free non-ballast stack memory. */
    stack.reset();
}

void
GCMarker::reset()
{
    color = BLACK;

    stack.reset();
    MOZ_ASSERT(isMarkStackEmpty());

    while (unmarkedArenaStackTop) {
        ArenaHeader* aheader = unmarkedArenaStackTop;
        MOZ_ASSERT(aheader->hasDelayedMarking);
        MOZ_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        aheader->markOverflow = 0;
        aheader->allocatedDuringIncremental = 0;
        markLaterArenas--;
    }
    MOZ_ASSERT(isDrained());
    MOZ_ASSERT(!markLaterArenas);
}

void
GCMarker::markDelayedChildren(ArenaHeader* aheader)
{
    if (aheader->markOverflow) {
        bool always = aheader->allocatedDuringIncremental;
        aheader->markOverflow = 0;

        for (ArenaCellIterUnderGC i(aheader); !i.done(); i.next()) {
            TenuredCell* t = i.getCell();
            if (always || t->isMarked()) {
                t->markIfUnmarked();
                JS_TraceChildren(this, t, MapAllocToTraceKind(aheader->getAllocKind()));
            }
        }
    } else {
        MOZ_ASSERT(aheader->allocatedDuringIncremental);
        PushArena(this, aheader);
    }
    aheader->allocatedDuringIncremental = 0;
    /*
     * Note that during an incremental GC we may still be allocating into
     * aheader. However, prepareForIncrementalGC sets the
     * allocatedDuringIncremental flag if we continue marking.
     */
}

bool
GCMarker::markDelayedChildren(SliceBudget& budget)
{
    GCRuntime& gc = runtime()->gc;
    gcstats::AutoPhase ap(gc.stats, gc.state() == MARK, gcstats::PHASE_MARK_DELAYED);

    MOZ_ASSERT(unmarkedArenaStackTop);
    do {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its hasDelayedMarking flag before we begin the marking.
         */
        ArenaHeader* aheader = unmarkedArenaStackTop;
        MOZ_ASSERT(aheader->hasDelayedMarking);
        MOZ_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        markLaterArenas--;
        markDelayedChildren(aheader);

        budget.step(150);
        if (budget.isOverBudget())
            return false;
    } while (unmarkedArenaStackTop);
    MOZ_ASSERT(!markLaterArenas);

    return true;
}

#ifdef DEBUG
void
GCMarker::checkZone(void* p)
{
    MOZ_ASSERT(started);
    DebugOnly<Cell*> cell = static_cast<Cell*>(p);
    MOZ_ASSERT_IF(cell->isTenured(), cell->asTenured().zone()->isCollecting());
}
#endif

size_t
GCMarker::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t size = stack.sizeOfExcludingThis(mallocSizeOf);
    for (ZonesIter zone(runtime(), WithAtoms); !zone.done(); zone.next())
        size += zone->gcGrayRoots.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

void
js::SetMarkStackLimit(JSRuntime* rt, size_t limit)
{
    rt->gc.setMarkStackLimit(limit);
}
