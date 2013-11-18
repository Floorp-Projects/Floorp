/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "jscntxt.h"
#include "jsgc.h"
#include "jsonparser.h"
#include "jsprf.h"
#include "jstypes.h"
#include "jswatchpoint.h"

#include "builtin/MapObject.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#ifdef JS_ION
# include "jit/IonMacroAssembler.h"
#endif
#include "js/HashTable.h"
#include "vm/Debugger.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

#ifdef JSGC_USE_EXACT_ROOTING
static inline void
MarkExactStackRoot(JSTracer *trc, Rooted<void*> *rooter, ThingRootKind kind)
{
    void **addr = (void **)rooter->address();
    if (IsNullTaggedPointer(*addr))
        return;

    if (kind == THING_ROOT_OBJECT && *addr == Proxy::LazyProto)
        return;

    switch (kind) {
      case THING_ROOT_OBJECT:      MarkObjectRoot(trc, (JSObject **)addr, "exact-object"); break;
      case THING_ROOT_STRING:      MarkStringRoot(trc, (JSString **)addr, "exact-string"); break;
      case THING_ROOT_SCRIPT:      MarkScriptRoot(trc, (JSScript **)addr, "exact-script"); break;
      case THING_ROOT_SHAPE:       MarkShapeRoot(trc, (Shape **)addr, "exact-shape"); break;
      case THING_ROOT_BASE_SHAPE:  MarkBaseShapeRoot(trc, (BaseShape **)addr, "exact-baseshape"); break;
      case THING_ROOT_TYPE:        MarkTypeRoot(trc, (types::Type *)addr, "exact-type"); break;
      case THING_ROOT_TYPE_OBJECT: MarkTypeObjectRoot(trc, (types::TypeObject **)addr, "exact-typeobject"); break;
      case THING_ROOT_ION_CODE:    MarkIonCodeRoot(trc, (jit::IonCode **)addr, "exact-ioncode"); break;
      case THING_ROOT_VALUE:       MarkValueRoot(trc, (Value *)addr, "exact-value"); break;
      case THING_ROOT_ID:          MarkIdRoot(trc, (jsid *)addr, "exact-id"); break;
      case THING_ROOT_PROPERTY_ID: MarkIdRoot(trc, &((js::PropertyId *)addr)->asId(), "exact-propertyid"); break;
      case THING_ROOT_BINDINGS:    ((Bindings *)addr)->trace(trc); break;
      case THING_ROOT_PROPERTY_DESCRIPTOR: ((JSPropertyDescriptor *)addr)->trace(trc); break;
      default: MOZ_ASSUME_UNREACHABLE("Invalid THING_ROOT kind"); break;
    }
}

static inline void
MarkExactStackRootList(JSTracer *trc, Rooted<void*> *rooter, ThingRootKind kind)
{
    while (rooter) {
        MarkExactStackRoot(trc, rooter, kind);
        rooter = rooter->previous();
    }
}

static void
MarkExactStackRoots(JSTracer *trc)
{
    for (unsigned i = 0; i < THING_ROOT_LIMIT; i++) {
        for (ContextIter cx(trc->runtime); !cx.done(); cx.next())
            MarkExactStackRootList(trc, cx->thingGCRooters[i], ThingRootKind(i));

        MarkExactStackRootList(trc, trc->runtime->mainThread.thingGCRooters[i], ThingRootKind(i));
    }
}
#endif /* JSGC_USE_EXACT_ROOTING */

enum ConservativeGCTest
{
    CGCT_VALID,
    CGCT_LOWBITSET, /* excluded because one of the low bits was set */
    CGCT_NOTARENA,  /* not within arena range in a chunk */
    CGCT_OTHERCOMPARTMENT,  /* in another compartment */
    CGCT_NOTCHUNK,  /* not within a valid chunk */
    CGCT_FREEARENA, /* within arena containing only free things */
    CGCT_NOTLIVE,   /* gcthing is not allocated */
    CGCT_END
};

/*
 * Tests whether w is a (possibly dead) GC thing. Returns CGCT_VALID and
 * details about the thing if so. On failure, returns the reason for rejection.
 */
