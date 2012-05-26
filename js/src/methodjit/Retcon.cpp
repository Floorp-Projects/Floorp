/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JS_METHODJIT

#include "Retcon.h"
#include "MethodJIT.h"
#include "Compiler.h"
#include "StubCalls.h"
#include "jsdbgapi.h"
#include "jsnum.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"

#include "jscntxtinlines.h"
#include "jsinterpinlines.h"

using namespace js;
using namespace js::mjit;

namespace js {
namespace mjit {

static inline void
SetRejoinState(StackFrame *fp, const CallSite &site, void **location)
{
    if (site.rejoin == REJOIN_SCRIPTED) {
        fp->setRejoin(ScriptedRejoin(site.pcOffset));
        *location = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolineScripted);
    } else {
        fp->setRejoin(StubRejoin(site.rejoin));
        *location = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline);
    }
}

static inline bool
CallsiteMatches(uint8_t *codeStart, const CallSite &site, void *location)
{
    if (codeStart + site.codeOffset == location)
        return true;

#ifdef JS_CPU_ARM
    if (codeStart + site.codeOffset + 4 == location)
        return true;
#endif

    return false;
}

void
Recompiler::patchCall(JITChunk *chunk, StackFrame *fp, void **location)
{
    uint8_t* codeStart = (uint8_t *)chunk->code.m_code.executableAddress();

    CallSite *callSites_ = chunk->callSites();
    for (uint32_t i = 0; i < chunk->nCallSites; i++) {
        if (CallsiteMatches(codeStart, callSites_[i], *location)) {
            JS_ASSERT(callSites_[i].inlineIndex == analyze::CrossScriptSSA::OUTER_FRAME);
            SetRejoinState(fp, callSites_[i], location);
            return;
        }
    }

    JS_NOT_REACHED("failed to find call site");
}

void
Recompiler::patchNative(JSCompartment *compartment, JITChunk *chunk, StackFrame *fp,
                        jsbytecode *pc, RejoinState rejoin)
{
    /*
     * There is a native call or getter IC at pc which triggered recompilation.
     * The recompilation could have been triggered either by the native call
     * itself, or by a SplatApplyArgs preparing for the native call. Either
     * way, we don't want to patch up the call, but will instead steal the pool
     * for the IC so it doesn't get freed with the JITChunk, and patch up the
     * jump at the end to go to the interpoline.
     *
     * When doing this, we do not reset the the IC itself; there may be other
     * native calls from this chunk on the stack and we need to find and patch
     * all live stubs before purging the chunk's caches.
     */
    fp->setRejoin(StubRejoin(rejoin));

    /* :XXX: We might crash later if this fails. */
    compartment->rt->jaegerRuntime().orphanedNativeFrames.append(fp);

    DebugOnly<bool> found = false;

    /*
     * Find and patch all native call stubs attached to the given PC. There may
     * be multiple ones for getter stubs attached to e.g. a GETELEM.
     */
    for (unsigned i = 0; i < chunk->nativeCallStubs.length(); i++) {
        NativeCallStub &stub = chunk->nativeCallStubs[i];
        if (stub.pc != pc)
            continue;

        found = true;

        /* Check for pools that were already patched. */
        if (!stub.pool)
            continue;

        /* Patch the native fallthrough to go to the interpoline. */
        {
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
            /* Win64 needs stack adjustment */
            void *interpoline = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolinePatched);
#else
            void *interpoline = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline);
#endif
            uint8_t *start = (uint8_t *)stub.jump.executableAddress();
            JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));
#ifdef JS_CPU_X64
            repatch.repatch(stub.jump, interpoline);
#else
            repatch.relink(stub.jump, JSC::CodeLocationLabel(interpoline));
#endif
        }

        /* :XXX: We leak the pool if this fails. Oh well. */
        compartment->rt->jaegerRuntime().orphanedNativePools.append(stub.pool);

        /* Mark as stolen in case there are multiple calls on the stack. */
        stub.pool = NULL;
    }

    JS_ASSERT(found);
}

