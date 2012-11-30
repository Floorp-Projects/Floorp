/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"

#include "js/HashTable.h"
#include "gc/GCInternals.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

#if defined(DEBUG) && defined(JS_GC_ZEAL) && defined(JSGC_ROOT_ANALYSIS) && !defined(JS_THREADSAFE)

JS_ALWAYS_INLINE bool
CheckStackRootThing(uintptr_t *w, void *address, ThingRootKind kind)
{
    if (kind != THING_ROOT_BINDINGS)
        return address == static_cast<void*>(w);

    Bindings *bp = static_cast<Bindings*>(address);
    return w >= (uintptr_t*)bp && w < (uintptr_t*)(bp + 1);
}

JS_ALWAYS_INLINE void
CheckStackRootThings(uintptr_t *w, Rooted<void*> *rooter, ThingRootKind kind, bool *matched)
{
    while (rooter) {
        if (CheckStackRootThing(w, rooter->address(), kind))
            *matched = true;
        rooter = rooter->previous();
    }
}

static void
CheckStackRoot(JSTracer *trc, uintptr_t *w)
{
    /* Mark memory as defined for valgrind, as in MarkWordConservatively. */
#ifdef JS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w));
#endif

    ConservativeGCTest test = MarkIfGCThingWord(trc, *w);

    if (test == CGCT_VALID) {
        bool matched = false;
        JSRuntime *rt = trc->runtime;
        for (unsigned i = 0; i < THING_ROOT_LIMIT; i++) {
            CheckStackRootThings(w, rt->mainThread.thingGCRooters[i],
                                 ThingRootKind(i), &matched);
            for (ContextIter cx(rt); !cx.done(); cx.next()) {
                CheckStackRootThings(w, cx->thingGCRooters[i], ThingRootKind(i), &matched);
                SkipRoot *skip = cx->skipGCRooters;
                while (skip) {
                    if (skip->contains(reinterpret_cast<uint8_t*>(w), sizeof(w)))
                        matched = true;
                    skip = skip->previous();
                }
            }
        }
        if (!matched) {
            /*
             * Only poison the last byte in the word. It is easy to get
             * accidental collisions when a value that does not occupy a full
             * word is used to overwrite a now-dead GC thing pointer. In this
             * case we want to avoid damaging the smaller value.
             */
            PoisonPtr(w);
        }
    }
}

static void
CheckStackRootsRange(JSTracer *trc, uintptr_t *begin, uintptr_t *end)
{
    JS_ASSERT(begin <= end);
    for (uintptr_t *i = begin; i != end; ++i)
        CheckStackRoot(trc, i);
}

static void
CheckStackRootsRangeAndSkipIon(JSRuntime *rt, JSTracer *trc, uintptr_t *begin, uintptr_t *end)
{
    /*
     * Regions of the stack between Ion activiations are marked exactly through
     * a different mechanism. We need to skip these regions when checking the
     * stack so that we do not poison IonMonkey's things.
     */
    uintptr_t *i = begin;

#if JS_STACK_GROWTH_DIRECTION < 0 && defined(JS_ION)
    for (ion::IonActivationIterator ion(rt); ion.more(); ++ion) {
        uintptr_t *ionMin, *ionEnd;
        ion.ionStackRange(ionMin, ionEnd);

        CheckStackRootsRange(trc, i, ionMin);
        i = ionEnd;
    }
#endif

    /* The topmost Ion activiation may be beyond our prior top. */
    if (i <= end)
        CheckStackRootsRange(trc, i, end);
}