static inline ConservativeGCTest
IsAddressableGCThing(JSRuntime *rt, uintptr_t w,
                     bool skipUncollectedCompartments,
                     gc::AllocKind *thingKindPtr,
                     ArenaHeader **arenaHeader,
                     void **thing)
{
    /*
     * We assume that the compiler never uses sub-word alignment to store
     * pointers and does not tag pointers on its own. Additionally, the value
     * representation for all values and the jsid representation for GC-things
     * do not touch the low two bits. Thus any word with the low two bits set
     * is not a valid GC-thing.
     */
    JS_STATIC_ASSERT(JSID_TYPE_STRING == 0 && JSID_TYPE_OBJECT == 4);
    if (w & 0x3)
        return CGCT_LOWBITSET;

    /*
     * An object jsid has its low bits tagged. In the value representation on
     * 64-bit, the high bits are tagged.
     */
    const uintptr_t JSID_PAYLOAD_MASK = ~uintptr_t(JSID_TYPE_MASK);
#if JS_BITS_PER_WORD == 32
    uintptr_t addr = w & JSID_PAYLOAD_MASK;
#elif JS_BITS_PER_WORD == 64
    uintptr_t addr = w & JSID_PAYLOAD_MASK & JSVAL_PAYLOAD_MASK;
#endif

    Chunk *chunk = Chunk::fromAddress(addr);

    if (!rt->gcChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    /*
     * We query for pointers outside the arena array after checking for an
     * allocated chunk. Such pointers are rare and we want to reject them
     * after doing more likely rejections.
     */
    if (!Chunk::withinArenasRange(addr))
        return CGCT_NOTARENA;

    /* If the arena is not currently allocated, don't access the header. */
    size_t arenaOffset = Chunk::arenaIndex(addr);
    if (chunk->decommittedArenas.get(arenaOffset))
        return CGCT_FREEARENA;

    ArenaHeader *aheader = &chunk->arenas[arenaOffset].aheader;

    if (!aheader->allocated())
        return CGCT_FREEARENA;

    if (skipUncollectedCompartments && !aheader->zone->isCollecting())
        return CGCT_OTHERCOMPARTMENT;

    AllocKind thingKind = aheader->getAllocKind();
    uintptr_t offset = addr & ArenaMask;
    uintptr_t minOffset = Arena::firstThingOffset(thingKind);
    if (offset < minOffset)
        return CGCT_NOTARENA;

    /* addr can point inside the thing so we must align the address. */
    uintptr_t shift = (offset - minOffset) % Arena::thingSize(thingKind);
    addr -= shift;

    if (thing)
        *thing = reinterpret_cast<void *>(addr);
    if (arenaHeader)
        *arenaHeader = aheader;
    if (thingKindPtr)
        *thingKindPtr = thingKind;
    return CGCT_VALID;
}

#ifdef JSGC_ROOT_ANALYSIS
void *
js::gc::GetAddressableGCThing(JSRuntime *rt, uintptr_t w)
{
    void *thing;
    ArenaHeader *aheader;
    AllocKind thingKind;
    ConservativeGCTest status =
        IsAddressableGCThing(rt, w, false, &thingKind, &aheader, &thing);
    if (status != CGCT_VALID)
        return nullptr;
    return thing;
}
#endif

/*
 * Returns CGCT_VALID and mark it if the w can be a  live GC thing and sets
 * thingKind accordingly. Otherwise returns the reason for rejection.
 */
static inline ConservativeGCTest
MarkIfGCThingWord(JSTracer *trc, uintptr_t w)
{
    void *thing;
    ArenaHeader *aheader;
    AllocKind thingKind;
    ConservativeGCTest status =
        IsAddressableGCThing(trc->runtime, w, IS_GC_MARKING_TRACER(trc),
                             &thingKind, &aheader, &thing);
    if (status != CGCT_VALID)
        return status;

    /*
     * Check if the thing is free. We must use the list of free spans as at
     * this point we no longer have the mark bits from the previous GC run and
     * we must account for newly allocated things.
     */
    if (InFreeList(aheader, thing))
        return CGCT_NOTLIVE;

    JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
#ifdef DEBUG
    const char pattern[] = "machine_stack %p";
    char nameBuf[sizeof(pattern) - 2 + sizeof(thing) * 2];
    JS_snprintf(nameBuf, sizeof(nameBuf), pattern, thing);
    JS_SET_TRACING_NAME(trc, nameBuf);
#endif
    JS_SET_TRACING_LOCATION(trc, (void *)w);
    void *tmp = thing;
    MarkKind(trc, &tmp, traceKind);
    JS_ASSERT(tmp == thing);

#ifdef DEBUG
    if (trc->runtime->gcIncrementalState == MARK_ROOTS)
        trc->runtime->mainThread.gcSavedRoots.append(
            PerThreadData::SavedGCRoot(thing, traceKind));
#endif

    return CGCT_VALID;
}

static void
MarkWordConservatively(JSTracer *trc, uintptr_t w)
{
    /*
     * The conservative scanner may access words that valgrind considers as
     * undefined. To avoid false positives and not to alter valgrind view of
     * the memory we make as memcheck-defined the argument, a copy of the
     * original word. See bug 572678.
     */
#ifdef MOZ_VALGRIND
    JS_SILENCE_UNUSED_VALUE_IN_EXPR(VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w)));
