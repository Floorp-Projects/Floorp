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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "jsscope.h"
#include "jsnum.h"
#include "MonoIC.h"
#include "StubCalls.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/assembler/MacroAssembler.h"
#include "CodeGenIncludes.h"
#include "jsobj.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;
using namespace js::mjit;
using namespace js::mjit::ic;

#if defined JS_MONOIC

static void
PatchGetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stubs::GetGlobalName));
    repatch.relink(mic.stubCall, fptr);
}

void JS_FASTCALL
ic::GetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp()->getScopeChain()->getGlobal();
    ic::MICInfo &mic = f.fp()->getScript()->mics[index];
    JSAtom *atom = f.fp()->getScript()->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::GET);

    JS_LOCK_OBJ(f.cx, obj);
    const Shape *shape = obj->nativeLookup(id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->hasSlot())
    {
        JS_UNLOCK_OBJ(f.cx, obj);
        if (shape)
            PatchGetFallback(f, mic);
        stubs::GetGlobalName(f);
        return;
    }
    uint32 slot = shape->slot;
    JS_UNLOCK_OBJ(f.cx, obj);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, obj->shape());

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);
    JSC::RepatchBuffer loads(mic.load.executableAddress(), 32, false);
#if defined JS_CPU_X86
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_DATA_OFFSET), slot);
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_TYPE_OFFSET), slot + 4);
#elif defined JS_CPU_ARM
    // mic.load actually points to the LDR instruction which fetches the offset, but 'repatch'
    // knows how to dereference it to find the integer value.
    loads.repatch(mic.load.dataLabel32AtOffset(0), slot);
#elif defined JS_PUNBOX64
    loads.repatch(mic.load.dataLabel32AtOffset(mic.patchValueOffset), slot);
#endif

    /* Do load anyway... this time. */
    stubs::GetGlobalName(f);
}

static void JS_FASTCALL
SetGlobalNameSlow(VMFrame &f, uint32 index)
{
    JSAtom *atom = f.fp()->getScript()->getAtom(GET_INDEX(f.regs.pc));
    stubs::SetGlobalName(f, atom);
}

static void
PatchSetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, SetGlobalNameSlow));
    repatch.relink(mic.stubCall, fptr);
}

static VoidStubAtom
GetStubForSetGlobalName(VMFrame &f)
{
    // The property cache doesn't like inc ops, so we use a simpler
    // stub for that case.
    return js_CodeSpec[*f.regs.pc].format & (JOF_INC | JOF_DEC)
         ? stubs::SetGlobalNameDumb
         : stubs::SetGlobalName;
}

void JS_FASTCALL
ic::SetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp()->getScopeChain()->getGlobal();
    ic::MICInfo &mic = f.fp()->getScript()->mics[index];
    JSAtom *atom = f.fp()->getScript()->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::SET);

    JS_LOCK_OBJ(f.cx, obj);
    const Shape *shape = obj->nativeLookup(id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->writable() ||
        !shape->hasSlot())
    {
        JS_UNLOCK_OBJ(f.cx, obj);
        if (shape)
            PatchSetFallback(f, mic);
        GetStubForSetGlobalName(f)(f, atom);
        return;
    }
    uint32 slot = shape->slot;
    JS_UNLOCK_OBJ(f.cx, obj);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, obj->shape());

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);

    JSC::RepatchBuffer stores(mic.load.executableAddress(), 32, false);
#if defined JS_CPU_X86
    stores.repatch(mic.load.dataLabel32AtOffset(MICInfo::SET_TYPE_OFFSET), slot + 4);

    uint32 dataOffset;
    if (mic.u.name.typeConst)
        dataOffset = MICInfo::SET_DATA_CONST_TYPE_OFFSET;
    else
        dataOffset = MICInfo::SET_DATA_TYPE_OFFSET;
    stores.repatch(mic.load.dataLabel32AtOffset(dataOffset), slot);
#elif defined JS_CPU_ARM
    // mic.load actually points to the LDR instruction which fetches the offset, but 'repatch'
    // knows how to dereference it to find the integer value.
    stores.repatch(mic.load.dataLabel32AtOffset(0), slot);
#elif defined JS_PUNBOX64
    stores.repatch(mic.load.dataLabel32AtOffset(mic.patchValueOffset), slot);
#endif

    // Actually implement the op the slow way.
    GetStubForSetGlobalName(f)(f, atom);
}

#ifdef JS_CPU_X86

ic::NativeCallCompiler::NativeCallCompiler()
    : jumps(SystemAllocPolicy())
{}

void
ic::NativeCallCompiler::finish(JSScript *script, uint8 *start, uint8 *fallthrough)
{
    /* Add a jump to fallthrough. */
    Jump fallJump = masm.jump();
    addLink(fallJump, fallthrough);

    uint8 *result = (uint8 *)script->jit->execPool->alloc(masm.size());
    JSC::ExecutableAllocator::makeWritable(result, masm.size());
    masm.executableCopy(result);

    /* Overwrite start with a jump to the call buffer. */
    BaseAssembler::insertJump(start, result);

    /* Patch all calls with the correct target. */
    masm.finalize(result);

    /* Patch all jumps with the correct target. */
    JSC::LinkBuffer linkmasm(result, masm.size());

    for (size_t i = 0; i < jumps.length(); i++)
        linkmasm.link(jumps[i].from, JSC::CodeLocationLabel(jumps[i].to));
}