static void
EmptyMarkCallback(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{}

void
JS::CheckStackRoots(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    if (rt->gcZeal_ != ZealStackRootingSafeValue && rt->gcZeal_ != ZealStackRootingValue)
        return;
    if (rt->gcZeal_ == ZealStackRootingSafeValue && !rt->gcExactScanningEnabled)
        return;

    // If this assertion fails, it means that an AutoAssertNoGC was placed
    // around code that could trigger GC, and is therefore wrong. The
    // AutoAssertNoGC should be removed and the code it was guarding should be
    // modified to properly root any gcthings, and very possibly any code
    // calling that function should also be modified if it was improperly
    // assuming that GC could not happen at all within the called function.
    // (The latter may not apply if the AutoAssertNoGC only protected a portion
    // of a function, so the callers were already assuming that GC could
    // happen.)
    JS_ASSERT(!InNoGCScope());

    // GCs can't happen when analysis/inference/compilation are active.
    if (cx->compartment->activeAnalysis)
        return;

    // Can switch to the atoms compartment during analysis.
    if (IsAtomsCompartment(cx->compartment)) {
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            if (c.get()->activeAnalysis)
                return;
        }
    }

    AutoCopyFreeListToArenas copy(rt);

    JSTracer checker;
    JS_TracerInit(&checker, rt, EmptyMarkCallback);

    ConservativeGCData *cgcd = &rt->conservativeGC;
    cgcd->recordStackTop();

    JS_ASSERT(cgcd->hasStackToScan());
    uintptr_t *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = rt->nativeStackBase;
    stackEnd = cgcd->nativeStackTop;
#else
    stackMin = cgcd->nativeStackTop + 1;
    stackEnd = reinterpret_cast<uintptr_t *>(rt->nativeStackBase);

    uintptr_t *&oldStackMin = cgcd->oldStackMin, *&oldStackEnd = cgcd->oldStackEnd;
    uintptr_t *&oldStackData = cgcd->oldStackData;
    uintptr_t &oldStackCapacity = cgcd->oldStackCapacity;

    /*
     * Adjust the stack to remove regions which have not changed since the
     * stack was last scanned, and update the last scanned state.
     */
    if (stackEnd != oldStackEnd) {
        js_free(oldStackData);
        oldStackCapacity = rt->nativeStackQuota / sizeof(uintptr_t);
        oldStackData = (uintptr_t *) rt->malloc_(oldStackCapacity * sizeof(uintptr_t));
        if (!oldStackData) {
            oldStackCapacity = 0;
        } else {
            uintptr_t *existing = stackEnd - 1, *copy = oldStackData;
            while (existing >= stackMin && size_t(copy - oldStackData) < oldStackCapacity)
                *copy++ = *existing--;
            oldStackEnd = stackEnd;
            oldStackMin = existing + 1;
        }
    } else {
        uintptr_t *existing = stackEnd - 1, *copy = oldStackData;
        while (existing >= stackMin && existing >= oldStackMin && *existing == *copy) {
            copy++;
            existing--;
        }
        stackEnd = existing + 1;
        while (existing >= stackMin && size_t(copy - oldStackData) < oldStackCapacity)
            *copy++ = *existing--;
        oldStackMin = existing + 1;
    }
#endif

    JS_ASSERT(stackMin <= stackEnd);
    CheckStackRootsRangeAndSkipIon(rt, &checker, stackMin, stackEnd);
    CheckStackRootsRange(&checker, cgcd->registerSnapshot.words,
                         ArrayEnd(cgcd->registerSnapshot.words));
}

#endif /* DEBUG && JS_GC_ZEAL && JSGC_ROOT_ANALYSIS && !JS_THREADSAFE */

#ifdef JS_GC_ZEAL

/*
 * Write barrier verification
 *
 * The next few functions are for write barrier verification.
 *
 * The VerifyBarriers function is a shorthand. It checks if a verification phase
 * is currently running. If not, it starts one. Otherwise, it ends the current
 * phase and starts a new one.
 *
 * The user can adjust the frequency of verifications, which causes
 * VerifyBarriers to be a no-op all but one out of N calls. However, if the
 * |always| parameter is true, it starts a new phase no matter what.
 *
 * Pre-Barrier Verifier:
 *   When StartVerifyBarriers is called, a snapshot is taken of all objects in
 *   the GC heap and saved in an explicit graph data structure. Later,
 *   EndVerifyBarriers traverses the heap again. Any pointer values that were in
 *   the snapshot and are no longer found must be marked; otherwise an assertion
 *   triggers. Note that we must not GC in between starting and finishing a
 *   verification phase.
 *
 * Post-Barrier Verifier:
 *   When StartVerifyBarriers is called, we create a virtual "Nursery Set" which
 *   future allocations are recorded in and turn on the StoreBuffer. Later,
 *   EndVerifyBarriers traverses the heap and ensures that the set of cross-
 *   generational pointers we find is a subset of the pointers recorded in our
 *   StoreBuffer.
 */

