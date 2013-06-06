/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"

#include "vm/Debugger.h"
#include "js/HashTable.h"
#include "gc/GCInternals.h"

#ifdef JS_ION
#include "ion/BaselineJIT.h"
#include "ion/IonCompartment.h"
#include "ion/Ion.h"
#endif

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

JS::Zone::Zone(JSRuntime *rt)
  : rt(rt),
    allocator(this),
    hold(false),
    ionUsingBarriers_(false),
    active(false),
    gcScheduled(false),
    gcState(NoGC),
    gcPreserveCode(false),
    gcBytes(0),
    gcTriggerBytes(0),
    gcHeapGrowthFactor(3.0),
    isSystem(false),
    scheduledForDestruction(false),
    maybeAlive(true),
    gcMallocBytes(0),
    gcGrayRoots(),
    types(this)
{
    /* Ensure that there are no vtables to mess us up here. */
    JS_ASSERT(reinterpret_cast<JS::shadow::Zone *>(this) ==
              static_cast<JS::shadow::Zone *>(this));

    setGCMaxMallocBytes(rt->gcMaxMallocBytes * 0.9);
}

Zone::~Zone()
{
    if (this == rt->systemZone)
        rt->systemZone = NULL;
}

bool
Zone::init(JSContext *cx)
{
    types.init(cx);
    return true;
}

void
Zone::setNeedsBarrier(bool needs, ShouldUpdateIon updateIon)
{
#ifdef JS_ION
    if (updateIon == UpdateIon && needs != ionUsingBarriers_) {
        ion::ToggleBarriers(this, needs);
        ionUsingBarriers_ = needs;
    }
#endif

    needsBarrier_ = needs;
}

void
Zone::markTypes(JSTracer *trc)
{
    /*
     * Mark all scripts, type objects and singleton JS objects in the
     * compartment. These can be referred to directly by type sets, which we
     * cannot modify while code which depends on these type sets is active.
     */
    JS_ASSERT(isPreservingCode());

    for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        MarkScriptRoot(trc, &script, "mark_types_script");
        JS_ASSERT(script == i.get<JSScript>());
    }

    for (size_t thingKind = FINALIZE_OBJECT0; thingKind < FINALIZE_OBJECT_LIMIT; thingKind++) {
        ArenaHeader *aheader = allocator.arenas.getFirstArena(static_cast<AllocKind>(thingKind));
        if (aheader)
            rt->gcMarker.pushArenaList(aheader);
    }

    for (CellIterUnderGC i(this, FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        types::TypeObject *type = i.get<types::TypeObject>();
        MarkTypeObjectRoot(trc, &type, "mark_types_scan");
        JS_ASSERT(type == i.get<types::TypeObject>());
    }
}

void
Zone::resetGCMallocBytes()
{
    gcMallocBytes = ptrdiff_t(gcMaxMallocBytes);
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
    TriggerZoneGC(this, gcreason::TOO_MUCH_MALLOC);
}

void
Zone::sweep(FreeOp *fop, bool releaseTypes)
{
    /*
     * Periodically release observed types for all scripts. This is safe to
     * do when there are no frames for the zone on the stack.
     */
    if (active)
        releaseTypes = false;

    if (!isPreservingCode()) {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DISCARD_ANALYSIS);
        types.sweep(fop, releaseTypes);
    }

    if (!rt->debuggerList.isEmpty())
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

    gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_SWEEP_TABLES);
    gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_BREAKPOINT);

    for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (!script->hasAnyBreakpointsOrStepMode())
            continue;
        bool scriptGone = IsScriptAboutToBeFinalized(&script);
        JS_ASSERT(script == i.get<JSScript>());
        for (unsigned i = 0; i < script->length; i++) {
            BreakpointSite *site = script->getBreakpointSite(script->code + i);
            if (!site)
                continue;
            Breakpoint *nextbp;
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = nextbp) {
                nextbp = bp->nextInSite();
                if (scriptGone || IsObjectAboutToBeFinalized(&bp->debugger->toJSObjectRef()))
                    bp->destroy(fop);
            }
        }
    }
}

void
Zone::discardJitCode(FreeOp *fop, bool discardConstraints)
{
#ifdef JS_ION
    if (isPreservingCode()) {
        PurgeJITCaches(this);
    } else {

# ifdef DEBUG
        /* Assert no baseline scripts are marked as active. */
        for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            JS_ASSERT_IF(script->hasBaselineScript(), !script->baselineScript()->active());
        }
# endif

        /* Mark baseline scripts on the stack as active. */
        ion::MarkActiveBaselineScripts(this);

        /* Only mark OSI points if code is being discarded. */
        ion::InvalidateAll(fop, this);

        for (CellIterUnderGC i(this, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            ion::FinishInvalidation(fop, script);

            /*
             * Discard baseline script if it's not marked as active. Note that
             * this also resets the active flag.
             */
            ion::FinishDiscardBaselineScript(fop, script);

            /*
             * Use counts for scripts are reset on GC. After discarding code we
             * need to let it warm back up to get information such as which
             * opcodes are setting array holes or accessing getter properties.
             */
            script->resetUseCount();
        }

        for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next()) {
            /* Free optimized baseline stubs. */
            if (comp->ionCompartment())
                comp->ionCompartment()->optimizedStubSpace()->free();

            comp->types.sweepCompilerOutputs(fop, discardConstraints);
        }
    }
#endif
}