#endif

    MarkIfGCThingWord(trc, w);
}

MOZ_ASAN_BLACKLIST
static void
MarkRangeConservatively(JSTracer *trc, const uintptr_t *begin, const uintptr_t *end)
{
    JS_ASSERT(begin <= end);
    for (const uintptr_t *i = begin; i < end; ++i)
        MarkWordConservatively(trc, *i);
}

#ifndef JSGC_USE_EXACT_ROOTING
static void
MarkRangeConservativelyAndSkipIon(JSTracer *trc, JSRuntime *rt, const uintptr_t *begin, const uintptr_t *end)
{
    const uintptr_t *i = begin;

#if JS_STACK_GROWTH_DIRECTION < 0 && defined(JS_ION)
    // Walk only regions in between JIT activations. Note that non-volatile
    // registers are spilled to the stack before the entry frame, ensuring
    // that the conservative scanner will still see them.
    for (jit::JitActivationIterator iter(rt); !iter.done(); ++iter) {
        uintptr_t *jitMin, *jitEnd;
        iter.jitStackRange(jitMin, jitEnd);

        MarkRangeConservatively(trc, i, jitMin);
        i = jitEnd;
    }
#endif

    // Mark everything after the most recent Ion activation.
    MarkRangeConservatively(trc, i, end);
}

static JS_NEVER_INLINE void
MarkConservativeStackRoots(JSTracer *trc, bool useSavedRoots)
{
    JSRuntime *rt = trc->runtime;

#ifdef DEBUG
    if (useSavedRoots) {
        for (PerThreadData::SavedGCRoot *root = rt->mainThread.gcSavedRoots.begin();
             root != rt->mainThread.gcSavedRoots.end();
             root++)
        {
            JS_SET_TRACING_NAME(trc, "cstack");
            MarkKind(trc, &root->thing, root->kind);
        }
        return;
    }

    if (rt->gcIncrementalState == MARK_ROOTS)
        rt->mainThread.gcSavedRoots.clearAndFree();
#endif

    ConservativeGCData *cgcd = &rt->conservativeGC;
    if (!cgcd->hasStackToScan()) {
#ifdef JS_THREADSAFE
        JS_ASSERT(!rt->requestDepth);
#endif
        return;
    }

    uintptr_t *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = rt->nativeStackBase;
    stackEnd = cgcd->nativeStackTop;
#else
    stackMin = cgcd->nativeStackTop + 1;
    stackEnd = reinterpret_cast<uintptr_t *>(rt->nativeStackBase);
#endif

    JS_ASSERT(stackMin <= stackEnd);
    MarkRangeConservativelyAndSkipIon(trc, rt, stackMin, stackEnd);
    MarkRangeConservatively(trc, cgcd->registerSnapshot.words,
                            ArrayEnd(cgcd->registerSnapshot.words));
}

#endif /* JSGC_USE_EXACT_ROOTING */

void
js::MarkStackRangeConservatively(JSTracer *trc, Value *beginv, Value *endv)
{
    const uintptr_t *begin = beginv->payloadUIntPtr();
    const uintptr_t *end = endv->payloadUIntPtr();
#ifdef JS_NUNBOX32
    /*
     * With 64-bit jsvals on 32-bit systems, we can optimize a bit by
     * scanning only the payloads.
     */
    JS_ASSERT(begin <= end);
    for (const uintptr_t *i = begin; i < end; i += sizeof(Value) / sizeof(uintptr_t))
        MarkWordConservatively(trc, *i);
#else
    MarkRangeConservatively(trc, begin, end);
#endif
}

