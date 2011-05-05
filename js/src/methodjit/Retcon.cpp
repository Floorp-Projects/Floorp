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

Recompiler::PatchableAddress
Recompiler::findPatch(JITScript *jit, void **location)
{
    uint8* codeStart = (uint8 *)jit->code.m_code.executableAddress();

    CallSite *callSites_ = jit->callSites();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        if (callSites_[i].codeOffset + codeStart == *location) {
            JS_ASSERT(callSites_[i].inlineIndex == analyze::CrossScriptSSA::OUTER_FRAME);
            PatchableAddress result;
            result.location = location;
            result.callSite = callSites_[i];
            return result;
        }
    }

    RejoinSite *rejoinSites_ = jit->rejoinSites();
    for (uint32 i = 0; i < jit->nRejoinSites; i++) {
        const RejoinSite &rs = rejoinSites_[i];
        if (rs.codeOffset + codeStart == *location) {
            PatchableAddress result;
            result.location = location;
            result.callSite.initialize(rs.codeOffset, uint32(-1), rs.pcOffset, rs.id);
            return result;
        }
    }

    JS_NOT_REACHED("failed to find call site");
    return PatchableAddress();
}

void *
Recompiler::findRejoin(JITScript *jit, const CallSite &callSite)
{
    JS_ASSERT(callSite.inlineIndex == uint32(-1));

    RejoinSite *rejoinSites_ = jit->rejoinSites();
    for (uint32 i = 0; i < jit->nRejoinSites; i++) {
        RejoinSite &rs = rejoinSites_[i];
        if (rs.pcOffset == callSite.pcOffset &&
            (rs.id == callSite.id || rs.id == RejoinSite::VARIADIC_ID)) {
            /*
             * We should not catch rejoin sites for scripted calls with a
             * variadic id, the rejoin code for these is different.
             */
            JS_ASSERT_IF(rs.id == RejoinSite::VARIADIC_ID,
                         callSite.id != CallSite::NCODE_RETURN_ID);
            uint8* codeStart = (uint8 *)jit->code.m_code.executableAddress();
            return codeStart + rs.codeOffset;
        }
    }

    /* We have no idea where to patch up to. */
    JS_NOT_REACHED("Call site vanished.");
    return NULL;
}

void
Recompiler::applyPatch(JITScript *jit, PatchableAddress& toPatch)
{
    void *result = findRejoin(jit, toPatch.callSite);
    JS_ASSERT(result);
    *toPatch.location = result;
}

Recompiler::PatchableNative
Recompiler::stealNative(JITScript *jit, jsbytecode *pc)
{
    /*
     * There is a native IC at pc which triggered a recompilation. The recompilation
     * could have been triggered either by the native call itself, or by a SplatApplyArgs
     * preparing for the native call. Either way, we don't want to patch up the call,
     * but will instead steal the pool for the native IC so it doesn't get freed
     * with the old script, and patch up the jump at the end to point to the slow join
     * point in the new script.
     */
    unsigned i;
    ic::CallICInfo *callICs = jit->callICs();
    for (i = 0; i < jit->nCallICs; i++) {
        CallSite *call = callICs[i].call;
        if (call->inlineIndex == uint32(-1) && call->pcOffset == uint32(pc - jit->script->code))
            break;
    }
    JS_ASSERT(i < jit->nCallICs);
    ic::CallICInfo &ic = callICs[i];
    JS_ASSERT(ic.fastGuardedNative);

    JSC::ExecutablePool *&pool = ic.pools[ic::CallICInfo::Pool_NativeStub];

    if (!pool) {
        /* Already stole this stub. */
        PatchableNative native;
        native.pc = NULL;
        native.guardedNative = NULL;
        native.pool = NULL;
        return native;
    }

    PatchableNative native;
    native.pc = pc;
    native.guardedNative = ic.fastGuardedNative;
    native.pool = pool;
    native.nativeStart = ic.nativeStart;
    native.nativeFunGuard = ic.nativeFunGuard;
    native.nativeJump = ic.nativeJump;

    /*
     * Mark as stolen in case there are multiple calls on the stack. Note that if
     * recompilation fails due to low memory then this pool will leak.
     */
    pool = NULL;

    return native;
}

