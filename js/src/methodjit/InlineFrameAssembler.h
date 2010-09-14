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
 * This is used for emitting code to inline callee-side frame creation.
 * Specifically, it initializes the following members:
 *
 *  savedPC
 *  argc
 *  flags
 *  scopeChain
 *  argv
 *  thisv
 *  down
 *
 * Once finished, JSFrameReg is advanced to be the new fp.
 */
class InlineFrameAssembler {
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::ImmPtr ImmPtr;

    Assembler &masm;
    bool       isConstantThis;  // Is |thisv| constant?
    Value      constantThis;    // If so, this is the value.
    uint32     frameDepth;      // script->nfixed + stack depth at caller call site
    uint32     argc;            // number of args being passed to the function
    RegisterID funObjReg;       // register containing the function object (callee)
    jsbytecode *pc;             // bytecode location at the caller call site
    uint32     flags;           // frame flags

  public:
    /*
     * Register state, so consumers of this class can restrict which registers
     * can and can't be clobbered.
     */
    Registers  tempRegs;

    InlineFrameAssembler(Assembler &masm, JSContext *cx, ic::CallICInfo &ic, uint32 flags)
      : masm(masm), flags(flags)
    {
        isConstantThis = ic.isConstantThis;
        constantThis = ic.constantThis;
        frameDepth = ic.frameDepth;
        argc = ic.argc;
        funObjReg = ic.funObjReg;
        pc = cx->regs->pc;
        tempRegs.takeReg(ic.funPtrReg);
        tempRegs.takeReg(funObjReg);
    }

    InlineFrameAssembler(Assembler &masm, Compiler::CallGenInfo &gen, jsbytecode *pc, uint32 flags)
      : masm(masm), pc(pc), flags(flags)
    {
        isConstantThis = gen.isConstantThis;
        constantThis = gen.constantThis;
        frameDepth = gen.frameDepth;
        argc = gen.argc;
        funObjReg = gen.funObjReg;
        tempRegs.takeReg(funObjReg);
    }

    inline void assemble()
    {
        RegisterID t0 = tempRegs.takeAnyReg();

        /* Note: savedPC goes into the down frame. */
        masm.storePtr(ImmPtr(pc), Address(JSFrameReg, offsetof(JSStackFrame, savedPC)));

        AdjustedFrame adj(sizeof(JSStackFrame) + frameDepth * sizeof(Value));
        masm.store32(Imm32(argc), adj.addrOf(offsetof(JSStackFrame, argc)));
        masm.store32(Imm32(flags), adj.addrOf(offsetof(JSStackFrame, flags)));
        masm.loadPtr(Address(funObjReg, offsetof(JSObject, parent)), t0);
        masm.storePtr(t0, adj.addrOf(JSStackFrame::offsetScopeChain()));
        masm.addPtr(Imm32(adj.baseOffset - (argc * sizeof(Value))), JSFrameReg, t0);
        masm.storePtr(t0, adj.addrOf(offsetof(JSStackFrame, argv)));

        Address targetThis = adj.addrOf(JSStackFrame::offsetThisValue());
        if (isConstantThis) {
            masm.storeValue(constantThis, targetThis);
        } else {
            Address thisvAddr = Address(t0, -int32(sizeof(Value) * 1));
#ifdef JS_NUNBOX32
            RegisterID t1 = tempRegs.takeAnyReg();
            masm.loadPayload(thisvAddr, t1);
            masm.storePayload(t1, targetThis);
            masm.loadTypeTag(thisvAddr, t1);
            masm.storeTypeTag(t1, targetThis);
            tempRegs.putReg(t1);
#elif JS_PUNBOX64
            masm.loadPtr(thisvAddr, t0);
            masm.storePtr(t0, targetThis);
#endif
        }

        masm.storePtr(JSFrameReg, adj.addrOf(offsetof(JSStackFrame, down)));

        /* Adjust JSFrameReg. Callee fills in the rest. */
        masm.addPtr(Imm32(sizeof(JSStackFrame) + sizeof(Value) * frameDepth), JSFrameReg);

        tempRegs.putReg(t0);
    }
};


} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_inl_frame_asm_h__ */

