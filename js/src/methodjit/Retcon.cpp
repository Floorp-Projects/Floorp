/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Jaegermonkey.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <drakedevel@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "MethodJIT-inl.h"

using namespace js;
using namespace js::mjit;

namespace js {
namespace mjit {

AutoScriptRetrapper::~AutoScriptRetrapper()
{
    while (!traps.empty()) {
        jsbytecode *pc = traps.back();
        traps.popBack();
        *pc = JSOP_TRAP;
    }
}

bool
AutoScriptRetrapper::untrap(jsbytecode *pc)
{
    if (!traps.append(pc))
        return false;
    *pc = JS_GetTrapOpcode(traps.allocPolicy().context(), script, pc);
    return true;
}

static inline JSRejoinState ScriptedRejoin(uint32 pcOffset)
{
    return REJOIN_SCRIPTED | (pcOffset << 1);
}

static inline JSRejoinState StubRejoin(RejoinState rejoin)
{
    JS_ASSERT(rejoin != REJOIN_NONE);
    return rejoin << 1;
}

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
CallsiteMatches(uint8 *codeStart, const CallSite &site, void *location)
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
Recompiler::patchCall(JITScript *jit, StackFrame *fp, void **location)
{
    uint8* codeStart = (uint8 *)jit->code.m_code.executableAddress();

    CallSite *callSites_ = jit->callSites();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        if (CallsiteMatches(codeStart, callSites_[i], *location)) {
            JS_ASSERT(callSites_[i].inlineIndex == analyze::CrossScriptSSA::OUTER_FRAME);
            SetRejoinState(fp, callSites_[i], location);
            return;
        }
    }

    JS_NOT_REACHED("failed to find call site");
}

void
Recompiler::patchNative(JSCompartment *compartment, JITScript *jit, StackFrame *fp,
                        jsbytecode *pc, RejoinState rejoin)
{
    /*
     * There is a native call or getter IC at pc which triggered recompilation.
     * The recompilation could have been triggered either by the native call
     * itself, or by a SplatApplyArgs preparing for the native call. Either
     * way, we don't want to patch up the call, but will instead steal the pool
     * for the IC so it doesn't get freed with the JITScript, and patch up the
     * jump at the end to go to the interpoline.
     *
     * When doing this, we do not reset the the IC itself; the JITScript must
     * be dead and about to be released due to the recompilation (or a GC).
     */
    fp->setRejoin(StubRejoin(rejoin));

    /* :XXX: We might crash later if this fails. */
    compartment->jaegerCompartment()->orphanedNativeFrames.append(fp);

    DebugOnly<bool> found = false;

    /*
     * Find and patch all native call stubs attached to the given PC. There may
     * be multiple ones for getter stubs attached to e.g. a GETELEM.
     */
    for (unsigned i = 0; i < jit->nativeCallStubs.length(); i++) {
        NativeCallStub &stub = jit->nativeCallStubs[i];
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
            uint8 *start = (uint8 *)stub.jump.executableAddress();
            JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));
#ifdef JS_CPU_X64
            repatch.repatch(stub.jump, interpoline);
#else
            repatch.relink(stub.jump, JSC::CodeLocationLabel(interpoline));
#endif
        }

        /* :XXX: We leak the pool if this fails. Oh well. */
        compartment->jaegerCompartment()->orphanedNativePools.append(stub.pool);

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
            patchNative(compartment, fp->jit(), fp, f->regs.pc, rejoin);
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
    } else if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
        patchCall(script->jitCtor, fp, addr);
    } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
        patchCall(script->jitNormal, fp, addr);
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

    StackFrame *fp = (StackFrame *) ((uint8 *)outer + sizeof(Value) * inner->depth);
    fp->initInlineFrame(inner->fun, parent, inner->parentpc);
    uint32 pcOffset = inner->parentpc - parent->script()->code;

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

    /*
     * Patch the VMFrame's return address if it is returning at the given inline site.
     * Note there is no worry about handling a native or CompileFunction call here,
     * as such IC stubs are not generated within inline frames.
     */
    void **frameAddr = f->returnAddressLocation();
    uint8* codeStart = (uint8 *)fp->jit()->code.m_code.executableAddress();

    InlineFrame *inner = &fp->jit()->inlineFrames()[inlined->inlineIndex];
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
    if (!compartment || !compartment->hasJaegerCompartment())
        return;

    for (VMFrame *f = compartment->jaegerCompartment()->activeFrame();
         f != NULL;
         f = f->previous) {

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
    if (!compartment || !compartment->hasJaegerCompartment())
        return;

    ExpandInlineFrames(compartment);

    for (VMFrame *f = compartment->jaegerCompartment()->activeFrame();
         f != NULL;
         f = f->previous) {

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
}

Recompiler::Recompiler(JSContext *cx, JSScript *script)
  : cx(cx), script(script)
{    
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
Recompiler::recompile(bool resetUses)
{
    JS_ASSERT(script->hasJITCode());

    JaegerSpew(JSpew_Recompile, "recompiling script (file \"%s\") (line \"%d\") (length \"%d\")\n",
               script->filename, script->lineno, script->length);

    types::AutoEnterTypeInference enter(cx, true);

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
    for (VMFrame *f = script->compartment()->jaegerCompartment()->activeFrame();
         f != NULL;
         f = f->previous) {

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
                    JS_ASSERT(fp->jit()->isValidCode(*addr));
                    patchCall(fp->jit(), fp, addr);
                }
            }

            next = fp;
        }

        patchFrame(cx->compartment, f, script);
    }

    if (script->jitNormal) {
        cleanup(script->jitNormal);
        ReleaseScriptCode(cx, script, false);
    }
    if (script->jitCtor) {
        cleanup(script->jitCtor);
        ReleaseScriptCode(cx, script, true);
    }

    if (resetUses) {
        /*
         * Wait for the script to get warm again before doing another compile,
         * unless we are recompiling *because* the script got hot.
         */
        script->resetUseCount();
    }

    cx->compartment->types.recompilations++;
}

void
Recompiler::cleanup(JITScript *jit)
{
    while (!JS_CLIST_IS_EMPTY(&jit->callers)) {
        JaegerSpew(JSpew_Recompile, "Purging IC caller\n");

        JS_STATIC_ASSERT(offsetof(ic::CallICInfo, links) == 0);
        ic::CallICInfo *ic = (ic::CallICInfo *) jit->callers.next;

        uint8 *start = (uint8 *)ic->funGuard.executableAddress();
        JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));

        repatch.repatch(ic->funGuard, NULL);
        repatch.relink(ic->funJump, ic->slowPathStart);
        ic->purgeGuardedObject();
    }
}

} /* namespace mjit */
} /* namespace js */

#endif /* JS_METHODJIT */