struct EdgeValue
{
    void *thing;
    JSGCTraceKind kind;
    char *label;
};

struct VerifyNode
{
    void *thing;
    JSGCTraceKind kind;
    uint32_t count;
    EdgeValue edges[1];
};

typedef HashMap<void *, VerifyNode *, DefaultHasher<void *>, SystemAllocPolicy> NodeMap;

/*
 * The verifier data structures are simple. The entire graph is stored in a
 * single block of memory. At the beginning is a VerifyNode for the root
 * node. It is followed by a sequence of EdgeValues--the exact number is given
 * in the node. After the edges come more nodes and their edges.
 *
 * The edgeptr and term fields are used to allocate out of the block of memory
 * for the graph. If we run out of memory (i.e., if edgeptr goes beyond term),
 * we just abandon the verification.
 *
 * The nodemap field is a hashtable that maps from the address of the GC thing
 * to the VerifyNode that represents it.
 */
struct VerifyPreTracer : JSTracer {
    /* The gcNumber when the verification began. */
    uint64_t number;

    /* This counts up to gcZealFrequency to decide whether to verify. */
    int count;

    /* This graph represents the initial GC "snapshot". */
    VerifyNode *curnode;
    VerifyNode *root;
    char *edgeptr;
    char *term;
    NodeMap nodemap;

    VerifyPreTracer() : root(NULL) {}
    ~VerifyPreTracer() { js_free(root); }
};

/*
 * This function builds up the heap snapshot by adding edges to the current
 * node.
 */
static void
AccumulateEdge(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyPreTracer *trc = (VerifyPreTracer *)jstrc;

    trc->edgeptr += sizeof(EdgeValue);
    if (trc->edgeptr >= trc->term) {
        trc->edgeptr = trc->term;
        return;
    }

    VerifyNode *node = trc->curnode;
    uint32_t i = node->count;

    node->edges[i].thing = *thingp;
    node->edges[i].kind = kind;
    node->edges[i].label = trc->debugPrinter ? NULL : (char *)trc->debugPrintArg;
    node->count++;
}

static VerifyNode *
MakeNode(VerifyPreTracer *trc, void *thing, JSGCTraceKind kind)
{
    NodeMap::AddPtr p = trc->nodemap.lookupForAdd(thing);
    if (!p) {
        VerifyNode *node = (VerifyNode *)trc->edgeptr;
        trc->edgeptr += sizeof(VerifyNode) - sizeof(EdgeValue);
        if (trc->edgeptr >= trc->term) {
            trc->edgeptr = trc->term;
            return NULL;
        }

        node->thing = thing;
        node->count = 0;
        node->kind = kind;
        trc->nodemap.add(p, thing, node);
        return node;
    }
    return NULL;
}

static VerifyNode *
NextNode(VerifyNode *node)
{
    if (node->count == 0)
        return (VerifyNode *)((char *)node + sizeof(VerifyNode) - sizeof(EdgeValue));
    else
        return (VerifyNode *)((char *)node + sizeof(VerifyNode) +
                             sizeof(EdgeValue)*(node->count - 1));
}

