/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

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
#include "vm/PropDesc.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;

using JS::AutoGCRooter;

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

#ifdef JSGC_USE_EXACT_ROOTING

// Note: the following two functions cannot be static as long as we are using
// GCC 4.4, since it requires template function parameters to have external
// linkage.

void
MarkBindingsRoot(JSTracer *trc, Bindings *bindings, const char *name)
{
    bindings->trace(trc);
}

void
MarkPropertyDescriptorRoot(JSTracer *trc, JSPropertyDescriptor *pd, const char *name)
{
    pd->trace(trc);
}

void
MarkPropDescRoot(JSTracer *trc, PropDesc *pd, const char *name)
{
    pd->trace(trc);
}

template <class T>
static inline bool
IgnoreExactRoot(T *thingp)
{
    return false;
}

template <class T>
inline bool
IgnoreExactRoot(T **thingp)
{
    return IsNullTaggedPointer(*thingp);
}

template <>
inline bool
IgnoreExactRoot(JSObject **thingp)
{
    return IsNullTaggedPointer(*thingp) || *thingp == TaggedProto::LazyProto;
}

template <class T, void MarkFunc(JSTracer *trc, T *ref, const char *name), class Source>
static inline void
MarkExactStackRootList(JSTracer *trc, Source *s, const char *name)
{
    Rooted<T> *rooter = s->template gcRooters<T>();
    while (rooter) {
        T *addr = rooter->address();
        if (!IgnoreExactRoot(addr))
            MarkFunc(trc, addr, name);
        rooter = rooter->previous();
    }
}

template <class T, void (MarkFunc)(JSTracer *trc, T *ref, const char *name)>
static inline void
MarkExactStackRootsForType(JSTracer *trc, const char *name = nullptr)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next())
        MarkExactStackRootList<T, MarkFunc>(trc, cx.get(), name);
    MarkExactStackRootList<T, MarkFunc>(trc, &trc->runtime()->mainThread, name);
}