JS_NEVER_INLINE void
ConservativeGCData::recordStackTop()
{
    /* Update the native stack pointer if it points to a bigger stack. */
    uintptr_t dummy;
    nativeStackTop = &dummy;

    /*
     * To record and update the register snapshot for the conservative scanning
     * with the latest values we use setjmp.
     */
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4611)
#endif
    (void) setjmp(registerSnapshot.jmpbuf);
#if defined(_MSC_VER)
# pragma warning(pop)
#endif
}

void
JS::AutoIdArray::trace(JSTracer *trc)
{
    JS_ASSERT(tag_ == IDARRAY);
    gc::MarkIdRange(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag_) {
      case PARSER:
        frontend::MarkParser(trc, this);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        MarkIdRange(trc, ida->length, ida->vector, "JS::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropDescArray &descriptors =
            static_cast<AutoPropDescArrayRooter *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropDesc &desc = descriptors[i];
            MarkValueRoot(trc, &desc.pd_, "PropDesc::pd_");
            MarkValueRoot(trc, &desc.value_, "PropDesc::value_");
            MarkValueRoot(trc, &desc.get_, "PropDesc::get_");
            MarkValueRoot(trc, &desc.set_, "PropDesc::set_");
        }
        return;
      }

      case ID:
        MarkIdRoot(trc, &static_cast<AutoIdRooter *>(this)->id_, "JS::AutoIdRooter.id_");
        return;

      case VALVECTOR: {
        AutoValueVector::VectorImpl &vector = static_cast<AutoValueVector *>(this)->vector;
        MarkValueRootRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (static_cast<AutoStringRooter *>(this)->str_)
            MarkStringRoot(trc, &static_cast<AutoStringRooter *>(this)->str_,
                           "JS::AutoStringRooter.str_");
        return;

      case IDVECTOR: {
        AutoIdVector::VectorImpl &vector = static_cast<AutoIdVector *>(this)->vector;
        MarkIdRootRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }

      case SHAPEVECTOR: {
        AutoShapeVector::VectorImpl &vector = static_cast<js::AutoShapeVector *>(this)->vector;
        MarkShapeRootRange(trc, vector.length(), const_cast<Shape **>(vector.begin()),
                           "js::AutoShapeVector.vector");
        return;
      }

      case OBJVECTOR: {
        AutoObjectVector::VectorImpl &vector = static_cast<AutoObjectVector *>(this)->vector;
        MarkObjectRootRange(trc, vector.length(), vector.begin(), "js::AutoObjectVector.vector");
        return;
      }

      case FUNVECTOR: {
        AutoFunctionVector::VectorImpl &vector = static_cast<AutoFunctionVector *>(this)->vector;
        MarkObjectRootRange(trc, vector.length(), vector.begin(), "js::AutoFunctionVector.vector");
        return;
      }

      case STRINGVECTOR: {
        AutoStringVector::VectorImpl &vector = static_cast<AutoStringVector *>(this)->vector;
        MarkStringRootRange(trc, vector.length(), vector.begin(), "js::AutoStringVector.vector");
        return;
      }

      case NAMEVECTOR: {
        AutoNameVector::VectorImpl &vector = static_cast<AutoNameVector *>(this)->vector;
        MarkStringRootRange(trc, vector.length(), vector.begin(), "js::AutoNameVector.vector");
        return;
      }

      case VALARRAY: {
        AutoValueArray *array = static_cast<AutoValueArray *>(this);
        MarkValueRootRange(trc, array->length(), array->start(), "js::AutoValueArray");
        return;
      }

      case SCRIPTVECTOR: {
        AutoScriptVector::VectorImpl &vector = static_cast<AutoScriptVector *>(this)->vector;
        MarkScriptRootRange(trc, vector.length(), vector.begin(), "js::AutoScriptVector.vector");
        return;
      }

      case OBJOBJHASHMAP: {
        AutoObjectObjectHashMap::HashMapImpl &map = static_cast<AutoObjectObjectHashMap *>(this)->map;
        for (AutoObjectObjectHashMap::Enum e(map); !e.empty(); e.popFront()) {
            MarkObjectRoot(trc, &e.front().value, "AutoObjectObjectHashMap value");
            JS_SET_TRACING_LOCATION(trc, (void *)&e.front().key);
            JSObject *key = e.front().key;
            MarkObjectRoot(trc, &key, "AutoObjectObjectHashMap key");
            if (key != e.front().key)
                e.rekeyFront(key);
        }
        return;
      }

      case OBJU32HASHMAP: {
        AutoObjectUnsigned32HashMap *self = static_cast<AutoObjectUnsigned32HashMap *>(this);
        AutoObjectUnsigned32HashMap::HashMapImpl &map = self->map;
        for (AutoObjectUnsigned32HashMap::Enum e(map); !e.empty(); e.popFront()) {
            JSObject *key = e.front().key;
            MarkObjectRoot(trc, &key, "AutoObjectUnsignedHashMap key");
            if (key != e.front().key)
                e.rekeyFront(key);
        }
        return;
      }

      case OBJHASHSET: {
        AutoObjectHashSet *self = static_cast<AutoObjectHashSet *>(this);
        AutoObjectHashSet::HashSetImpl &set = self->set;
        for (AutoObjectHashSet::Enum e(set); !e.empty(); e.popFront()) {
            JSObject *obj = e.front();
            MarkObjectRoot(trc, &obj, "AutoObjectHashSet value");
            if (obj != e.front())
                e.rekeyFront(obj);
        }
        return;
      }

      case HASHABLEVALUE: {
        AutoHashableValueRooter *rooter = static_cast<AutoHashableValueRooter *>(this);
        rooter->trace(trc);
        return;
      }

      case IONMASM: {
#ifdef JS_ION
        static_cast<js::jit::MacroAssembler::AutoRooter *>(this)->masm()->trace(trc);
#endif
        return;
      }

      case IONALLOC: {
#ifdef JS_ION
        static_cast<js::jit::AutoTempAllocatorRooter *>(this)->trace(trc);
#endif
        return;
      }

      case WRAPPER: {
        /*
         * We need to use MarkValueUnbarriered here because we mark wrapper
         * roots in every slice. This is because of some rule-breaking in
         * RemapAllWrappersForObject; see comment there.
         */
          MarkValueUnbarriered(trc, &static_cast<AutoWrapperRooter *>(this)->value.get(),
                               "JS::AutoWrapperRooter.value");
        return;
      }

      case WRAPVECTOR: {
        AutoWrapperVector::VectorImpl &vector = static_cast<AutoWrapperVector *>(this)->vector;
        /*
         * We need to use MarkValueUnbarriered here because we mark wrapper
         * roots in every slice. This is because of some rule-breaking in
         * RemapAllWrappersForObject; see comment there.
         */
        for (WrapperValue *p = vector.begin(); p < vector.end(); p++)
            MarkValueUnbarriered(trc, &p->get(), "js::AutoWrapperVector.vector");
        return;
      }

      case JSONPARSER:
        static_cast<js::JSONParser *>(this)->trace(trc);
        return;

      case CUSTOM:
        static_cast<JS::CustomAutoRooter *>(this)->trace(trc);
        return;
    }

    JS_ASSERT(tag_ >= 0);
    if (Value *vp = static_cast<AutoArrayRooter *>(this)->array)
        MarkValueRootRange(trc, tag_, vp, "JS::AutoArrayRooter.array");
}