void
gc::StartVerifyPreBarriers(JSRuntime *rt)
{
    if (rt->gcVerifyPreData ||
        rt->gcIncrementalState != NO_INCREMENTAL ||
        !IsIncrementalGCSafe(rt))
    {
        return;
    }

    AutoPrepareForTracing prep(rt);

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    VerifyPreTracer *trc = js_new<VerifyPreTracer>();

    rt->gcNumber++;
    trc->number = rt->gcNumber;
    trc->count = 0;

    JS_TracerInit(trc, rt, AccumulateEdge);

    const size_t size = 64 * 1024 * 1024;
    trc->root = (VerifyNode *)js_malloc(size);
    JS_ASSERT(trc->root);
    trc->edgeptr = (char *)trc->root;
    trc->term = trc->edgeptr + size;

    if (!trc->nodemap.init())
        return;

    /* Create the root node. */
    trc->curnode = MakeNode(trc, NULL, JSGCTraceKind(0));

    /* We want MarkRuntime to save the roots to gcSavedRoots. */
    rt->gcIncrementalState = MARK_ROOTS;

    /* Make all the roots be edges emanating from the root node. */
    MarkRuntime(trc);

    VerifyNode *node = trc->curnode;
    if (trc->edgeptr == trc->term)
        goto oom;

    /* For each edge, make a node for it if one doesn't already exist. */
    while ((char *)node < trc->edgeptr) {
        for (uint32_t i = 0; i < node->count; i++) {
            EdgeValue &e = node->edges[i];
            VerifyNode *child = MakeNode(trc, e.thing, e.kind);
            if (child) {
                trc->curnode = child;
                JS_TraceChildren(trc, e.thing, e.kind);
            }
            if (trc->edgeptr == trc->term)
                goto oom;
        }

        node = NextNode(node);
    }

    rt->gcVerifyPreData = trc;
    rt->gcIncrementalState = MARK;
    rt->gcMarker.start(rt);
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        PurgeJITCaches(c);
        c->setNeedsBarrier(true, JSCompartment::UpdateIon);
        c->arenas.purge();
    }

    return;

oom:
    rt->gcIncrementalState = NO_INCREMENTAL;
    trc->~VerifyPreTracer();
    js_free(trc);
}

static bool
IsMarkedOrAllocated(Cell *cell)
{
    return cell->isMarked() || cell->arenaHeader()->allocatedDuringIncremental;
}

const static uint32_t MAX_VERIFIER_EDGES = 1000;

/*
 * This function is called by EndVerifyBarriers for every heap edge. If the edge
 * already existed in the original snapshot, we "cancel it out" by overwriting
 * it with NULL. EndVerifyBarriers later asserts that the remaining non-NULL
 * edges (i.e., the ones from the original snapshot that must have been
 * modified) must point to marked objects.
 */
static void
CheckEdge(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyPreTracer *trc = (VerifyPreTracer *)jstrc;
    VerifyNode *node = trc->curnode;

    /* Avoid n^2 behavior. */
    if (node->count > MAX_VERIFIER_EDGES)
        return;

    for (uint32_t i = 0; i < node->count; i++) {
        if (node->edges[i].thing == *thingp) {
            JS_ASSERT(node->edges[i].kind == kind);
            node->edges[i].thing = NULL;
            return;
        }
    }
}

static void
AssertMarkedOrAllocated(const EdgeValue &edge)
{
    if (!edge.thing || IsMarkedOrAllocated(static_cast<Cell *>(edge.thing)))
        return;

    char msgbuf[1024];
    const char *label = edge.label ? edge.label : "<unknown>";

    JS_snprintf(msgbuf, sizeof(msgbuf), "[barrier verifier] Unmarked edge: %s", label);
    MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
    MOZ_CRASH();
}

