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
 * should jit code equivalent to JSStackFrame::initCallFrameCallerHalf.
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
    uint32     frameDepth;      // script->nfixed + stack depth at caller call site
    RegisterID funObjReg;       // register containing the function object (callee)
    jsbytecode *pc;             // bytecode location at the caller call site
    uint32     flags;           // frame flags

  public:
    /*
     * Register state, so consumers of this class can restrict which registers
     * can and can't be clobbered.
     */
    Registers  tempRegs;

    InlineFrameAssembler(Assembler &masm, ic::CallICInfo &ic, uint32 flags)
      : masm(masm), pc(ic.pc), flags(flags)
    {
        frameDepth = ic.frameDepth;
        funObjReg = ic.funObjReg;
        tempRegs.takeReg(ic.funPtrReg);
        tempRegs.takeReg(funObjReg);
    }

    InlineFrameAssembler(Assembler &masm, Compiler::CallGenInfo &gen, uint32 flags)
      : masm(masm), pc(gen.pc), flags(flags)
    {
        frameDepth = gen.frameDepth;
        funObjReg = gen.funObjReg;
        tempRegs.takeReg(funObjReg);
    }

    DataLabelPtr assemble(void *ncode)
    {
        JS_ASSERT((flags & ~JSFRAME_CONSTRUCTING) == 0);

        RegisterID t0 = tempRegs.takeAnyReg();

        AdjustedFrame adj(sizeof(JSStackFrame) + frameDepth * sizeof(Value));
        masm.store32(Imm32(JSFRAME_FUNCTION | flags), adj.addrOf(JSStackFrame::offsetOfFlags()));
        masm.storePtr(JSFrameReg, adj.addrOf(JSStackFrame::offsetOfPrev()));

        DataLabelPtr ncodePatch =
            masm.storePtrWithPatch(ImmPtr(ncode), adj.addrOf(JSStackFrame::offsetOfncode()));

        /* Adjust JSFrameReg. Callee fills in the rest. */
        masm.addPtr(Imm32(sizeof(JSStackFrame) + sizeof(Value) * frameDepth), JSFrameReg);

        tempRegs.putReg(t0);

        return ncodePatch;
    }
};


} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_inl_frame_asm_h__ */

