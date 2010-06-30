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
#if !defined jsjaeger_baseassembler_h__ && defined JS_METHODJIT
#define jsjaeger_baseassembler_h__

#include "jscntxt.h"
#include "jstl.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/moco/MocoStubs.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/MachineRegs.h"

namespace js {
namespace mjit {

//#define JS_METHODJIT_PROFILE_STUBS

struct FrameAddress : JSC::MacroAssembler::Address
{
    FrameAddress(int32 offset)
      : Address(JSC::MacroAssembler::stackPointerRegister, offset)
    { }
};

struct ImmIntPtr : public JSC::MacroAssembler::ImmPtr
{
    ImmIntPtr(intptr_t val)
      : ImmPtr(reinterpret_cast<void*>(val))
    { }
};

class BaseAssembler : public JSC::MacroAssembler
{
    struct CallPatch {
        CallPatch(ptrdiff_t distance, void *fun)
          : distance(distance), fun(fun)
        { }

        ptrdiff_t distance;
        JSC::FunctionPtr fun;
    };

    /* Need a temp reg that is not ArgReg1. */
#if defined(JS_CPU_X86) || defined(JS_CPU_ARM)
    static const RegisterID ClobberInCall = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
    static const RegisterID ClobberInCall = JSC::ARMRegisters::r2;
#endif

    /* :TODO: OOM */
    Label startLabel;
    Vector<CallPatch, 64, SystemAllocPolicy> callPatches;

  public:
    BaseAssembler()
      : callPatches(SystemAllocPolicy())
    {
        startLabel = label();
    }

    /* Total number of floating-point registers. */
    static const uint32 TotalFPRegisters = FPRegisters::TotalFPRegisters;

    /*
     * JSFrameReg is used to home the current JSStackFrame*.
     */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID JSFrameReg = JSC::X86Registers::ebx;
#elif defined(JS_CPU_ARM)
    static const RegisterID JSFrameReg = JSC::X86Registers::r11;
#endif

    size_t distanceOf(Label l) {
        return differenceBetween(startLabel, l);
    }

    void load32FromImm(void *ptr, RegisterID reg) {
        load32(ptr, reg);
    }

    void loadShape(RegisterID obj, RegisterID shape) {
        loadPtr(Address(obj, offsetof(JSObject, map)), shape);
        load32(Address(shape, offsetof(JSObjectMap, shape)), shape);
    }

    /*
     * Finds and returns the address of a known object and slot.
     */
    Address objSlotRef(JSObject *obj, RegisterID reg, uint32 slot) {
        if (slot < JS_INITIAL_NSLOTS) {
            void *vp = &obj->getSlotRef(slot);
            move(ImmPtr(vp), reg);
            return Address(reg, 0);
        }
        move(ImmPtr(&obj->dslots), reg);
        loadPtr(reg, reg);
        return Address(reg, (slot - JS_INITIAL_NSLOTS) * sizeof(Value));
    }

    /*
     * Prepares for a stub call.
     */
    void * getCallTarget(void *fun) {
#ifdef JS_CPU_ARM
        /*
         * Insert a veneer for ARM to allow it to catch exceptions. There is no
         * reliable way to determine the location of the return address on the
         * stack, so it cannot be hijacked.
         *
         * :TODO: It wouldn't surprise me if GCC always pushes LR first. In that
         * case, this looks like the x86-style call, and we can hijack the stack
         * slot accordingly, thus avoiding the cost of a veneer. This should be
         * investigated.
         */

        void *pfun = JS_FUNC_TO_DATA_PTR(void *, JaegerStubVeneer);

        /*
         * We put the real target address into IP, as this won't conflict with
         * the EABI argument-passing mechanism. Technically, this isn't ABI-
         * compliant.
         */
        move(Imm32(intptr_t(fun)), ARMRegisters::ip);
#else
        /*
         * Architectures that push the return address to an easily-determined
         * location on the stack can hijack C++'s return mechanism by overwriting
         * that address, so a veneer is not required.
         */
        void *pfun = fun;
#endif
        return pfun;
    }


#define STUB_CALL_TYPE(type)                                    \
    Call stubCall(type stub, jsbytecode *pc, uint32 fd) {       \
        return stubCall(JS_FUNC_TO_DATA_PTR(void *, stub),      \
                        pc, fd);                                \
    }

    STUB_CALL_TYPE(JSObjStub);
    STUB_CALL_TYPE(VoidPtrStubUInt32);
    STUB_CALL_TYPE(VoidStubUInt32);

#undef STUB_CALL_TYPE

    Call stubCall(void *ptr, jsbytecode *pc, uint32 frameDepth) {
        JS_STATIC_ASSERT(ClobberInCall != Registers::ArgReg1);

        void *pfun = getCallTarget(ptr);

        /* PC -> regs->pc :( */
        storePtr(ImmPtr(pc),
                 FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, pc)));

        /* Store sp */
        fixScriptStack(frameDepth);

        /* VMFrame -> ArgReg0 */
        setupVMFrame();

#ifdef JS_METHODJIT_PROFILE_STUBS
        push(Registers::ArgReg0);
        push(Registers::ArgReg1);
        call(JS_FUNC_TO_DATA_PTR(void *, mjit::ProfileStubCall));
        pop(Registers::ArgReg1);
        pop(Registers::ArgReg0);
#endif
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        push(Registers::ArgReg1);
        push(Registers::ArgReg0);
#endif
        Call cl = call(pfun);
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        pop();
        pop();
#endif
        return cl;
    }

    void fixScriptStack(uint32 frameDepth) {
        /* sp = fp + slots() + stackDepth */
        addPtr(Imm32(sizeof(JSStackFrame) + frameDepth * sizeof(jsval)),
               JSFrameReg,
               ClobberInCall);

        /* regs->sp = sp */
        storePtr(ClobberInCall,
                 FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, sp)));
    }

    void setupVMFrame() {
        move(MacroAssembler::stackPointerRegister, Registers::ArgReg0);
    }

    Call call(void *fun) {
#if defined(_MSC_VER) && defined(_M_X64)
        subPtr(JSC::MacroAssembler::Imm32(32),
               JSC::MacroAssembler::stackPointerRegister);
#endif

        Call cl = JSC::MacroAssembler::call();

#if defined(_MSC_VER) && defined(_M_X64)
        addPtr(JSC::MacroAssembler::Imm32(32),
               JSC::MacroAssembler::stackPointerRegister);
#endif

        callPatches.append(CallPatch(differenceBetween(startLabel, cl), fun));
        return cl;
    }

    Call call(RegisterID reg) {
        return MacroAssembler::call(reg);
    }

    void finalize(uint8 *ncode) {
        JSC::JITCode jc(ncode, size());
        JSC::CodeBlock cb(jc);
        JSC::RepatchBuffer repatchBuffer(&cb);

        for (size_t i = 0; i < callPatches.length(); i++) {
            JSC::MacroAssemblerCodePtr cp(ncode + callPatches[i].distance);
            repatchBuffer.relink(JSC::CodeLocationCall(cp), callPatches[i].fun);
        }
    }
};

/* Save some typing. */
static const JSC::MacroAssembler::RegisterID JSFrameReg = BaseAssembler::JSFrameReg;

} /* namespace mjit */
} /* namespace js */

#endif