void
ic::CallNative(JSContext *cx, JSScript *script, MICInfo &mic, JSFunction *fun, bool isNew)
{
    if (mic.u.generated) {
        /* Already generated a MIC at this site, don't make another one. */
        return;
    }
    mic.u.generated = true;

    JS_ASSERT(fun->isNative());
    if (isNew)
        JS_ASSERT(fun->isConstructor());

    Native native = fun->u.n.native;

    typedef JSC::MacroAssembler::ImmPtr ImmPtr;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Jump Jump;

    uint8 *start = (uint8*) mic.knownObject.executableAddress();
    uint8 *stubEntry = (uint8*) mic.stubEntry.executableAddress();
    uint8 *fallthrough = (uint8*) mic.callEnd.executableAddress();

    NativeCallCompiler ncc;

    Jump differentFunction = ncc.masm.branchPtr(Assembler::NotEqual, mic.dataReg, ImmPtr(fun));
    ncc.addLink(differentFunction, stubEntry);

    /* Manually construct the X86 stack. TODO: get a more portable way of doing this. */

    /* Register to use for filling in the fast native's arguments. */
    JSC::MacroAssembler::RegisterID temp = mic.dataReg;

    /* Store the pc, which is the same as for the current slow call. */
    ncc.masm.storePtr(ImmPtr(cx->regs->pc),
                      FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, pc)));

    /* Store sp. */
    uint32 spOffset = sizeof(JSStackFrame) + (mic.frameDepth + mic.argc + 2) * sizeof(jsval);
    ncc.masm.addPtr(Imm32(spOffset), JSFrameReg, temp);
    ncc.masm.storePtr(temp, FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, sp)));

    /* Make room for the three arguments on the stack, preserving stack register alignment. */
    const uint32 stackAdjustment = 16;
    ncc.masm.sub32(Imm32(stackAdjustment), JSC::X86Registers::esp);

    /* Compute and push vp */
    uint32 vpOffset = sizeof(JSStackFrame) + mic.frameDepth * sizeof(jsval);
    ncc.masm.addPtr(Imm32(vpOffset), JSFrameReg, temp);
    ncc.masm.storePtr(temp, Address(JSC::X86Registers::esp, 0x8));

    if (isNew) {
        /* Mark 'this' as magic. */
        Value magicCtorThis;
        magicCtorThis.setMagicWithObjectOrNullPayload(NULL);
        ncc.masm.storeValue(magicCtorThis, Address(temp, sizeof(Value)));
    }

    /* Push argc */
    ncc.masm.store32(Imm32(mic.argc), Address(JSC::X86Registers::esp, 0x4));

    /* Push cx. The VMFrame is homed at the stack register, so adjust for the amount we pushed. */
    ncc.masm.loadPtr(FrameAddress(stackAdjustment + offsetof(VMFrame, cx)), temp);
    ncc.masm.storePtr(temp, Address(JSC::X86Registers::esp, 0));

    /* Do the call. */
    ncc.masm.call(JS_FUNC_TO_DATA_PTR(void *, native));

    /* Restore stack. */
    ncc.masm.add32(Imm32(stackAdjustment), JSC::X86Registers::esp);

#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
    // Usually JaegerThrowpoline got called from return address.
    // So in JaegerThrowpoline without fastcall, esp was added by 8.
    // If we just want to jump there, we need to sub esp by 8 first.
    ncc.masm.sub32(Imm32(8), JSC::X86Registers::esp);
#endif

    /* Check if the call is throwing, and jump to the throwpoline. */
    Jump hasException =
        ncc.masm.branchTest32(Assembler::Zero, Registers::ReturnReg, Registers::ReturnReg);
    ncc.addLink(hasException, JS_FUNC_TO_DATA_PTR(uint8 *, JaegerThrowpoline));

#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
    ncc.masm.add32(Imm32(8), JSC::X86Registers::esp);
#endif

    /* Load *vp into the return register pair. */
    Address rval(JSFrameReg, vpOffset);
    ncc.masm.loadPayload(rval, JSReturnReg_Data);
    ncc.masm.loadTypeTag(rval, JSReturnReg_Type);

    ncc.finish(script, start, fallthrough);
}

#endif /* JS_CPU_X86 */

void
ic::PurgeMICs(JSContext *cx, JSScript *script)
{
    /* MICs are purged during GC to handle changing shapes. */
    JS_ASSERT(cx->runtime->gcRegenShapes);

    uint32 nmics = script->jit->nMICs;
    for (uint32 i = 0; i < nmics; i++) {
        ic::MICInfo &mic = script->mics[i];
        switch (mic.kind) {
          case ic::MICInfo::SET:
          case ic::MICInfo::GET:
          {
            /* Patch shape guard. */
            JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
            repatch.repatch(mic.shape, int(JSObjectMap::INVALID_SHAPE));

            /* 
             * If the stub call was patched, leave it alone -- it probably will
             * just be invalidated again.
             */
            break;
          }
          case ic::MICInfo::CALL:
          case ic::MICInfo::EMPTYCALL:
          case ic::MICInfo::TRACER:
            /* Nothing to patch! */
            break;
          default:
            JS_NOT_REACHED("Unknown MIC type during purge");
            break;
        }
    }
}

#endif /* JS_MONOIC */