void
Recompiler::patchFrame(JSCompartment *compartment, VMFrame *f, JSScript *script)
{
    /*
     * Check if the VMFrame returns directly into the script's jitcode. This
     * depends on the invariant that f->fp() reflects the frame at the point
     * where the call occurred, irregardless of any frames which were pushed
     * inside the call.
     */
    StackFrame *fp = f->fp();
    void **addr = f->returnAddressLocation();
    RejoinState rejoin = (RejoinState) f->stubRejoin;
    if (rejoin == REJOIN_NATIVE ||
        rejoin == REJOIN_NATIVE_LOWERED ||
        rejoin == REJOIN_NATIVE_GETTER) {
        /* Native call. */
        if (fp->script() == script) {
            patchNative(compartment, fp->jit()->chunk(f->regs.pc), fp, f->regs.pc, rejoin);
            f->stubRejoin = REJOIN_NATIVE_PATCHED;
        }
    } else if (rejoin == REJOIN_NATIVE_PATCHED) {
        /* Already patched, don't do anything. */
    } else if (rejoin) {
        /* Recompilation triggered by CompileFunction. */
        if (fp->script() == script) {
            fp->setRejoin(StubRejoin(rejoin));
            *addr = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline);
            f->stubRejoin = 0;
        }
    } else {
        for (int constructing = 0; constructing <= 1; constructing++) {
            for (int barriers = 0; barriers <= 1; barriers++) {
                JITScript *jit = script->getJIT((bool) constructing, (bool) barriers);
                if (jit) {
                    JITChunk *chunk = jit->findCodeChunk(*addr);
                    if (chunk)
                        patchCall(chunk, fp, addr);
                }
            }
        }
    }
}

StackFrame *
Recompiler::expandInlineFrameChain(StackFrame *outer, InlineFrame *inner)
{
    StackFrame *parent;
    if (inner->parent)
        parent = expandInlineFrameChain(outer, inner->parent);
    else
        parent = outer;

    JaegerSpew(JSpew_Recompile, "Expanding inline frame\n");

    StackFrame *fp = (StackFrame *) ((uint8_t *)outer + sizeof(Value) * inner->depth);
    fp->initInlineFrame(inner->fun, parent, inner->parentpc);
    uint32_t pcOffset = inner->parentpc - parent->script()->code;

    void **location = fp->addressOfNativeReturnAddress();
    *location = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolineScripted);
    parent->setRejoin(ScriptedRejoin(pcOffset));

    return fp;
}

/*
 * Whether a given return address for a frame indicates it returns directly
 * into JIT code.
 */
static inline bool
JITCodeReturnAddress(void *data)
{
    return data != NULL  /* frame is interpreted */
        && data != JS_FUNC_TO_DATA_PTR(void *, JaegerTrampolineReturn)
        && data != JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline)
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
        && data != JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolinePatched)
#endif
        && data != JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolineScripted);
}

/*
 * Expand all inlined frames within fp per 'inlined' and update next and regs
 * to refer to the new innermost frame.
 */
