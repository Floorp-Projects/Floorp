/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/CodeCoverage.h"

#include "mozilla/Atomics.h"
#include "mozilla/IntegerPrintfMacros.h"

#include <stdio.h>
#if defined(XP_WIN)
# include <windows.h>
#else
# include <unistd.h>
#endif

#include "jscompartment.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscript.h"

#include "vm/Runtime.h"
#include "vm/Time.h"

// This file contains a few functions which are used to produce files understood
// by lcov tools. A detailed description of the format is available in the man
// page for "geninfo" [1].  To make it short, the following paraphrases what is
// commented in the man page by using curly braces prefixed by for-each to
// express repeated patterns.
//
//   TN:<compartment name>
//   for-each <source file> {
//     SN:<filename>
//     for-each <script> {
//       FN:<line>,<name>
//     }
//     for-each <script> {
//       FNDA:<hits>,<name>
//     }
//     FNF:<number of scripts>
//     FNH:<sum of scripts hits>
//     for-each <script> {
//       for-each <branch> {
//         BRDA:<line>,<block id>,<target id>,<taken>
//       }
//     }
//     BRF:<number of branches>
//     BRH:<sum of branches hits>
//     for-each <script> {
//       for-each <line> {
//         DA:<line>,<hits>
//       }
//     }
//     LF:<number of lines>
//     LH:<sum of lines hits>
//   }
//
// [1] http://ltp.sourceforge.net/coverage/lcov/geninfo.1.php
//
namespace js {
namespace coverage {

LCovSource::LCovSource(LifoAlloc* alloc, JSObject* sso)
  : source_(sso),
    outSF_(alloc),
    outFN_(alloc),
    outFNDA_(alloc),
    numFunctionsFound_(0),
    numFunctionsHit_(0),
    outBRDA_(alloc),
    numBranchesFound_(0),
    numBranchesHit_(0),
    outDA_(alloc),
    numLinesInstrumented_(0),
    numLinesHit_(0),
    hasFilename_(false),
    hasScripts_(false)
{
}

void
LCovSource::exportInto(GenericPrinter& out) const
{
    // Only write if everything got recorded.
    if (!hasFilename_ || !hasScripts_)
        return;

    outSF_.exportInto(out);

    outFN_.exportInto(out);
    outFNDA_.exportInto(out);
    out.printf("FNF:%d\n", numFunctionsFound_);
    out.printf("FNH:%d\n", numFunctionsHit_);

    outBRDA_.exportInto(out);
    out.printf("BRF:%d\n", numBranchesFound_);
    out.printf("BRH:%d\n", numBranchesHit_);

    outDA_.exportInto(out);
    out.printf("LF:%d\n", numLinesInstrumented_);
    out.printf("LH:%d\n", numLinesHit_);

    out.put("end_of_record\n");
}

bool
LCovSource::writeTopLevelScript(JSScript* script)
{
    MOZ_ASSERT(script->isTopLevel());

    Vector<JSScript*, 8, SystemAllocPolicy> queue;
    if (!queue.append(script))
        return false;

    do {
        script = queue.popCopy();

        // Save the lcov output of the current script.
        if (!writeScript(script))
            return false;

        // Iterate from the last to the first object in order to have the
        // functions visited in the opposite order when popping elements from
        // the queue of remaining scripts, such that the functions are listed
        // with increasing line numbers.
        if (!script->hasObjects())
            continue;

        size_t idx = script->objects()->length;
        while (idx--) {
            JSObject* obj = script->getObject(idx);

            // Only continue on JSFunction objects.
            if (!obj->is<JSFunction>())
                continue;
            JSFunction& fun = obj->as<JSFunction>();

            // Let's skip asm.js for now.
            if (!fun.isInterpreted())
                continue;
            MOZ_ASSERT(!fun.isInterpretedLazy());

            // Eval scripts can refer to their parent script in order to extend
            // their scope.  We only care about the inner functions, which are
            // in the same source, and which are assumed to be visited in the
            // same order as the source content.
            //
            // Note: It is possible that the JSScript visited here has already
            // been finalized, in which case the sourceObject() will be a
            // poisoned pointer.  This is safe because all scripts are currently
            // finalized in the foreground.
            JSScript* child = fun.nonLazyScript();
            if (child->sourceObject() != source_)
                continue;

            // Queue the script in the list of script associated to the
            // current source.
            if (!queue.append(fun.nonLazyScript()))
                return false;
        }
    } while (!queue.empty());

    if (outFN_.hadOutOfMemory() ||
        outFNDA_.hadOutOfMemory() ||
        outBRDA_.hadOutOfMemory() ||
        outDA_.hadOutOfMemory())
    {
        return false;
    }

    hasScripts_ = true;
    return true;
}

bool
LCovSource::writeSourceFilename(ScriptSourceObject* sso)
{
    outSF_.printf("SF:%s\n", sso->source()->filename());
    if (outSF_.hadOutOfMemory())
        return false;

    hasFilename_ = true;
    return true;
}

bool
LCovSource::writeScriptName(LSprinter& out, JSScript* script)
{
    JSFunction* fun = script->functionNonDelazifying();
    if (fun && fun->displayAtom())
        return EscapedStringPrinter(out, fun->displayAtom(), 0);
    out.printf("top-level");
    return true;
}

bool
LCovSource::writeScript(JSScript* script)
{
    numFunctionsFound_++;
    outFN_.printf("FN:%d,", script->lineno());
    if (!writeScriptName(outFN_, script))
        return false;
    outFN_.put("\n", 1);

    uint64_t hits = 0;
    ScriptCounts* sc = nullptr;
    if (script->hasScriptCounts()) {
        sc = &script->getScriptCounts();
        numFunctionsHit_++;
        const PCCounts* counts = sc->maybeGetPCCounts(script->pcToOffset(script->main()));
        outFNDA_.printf("FNDA:%" PRIu64 ",", counts->numExec());
        if (!writeScriptName(outFNDA_, script))
            return false;
        outFNDA_.put("\n", 1);

        // Set the hit count of the pre-main code to 1, if the function ever got
        // visited.
        hits = 1;
    }

    jsbytecode* snpc = script->code();
    jssrcnote* sn = script->notes();
    if (!SN_IS_TERMINATOR(sn))
        snpc += SN_DELTA(sn);

    size_t lineno = script->lineno();
    jsbytecode* end = script->codeEnd();
    size_t blockId = 0;
    for (jsbytecode* pc = script->code(); pc != end; pc = GetNextPc(pc)) {
        JSOp op = JSOp(*pc);
        bool jump = IsJumpOpcode(op);
        bool fallsthrough = BytecodeFallsThrough(op);

        // If the current script & pc has a hit-count report, then update the
        // current number of hits.
        if (sc) {
            const PCCounts* counts = sc->maybeGetPCCounts(script->pcToOffset(pc));
            if (counts)
                hits = counts->numExec();
        }

        // If we have additional source notes, walk all the source notes of the
        // current pc.
        if (snpc <= pc) {
            size_t oldLine = lineno;
            while (!SN_IS_TERMINATOR(sn) && snpc <= pc) {
                SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
                if (type == SRC_SETLINE)
                    lineno = size_t(GetSrcNoteOffset(sn, 0));
                else if (type == SRC_NEWLINE)
                    lineno++;

                sn = SN_NEXT(sn);
                snpc += SN_DELTA(sn);
            }

            if (oldLine != lineno && fallsthrough) {
                outDA_.printf("DA:%d,%" PRIu64 "\n", lineno, hits);

                // Count the number of lines instrumented & hit.
                numLinesInstrumented_++;
                if (hits)
                    numLinesHit_++;
            }
        }

        // If the current instruction has thrown, then decrement the hit counts
        // with the number of throws.
        if (sc) {
            const PCCounts* counts = sc->maybeGetThrowCounts(script->pcToOffset(pc));
            if (counts)
                hits -= counts->numExec();
        }

        // If the current pc corresponds to a conditional jump instruction, then reports
        // branch hits.
        if (jump && fallsthrough) {
            jsbytecode* target = pc + GET_JUMP_OFFSET(pc);
            jsbytecode* fallthroughTarget = GetNextPc(pc);
            uint64_t fallthroughHits = 0;
            if (sc) {
                const PCCounts* counts = sc->maybeGetPCCounts(script->pcToOffset(fallthroughTarget));
                if (counts)
                    fallthroughHits = counts->numExec();
            }

            size_t targetId = script->pcToOffset(target);
            uint64_t taken = hits - fallthroughHits;
            outBRDA_.printf("BRDA:%d,%d,%d,", lineno, blockId, targetId);
            if (hits)
                outBRDA_.printf("%d\n", taken);
            else
                outBRDA_.put("-\n", 2);

            // Count the number of branches, and the number of branches hit.
            numBranchesFound_++;
            if (hits)
                numBranchesHit_++;

            // Update the blockId when there is a discontinuity.
            blockId = script->pcToOffset(fallthroughTarget);
        }
    }

    return true;
}

LCovCompartment::LCovCompartment()
  : alloc_(4096),
    outTN_(&alloc_),
    sources_(nullptr)
{
    MOZ_ASSERT(alloc_.isEmpty());
}

void
LCovCompartment::collectCodeCoverageInfo(JSCompartment* comp, JSObject* sso,
                                         JSScript* topLevel)
{
    // Skip any operation if we already some out-of memory issues.
    if (outTN_.hadOutOfMemory())
        return;

    // We expect to visit the source before visiting any of its scripts.
    if (!sources_)
        return;

    // Get the existing source LCov summary, or create a new one.
    LCovSource* source = lookupOrAdd(comp, sso);
    if (!source)
        return;

    // Write code coverage data into the LCovSource.
    if (!source->writeTopLevelScript(topLevel)) {
        outTN_.reportOutOfMemory();
        return;
    }
}

void
LCovCompartment::collectSourceFile(JSCompartment* comp, ScriptSourceObject* sso)
{
    // Do not add sources if there is no file name associated to it.
    if (!sso->source()->filename())
        return;

    // Skip any operation if we already some out-of memory issues.
    if (outTN_.hadOutOfMemory())
        return;

    // Get the existing source LCov summary, or create a new one.
    LCovSource* source = lookupOrAdd(comp, sso);
    if (!source)
        return;

    // Write source filename into the LCovSource.
    if (!source->writeSourceFilename(sso)) {
        outTN_.reportOutOfMemory();
        return;
    }
}

LCovSource*
LCovCompartment::lookupOrAdd(JSCompartment* comp, JSObject* sso)
{
    // On the first call, write the compartment name, and allocate a LCovSource
    // vector in the LifoAlloc.
    if (!sources_) {
        if (!writeCompartmentName(comp))
            return nullptr;

        LCovSourceVector* raw = alloc_.pod_malloc<LCovSourceVector>();
        if (!raw) {
            outTN_.reportOutOfMemory();
            return nullptr;
        }

        sources_ = new(raw) LCovSourceVector(alloc_);
    } else {
        // Find the first matching source.
        for (LCovSource& source : *sources_) {
            if (source.match(sso))
                return &source;
        }
    }

    // Allocate a new LCovSource for the current top-level.
    if (!sources_->append(Move(LCovSource(&alloc_, sso)))) {
        outTN_.reportOutOfMemory();
        return nullptr;
    }

    return &sources_->back();
}

void
LCovCompartment::exportInto(GenericPrinter& out) const
{
    if (!sources_ || outTN_.hadOutOfMemory())
        return;

    outTN_.exportInto(out);
    for (const LCovSource& sc : *sources_)
        sc.exportInto(out);
}

bool
LCovCompartment::writeCompartmentName(JSCompartment* comp)
{
    JSRuntime* rt = comp->runtimeFromMainThread();

    // lcov trace files are starting with an optional test case name, that we
    // recycle to be a compartment name.
    //
    // Note: The test case name has some constraint in terms of valid character,
    // thus we escape invalid chracters with a "_" symbol in front of its
    // hexadecimal code.
    outTN_.put("TN:");
    if (rt->compartmentNameCallback) {
        char name[1024];
        {
            // Hazard analysis cannot tell that the callback does not GC.
            JS::AutoSuppressGCAnalysis nogc;
            (*rt->compartmentNameCallback)(rt, comp, name, sizeof(name));
        }
        for (char *s = name; s < name + sizeof(name) && *s; s++) {
            if (('a' <= *s && *s <= 'z') ||
                ('A' <= *s && *s <= 'Z') ||
                ('0' <= *s && *s <= '9'))
            {
                outTN_.put(s, 1);
                continue;
            }
            outTN_.printf("_%p", (void*) size_t(*s));
        }
        outTN_.put("\n", 1);
    } else {
        outTN_.printf("Compartment_%p%p\n", (void*) size_t('_'), comp);
    }

    return !outTN_.hadOutOfMemory();
}

LCovRuntime::LCovRuntime()
  : out_(),
#if defined(XP_WIN)
    pid_(GetCurrentProcessId())
#else
    pid_(getpid())
#endif
{
}

LCovRuntime::~LCovRuntime()
{
    if (out_.isInitialized())
        out_.finish();
}

void
LCovRuntime::init()
{
    const char* outDir = getenv("JS_CODE_COVERAGE_OUTPUT_DIR");
    if (!outDir || *outDir == 0)
        return;

    int64_t timestamp = static_cast<double>(PRMJ_Now()) / PRMJ_USEC_PER_SEC;
    static mozilla::Atomic<size_t> globalRuntimeId(0);
    size_t rid = globalRuntimeId++;

    char name[1024];
    size_t len = JS_snprintf(name, sizeof(name), "%s/%" PRId64 "-%d-%d.info",
                             outDir, timestamp, size_t(pid_), rid);
    if (sizeof(name) < len) {
        fprintf(stderr, "Warning: LCovRuntime::init: Cannot serialize file name.");
        return;
    }

    // If we cannot open the file, report a warning.
    if (!out_.init(name))
        fprintf(stderr, "Warning: LCovRuntime::init: Cannot open file named '%s'.", name);
}

void
LCovRuntime::writeLCovResult(LCovCompartment& comp)
{
    if (!out_.isInitialized())
        return;

#if defined(XP_WIN)
    size_t p = GetCurrentProcessId();
#else
    size_t p = getpid();
#endif
    if (pid_ != p) {
        pid_ = p;
        out_.finish();
        init();
        if (!out_.isInitialized())
            return;
    }

    comp.exportInto(out_);
    out_.flush();
}

} // namespace coverage
} // namespace js