void
gc::EndVerifyPreBarriers(JSRuntime *rt)
{
    AutoPrepareForTracing prep(rt);

    VerifyPreTracer *trc = (VerifyPreTracer *)rt->gcVerifyPreData;

    if (!trc)
        return;

    bool compartmentCreated = false;

    /* We need to disable barriers before tracing, which may invoke barriers. */
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (!c->needsBarrier())
            compartmentCreated = true;

        PurgeJITCaches(c);
        c->setNeedsBarrier(false, JSCompartment::UpdateIon);
    }

    /*
     * We need to bump gcNumber so that the methodjit knows that jitcode has
     * been discarded.
     */
    JS_ASSERT(trc->number == rt->gcNumber);
    rt->gcNumber++;

    rt->gcVerifyPreData = NULL;
    rt->gcIncrementalState = NO_INCREMENTAL;

    if (!compartmentCreated && IsIncrementalGCSafe(rt)) {
        JS_TracerInit(trc, rt, CheckEdge);

        /* Start after the roots. */
        VerifyNode *node = NextNode(trc->root);
        while ((char *)node < trc->edgeptr) {
            trc->curnode = node;
            JS_TraceChildren(trc, node->thing, node->kind);

            if (node->count <= MAX_VERIFIER_EDGES) {
                for (uint32_t i = 0; i < node->count; i++)
                    AssertMarkedOrAllocated(node->edges[i]);
            }

            node = NextNode(node);
        }
    }

    rt->gcMarker.reset();
    rt->gcMarker.stop();

    trc->~VerifyPreTracer();
    js_free(trc);
}

/*** Post-Barrier Verifyier ***/

struct VerifyPostTracer : JSTracer {
    /* The gcNumber when the verification began. */
    uint64_t number;

    /* This counts up to gcZealFrequency to decide whether to verify. */
    int count;
};

/*
 * The post-barrier verifier runs the full store buffer and a fake nursery when
 * running and when it stops, walks the full heap to ensure that all the
 * important edges were inserted into the storebuffer.
 */
void
gc::StartVerifyPostBarriers(JSRuntime *rt)
{
#ifdef JSGC_GENERATIONAL
    if (!rt->gcExactScanningEnabled ||
        rt->gcVerifyPostData ||
        rt->gcIncrementalState != NO_INCREMENTAL)
    {
        return;
    }
    VerifyPostTracer *trc = js_new<VerifyPostTracer>();
    rt->gcVerifyPostData = trc;
    rt->gcNumber++;
    trc->number = rt->gcNumber;
    trc->count = 0;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (IsAtomsCompartment(c))
            continue;

        if (!c->gcNursery.enable())
            goto oom;

        if (!c->gcStoreBuffer.enable())
            goto oom;
    }
    return;
oom:
    trc->~VerifyPostTracer();
    js_free(trc);
    rt->gcVerifyPostData = NULL;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcNursery.disable();
        c->gcStoreBuffer.disable();
    }
#endif
}

#ifdef JSGC_GENERATIONAL
static void
AssertStoreBufferContainsEdge(StoreBuffer *storebuf, void *loc, void *dst)
{
    if (storebuf->containsEdgeAt(loc))
        return;

    char msgbuf[1024];
    JS_snprintf(msgbuf, sizeof(msgbuf), "[post-barrier verifier] Missing edge @ %p to %p",
                loc, dst);
    MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
    MOZ_CRASH();
}

void
gc::PostVerifierVisitEdge(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    VerifyPostTracer *trc = (VerifyPostTracer *)jstrc;
    Cell *dst = (Cell *)*thingp;
    JSCompartment *comp = dst->compartment();

    /*
     * Note: watchpoint markAll will give us cross-compartment pointers into the
     * atoms compartment.
     */
    if (IsAtomsCompartment(comp))
        return;

    /* Filter out non cross-generational edges. */
    if (!comp->gcNursery.isInside(dst))
        return;

    /*
     * Note: since Value travels through the stack to get Cell**, we need to use
     * the annotated location in the tracer instead of the indirect location for
     * these edges.
     */
    Cell *loc = (Cell *)(trc->realLocation != NULL ? trc->realLocation : thingp);

    AssertStoreBufferContainsEdge(&comp->gcStoreBuffer, loc, dst);
}
#endif