void
Recompiler::patchNative(JITScript *jit, PatchableNative &native)
{
    if (!native.pc)
        return;

    unsigned i;
    ic::CallICInfo *callICs = jit->callICs();
    for (i = 0; i < jit->nCallICs; i++) {
        CallSite *call = callICs[i].call;
        if (call->inlineIndex == uint32(-1) && call->pcOffset == uint32(native.pc - jit->script->code))
            break;
    }
    JS_ASSERT(i < jit->nCallICs);
    ic::CallICInfo &ic = callICs[i];

    ic.fastGuardedNative = native.guardedNative;
    ic.pools[ic::CallICInfo::Pool_NativeStub] = native.pool;
    ic.nativeStart = native.nativeStart;
    ic.nativeFunGuard = native.nativeFunGuard;
    ic.nativeJump = native.nativeJump;

    /* Patch the jump on object identity to go to the native stub. */
    {
        uint8 *start = (uint8 *)ic.funJump.executableAddress();
        JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(ic.funJump, ic.nativeStart);
    }

    /* Patch the native function guard to go to the slow path. */
    {
        uint8 *start = (uint8 *)native.nativeFunGuard.executableAddress();
        JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(native.nativeFunGuard, ic.slowPathStart);
    }

    /* Patch the native fallthrough to go to the slow join point. */
    {
        JSC::CodeLocationLabel joinPoint = ic.slowPathStart.labelAtOffset(ic.slowJoinOffset);
        uint8 *start = (uint8 *)native.nativeJump.executableAddress();
        JSC::RepatchBuffer repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(native.nativeJump, joinPoint);
    }
}

StackFrame *
Recompiler::expandInlineFrameChain(JSContext *cx, StackFrame *outer, InlineFrame *inner)
{
    StackFrame *parent;
    if (inner->parent)
        parent = expandInlineFrameChain(cx, outer, inner->parent);
    else
        parent = outer;

    JaegerSpew(JSpew_Recompile, "Expanding inline frame\n");

    StackFrame *fp = (StackFrame *) ((uint8 *)outer + sizeof(Value) * inner->depth);
    fp->initInlineFrame(inner->fun, parent, inner->parentpc);
    uint32 pcOffset = inner->parentpc - parent->script()->code;

    /*
     * We should have ensured during compilation that the erased frame has JIT
     * code with rejoin points added. We don't try to compile such code on
     * demand as this can trigger recompilations and a reentrant invocation of
     * expandInlineFrames. Note that the outer frame does not need to have
     * rejoin points, as it is definitely at an inline call and rejoin points
     * are always added for such calls.
     */
    JS_ASSERT(fp->jit() && fp->jit()->rejoinPoints);

    PatchableAddress patch;
    patch.location = fp->addressOfNativeReturnAddress();
    patch.callSite.initialize(0, uint32(-1), pcOffset, CallSite::NCODE_RETURN_ID);
    applyPatch(parent->jit(), patch);

    return fp;
}

/*
 * Expand all inlined frames within fp per 'inlined' and update next and regs
 * to refer to the new innermost frame.
 */
void
Recompiler::expandInlineFrames(JSContext *cx, StackFrame *fp, mjit::CallSite *inlined,
                               StackFrame *next, VMFrame *f)
{
    JS_ASSERT_IF(next, next->prev() == fp && next->prevInline() == inlined);

    /*
     * Treat any frame expansion as a recompilation event, so that f.jit() is
     * stable if no recompilations have occurred.
     */
    cx->compartment->types.frameExpansions++;

    /* Patch the VMFrame's return address if it is returning at the given inline site. */
    void **frameAddr = f->returnAddressLocation();
    uint8* codeStart = (uint8 *)fp->jit()->code.m_code.executableAddress();
    bool patchFrameReturn =
        (f->scratch != NATIVE_CALL_SCRATCH_VALUE) &&
        (*frameAddr == codeStart + inlined->codeOffset);

    InlineFrame *inner = &fp->jit()->inlineFrames()[inlined->inlineIndex];
    jsbytecode *innerpc = inner->fun->script()->code + inlined->pcOffset;

    StackFrame *innerfp = expandInlineFrameChain(cx, fp, inner);
    JITScript *jit = innerfp->jit();

    if (f->regs.fp() == fp) {
        JS_ASSERT(f->regs.inlined() == inlined);
        f->regs.expandInline(innerfp, innerpc);
    }

    if (patchFrameReturn) {
        PatchableAddress patch;
        patch.location = frameAddr;
        patch.callSite.initialize(0, uint32(-1), inlined->pcOffset, inlined->id);
        applyPatch(jit, patch);
    }

    if (next) {
        next->resetInlinePrev(innerfp, innerpc);
        void **addr = next->addressOfNativeReturnAddress();
        if (*addr != NULL && *addr != JS_FUNC_TO_DATA_PTR(void *, JaegerTrampolineReturn)) {
            PatchableAddress patch;
            patch.location = addr;
            patch.callSite.initialize(0, uint32(-1), inlined->pcOffset, CallSite::NCODE_RETURN_ID);
            applyPatch(jit, patch);
        }
    }
}

