/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone.h"

#include "jsgc.h"

#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/JitCompartment.h"
#include "vm/Debugger.h"
#include "vm/Runtime.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

JS::Zone::Zone(JSRuntime *rt)
  : JS::shadow::Zone(rt, &rt->gc.marker),
    allocator(this),
    types(this),
    compartments(),
    gcGrayRoots(),
    gcMallocBytes(0),
    gcMallocGCTriggered(false),
    usage(&rt->gc.usage),
    data(nullptr),
    isSystem(false),
    usedByExclusiveThread(false),
    active(false),
    jitZone_(nullptr),
    gcState_(NoGC),
    gcScheduled_(false),
    gcPreserveCode_(false),
    jitUsingBarriers_(false)
{
    /* Ensure that there are no vtables to mess us up here. */
    JS_ASSERT(reinterpret_cast<JS::shadow::Zone *>(this) ==
              static_cast<JS::shadow::Zone *>(this));

    threshold.updateAfterGC(8192, GC_NORMAL, rt->gc.tunables, rt->gc.schedulingState);
    setGCMaxMallocBytes(rt->gc.maxMallocBytesAllocated() * 0.9);
}

Zone::~Zone()
{
    JSRuntime *rt = runtimeFromMainThread();
    if (this == rt->gc.systemZone)
        rt->gc.systemZone = nullptr;

    js_delete(jitZone_);
}

bool Zone::init(bool isSystemArg)
{
    isSystem = isSystemArg;
    return gcZoneGroupEdges.init();
}

void
Zone::setNeedsIncrementalBarrier(bool needs, ShouldUpdateJit updateJit)
{
    if (updateJit == UpdateJit && needs != jitUsingBarriers_) {
        jit::ToggleBarriers(this, needs);
        jitUsingBarriers_ = needs;
    }

    if (needs && runtimeFromMainThread()->isAtomsZone(this))
        JS_ASSERT(!runtimeFromMainThread()->exclusiveThreadsPresent());

    JS_ASSERT_IF(needs, canCollect());
    needsIncrementalBarrier_ = needs;
}

void
Zone::resetGCMallocBytes()
{
    gcMallocBytes = ptrdiff_t(gcMaxMallocBytes);
    gcMallocGCTriggered = false;
}

void
Zone::setGCMaxMallocBytes(size_t value)
{
    /*
     * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
     * mean that value.
     */
    gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
    resetGCMallocBytes();
}

void
Zone::onTooMuchMalloc()
{
    if (!gcMallocGCTriggered)
        gcMallocGCTriggered = TriggerZoneGC(this, JS::gcreason::TOO_MUCH_MALLOC);
}

void
Zone::sweep(FreeOp *fop, bool releaseTypes, bool *oom)
{
    /*
     * Periodically release observed types for all scripts. This is safe to
     * do when there are no frames for the zone on the stack.
     */
    if (active)
        releaseTypes = false;

    {
        gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_DISCARD_ANALYSIS);
        types.sweep(fop, releaseTypes, oom);
    }

    if (!fop->runtime()->debuggerList.isEmpty())
        sweepBreakpoints(fop);

    active = false;
}

void
Zone::sweepBreakpoints(FreeOp *fop)
{
    /*
     * Sweep all compartments in a zone at the same time, since there is no way
     * to iterate over the scripts belonging to a single compartment in a zone.
     */

    gcstats::AutoPhase ap1(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_TABLES);
    gcstats::AutoPhase ap2(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_TABLES_BREAKPOINT);

    JS_ASSERT(isGCSweeping());
    for (ZoneCellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        JS_ASSERT(script->zone()->isGCSweeping());
        if (!script->hasAnyBreakpointsOrStepMode())
            continue;

        bool scriptGone = IsScriptAboutToBeFinalized(&script);
        JS_ASSERT(script == i.get<JSScript>());
        for (unsigned i = 0; i < script->length(); i++) {
            BreakpointSite *site = script->getBreakpointSite(script->offsetToPC(i));
            if (!site)
                continue;

            Breakpoint *nextbp;
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = nextbp) {
                nextbp = bp->nextInSite();
                HeapPtrObject &dbgobj = bp->debugger->toJSObjectRef();
                JS_ASSERT_IF(dbgobj->zone()->isCollecting(), dbgobj->zone()->isGCSweeping());
                bool dying = scriptGone || IsObjectAboutToBeFinalized(&dbgobj);
                JS_ASSERT_IF(!dying, !IsAboutToBeFinalized(&bp->getHandlerRef()));
                if (dying)
                    bp->destroy(fop);
            }
        }
    }
}

void
Zone::discardJitCode(FreeOp *fop)
{
    if (!jitZone())
        return;

    if (isPreservingCode()) {
        PurgeJITCaches(this);
    } else {

#ifdef DEBUG
        /* Assert no baseline scripts are marked as active. */
        for (ZoneCellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            JS_ASSERT_IF(script->hasBaselineScript(), !script->baselineScript()->active());
        }
#endif

        /* Mark baseline scripts on the stack as active. */
        jit::MarkActiveBaselineScripts(this);

        /* Only mark OSI points if code is being discarded. */
        jit::InvalidateAll(fop, this);

        for (ZoneCellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            jit::FinishInvalidation<SequentialExecution>(fop, script);
            jit::FinishInvalidation<ParallelExecution>(fop, script);

            /*
             * Discard baseline script if it's not marked as active. Note that
             * this also resets the active flag.
             */
            jit::FinishDiscardBaselineScript(fop, script);

            /*
             * Use counts for scripts are reset on GC. After discarding code we
             * need to let it warm back up to get information such as which
             * opcodes are setting array holes or accessing getter properties.
             */
            script->resetUseCount();
        }

        jitZone()->optimizedStubSpace()->free();
    }
}

uint64_t
Zone::gcNumber()
{
    // Zones in use by exclusive threads are not collected, and threads using
    // them cannot access the main runtime's gcNumber without racing.
    return usedByExclusiveThread ? 0 : runtimeFromMainThread()->gc.gcNumber();
}

js::jit::JitZone *
Zone::createJitZone(JSContext *cx)
{
    MOZ_ASSERT(!jitZone_);

    if (!cx->runtime()->getJitRuntime(cx))
        return nullptr;

    jitZone_ = cx->new_<js::jit::JitZone>();
    return jitZone_;
}

JS::Zone *
js::ZoneOfObjectFromAnyThread(const JSObject &obj)
{
    return obj.zoneFromAnyThread();
}

bool
Zone::hasMarkedCompartments()
{
    for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
        if (comp->marked)
            return true;
    }
    return false;
}

bool
Zone::canCollect()
{
    // Zones cannot be collected while in use by other threads.
    if (usedByExclusiveThread)
        return false;
    JSRuntime *rt = runtimeFromAnyThread();
    if (rt->isAtomsZone(this) && rt->exclusiveThreadsPresent())
        return false;
    return true;
}

JS::Zone *
js::ZoneOfValue(const JS::Value &value)
{
    JS_ASSERT(value.isMarkable());
    if (value.isObject())
        return value.toObject().zone();
    return static_cast<js::gc::Cell *>(value.toGCThing())->tenuredZone();
}

bool
js::ZonesIter::atAtomsZone(JSRuntime *rt)
{
    return rt->isAtomsZone(*it);
}