/* static */ void
AutoGCRooter::traceAll(JSTracer *trc)
{
    for (ContextIter cx(trc->runtime); !cx.done(); cx.next()) {
        for (js::AutoGCRooter *gcr = cx->autoGCRooters; gcr; gcr = gcr->down)
            gcr->trace(trc);
    }
}

/* static */ void
AutoGCRooter::traceAllWrappers(JSTracer *trc)
{
    for (ContextIter cx(trc->runtime); !cx.done(); cx.next()) {
        for (js::AutoGCRooter *gcr = cx->autoGCRooters; gcr; gcr = gcr->down) {
            if (gcr->tag_ == WRAPVECTOR || gcr->tag_ == WRAPPER)
                gcr->trace(trc);
        }
    }
}

void
AutoHashableValueRooter::trace(JSTracer *trc)
{
    MarkValueRoot(trc, reinterpret_cast<Value*>(&value), "AutoHashableValueRooter");
}

void
StackShape::AutoRooter::trace(JSTracer *trc)
{
    if (shape->base)
        MarkBaseShapeRoot(trc, (BaseShape**) &shape->base, "StackShape::AutoRooter base");
    MarkIdRoot(trc, (jsid*) &shape->propid, "StackShape::AutoRooter id");
}

void
JSPropertyDescriptor::trace(JSTracer *trc)
{
    if (obj)
        MarkObjectRoot(trc, &obj, "Descriptor::obj");
    MarkValueRoot(trc, &value, "Descriptor::value");
    if ((attrs & JSPROP_GETTER) && getter) {
        JSObject *tmp = JS_FUNC_TO_DATA_PTR(JSObject *, getter);
        MarkObjectRoot(trc, &tmp, "Descriptor::get");
        getter = JS_DATA_TO_FUNC_PTR(JSPropertyOp, tmp);
    }
    if ((attrs & JSPROP_SETTER) && setter) {
        JSObject *tmp = JS_FUNC_TO_DATA_PTR(JSObject *, setter);
        MarkObjectRoot(trc, &tmp, "Descriptor::set");
        setter = JS_DATA_TO_FUNC_PTR(JSStrictPropertyOp, tmp);
    }
}

