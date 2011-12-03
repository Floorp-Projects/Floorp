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

#if !defined jsjaeger_inl_frame_asm_h__ && defined JS_METHODJIT && defined JS_MONOIC
#define jsjaeger_inl_frame_asm_h__

#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/MethodJIT.h"
#include "CodeGenIncludes.h"

namespace js {
namespace mjit {

struct AdjustedFrame {
    AdjustedFrame(uint32 baseOffset)
     : baseOffset(baseOffset)
    { }

    uint32 baseOffset;

    JSC::MacroAssembler::Address addrOf(uint32 offset) {
        return JSC::MacroAssembler::Address(JSFrameReg, baseOffset + offset);
    }
};

/*
 * This is used for emitting code to inline callee-side frame creation and
 * should jit code equivalent to StackFrame::initCallFrameCallerHalf.
 *
 * Once finished, JSFrameReg is advanced to be the new fp.
 */
class InlineFrameAssembler {
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::ImmPtr ImmPtr;
    typedef JSC::MacroAssembler::DataLabelPtr DataLabelPtr;

    Assembler &masm;
    FrameSize  frameSize;       // size of the caller's frame
    RegisterID funObjReg;       // register containing the function object (callee)
    uint32     flags;           // frame flags

  public:
    /*
     * Register state, so consumers of this class can restrict which registers
     * can and can't be clobbered.
     */
    Registers  tempRegs;

    InlineFrameAssembler(Assembler &masm, ic::CallICInfo &ic, uint32 flags)
      : masm(masm), flags(flags), tempRegs(Registers::AvailRegs)
    {
        frameSize = ic.frameSize;
        funObjReg = ic.funObjReg;
        tempRegs.takeReg(funObjReg);
    }

    InlineFrameAssembler(Assembler &masm, Compiler::CallGenInfo &gen, uint32 flags)
      : masm(masm), flags(flags), tempRegs(Registers::AvailRegs)
    {
        frameSize = gen.frameSize;
        funObjReg = gen.funObjReg;
        tempRegs.takeReg(funObjReg);
    }

    DataLabelPtr assemble(void *ncode, jsbytecode *pc)
    {
        JS_ASSERT((flags & ~StackFrame::CONSTRUCTING) == 0);

        /* Generate StackFrame::initCallFrameCallerHalf. */

        /* Get the actual flags to write. */
        JS_ASSERT(!(flags & ~StackFrame::CONSTRUCTING));
        uint32 flags = this->flags | StackFrame::FUNCTION;
        if (frameSize.lowered(pc))
            flags |= StackFrame::LOWERED_CALL_APPLY;

        DataLabelPtr ncodePatch;
        if (frameSize.isStatic()) {
            uint32 frameDepth = frameSize.staticLocalSlots();
            AdjustedFrame newfp(sizeof(StackFrame) + frameDepth * sizeof(Value));

            Address flagsAddr = newfp.addrOf(StackFrame::offsetOfFlags());
            masm.store32(Imm32(flags), flagsAddr);
            Address prevAddr = newfp.addrOf(StackFrame::offsetOfPrev());
            masm.storePtr(JSFrameReg, prevAddr);
            Address ncodeAddr = newfp.addrOf(StackFrame::offsetOfNcode());
            ncodePatch = masm.storePtrWithPatch(ImmPtr(ncode), ncodeAddr);

            masm.addPtr(Imm32(sizeof(StackFrame) + frameDepth * sizeof(Value)), JSFrameReg);
        } else {
            /*
             * If the frame size is dynamic, then the fast path generated by
             * generateFullCallStub must be used. Thus, this code is executed
             * after stubs::SplatApplyArgs has been called. SplatApplyArgs
             * stores the dynamic stack pointer (i.e., regs.sp after pushing a
             * dynamic number of arguments) to VMFrame.regs, so we just load it
             * here to get the new frame pointer.
             */
            RegisterID newfp = tempRegs.takeAnyReg().reg();
            masm.loadPtr(FrameAddress(VMFrame::offsetOfRegsSp()), newfp);

            Address flagsAddr(newfp, StackFrame::offsetOfFlags());
            masm.store32(Imm32(flags), flagsAddr);
            Address prevAddr(newfp, StackFrame::offsetOfPrev());
            masm.storePtr(JSFrameReg, prevAddr);
            Address ncodeAddr(newfp, StackFrame::offsetOfNcode());
            ncodePatch = masm.storePtrWithPatch(ImmPtr(ncode), ncodeAddr);

            masm.move(newfp, JSFrameReg);
            tempRegs.putReg(newfp);
        }

        return ncodePatch;
    }
};


} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_inl_frame_asm_h__ */

