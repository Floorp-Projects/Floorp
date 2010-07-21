/* ***** BEGIN LICENSE BLOCK *****
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
Recompiler::findPatch(void **location)
{ 
    for (uint32 i = 0; i < script->callSites[-1].nCallSites; i++) {
        if (script->callSites[i].c.codeOffset + (uint8*)script->nmap[-1] == *location) {
            PatchableAddress result;
            result.location = location;
            result.callSite = script->callSites[i];
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

Recompiler::Recompiler(JSContext *cx, JSScript *script) : cx(cx), script(script)
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
    JS_ASSERT(script->ncode && script->ncode != JS_UNJITTABLE_METHOD);

    Vector<PatchableAddress> toPatch(cx);

    /* Scan the stack, saving the ncode elements of the frames. */
    JSStackFrame *firstFrame = NULL;
    for (CallStackIterator cs(cx); !cs.done(); ++cs) {
        FrameIterator fp = cs.top();
        for (FrameIterator fp = cs.top(); fp != cs.bottom(); ++fp) {
            if (!firstFrame && fp.fp()->script == script)
                firstFrame = fp.fp();
            if (script->isValidJitCode(fp.fp()->ncode)) {
                if (!toPatch.append(findPatch(&fp.fp()->ncode)))
                    return false;
            }
        }
    }

    /* Iterate over VMFrames saving the machine and scripted return. */
    for (VMFrame *f = JS_METHODJIT_DATA(cx).activeFrame;
         f != NULL;
         f = f->previous) {

        if (script->isValidJitCode(f->scriptedReturn)) {
            if (!toPatch.append(findPatch(&f->scriptedReturn)))
                return false;
        }

        void **machineReturn = f->returnAddressLocation();
        if (script->isValidJitCode(*machineReturn))
            if (!toPatch.append(findPatch(machineReturn)))
                return false;
    }

    ReleaseScriptCode(cx, script);

    /* No need to actually compile or fixup if no frames on the stack */
    if (!firstFrame)
        return true;

    Compiler c(cx, script, firstFrame->fun, firstFrame->scopeChain);
    if (c.Compile() != Compile_Okay)
        return false;

    /* Perform the earlier scanned patches */
    for (uint32 i = 0; i < toPatch.length(); i++)
        applyPatch(c, toPatch[i]);

    return true;
}

FrameIterator&
FrameIterator::operator++() {
    JS_ASSERT(curfp);
    curfp = curfp->down;
    return *this;
}

bool
FrameIterator::operator==(const FrameIterator& other) const {
    return curfp == other.curfp;
}

bool
FrameIterator::operator!=(const FrameIterator& other) const {
    return curfp != other.curfp;
}

CallStackIterator&
CallStackIterator::operator++() {
    JS_ASSERT(curcs);
    curcs = curcs->getPreviousInMemory();
    return *this;
}

FrameIterator
CallStackIterator::top() const {
    JS_ASSERT(curcs);
    return FrameIterator(curcs->getCurrentFrame());
}

FrameIterator
CallStackIterator::bottom() const {
    JS_ASSERT(curcs);
    return FrameIterator(curcs->getInitialFrame()->down);
}

}
}

#endif