// Mark a chain of PersistentRooted pointers that might be null.
template<typename Referent>
static void
MarkPersistentRootedChain(JSTracer *trc,
                          mozilla::LinkedList<PersistentRooted<Referent *> > &list,
                          void (*marker)(JSTracer *trc, Referent **ref, const char *name),
                          const char *name)
{
    for (PersistentRooted<Referent *> *r = list.getFirst();
         r != nullptr;
         r = r->getNext())
    {
        if (r->get())
            marker(trc, r->address(), name);
    }
}

void
js::gc::MarkPersistentRootedChains(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;

    MarkPersistentRootedChain(trc, rt->functionPersistentRooteds, &MarkObjectRoot,
                              "PersistentRooted<JSFunction *>");
    MarkPersistentRootedChain(trc, rt->objectPersistentRooteds, &MarkObjectRoot,
                              "PersistentRooted<JSObject *>");
    MarkPersistentRootedChain(trc, rt->scriptPersistentRooteds, &MarkScriptRoot,
                              "PersistentRooted<JSScript *>");
    MarkPersistentRootedChain(trc, rt->stringPersistentRooteds, &MarkStringRoot, 
                              "PersistentRooted<JSString *>");

    // Mark the PersistentRooted chains of types that are never null.
    for (JS::PersistentRootedId *r = rt->idPersistentRooteds.getFirst(); r != nullptr; r = r->getNext())
        MarkIdRoot(trc, r->address(), "PersistentRooted<jsid>");
    for (JS::PersistentRootedValue *r = rt->valuePersistentRooteds.getFirst(); r != nullptr; r = r->getNext())
        MarkValueRoot(trc, r->address(), "PersistentRooted<Value>");
}

