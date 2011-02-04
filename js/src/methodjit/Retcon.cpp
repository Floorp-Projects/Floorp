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
    CallSite *callSites_ = jit->callSites();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        if (callSites_[i].codeOffset + codeStart == *location) {
            PatchableAddress result;
            result.location = location;
            result.callSite = callSites_[i];
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

Recompiler::Recompiler(JSContext *cx, JSScript *script)
  : cx(cx), script(script)
{    
}

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
bool
Recompiler::recompile()
{
    JS_ASSERT(script->hasJITCode());

    Vector<PatchableAddress> normalPatches(cx);
    Vector<PatchableAddress> ctorPatches(cx);

    JSStackFrame *firstCtorFrame = NULL;
    JSStackFrame *firstNormalFrame = NULL;

    // Find all JIT'd stack frames to account for return addresses that will
    // need to be patched after recompilation.
    for (VMFrame *f = script->compartment->jaegerCompartment->activeFrame();
         f != NULL;
         f = f->previous) {

        // Scan all frames owned by this VMFrame.
        JSStackFrame *end = f->entryfp->prev();
        for (JSStackFrame *fp = f->fp(); fp != end; fp = fp->prev()) {
            // Remember the latest frame for each type of JIT'd code, so the
            // compiler will have a frame to re-JIT from.
            if (!firstCtorFrame && fp->script() == script && fp->isConstructing())
                firstCtorFrame = fp;
            else if (!firstNormalFrame && fp->script() == script && !fp->isConstructing())
                firstNormalFrame = fp;

            void **addr = fp->addressOfNativeReturnAddress();
            if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
                if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                    return false;
            } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
                if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                    return false;
            }
        }

        void **addr = f->returnAddressLocation();
        if (script->jitCtor && script->jitCtor->isValidCode(*addr)) {
            if (!ctorPatches.append(findPatch(script->jitCtor, addr)))
                return false;
        } else if (script->jitNormal && script->jitNormal->isValidCode(*addr)) {
            if (!normalPatches.append(findPatch(script->jitNormal, addr)))
                return false;
        }
    }

    Vector<CallSite> normalSites(cx);
    Vector<CallSite> ctorSites(cx);

    if (script->jitNormal && !saveTraps(script->jitNormal, &normalSites))
        return false;
    if (script->jitCtor && !saveTraps(script->jitCtor, &ctorSites))
        return false;

    ReleaseScriptCode(cx, script);

    if (normalPatches.length() &&
        !recompile(firstNormalFrame, normalPatches, normalSites)) {
        return false;
    }

    if (ctorPatches.length() &&
        !recompile(firstCtorFrame, ctorPatches, ctorSites)) {
        return false;
    }

    return true;
}

bool
Recompiler::saveTraps(JITScript *jit, Vector<CallSite> *sites)
{
    CallSite *callSites_ = jit->callSites();
    for (uint32 i = 0; i < jit->nCallSites; i++) {
        CallSite &site = callSites_[i];
        if (site.isTrap() && !sites->append(site))
            return false;
    }
    return true;
}

bool
Recompiler::recompile(JSStackFrame *fp, Vector<PatchableAddress> &patches,
                      Vector<CallSite> &sites)
{
    /* If we get this far, the script is live, and we better be safe to re-jit. */
    JS_ASSERT(cx->compartment->debugMode);
    JS_ASSERT(fp);

    Compiler c(cx, fp);
    if (!c.loadOldTraps(sites))
        return false;
    if (c.compile() != Compile_Okay)
        return false;

    /* Perform the earlier scanned patches */
    for (uint32 i = 0; i < patches.length(); i++)
        applyPatch(c, patches[i]);

    return true;
}

} /* namespace mjit */
} /* namespace js */

#endif /* JS_METHODJIT */