void
Recompiler::expandInlineFrames(JSCompartment *compartment,
                               StackFrame *fp, mjit::CallSite *inlined,
                               StackFrame *next, VMFrame *f)
{
    JS_ASSERT_IF(next, next->prev() == fp && next->prevInline() == inlined);

    /*
     * Treat any frame expansion as a recompilation event, so that f.jit() is
     * stable if no recompilations have occurred.
     */
    compartment->types.frameExpansions++;

    jsbytecode *pc = next ? next->prevpc(NULL) : f->regs.pc;
    JITChunk *chunk = fp->jit()->chunk(pc);

    /*
     * Patch the VMFrame's return address if it is returning at the given inline site.
     * Note there is no worry about handling a native or CompileFunction call here,
     * as such IC stubs are not generated within inline frames.
     */
    void **frameAddr = f->returnAddressLocation();
    uint8_t* codeStart = (uint8_t *)chunk->code.m_code.executableAddress();

    InlineFrame *inner = &chunk->inlineFrames()[inlined->inlineIndex];
    jsbytecode *innerpc = inner->fun->script()->code + inlined->pcOffset;

    StackFrame *innerfp = expandInlineFrameChain(fp, inner);

    /* Check if the VMFrame returns into the inlined frame. */
    if (f->stubRejoin && f->fp() == fp) {
        /* The VMFrame is calling CompileFunction. */
        JS_ASSERT(f->stubRejoin != REJOIN_NATIVE &&
                  f->stubRejoin != REJOIN_NATIVE_LOWERED &&
                  f->stubRejoin != REJOIN_NATIVE_GETTER &&
                  f->stubRejoin != REJOIN_NATIVE_PATCHED);
        innerfp->setRejoin(StubRejoin((RejoinState) f->stubRejoin));
        *frameAddr = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline);
        f->stubRejoin = 0;
    }
    if (CallsiteMatches(codeStart, *inlined, *frameAddr)) {
        /* The VMFrame returns directly into the expanded frame. */
        SetRejoinState(innerfp, *inlined, frameAddr);
    }

    if (f->fp() == fp) {
        JS_ASSERT(f->regs.inlined() == inlined);
        f->regs.expandInline(innerfp, innerpc);
    }

    /*
     * Note: unlike the case for recompilation, during frame expansion we don't
     * need to worry about the next VMFrame holding a reference to the inlined
     * frame in its entryncode. entryncode is non-NULL only if the next frame's
     * code was discarded and has executed via the Interpoline, which can only
     * happen after all inline frames have been expanded.
     */

    if (next) {
        next->resetInlinePrev(innerfp, innerpc);
        void **addr = next->addressOfNativeReturnAddress();
        if (JITCodeReturnAddress(*addr)) {
            innerfp->setRejoin(ScriptedRejoin(inlined->pcOffset));
            *addr = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolineScripted);
        }
    }
}

void
ExpandInlineFrames(JSCompartment *compartment)
{
    if (!compartment || !compartment->rt->hasJaegerRuntime())
        return;

    for (VMFrame *f = compartment->rt->jaegerRuntime().activeFrame();
         f != NULL;
         f = f->previous) {

        if (f->entryfp->compartment() != compartment)
            continue;

        if (f->regs.inlined())
            mjit::Recompiler::expandInlineFrames(compartment, f->fp(), f->regs.inlined(), NULL, f);

        StackFrame *end = f->entryfp->prev();
        StackFrame *next = NULL;
        for (StackFrame *fp = f->fp(); fp != end; fp = fp->prev()) {
            if (!next) {
                next = fp;
                continue;
            }
            mjit::CallSite *inlined;
            next->prevpc(&inlined);
            if (inlined) {
                mjit::Recompiler::expandInlineFrames(compartment, fp, inlined, next, f);
                fp = next;
                next = NULL;
            } else {
                if (fp->downFramesExpanded())
                    break;
                next = fp;
            }
            fp->setDownFramesExpanded();
        }
    }
}

void
ClearAllFrames(JSCompartment *compartment)
{
    if (!compartment || !compartment->rt->hasJaegerRuntime())
        return;

    ExpandInlineFrames(compartment);

    compartment->types.recompilations++;

    for (VMFrame *f = compartment->rt->jaegerRuntime().activeFrame();
         f != NULL;
         f = f->previous)
    {
        if (f->entryfp->compartment() != compartment)
            continue;

        Recompiler::patchFrame(compartment, f, f->fp()->script());

        // Clear ncode values from all frames associated with the VMFrame.
        // Patching the VMFrame's return address will cause all its frames to
        // finish in the interpreter, unless the interpreter enters one of the
        // intermediate frames at a loop boundary (where EnterMethodJIT will
        // overwrite ncode). However, leaving stale values for ncode in stack
        // frames can confuse the recompiler, which may see the VMFrame before
        // it has resumed execution.

        for (StackFrame *fp = f->fp(); fp != f->entryfp; fp = fp->prev())
            fp->setNativeReturnAddress(NULL);
    }

    // Purge all ICs in chunks for which we patched any native frames, see patchNative.
    for (VMFrame *f = compartment->rt->jaegerRuntime().activeFrame();
         f != NULL;
         f = f->previous)
    {
        if (f->entryfp->compartment() != compartment)
            continue;

        JS_ASSERT(f->stubRejoin != REJOIN_NATIVE &&
                  f->stubRejoin != REJOIN_NATIVE_LOWERED &&
                  f->stubRejoin != REJOIN_NATIVE_GETTER);
        if (f->stubRejoin == REJOIN_NATIVE_PATCHED && f->jit() && f->chunk())
            f->chunk()->purgeCaches();
    }
}