void
js::gc::MarkRuntime(JSTracer *trc, bool useSavedRoots)
{
    JSRuntime *rt = trc->runtime;
    JS_ASSERT(trc->callback != GCMarker::GrayCallback);

    JS_ASSERT(!rt->mainThread.suppressGC);

    if (IS_GC_MARKING_TRACER(trc)) {
        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
            if (!c->zone()->isCollecting())
                c->markCrossCompartmentWrappers(trc);
        }
        Debugger::markCrossCompartmentDebuggerObjectReferents(trc);
    }

    AutoGCRooter::traceAll(trc);

    if (!rt->isBeingDestroyed()) {
#ifdef JSGC_USE_EXACT_ROOTING
        MarkExactStackRoots(trc);
#else
        MarkConservativeStackRoots(trc, useSavedRoots);
#endif
        rt->markSelfHostingGlobal(trc);
    }

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        const RootEntry &entry = r.front();
        const char *name = entry.value.name ? entry.value.name : "root";
        if (entry.value.type == JS_GC_ROOT_VALUE_PTR) {
            MarkValueRoot(trc, reinterpret_cast<Value *>(entry.key), name);
        } else if (*reinterpret_cast<void **>(entry.key)){
            if (entry.value.type == JS_GC_ROOT_STRING_PTR)
                MarkStringRoot(trc, reinterpret_cast<JSString **>(entry.key), name);
            else if (entry.value.type == JS_GC_ROOT_OBJECT_PTR)
                MarkObjectRoot(trc, reinterpret_cast<JSObject **>(entry.key), name);
            else if (entry.value.type == JS_GC_ROOT_SCRIPT_PTR)
                MarkScriptRoot(trc, reinterpret_cast<JSScript **>(entry.key), name);
            else
                MOZ_ASSUME_UNREACHABLE("unexpected js::RootInfo::type value");
        }
    }

    MarkPersistentRootedChains(trc);

    if (rt->scriptAndCountsVector) {
        ScriptAndCountsVector &vec = *rt->scriptAndCountsVector;
        for (size_t i = 0; i < vec.length(); i++)
            MarkScriptRoot(trc, &vec[i].script, "scriptAndCountsVector");
    }

    if (!rt->isBeingDestroyed() && !trc->runtime->isHeapMinorCollecting()) {
        if (!IS_GC_MARKING_TRACER(trc) || rt->atomsCompartment()->zone()->isCollecting()) {
            MarkAtoms(trc);
            rt->staticStrings.trace(trc);
#ifdef JS_ION
            jit::JitRuntime::Mark(trc);
#endif
        }
    }

    for (ContextIter acx(rt); !acx.done(); acx.next())
        acx->mark(trc);

    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        if (IS_GC_MARKING_TRACER(trc) && !zone->isCollecting())
            continue;

        if (IS_GC_MARKING_TRACER(trc) && zone->isPreservingCode()) {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_MARK_TYPES);
            zone->markTypes(trc);
        }

        /* Do not discard scripts with counts while profiling. */
        if (rt->profilingScripts && !rt->isHeapMinorCollecting()) {
            for (CellIterUnderGC i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
                JSScript *script = i.get<JSScript>();
                if (script->hasScriptCounts) {
                    MarkScriptRoot(trc, &script, "profilingScripts");
                    JS_ASSERT(script == i.get<JSScript>());
                }
            }
        }
    }

    /* We can't use GCCompartmentsIter if we're called from TraceRuntime. */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        if (trc->runtime->isHeapMinorCollecting())
            c->globalWriteBarriered = false;

        if (IS_GC_MARKING_TRACER(trc) && !c->zone()->isCollecting())
            continue;

        /* During a GC, these are treated as weak pointers. */
        if (!IS_GC_MARKING_TRACER(trc)) {
            if (c->watchpointMap)
                c->watchpointMap->markAll(trc);
        }

        /* Mark debug scopes, if present */
        if (c->debugScopes)
            c->debugScopes->mark(trc);
    }

    MarkInterpreterActivations(rt, trc);

#ifdef JS_ION
    jit::MarkJitActivations(rt, trc);
#endif

    if (!rt->isHeapMinorCollecting()) {
        /*
         * All JSCompartment::mark does is mark the globals for compartments
         * which have been entered. Globals aren't nursery allocated so there's
         * no need to do this for minor GCs.
         */
        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
            c->mark(trc);

        /*
         * The embedding can register additional roots here.
         *
         * We don't need to trace these in a minor GC because all pointers into
         * the nursery should be in the store buffer, and we want to avoid the
         * time taken to trace all these roots.
         */
        for (size_t i = 0; i < rt->gcBlackRootTracers.length(); i++) {
            const JSRuntime::ExtraTracer &e = rt->gcBlackRootTracers[i];
            (*e.op)(trc, e.data);
        }

        /* During GC, we don't mark gray roots at this stage. */
        if (JSTraceDataOp op = rt->gcGrayRootTracer.op) {
            if (!IS_GC_MARKING_TRACER(trc))
                (*op)(trc, rt->gcGrayRootTracer.data);
        }
    }
}

void
js::gc::BufferGrayRoots(GCMarker *gcmarker)
{
    JSRuntime *rt = gcmarker->runtime;
    if (JSTraceDataOp op = rt->gcGrayRootTracer.op) {
        gcmarker->startBufferingGrayRoots();
        (*op)(gcmarker, rt->gcGrayRootTracer.data);
        gcmarker->endBufferingGrayRoots();
    }
}