static void
MarkExactStackRoots(JSTracer *trc)
{
    MarkExactStackRootsForType<JSObject *, MarkObjectRoot>(trc, "exact-object");
    MarkExactStackRootsForType<Shape *, MarkShapeRoot>(trc, "exact-shape");
    MarkExactStackRootsForType<BaseShape *, MarkBaseShapeRoot>(trc, "exact-baseshape");
    MarkExactStackRootsForType<types::TypeObject *, MarkTypeObjectRoot>(trc, "exact-typeobject");
    MarkExactStackRootsForType<JSString *, MarkStringRoot>(trc, "exact-string");
    MarkExactStackRootsForType<jit::JitCode *, MarkJitCodeRoot>(trc, "exact-jitcode");
    MarkExactStackRootsForType<JSScript *, MarkScriptRoot>(trc, "exact-script");
    MarkExactStackRootsForType<LazyScript *, MarkLazyScriptRoot>(trc, "exact-lazy-script");
    MarkExactStackRootsForType<jsid, MarkIdRoot>(trc, "exact-id");
    MarkExactStackRootsForType<Value, MarkValueRoot>(trc, "exact-value");
    MarkExactStackRootsForType<types::Type, MarkTypeRoot>(trc, "exact-type");
    MarkExactStackRootsForType<Bindings, MarkBindingsRoot>(trc);
    MarkExactStackRootsForType<JSPropertyDescriptor, MarkPropertyDescriptorRoot>(trc);
    MarkExactStackRootsForType<PropDesc, MarkPropDescRoot>(trc);
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
#ifndef JSGC_USE_EXACT_ROOTING
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

    if (!rt->gc.chunkSet.has(chunk))
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
        IsAddressableGCThing(trc->runtime(), w, IS_GC_MARKING_TRACER(trc),
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
    trc->setTracingName(nameBuf);
#endif
    trc->setTracingLocation((void *)w);
    void *tmp = thing;
    MarkKind(trc, &tmp, traceKind);
    JS_ASSERT(tmp == thing);

#ifdef DEBUG
    if (trc->runtime()->gc.incrementalState == MARK_ROOTS)
        trc->runtime()->mainThread.gcSavedRoots.append(
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

static void
MarkRangeConservativelyAndSkipIon(JSTracer *trc, JSRuntime *rt, const uintptr_t *begin, const uintptr_t *end)
{
    const uintptr_t *i = begin;

#if JS_STACK_GROWTH_DIRECTION < 0 && defined(JS_ION) && !defined(JS_ARM_SIMULATOR) && !defined(JS_MIPS_SIMULATOR)
    // Walk only regions in between JIT activations. Note that non-volatile
    // registers are spilled to the stack before the entry frame, ensuring
    // that the conservative scanner will still see them.
    //
    // If the ARM or MIPS simulator is enabled, JIT activations are not on
    // the native stack but on the simulator stack, so we don't have to skip
    // JIT regions in this case.
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

MOZ_NEVER_INLINE void
gc::GCRuntime::markConservativeStackRoots(JSTracer *trc, bool useSavedRoots)
{
#ifdef DEBUG
    if (useSavedRoots) {
        for (PerThreadData::SavedGCRoot *root = rt->mainThread.gcSavedRoots.begin();
             root != rt->mainThread.gcSavedRoots.end();
             root++)
        {
            trc->setTracingName("cstack");
            MarkKind(trc, &root->thing, root->kind);
        }
        return;
    }

    if (incrementalState == MARK_ROOTS)
        rt->mainThread.gcSavedRoots.clearAndFree();
#endif

    if (!conservativeGC.hasStackToScan()) {
#ifdef JS_THREADSAFE
        JS_ASSERT(!rt->requestDepth);
#endif
        return;
    }

    uintptr_t *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = reinterpret_cast<uintptr_t *>(rt->nativeStackBase);
    stackEnd = conservativeGC.nativeStackTop;
#else
    stackMin = conservativeGC.nativeStackTop + 1;
    stackEnd = reinterpret_cast<uintptr_t *>(rt->nativeStackBase);
#endif

    JS_ASSERT(stackMin <= stackEnd);
    MarkRangeConservativelyAndSkipIon(trc, rt, stackMin, stackEnd);
    MarkRangeConservatively(trc, conservativeGC.registerSnapshot.words,
                            ArrayEnd(conservativeGC.registerSnapshot.words));
}

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

#endif /* JSGC_USE_EXACT_ROOTING */

MOZ_NEVER_INLINE void
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

      case DESCVECTOR: {
        AutoPropDescVector::VectorImpl &descriptors =
            static_cast<AutoPropDescVector *>(this)->vector;
        for (size_t i = 0, len = descriptors.length(); i < len; i++)
            descriptors[i].trace(trc);
        return;
      }

      case VALVECTOR: {
        AutoValueVector::VectorImpl &vector = static_cast<AutoValueVector *>(this)->vector;
        MarkValueRootRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

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
        /*
         * We don't know the template size parameter, but we can safely treat it
         * as an AutoValueArray<1> because the length is stored separately.
         */
        AutoValueArray<1> *array = static_cast<AutoValueArray<1> *>(this);
        MarkValueRootRange(trc, array->length(), array->begin(), "js::AutoValueArray");
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
            MarkObjectRoot(trc, &e.front().value(), "AutoObjectObjectHashMap value");
            trc->setTracingLocation((void *)&e.front().key());
            JSObject *key = e.front().key();
            MarkObjectRoot(trc, &key, "AutoObjectObjectHashMap key");
            if (key != e.front().key())
                e.rekeyFront(key);
        }
        return;
      }

      case OBJU32HASHMAP: {
        AutoObjectUnsigned32HashMap *self = static_cast<AutoObjectUnsigned32HashMap *>(this);
        AutoObjectUnsigned32HashMap::HashMapImpl &map = self->map;
        for (AutoObjectUnsigned32HashMap::Enum e(map); !e.empty(); e.popFront()) {
            JSObject *key = e.front().key();
            MarkObjectRoot(trc, &key, "AutoObjectUnsignedHashMap key");
            if (key != e.front().key())
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
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next()) {
        for (AutoGCRooter *gcr = cx->autoGCRooters; gcr; gcr = gcr->down)
            gcr->trace(trc);
    }
}

/* static */ void
AutoGCRooter::traceAllWrappers(JSTracer *trc)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next()) {
        for (AutoGCRooter *gcr = cx->autoGCRooters; gcr; gcr = gcr->down) {
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
StackShape::trace(JSTracer *trc)
{
    if (base)
        MarkBaseShapeRoot(trc, (BaseShape**) &base, "StackShape base");
    MarkIdRoot(trc, (jsid*) &propid, "StackShape id");
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

namespace js {
namespace gc {

template<typename T>
struct PersistentRootedMarker
{
    typedef PersistentRooted<T> Element;
    typedef mozilla::LinkedList<Element> List;
    typedef void (*MarkFunc)(JSTracer *trc, T *ref, const char *name);

    template <MarkFunc Mark>
    static void
    markChainIfNotNull(JSTracer *trc, List &list, const char *name)
    {
        for (Element *r = list.getFirst(); r; r = r->getNext()) {
            if (r->get())
                Mark(trc, r->address(), name);
        }
    }

    template <MarkFunc Mark>
    static void
    markChain(JSTracer *trc, List &list, const char *name)
    {
        for (Element *r = list.getFirst(); r; r = r->getNext())
            Mark(trc, r->address(), name);
    }
};
}
}

void
js::gc::MarkPersistentRootedChains(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime();

    // Mark the PersistentRooted chains of types that may be null.
    PersistentRootedMarker<JSFunction*>::markChainIfNotNull<MarkObjectRoot>(
        trc, rt->functionPersistentRooteds, "PersistentRooted<JSFunction *>");
    PersistentRootedMarker<JSObject*>::markChainIfNotNull<MarkObjectRoot>(
        trc, rt->objectPersistentRooteds, "PersistentRooted<JSObject *>");
    PersistentRootedMarker<JSScript*>::markChainIfNotNull<MarkScriptRoot>(
        trc, rt->scriptPersistentRooteds, "PersistentRooted<JSScript *>");
    PersistentRootedMarker<JSString*>::markChainIfNotNull<MarkStringRoot>(
        trc, rt->stringPersistentRooteds, "PersistentRooted<JSString *>");

    // Mark the PersistentRooted chains of types that are never null.
    PersistentRootedMarker<jsid>::markChain<MarkIdRoot>(trc, rt->idPersistentRooteds,
                                                        "PersistentRooted<jsid>");
    PersistentRootedMarker<Value>::markChain<MarkValueRoot>(trc, rt->valuePersistentRooteds,
                                                            "PersistentRooted<Value>");
}

void
js::gc::GCRuntime::markRuntime(JSTracer *trc, bool useSavedRoots)
{
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
        markConservativeStackRoots(trc, useSavedRoots);
#endif
        rt->markSelfHostingGlobal(trc);
    }

    for (RootRange r = rootsHash.all(); !r.empty(); r.popFront()) {
        const RootEntry &entry = r.front();
        const char *name = entry.value().name ? entry.value().name : "root";
        JSGCRootType type = entry.value().type;
        void *key = entry.key();
        if (type == JS_GC_ROOT_VALUE_PTR) {
            MarkValueRoot(trc, reinterpret_cast<Value *>(key), name);
        } else if (*reinterpret_cast<void **>(key)){
            if (type == JS_GC_ROOT_STRING_PTR)
                MarkStringRoot(trc, reinterpret_cast<JSString **>(key), name);
            else if (type == JS_GC_ROOT_OBJECT_PTR)
                MarkObjectRoot(trc, reinterpret_cast<JSObject **>(key), name);
            else if (type == JS_GC_ROOT_SCRIPT_PTR)
                MarkScriptRoot(trc, reinterpret_cast<JSScript **>(key), name);
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

    if (!rt->isBeingDestroyed() && !trc->runtime()->isHeapMinorCollecting()) {
        if (!IS_GC_MARKING_TRACER(trc) || rt->atomsCompartment()->zone()->isCollecting()) {
            MarkPermanentAtoms(trc);
            MarkAtoms(trc);
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

        /* Do not discard scripts with counts while profiling. */
        if (rt->profilingScripts && !isHeapMinorCollecting()) {
            for (ZoneCellIterUnderGC i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
                JSScript *script = i.get<JSScript>();
                if (script->hasScriptCounts()) {
                    MarkScriptRoot(trc, &script, "profilingScripts");
                    JS_ASSERT(script == i.get<JSScript>());
                }
            }
        }
    }

    /* We can't use GCCompartmentsIter if we're called from TraceRuntime. */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        if (trc->runtime()->isHeapMinorCollecting())
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

    if (!isHeapMinorCollecting()) {
        /*
         * All JSCompartment::mark does is mark the globals for compartments
         * which have been entered. Globals aren't nursery allocated so there's
         * no need to do this for minor GCs.
         */
        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
            c->markRoots(trc);

        /*
         * The embedding can register additional roots here.
         *
         * We don't need to trace these in a minor GC because all pointers into
         * the nursery should be in the store buffer, and we want to avoid the
         * time taken to trace all these roots.
         */
        for (size_t i = 0; i < blackRootTracers.length(); i++) {
            const Callback<JSTraceDataOp> &e = blackRootTracers[i];
            (*e.op)(trc, e.data);
        }

        /* During GC, we don't mark gray roots at this stage. */
        if (JSTraceDataOp op = grayRootTracer.op) {
            if (!IS_GC_MARKING_TRACER(trc))
                (*op)(trc, grayRootTracer.data);
        }
    }
}

void
js::gc::GCRuntime::bufferGrayRoots()
{
    marker.startBufferingGrayRoots();
    if (JSTraceDataOp op = grayRootTracer.op)
        (*op)(&marker, grayRootTracer.data);
    marker.endBufferingGrayRoots();
}