/*
 * Recompilation can be triggered either by the debugger (turning debug mode on for
 * a script or setting/clearing a trap), or by dynamic changes in type information
 * from type inference. When recompiling we don't immediately recompile the JIT
 * code, but destroy the old code and remove all references to the code, including
 * those from active stack frames. Things to do:
 *
 * - Purge scripted call inline caches calling into the script.
 *
 * - For frames with an ncode return address in the original script, redirect
 *   to the interpoline.
 *
 * - For VMFrames with a stub call return address in the original script,
 *   redirect to the interpoline.
 *
 * - For VMFrames whose entryncode address (the value of entryfp->ncode before
 *   being clobbered with JaegerTrampolineReturn) is in the original script,
 *   redirect that entryncode to the interpoline.
 */
void
Recompiler::clearStackReferences(FreeOp *fop, JSScript *script)
{
    JS_ASSERT(script->hasJITInfo());

    JaegerSpew(JSpew_Recompile, "recompiling script (file \"%s\") (line \"%d\") (length \"%d\")\n",
               script->filename, script->lineno, script->length);

    JSCompartment *comp = script->compartment();
    types::AutoEnterTypeInference enter(fop, comp);

    /*
     * The strategy for this goes as follows:
     *
     * 1) Scan the stack, looking at all return addresses that could go into JIT
     *    code.
     * 2) If an address corresponds to a call site registered by |callSite| during
     *    the last compilation, patch it to go to the interpoline.
     * 3) Purge the old compiled state.
     */

    // Find all JIT'd stack frames to account for return addresses that will
    // need to be patched after recompilation.
    for (VMFrame *f = fop->runtime()->jaegerRuntime().activeFrame();
         f != NULL;
         f = f->previous)
    {
        if (f->entryfp->compartment() != comp)
            continue;

        // Scan all frames owned by this VMFrame.
        StackFrame *end = f->entryfp->prev();
        StackFrame *next = NULL;
        for (StackFrame *fp = f->fp(); fp != end; fp = fp->prev()) {
            if (fp->script() != script) {
                next = fp;
                continue;
            }

            if (next) {
                // check for a scripted call returning into the recompiled script.
                // this misses scanning the entry fp, which cannot return directly
                // into JIT code.
                void **addr = next->addressOfNativeReturnAddress();

                if (JITCodeReturnAddress(*addr)) {
                    JITChunk *chunk = fp->jit()->findCodeChunk(*addr);
                    patchCall(chunk, fp, addr);
                }
            }

            next = fp;
        }

        patchFrame(comp, f, script);
    }

    comp->types.recompilations++;

    // Purge all ICs in chunks for which we patched any native frames, see patchNative.
    for (VMFrame *f = fop->runtime()->jaegerRuntime().activeFrame();
         f != NULL;
         f = f->previous)
    {
        if (f->fp()->script() == script) {
            JS_ASSERT(f->stubRejoin != REJOIN_NATIVE &&
                      f->stubRejoin != REJOIN_NATIVE_LOWERED &&
                      f->stubRejoin != REJOIN_NATIVE_GETTER);
            if (f->stubRejoin == REJOIN_NATIVE_PATCHED && f->jit() && f->chunk())
                f->chunk()->purgeCaches();
        }
    }
}

} /* namespace mjit */
} /* namespace js */

#endif /* JS_METHODJIT */