void
ExpandInlineFrames(JSContext *cx, bool all)
{
    if (!all) {
        VMFrame *f = cx->compartment->jaegerCompartment->activeFrame();
        if (f && f->regs.inlined() && cx->fp() == f->fp())
            mjit::Recompiler::expandInlineFrames(cx, f->fp(), f->regs.inlined(), NULL, f);
        return;
    }

    for (VMFrame *f = cx->compartment->jaegerCompartment->activeFrame();
         f != NULL;
         f = f->previous) {

        if (f->regs.inlined()) {
            StackSegment &seg = cx->stack.space().containingSegment(f->fp());
            FrameRegs &regs = seg.currentRegs();
            if (regs.fp() == f->fp()) {
                JS_ASSERT(&regs == &f->regs);
                mjit::Recompiler::expandInlineFrames(cx, f->fp(), f->regs.inlined(), NULL, f);
            } else {
                StackFrame *nnext = seg.computeNextFrame(f->fp());
                mjit::Recompiler::expandInlineFrames(cx, f->fp(), f->regs.inlined(), nnext, f);
            }
        }

        StackFrame *end = f->entryfp->prev();
        StackFrame *next = NULL;
        for (StackFrame *fp = f->fp(); fp != end; fp = fp->prev()) {
            mjit::CallSite *inlined;
            fp->pc(cx, next, &inlined);
            if (next && inlined) {
                mjit::Recompiler::expandInlineFrames(cx, fp, inlined, next, f);
                fp = next;
                next = NULL;
            } else {
                next = fp;
            }
        }
    }
}

Recompiler::Recompiler(JSContext *cx, JSScript *script)
  : cx(cx), script(script)
{    
}

/*
 * Recompilation can be triggered either by the debugger (turning debug mode on for
 * a script or setting/clearing a trap), or by dynamic changes in type information
 * from type inference. When recompiling we also need to change any references to
 * the old version of the script to refer to the new version of the script, including
 * references on the JS stack. Things to do:
 *
 * - Purge scripted call inline caches calling into the old script.
 *
 * - For arg/local/stack slots in frames on the stack that are now inferred
 *   as (int | double), make sure they are actually doubles. Before recompilation
 *   they may have been inferred as integers and stored to the stack as integers,
 *   but slots inferred as (int | double) are required to be definitely double.
 *
 * - For frames with an ncode return address in the original script, update
 *   to point to the corresponding return address in the new script.
 *
 * - For VMFrames with a stub call return address in the original script,
 *   update to point to the corresponding return address in the new script.
 *   This requires that the recompiled script has a superset of the stub calls
 *   in the original script. Stub calls are keyed to the function being called,
 *   so with less precise type information the call to a stub can move around
 *   (e.g. from inline to OOL path or vice versa) but can't disappear, and
 *   further operation after the stub should be consistent across compilations.
 *
 * - For VMFrames with a native call return address in a call IC in the original
 *   script (the only place where IC code makes calls), make a new stub to throw
 *   an exception or jump to the call's slow path join point.
 */