void
js::gc::EndVerifyPostBarriers(JSRuntime *rt)
{
#ifdef JSGC_GENERATIONAL
    AutoPrepareForTracing prep(rt);

    VerifyPostTracer *trc = (VerifyPostTracer *)rt->gcVerifyPostData;
    JS_TracerInit(trc, rt, PostVerifierVisitEdge);
    trc->count = 0;

    if (!rt->gcExactScanningEnabled)
        goto oom;

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->gcStoreBuffer.hasOverflowed())
            continue;
        if (!c->gcStoreBuffer.coalesceForVerification())
            goto oom;
    }

    /* Walk the heap. */
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (!c->gcStoreBuffer.isEnabled() ||
             c->gcStoreBuffer.hasOverflowed() ||
             IsAtomsCompartment(c))
        {
            continue;
        }

        if (c->watchpointMap)
            c->watchpointMap->markAll(trc);

        for (size_t kind = 0; kind < FINALIZE_LIMIT; ++kind) {
            for (CellIterUnderGC cells(c, AllocKind(kind)); !cells.done(); cells.next()) {
                Cell *src = cells.getCell();
                if (!c->gcNursery.isInside(src))
                    JS_TraceChildren(trc, src, MapAllocToTraceKind(AllocKind(kind)));
            }
        }
    }

oom:
    trc->~VerifyPostTracer();
    js_free(trc);
    rt->gcVerifyPostData = NULL;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcNursery.disable();
        c->gcStoreBuffer.disable();
        c->gcStoreBuffer.releaseVerificationData();
    }
#endif
}

/*** Barrier Verifier Scheduling ***/

static void
VerifyPreBarriers(JSRuntime *rt)
{
    if (rt->gcVerifyPreData)
        EndVerifyPreBarriers(rt);
    else
        StartVerifyPreBarriers(rt);
}

static void
VerifyPostBarriers(JSRuntime *rt)
{
    if (rt->gcVerifyPostData)
        EndVerifyPostBarriers(rt);
    else
        StartVerifyPostBarriers(rt);
}

void
gc::VerifyBarriers(JSRuntime *rt, VerifierType type)
{
    if (type == PreBarrierVerifier)
        VerifyPreBarriers(rt);
    else
        VerifyPostBarriers(rt);
}

static void
MaybeVerifyPreBarriers(JSRuntime *rt, bool always)
{
    if (rt->gcZeal() != ZealVerifierPreValue)
        return;

    if (VerifyPreTracer *trc = (VerifyPreTracer *)rt->gcVerifyPreData) {
        if (++trc->count < rt->gcZealFrequency && !always)
            return;

        EndVerifyPreBarriers(rt);
    }
    StartVerifyPreBarriers(rt);
}

static void
MaybeVerifyPostBarriers(JSRuntime *rt, bool always)
{
    if (rt->gcZeal() != ZealVerifierPostValue)
        return;

    if (VerifyPostTracer *trc = (VerifyPostTracer *)rt->gcVerifyPostData) {
        if (++trc->count < rt->gcZealFrequency && !always)
            return;

        EndVerifyPostBarriers(rt);
    }
    StartVerifyPostBarriers(rt);
}

void
js::gc::MaybeVerifyBarriers(JSContext *cx, bool always)
{
    MaybeVerifyPreBarriers(cx->runtime, always);
    MaybeVerifyPostBarriers(cx->runtime, always);
}

void
js::gc::FinishVerifier(JSRuntime *rt)
{
    if (VerifyPreTracer *trc = (VerifyPreTracer *)rt->gcVerifyPreData) {
        trc->~VerifyPreTracer();
        js_free(trc);
    }
#ifdef JSGC_GENERATIONAL
    if (VerifyPostTracer *trc = (VerifyPostTracer *)rt->gcVerifyPostData) {
        trc->~VerifyPostTracer();
        js_free(trc);
        for (CompartmentsIter c(rt); !c.done(); c.next()) {
            c->gcNursery.disable();
            c->gcStoreBuffer.disable();
        }
    }
#endif
}

#endif /* JS_GC_ZEAL */
