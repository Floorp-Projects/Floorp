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
#include "jsdbgapi.h"
#include "jsnum.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"

#include "jscntxtinlines.h"

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
    *pc = JS_GetTrapOpcode(cx, script, pc);
    return true;
}

Recompiler::PatchableAddress
Recompiler::findPatch(JITScript *jit, void **location)
{ 
    uint8* codeStart = (uint8 *)jit->code.m_code.executableAddress();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        if (jit->callSites[i].codeOffset + codeStart == *location) {
            PatchableAddress result;
            result.location = location;
            result.callSite = jit->callSites[i];
            return result;
        }
    }

    JS_NOT_REACHED("failed to find call site");
    return PatchableAddress();
}

void
Recompiler::applyPatch(Compiler& c, PatchableAddress& toPatch)
{
    void *result = c.findCallSite(toPatch.callSite);
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
    for (i = 0; i < jit->nCallICs; i++) {
        if (jit->callICs[i].pc == pc)
            break;
    }
    JS_ASSERT(i < jit->nCallICs);
    ic::CallICInfo &ic = jit->callICs[i];
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
    for (i = 0; i < jit->nCallICs; i++) {
        if (jit->callICs[i].pc == native.pc)
            break;
    }
    JS_ASSERT(i < jit->nCallICs);
    ic::CallICInfo &ic = jit->callICs[i];

    ic.fastGuardedNative = native.guardedNative;
    ic.pools[ic::CallICInfo::Pool_NativeStub] = native.pool;
    ic.nativeStart = native.nativeStart;
    ic.nativeFunGuard = native.nativeFunGuard;
    ic.nativeJump = native.nativeJump;

    /* Patch the jump on object identity to go to the native stub. */
    {
        uint8 *start = (uint8 *)ic.funJump.executableAddress();
        Repatcher repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(ic.funJump, ic.nativeStart);
    }

    /* Patch the native function guard to go to the slow path. */
    {
        uint8 *start = (uint8 *)native.nativeFunGuard.executableAddress();
        Repatcher repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(native.nativeFunGuard, ic.slowPathStart);
    }

    /* Patch the native fallthrough to go to the slow join point. */
    {
        JSC::CodeLocationLabel joinPoint = ic.slowPathStart.labelAtOffset(ic.slowJoinOffset);
        uint8 *start = (uint8 *)native.nativeJump.executableAddress();
        Repatcher repatch(JSC::JITCode(start - 32, 64));
        repatch.relink(native.nativeJump, joinPoint);
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
    Vector<JSStackFrame*> normalFrames(cx);
    Vector<JSStackFrame*> ctorFrames(cx);

    // Find all JIT'd stack frames to account for return addresses that will
    // need to be patched after recompilation.
    for (VMFrame *f = script->compartment->jaegerCompartment->activeFrame();
         f != NULL;
         f = f->previous) {

        // Scan all frames owned by this VMFrame.
        JSStackFrame *end = f->entryfp->prev();
        JSStackFrame *next = NULL;
        for (JSStackFrame *fp = f->fp(); fp != end; fp = fp->prev()) {
            if (fp->script() == script) {
                // Remember every frame for each type of JIT'd code.
                fp->pc(cx, next);
                if (fp->isConstructing() && !ctorFrames.append(fp))
                    return false;
                if (!fp->isConstructing() && !normalFrames.append(fp))
                    return false;
            }

            // check for a scripted call returning into the recompiled script.
            void **addr = fp->addressOfNativeReturnAddress();
            if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
                if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                    return false;
            } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
                if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                    return false;
            }

            next = fp;
        }

        // These return address checks will not pass when we were called from a native.
        // Native calls are not through the FASTCALL ABI, and use a different return
        // address from f->returnAddressLocation(). We make sure when calling a native that
        // the FASTCALL return address is overwritten with NULL, and that it won't
        // be used by the native itself. Otherwise we will read an uninitialized
        // value here (probably the return address for a previous FASTCALL).
        void **addr = f->returnAddressLocation();
        if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
            if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                return false;
        } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
            if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                return false;
        } else if (f->fp()->script() == script) {
            // Native call.
            if (f->fp()->isConstructing()) {
                if (!ctorNatives.append(stealNative(script->jitCtor, f->fp()->pc(cx, NULL))))
                    return false;
            } else {
                if (!normalNatives.append(stealNative(script->jitNormal, f->fp()->pc(cx, NULL))))
                    return false;
            }
        }
    }

    Vector<CallSite> normalSites(cx);
    Vector<CallSite> ctorSites(cx);
    uint32 normalRecompilations;
    uint32 ctorRecompilations;

    if (script->jitNormal && !cleanup(script->jitNormal, &normalSites, &normalRecompilations))
        return false;
    if (script->jitCtor && !cleanup(script->jitCtor, &ctorSites, &ctorRecompilations))
        return false;

    ReleaseScriptCode(cx, script);

    if (normalFrames.length() &&
        !recompile(normalFrames, normalPatches, normalSites, normalNatives,
                   normalRecompilations)) {
        return false;
    }

    if (ctorFrames.length() &&
        !recompile(ctorFrames, ctorPatches, ctorSites, ctorNatives,
                   ctorRecompilations)) {
        return false;
    }

    return true;
}

bool
Recompiler::cleanup(JITScript *jit, Vector<CallSite> *sites, uint32 *recompilations)
{
    while (!JS_CLIST_IS_EMPTY(&jit->callers)) {
        JaegerSpew(JSpew_Recompile, "Purging IC caller\n");

        JS_STATIC_ASSERT(offsetof(ic::CallICInfo, links) == 0);
        ic::CallICInfo *ic = (ic::CallICInfo *) jit->callers.next;

        uint8 *start = (uint8 *)ic->funGuard.executableAddress();
        Repatcher repatch(JSC::JITCode(start - 32, 64));

        repatch.repatch(ic->funGuard, NULL);
        repatch.relink(ic->funJump, ic->slowPathStart);
        ic->purgeGuardedObject();
    }

    for (uint32 i = 0; i < jit->nCallSites; i++) {
        CallSite &site = jit->callSites[i];
        if (site.isTrap() && !sites->append(site))
            return false;
    }

    *recompilations = jit->recompilations;

    return true;
}

bool
Recompiler::recompile(Vector<JSStackFrame*> &frames, Vector<PatchableAddress> &patches, Vector<CallSite> &sites,
                      Vector<PatchableNative> &natives,
                      uint32 recompilations)
{
    JSStackFrame *fp = frames[0];

    JaegerSpew(JSpew_Recompile, "On stack recompilation, %u patches, %u natives\n",
               patches.length(), natives.length());

    Compiler c(cx, fp);
    if (!c.loadOldTraps(sites))
        return false;
    if (c.compile(&frames) != Compile_Okay)
        return false;

    script->getJIT(fp->isConstructing())->recompilations = recompilations + 1;

    /* Perform the earlier scanned patches */
    for (uint32 i = 0; i < patches.length(); i++)
        applyPatch(c, patches[i]);
    for (uint32 i = 0; i < natives.length(); i++)
        patchNative(script->getJIT(fp->isConstructing()), natives[i]);

    return true;
}

} /* namespace mjit */
} /* namespace js */

#endif /* JS_METHODJIT */