bool
Recompiler::recompile()
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
     *    the last compilation, remember it.
     * 3) Purge the old compiled state and return if there were no active frames of 
     *    this script on the stack.
     * 4) Fix up the stack by replacing all saved addresses with the addresses the
     *    new compiler gives us for the call sites.
     */
    Vector<PatchableAddress> normalPatches(cx);
    Vector<PatchableAddress> ctorPatches(cx);
    Vector<PatchableNative> normalNatives(cx);
    Vector<PatchableNative> ctorNatives(cx);

    /* Frames containing data that may need to be patched from int to double. */
    Vector<PatchableFrame> normalFrames(cx);
    Vector<PatchableFrame> ctorFrames(cx);

    // Find all JIT'd stack frames to account for return addresses that will
    // need to be patched after recompilation.
    for (VMFrame *f = script->compartment->jaegerCompartment->activeFrame();
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

            // Remember every frame for each type of JIT'd code.
            PatchableFrame frame;
            frame.fp = fp;
            frame.pc = fp->pc(cx, next);
            frame.scriptedCall = false;

            if (next) {
                // check for a scripted call returning into the recompiled script.
                // this misses scanning the entry fp, which cannot return directly
                // into JIT code.
                void **addr = next->addressOfNativeReturnAddress();

                if (!*addr) {
                    // next is an interpreted frame.
                } else if (*addr == JS_FUNC_TO_DATA_PTR(void *, JaegerTrampolineReturn)) {
                    // next entered from the interpreter.
                } else if (fp->isConstructing()) {
                    JS_ASSERT(script->jitCtor && script->jitCtor->isValidCode(*addr));
                    frame.scriptedCall = true;
                    if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                        return false;
                } else {
                    JS_ASSERT(script->jitNormal && script->jitNormal->isValidCode(*addr));
                    frame.scriptedCall = true;
                    if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                        return false;
                }
            }

            if (fp->isConstructing() && !ctorFrames.append(frame))
                return false;
            if (!fp->isConstructing() && !normalFrames.append(frame))
                return false;

            next = fp;
        }

        /* Check if the VMFrame returns directly into the recompiled script. */
        StackFrame *fp = f->fp();
        void **addr = f->returnAddressLocation();
        if (f->scratch == NATIVE_CALL_SCRATCH_VALUE) {
            // Native call.
            if (fp->script() == script && fp->isConstructing()) {
                if (!ctorNatives.append(stealNative(script->jitCtor, fp->pc(cx, NULL))))
                    return false;
            } else if (fp->script() == script) {
                if (!normalNatives.append(stealNative(script->jitNormal, fp->pc(cx, NULL))))
                    return false;
            }
        } else if (f->scratch == COMPILE_FUNCTION_SCRATCH_VALUE) {
            if (fp->prev()->script() == script) {
                PatchableAddress patch;
                patch.location = addr;
                patch.callSite.initialize(0, uint32(-1), fp->prev()->pc(cx, NULL) - script->code,
                                          (size_t) JS_FUNC_TO_DATA_PTR(void *, stubs::UncachedCall));
                if (fp->prev()->isConstructing()) {
                    if (!ctorPatches.append(patch))
                        return false;
                } else {
                    if (!normalPatches.append(patch))
                        return false;
                }
            }
        } else if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
            if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                return false;
        } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
            if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                return false;
        }
    }

    Vector<CallSite> normalSites(cx);
    Vector<CallSite> ctorSites(cx);

    if (script->jitNormal && !cleanup(script->jitNormal, &normalSites))
        return false;
    if (script->jitCtor && !cleanup(script->jitCtor, &ctorSites))
        return false;

    ReleaseScriptCode(cx, script, true);
    ReleaseScriptCode(cx, script, false);

    /*
     * Regenerate the code if there are JIT frames on the stack, if this script
     * has inline parents and thus always needs JIT code, or if it is a newly
     * pushed frame by e.g. the interpreter. :XXX: it would be nice if we could
     * ensure that compiling a script does not then trigger its recompilation.
     */
    StackFrame *top = (cx->running() && cx->fp()->isScriptFrame()) ? cx->fp() : NULL;
    bool keepNormal = !normalFrames.empty() || script->inlineParents ||
        (top && top->script() == script && !top->isConstructing());
    bool keepCtor = !ctorFrames.empty() ||
        (top && top->script() == script && top->isConstructing());

    if (keepNormal && !recompile(script, false,
                                 normalFrames, normalPatches, normalSites, normalNatives)) {
        return false;
    }
    if (keepCtor && !recompile(script, true,
                               ctorFrames, ctorPatches, ctorSites, ctorNatives)) {
        return false;
    }

    JS_ASSERT_IF(keepNormal, script->jitNormal);
    JS_ASSERT_IF(keepCtor, script->jitCtor);

    cx->compartment->types.recompilations++;
    return true;
}

bool
Recompiler::cleanup(JITScript *jit, Vector<CallSite> *sites)
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

    CallSite *callSites_ = jit->callSites();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        CallSite &site = callSites_[i];
        if (site.isTrap() && !sites->append(site))
            return false;
    }

    return true;
}

bool
Recompiler::recompile(JSScript *script, bool isConstructing,
                      Vector<PatchableFrame> &frames,
                      Vector<PatchableAddress> &patches, Vector<CallSite> &sites,
                      Vector<PatchableNative> &natives)
{
    JaegerSpew(JSpew_Recompile, "On stack recompilation, %u frames, %u patches, %u natives\n",
               frames.length(), patches.length(), natives.length());

    CompileStatus status = Compile_Retry;
    while (status == Compile_Retry) {
        Compiler cc(cx, script, isConstructing, &frames);
        if (!cc.loadOldTraps(sites))
            return false;
        status = cc.compile();
    }
    if (status != Compile_Okay)
        return false;

    JITScript *jit = script->getJIT(isConstructing);

    /* Perform the earlier scanned patches */
    for (uint32 i = 0; i < patches.length(); i++)
        applyPatch(jit, patches[i]);
    for (uint32 i = 0; i < natives.length(); i++)
        patchNative(jit, natives[i]);

    return true;
}

} /* namespace mjit */
} /* namespace js */

#endif /* JS_METHODJIT */

