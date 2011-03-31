/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * STMicroelectronics.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Cédric VINCENT <cedric.vincent@st.com> for STMicroelectronics
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

#include "nanojit.h"

#if defined FEATURE_NANOJIT && defined NANOJIT_SH4

namespace nanojit
{
    const int      Assembler::NumArgRegs  = 4;
    const Register Assembler::argRegs[]   = { R4, R5, R6, R7 };
    const Register Assembler::retRegs[]   = { R0, R1 };
    const Register Assembler::savedRegs[] = { R8, R9, R10, R11, R12, R13 };

    const int      Assembler::NumArgDregs = 4;
    const Register Assembler::argDregs[] = { _D2, _D3, _D4, _D5 };
    const Register Assembler::retDregs[] = { _D0 };

    const char *regNames[] = {
        "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6", "r7",
        "r8",  "r9",  "r10", "r11", "r12", "r13", "fp", "sp",
        "d0",  "d0~",  "d1",  "d1~",  "d2",  "d2~",  "d3", "d3~",
        "d4",  "d4~",  "d5",  "d5~",  "d6",  "d6~",  "d7", "d7~"
    };

    struct pool Assembler::current_pool;

    inline void Assembler::SH4_emit16(uint16_t value) {
        _nIns--;
        NanoAssert((uintptr_t)_nIns >= (uintptr_t)codeStart);
        *((uint16_t*)_nIns) = value;
    }

    inline void Assembler::SH4_emit32(uint32_t value) {
        _nIns -= 2;
        NanoAssert((uintptr_t)_nIns >= (uintptr_t)codeStart);
        *((uint32_t *)(void *)_nIns) = value;
    }

#include "NativeSH4-auto-generated.h"

#define SH4_movl_PCrel(address, reg)                                    \
    SH4_movl_dispPC(((uint32_t)(address) - (((uint32_t)(_nIns + 1)) & ~0x3)), reg)

#define SH4_LABEL(target) (int32_t)((uint32_t)(target) - (uint32_t)(_nIns) - 2)

#define FP_RELATIVE(offset) (STATIC_FP + (offset))

    void Assembler::nativePageReset() { }

    void Assembler::nativePageSetup() {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
        current_pool.nb_slots = 0;
        current_pool.slots = NULL;
    }

    void Assembler::MR(Register dest, Register src) {
        underrunProtect(1 * sizeof(NIns));
        SH4_mov(src, dest);
    }

    void Assembler::JMP(NIns *target, bool from_underrunProtect) {
        // Take the opportunity of this unconditional branch to
        // pre-allocate space for constants right after.
        asm_generate_pool(0);

        underrunProtect(7 * sizeof(NIns));

        if (target != NULL && FITS_SH4_bra(SH4_LABEL(target + 1 * sizeof(NIns)))) {
            // We can reach the target with a "bra".
            SH4_nop();
            SH4_bra(SH4_LABEL(target));
        }
        else {
            // Don't use asm_immi() here since the address will likely
            // land in a constant slot, this allows to store the
            // constant right after the unconditional jump.

            NIns *target_addr;

            // 5. align the constant with nop
            if (((uint32_t)_nIns & 0x3) != 0)
                SH4_nop();

            // 4. constant slot
            SH4_emit32((uint32_t)target);
            target_addr = _nIns;

            // 3. restore Rtemp from the stack during the delay slot 
            // (if needed, see the last comment of this function).
            if (from_underrunProtect)
                SH4_movl_incRy(SP, Rtemp);
            else
                SH4_nop();
                
            // 3. Jump to the target.
            SH4_jmp_indRx(Rtemp);

            // 2. @(PC + offset) -> reg
            SH4_movl_PCrel(target_addr, Rtemp);
            
            // When compiling the following code, where XXX is "large":
            //
            //     array[XXX] = YYY
            //
            // The SH4 backend will produce the native code in two steps:
            //
            //     1.   mov.l @(pc, ZZZ), Rtemp
            //     2a.  add FP, Rtemp
            //     2b.  mov.l Rsrc, @Rtemp
            //
            // If the code buffer is full right after the first step,
            // it will add the [in]famous glue JMP:
            //
            //     1.   mov.l @(pc, ZZZ), Rtemp
            //          mov.l @(pc, TARGET), Rtemp    # Technically Rtemp is now the address of 2a.
            //          jmp @Rtemp                    # Jump to 2a.
            //     2a.  add FP, Rtemp
            //     2b.  mov.l Rsrc, @Rtemp
            //
            // As you can see, we have to keep/restore the value of
            // Rtemp when called from underrunProtect().
            if (from_underrunProtect)
                SH4_movl_decRx(Rtemp, SP);
        }
    }

    void Assembler::asm_arith(LIns *inst) {
        LOpcode opcode = inst->opcode();
        LIns *operand1 = inst->oprnd1();
        LIns *operand2 = inst->oprnd2();

        RegisterMask allow = GpRegs;

        Register result_reg = prepareResultReg(inst, allow);
        allow &= ~rmask(result_reg);

        Register operand1_reg = findRegFor(operand1, allow);
        allow &= ~rmask(operand1_reg);

        underrunProtect(7 * sizeof(NIns));

        // Special case on SH4: the instruction "add" is the only
        // arithmetic operation that can use an [small] immediate.
        if (operand2->isImmI() &&
            (inst->opcode() == LIR_addi || inst->opcode() == LIR_subi)) {
            int immI = operand2->immI();

            if (inst->opcode() == LIR_subi)
                immI = -immI;

            if (FITS_SH4_add_imm(immI)) {
                // 2. operand2 + result -> result
                SH4_add_imm(immI, result_reg);

                // 1. operand1 -> result
                if (result_reg != operand1_reg)
                    SH4_mov(operand1_reg, result_reg);

                freeResourcesOf(inst);
                return;
            }
        }

        Register operand2_reg = inst->oprnd1() == inst->oprnd2()
                                ? operand1_reg
                                : findRegFor(operand2, allow);

        // 2. operand2 OP result -> result
        NanoAssert(result_reg != Rtemp && operand1_reg != Rtemp && operand2_reg != Rtemp);
        switch (opcode) {
        case LIR_rshi:
            SH4_shad(Rtemp, result_reg);
            SH4_neg(Rtemp, Rtemp);
            SH4_and(operand2_reg, Rtemp);
            SH4_mov_imm(0x1f, Rtemp);
            break;

        case LIR_rshui:
            SH4_shld(Rtemp, result_reg);
            SH4_neg(Rtemp, Rtemp);
            SH4_and(operand2_reg, Rtemp);
            SH4_mov_imm(0x1f, Rtemp);
            break;

        case LIR_muli:
            SH4_sts_MACL(result_reg);
            SH4_mull(operand2_reg, result_reg);
            break;

        case LIR_muljovi:
        case LIR_mulxovi:
            // 3. Now the T-bit is equal to "sign_of(result_reg) == sign_of(operand1) XOR sign_of(operand2)".
            SH4_shll(Rtemp);
            SH4_xor(result_reg, Rtemp);

            // 2. Perform the actual multiplication.
            SH4_sts_MACL(result_reg);
            SH4_dmulsl(operand2_reg, result_reg);

            // 1. Now Rtemp[31] is equal to "sign_of(operand1) XOR sign_of(operand2)".
            SH4_xor(operand2_reg, Rtemp);
            SH4_mov(result_reg, Rtemp);
            break;

        case LIR_lshi:   SH4_shad(operand2_reg, result_reg); break;
        case LIR_ori:    SH4_or(operand2_reg, result_reg);  break;
        case LIR_subi:   SH4_sub(operand2_reg, result_reg); break;
        case LIR_addi:   SH4_add(operand2_reg, result_reg); break;
        case LIR_andi:   SH4_and(operand2_reg, result_reg); break;
        case LIR_xori:   SH4_xor(operand2_reg, result_reg); break;

        case LIR_subjovi:
        case LIR_subxovi: SH4_subv(operand2_reg, result_reg); break;

        case LIR_addjovi:
        case LIR_addxovi: SH4_addv(operand2_reg, result_reg); break;
        default:
            NanoAssertMsg(0, "asm_arith does not support this LIR opcode yet");
        }

        // 1. operand1 -> result
        if (result_reg != operand1_reg)
            SH4_mov(operand1_reg, result_reg);

        freeResourcesOf(inst);
    }

    void Assembler::asm_arg_regi(LIns *arg, Register reg) {
        if (arg->isExtant()) {
            if (arg->isInReg()) {
                // This argument is assigned to a register, thus we
                // just have to copy it to the right register.
                underrunProtect(1 * sizeof(NIns));
                SH4_mov(arg->getReg(), reg);
            }
            else {
                // This argument is not assigned to a register, thus
                // we have to get its frame address first and then
                // copy it to the right register.
                int offset = findMemFor(arg);

                if (arg->isop(LIR_allocp)) {
                    // Load the address of the argument, not its value.
                    underrunProtect(1 * sizeof(NIns));
                    offset = FP_RELATIVE(offset);
                    SH4_add(FP, reg);      // 2. arg_reg + FP -> arg_reg
                    asm_immi(offset, reg); // 1. offset -> arg_reg
                }
                else {
                    // Load the value of the argument.
                    asm_load32i(offset, FP, reg);
                }
            }
        }
        else {
            // This is the last use of "arg", so fine to
            // assign it to the right register immediately.
            findSpecificRegFor(arg, reg);
        }
    }

    void Assembler::asm_arg_regd(LIns *arg, Register reg) {
        if (arg->isExtant()) {
            if (arg->isInReg()) {
                // This argument is assigned to a register, thus we
                // just have to copy it to the right register.
                underrunProtect(2 * sizeof(NIns));
                SH4_fmov(arg->getReg(), reg);
                SH4_fmov(arg->getReg() + 1, reg + 1);
            }
            else {
                // This argument is not assigned to a register, thus
                // we have to get its frame address first and then
                // copy it to the right register.
                int offset = findMemFor(arg);
                asm_load64d(offset, FP, reg);
            }
        }
        else {
            // This is the last use of "arg", so fine to
            // assign it to the right register immediately.
            findSpecificRegFor(arg, reg);
        }
    }

    void Assembler::asm_arg_stacki(LIns *arg, int used_stack) {
        if (arg->isExtant() && arg->isInReg()) {
            Register result_reg = arg->getReg();
            // This argument is assigned to a register, thus we just
            // have to copy it to the right stack slot.
            asm_store32i(result_reg, used_stack, SP);
        }
        else {
            // This argument is not assigned to a register, thus we
            // have to get its frame address first and then copy it to
            // the right stack slot.
            int offset   = findMemFor(arg);
            Register reg = findRegFor(arg, GpRegs);

            asm_store32i(reg, used_stack, SP);

            if (arg->isop(LIR_allocp)) {
                // Load the address of the argument, not its value.
                underrunProtect(1 * sizeof(NIns));
                offset = FP_RELATIVE(offset);
                SH4_add(FP, reg);      // 2. arg_reg + FP -> arg_reg
                asm_immi(offset, reg); // 1. offset -> arg_reg
            }
            else {
                // Load the value of the argument.
                asm_load32i(offset, FP, reg);
            }
        }
    }

    void Assembler::asm_arg_stackd(LIns *arg, int used_stack) {
        if (arg->isExtant() && arg->isInReg()) {
            Register result_reg = arg->getReg();
            // This argument is assigned to a register, thus we just
            // have to copy it to the right stack slot.
            asm_store64d(result_reg, used_stack, SP);
        }
        else {
            // This argument is not assigned to a register, thus we
            // have to get its frame address first and then copy it to
            // the right stack slot.
            int offset   = findMemFor(arg);
            Register reg = findRegFor(arg, FpRegs);

            asm_store64d(reg, used_stack, SP);
            asm_load64d(offset, FP, reg);
        }
    }

    void Assembler::asm_call(LIns *inst) {
        if (!inst->isop(LIR_callv)) {
            Register result_reg = inst->isop(LIR_calld) ? retDregs[0] : retRegs[0];
            prepareResultReg(inst, rmask(result_reg));

            // Do this after we've handled the call result, so we don't
            // force the call result to be spilled unnecessarily.
            evictScratchRegsExcept(rmask(result_reg));
        } else {
            evictScratchRegsExcept(0);
        }
        ArgType types[MAXARGS];
        const CallInfo* call = inst->callInfo();
        uint32_t argc = call->getArgTypes(types);
        bool indirect = call->isIndirect();

        // Emit the branch.
        if (!indirect) {
            NIns *target = (NIns*)call->_address;

            underrunProtect(2 * sizeof(NIns));
            if (FITS_SH4_bsr(SH4_LABEL(target + 1 * sizeof(NIns)))) {
                // We can reach the target with a "bsr".
                SH4_nop();
                SH4_bsr(SH4_LABEL(target));
            }
            else {
                // General case.
                SH4_nop();
                SH4_jsr_indRx(Rtemp);         // 2. jump to target
                asm_immi((int)target, Rtemp); // 1. load target
            }
            // Call this now so that the argument setup can involve 'result_reg'.
            freeResourcesOf(inst);
        }
        else {
            // Indirect call: we assign the address arg to R0 since it's not
            // used for regular arguments, and is otherwise scratch since it's
            // clobberred by the call.
            underrunProtect(2 * sizeof(NIns));

            SH4_nop();
            SH4_jsr_indRx(R0);

            // Call this now so that the argument setup can involve 'result_reg'.
            freeResourcesOf(inst);

            argc--;
            asm_arg_regi(inst->arg(argc), R0);
        }

        // Emit the arguments.
        int ireg_index = 0;
        int dreg_index = 0;
        int used_stack = 0;

        for (int i = argc - 1; i >= 0; i--) {
            switch(types[i]) {
            case ARGTYPE_I:  // int32_t
            case ARGTYPE_UI: // uint32_t
                if (ireg_index < NumArgRegs) {
                    // Arguments are [still] stored into registers.
                    asm_arg_regi(inst->arg(i), argRegs[ireg_index]);
                    ireg_index++;
                }
                else {
                    // Arguments are [now] stored into the stack.
                    asm_arg_stacki(inst->arg(i), used_stack);
                    used_stack += sizeof(int);
                }
                break;

            case ARGTYPE_D: // double (64bit)
                if (dreg_index < NumArgDregs) {
                    // Arguments are [still] stored into registers.
                    asm_arg_regd(inst->arg(i), argDregs[dreg_index]);
                    dreg_index++;
                }
                else {
                    // Arguments are [now] stored into the stack.
                    asm_arg_stackd(inst->arg(i), used_stack);
                    used_stack += sizeof(double);
                }
                break;

            default:
                NanoAssertMsg(0, "asm_call does not support this size of argument yet");
            }
        }

        if (used_stack > max_stack_args)
            max_stack_args = used_stack;
    }

    void Assembler::asm_cmov(LIns *inst) {
        LIns* condition = inst->oprnd1();
        LIns* if_true   = inst->oprnd2();
        LIns* if_false  = inst->oprnd3();

        NanoAssert(condition->isCmp());
        NanoAssert(   (inst->isop(LIR_cmovi) && if_true->isI() && if_false->isI())
                   || (inst->isop(LIR_cmovd) && if_true->isD() && if_false->isD()));

        RegisterMask allow = inst->isD() ? FpRegs : GpRegs;

        Register dest_reg = prepareResultReg(inst, allow);

        // Try to re-use the result register for one of the arguments.
        Register src_true_reg  = if_true->isInReg()  ? if_true->getReg()  : dest_reg;
        Register src_false_reg = if_false->isInReg() ? if_false->getReg() : dest_reg;

        // Note that iftrue and iffalse may actually be the same, though
        // it shouldn't happen with the LIR optimizers turned on.
        if (src_true_reg == src_false_reg && if_true != if_false) {
            // We can't re-use the result register for both arguments,
            // so force one into its own register.
            src_false_reg = findRegFor(if_false, allow & ~rmask(dest_reg));
            NanoAssert(if_false->isInReg());
        }

        underrunProtect(6 * sizeof(NIns));
        // 3. If the test is "true", copy the "true" source register.
        NIns *after_mov_true = _nIns;
        if (inst->isop(LIR_cmovd)) {
            SH4_fmov(src_true_reg, dest_reg);
            SH4_fmov(src_true_reg + 1, dest_reg + 1);
        }
        else {
            SH4_mov(src_true_reg, dest_reg);
        }        

        // 2. If the test is "false", copy the "false" source register
        // then jump over the "mov if true".
        NIns *after_mov_false = _nIns;

        // Take the opportunity of this unconditional branch to
        // pre-allocate space for constants right after.  The size is
        // limited to 50 words because of the limitation of the
        // instruction bf/t: branch at most 256 bytes far away.
        //
        //     pool size                  =  50 * sizeof(word) = 200 bytes
        //   + pool alignement            =  ~1 * sizeof(half) =   2 bytes
        //   + instructions below         =  ~5 * sizeof(NIns) =  10 bytes
        //   + instructions in asm_branch = ~10 * sizeof(NIns) =  20 bytes
        //                                               total = 232 bytes
        //
        // Note asm_generate_pool() prevents any code buffer
        // allocation so there is no need for underrunProtect(),
        // however we still have to reserve space for the instructions
        // below.
        asm_generate_pool(4 * sizeof(sizeof(NIns)), 50);

        SH4_nop();
        SH4_bra(SH4_LABEL(after_mov_true));

        if (inst->isop(LIR_cmovd)) {
            SH4_fmov(src_false_reg, dest_reg);
            SH4_fmov(src_false_reg + 1, dest_reg + 1);
        }
        else {
            SH4_mov(src_false_reg, dest_reg);
        }

        freeResourcesOf(inst);

        // If we re-used the result register, mark it as active for either if_true
        // or if_false (or both in the corner-case where they're the same).
        if (src_true_reg == dest_reg) {
            NanoAssert(!if_true->isInReg());
            findSpecificRegForUnallocated(if_true, dest_reg);
        } else if (src_false_reg == dest_reg) {
            NanoAssert(!if_false->isInReg());
            findSpecificRegForUnallocated(if_false, dest_reg);
        } else {
            NanoAssert(if_false->isInReg());
            NanoAssert(if_true->isInReg());
        }

        // 1. Branch [or not] according to the condition.
        asm_branch(false, condition, after_mov_false);
    }

    void Assembler::asm_cond(LIns *inst) {
        Register result_reg = prepareResultReg(inst, GpRegs);
        LOpcode opcode = inst->opcode();

        // 2. Get the T-bit.
        underrunProtect(3 * sizeof(NIns));
        SH4_movt(result_reg);

        // Inverse the T-bit with a small trick.
        bool negated = simplifyOpcode(opcode);
        if (negated) {
            SH4_tst(result_reg, result_reg);
            SH4_movt(result_reg);
        }

        freeResourcesOf(inst);

        // 1. Do the comparison
        asm_cmp(opcode, inst);
    }

    void Assembler::asm_condd(LIns *inst) {
        asm_cond(inst);
    }

    void Assembler::asm_fneg(LIns *inst) {
        Register result_reg  = prepareResultReg(inst, FpRegs);
        Register operand_reg = findRegFor(inst->oprnd1(), FpRegs);

        // 2. -result -> result
        underrunProtect(1 * sizeof(NIns));
        SH4_fneg_double(result_reg);

        // 1. operand -> result
        if (result_reg != operand_reg) {
            underrunProtect(2 * sizeof(NIns));
            SH4_fmov(operand_reg, result_reg);
            SH4_fmov(operand_reg + 1, result_reg + 1);
        }

        freeResourcesOf(inst);
    }

    void Assembler::asm_fop(LIns *inst) {
        LOpcode opcode = inst->opcode();
        LIns *operand1 = inst->oprnd1();
        LIns *operand2 = inst->oprnd2();

        RegisterMask allow = FpRegs;

        Register result_reg = prepareResultReg(inst, allow);
        allow &= ~rmask(result_reg);

        Register operand1_reg = findRegFor(operand1, allow);
        allow &= ~rmask(operand1_reg);

        Register operand2_reg = inst->oprnd1() == inst->oprnd2()
                                ? operand1_reg
                                : findRegFor(operand2, allow);

        underrunProtect(3 * sizeof(NIns));

        // 2. operand2 OP result -> result
        switch (opcode) {
        case LIR_addd: SH4_fadd_double(operand2_reg, result_reg); break;
        case LIR_subd: SH4_fsub_double(operand2_reg, result_reg); break;
        case LIR_muld: SH4_fmul_double(operand2_reg, result_reg); break;
        case LIR_divd: SH4_fdiv_double(operand2_reg, result_reg); break;
        default:
            NanoAssertMsg(0, "asm_fop does not support this LIR opcode yet");
        }

        // 1. operand1 -> result
        if (result_reg != operand1_reg) {
            SH4_fmov(operand1_reg, result_reg);
            SH4_fmov(operand1_reg + 1, result_reg + 1);
        }

        freeResourcesOf(inst);
    }

    void Assembler::asm_d2i(LIns *inst) {
        Register result_reg   = prepareResultReg(inst, GpRegs);
        Register operand1_reg = findRegFor(inst->oprnd1(), FpRegs);

        underrunProtect(2 * sizeof(NIns));
        SH4_sts_FPUL(result_reg);
        SH4_ftrc_double_FPUL(operand1_reg);

        freeResourcesOf(inst);
    }

    void Assembler::asm_i2d(LIns *inst) {
        Register result_reg   = prepareResultReg(inst, FpRegs);
        Register operand1_reg = findRegFor(inst->oprnd1(), GpRegs);

        underrunProtect(2 * sizeof(NIns));
        SH4_float_FPUL_double(result_reg);
        SH4_lds_FPUL(operand1_reg);

        freeResourcesOf(inst);
    }

    void Assembler::asm_ui2d(LIns *inst) {
        Register result_reg   = prepareResultReg(inst, FpRegs);
        Register operand1_reg = findRegFor(inst->oprnd1(), GpRegs);

        underrunProtect(SH4_IMMD_NOCHK_SIZE + 5 * sizeof(NIns));
        NIns *over_conversion = _nIns;

        // 3. Otherwise adjust the result.
        SH4_fadd(Dtemp, result_reg);
        asm_immd_nochk(0x41F0000000000000LLU, Dtemp); // It does the trick™.

        // 2. Done if it was a positif integer.
        SH4_bt(SH4_LABEL(over_conversion));
        SH4_cmppz(operand1_reg);

        // 1. Convert the *signed* integer.
        SH4_float_FPUL_double(result_reg);
        SH4_lds_FPUL(operand1_reg);

        freeResourcesOf(inst);
    }

    void Assembler::asm_immi(LIns *inst) {
        Register result_reg = prepareResultReg(inst, GpRegs);
        asm_immi(inst->immI(), result_reg);
        freeResourcesOf(inst);
    }

    void Assembler::asm_base_offset(int offset, Register base_reg) {
        underrunProtect(2 * sizeof(NIns));

        if (base_reg == FP)
            offset = FP_RELATIVE(offset);

        if (offset != 0) {
            if (FITS_SH4_add_imm(offset)) {
                SH4_add_imm(offset, Rtemp);
                if (base_reg != Rtemp)
                    SH4_mov(base_reg, Rtemp);
            }
            else {
                NanoAssert(base_reg != Rtemp);
                SH4_add(base_reg, Rtemp); // b. base + temp -> temp
                asm_immi(offset, Rtemp);  // a. offset      -> temp
            }
        }
        else {
            if (base_reg != Rtemp)
                SH4_mov(base_reg, Rtemp);
        }
    }

    void Assembler::asm_load64d(int offset, Register base_reg, Register result_reg) {
        underrunProtect(2 * sizeof(NIns));

        // 2. Load the "double" from @Rtemp.
        SH4_fmovs_indRy(Rtemp, result_reg);
        SH4_fmovs_incRy(Rtemp, result_reg + 1);

        // 1. base + offset -> Rtemp.
        asm_base_offset(offset, base_reg);
    }

    void Assembler::asm_load32d(int offset, Register base_reg, Register result_reg) {
        underrunProtect(3 * sizeof(NIns));

        // 3. Convert to double precision.
        SH4_fcnvsd_FPUL_double(result_reg);
        SH4_flds_FPUL(Dtemp);

        // 2. Load the "float" from @Rtemp.
        SH4_fmovs_indRy(Rtemp, Dtemp);

        // 1. base + offset -> Rtemp.
        asm_base_offset(offset, base_reg);
    }

    void Assembler::asm_load32i(int offset, Register base_reg, Register result_reg) {
        // Rtemp is mandatory to handle the "worst" case.
        NanoAssert(result_reg != Rtemp);

        if (base_reg == FP)
            offset = FP_RELATIVE(offset);

        if (offset == 0) {
            // No displacement.
            underrunProtect(1 * sizeof(NIns));
            SH4_movl_indRy(base_reg, result_reg);
        }
        else if (FITS_SH4_movl_dispRy(offset)) {
            // The displacement fits the constraints of this instruction.
            underrunProtect(1 * sizeof(NIns));
            SH4_movl_dispRy(offset, base_reg, result_reg);
        }
        else {
            // General case.
            if ( result_reg == R0
                && base_reg != R0) {
                // Optimize when R0 is clobbered and not used as the base.
                underrunProtect(1 * sizeof(NIns));
                SH4_movl_dispR0Ry(base_reg, result_reg); // 2. @(R0 + base) -> R0
                asm_immi(offset, R0);                    // 1. offset       -> R0
            }
            else {
                // Worst case.
                underrunProtect(2 * sizeof(NIns));
                SH4_movl_indRy(Rtemp, result_reg); // 3. @(temp)     -> result
                SH4_add(base_reg, Rtemp);          // 2. base + temp -> temp
                asm_immi(offset, Rtemp);           // 1. offset      -> temp
            }
        }
    }

#define gen_asm_loadXXi(size, length)                                   \
    void Assembler::asm_load ## size ## i(int offset, Register base_reg, Register result_reg, bool sign_extend) { \
        underrunProtect(5 * sizeof(NIns));                              \
                                                                        \
        if (!sign_extend) {                                             \
            SH4_extu ## length(result_reg, result_reg);                 \
        }                                                               \
                                                                        \
        if (base_reg == FP)                                             \
            offset = FP_RELATIVE(offset);                               \
                                                                        \
        if (offset == 0) {                                              \
            /* No displacement. */                                      \
            SH4_mov ## length ## _indRy(base_reg, result_reg);          \
        }                                                               \
        else if (FITS_SH4_mov ## length ## _dispRy_R0(offset)) {        \
            /* The displacement is small enough to fit into the */      \
            /* special #size bits load "@(base + offset) -> R0". */     \
            if (result_reg == R0) {                                     \
                /* We are lucky, the result is R0. */                   \
                SH4_mov ## length ## _dispRy_R0(offset, base_reg); /* @(base + offset) -> R0 */ \
            }                                                           \
            else {                                                      \
                /* We have to save and restore R0. */                   \
                SH4_mov(Rtemp, R0);                                /* 4. Rtemp -> R0            */ \
                SH4_mov(R0, result_reg);                           /* 3. R0 -> result           */ \
                SH4_mov ## length ## _dispRy_R0(offset, base_reg); /* 2. @(base + offset) -> R0 */ \
                SH4_mov(R0, Rtemp);                                /* 1. R0 -> Rtemp            */ \
            }                                                           \
        }                                                               \
        else {                                                          \
            /* Worst case. */                                           \
            SH4_mov ## length ## _indRy(Rtemp, result_reg); /* 3. @(temp)     -> result */ \
            SH4_add(base_reg, Rtemp);                       /* 2. base + temp -> temp   */ \
            asm_immi(offset, Rtemp);                        /* 1. offset      -> temp   */ \
        }                                                               \
    }

    gen_asm_loadXXi(16, w)
    gen_asm_loadXXi(8, b)

    void Assembler::asm_load32(LIns *inst) {
        LIns* base = inst->oprnd1();
        int offset = inst->disp();

        Register result_reg = prepareResultReg(inst, GpRegs);
        Register base_reg   = getBaseReg(base, offset, GpRegs);

        switch (inst->opcode()) {
        case LIR_lduc2ui:
            asm_load8i(offset, base_reg, result_reg, false);
            break;
        case LIR_ldc2i:
            asm_load8i(offset, base_reg, result_reg, true);
            break;
        case LIR_ldus2ui:
            asm_load16i(offset, base_reg, result_reg, false);
            break;
        case LIR_lds2i:
            asm_load16i(offset, base_reg, result_reg, true);
            break;
        case LIR_ldi:
            asm_load32i(offset, base_reg, result_reg);
            break;
        default:
            NanoAssertMsg(0, "asm_load32 should never receive this LIR opcode");
        }
        freeResourcesOf(inst);
    }

    void Assembler::asm_load64(LIns *inst) {
        LIns* base = inst->oprnd1();
        int offset = inst->disp();

        Register result_reg = prepareResultReg(inst, FpRegs);
        Register base_reg   = getBaseReg(base, offset, GpRegs);

        switch (inst->opcode()) {
        case LIR_ldd:
            asm_load64d(offset, base_reg, result_reg);
            break;
        case LIR_ldf2d:
            asm_load32d(offset, base_reg, result_reg);
            break;
        default:
            NanoAssertMsg(0, "asm_load64 should never receive this LIR opcode");
        }

        freeResourcesOf(inst);
    }

    void Assembler::asm_neg_not(LIns *inst) {
        // TODO: Try to use the same register.
        Register result_reg  = prepareResultReg(inst, GpRegs);
        Register operand_reg = findRegFor(inst->oprnd1(), GpRegs);

        underrunProtect(1 * sizeof(NIns));
        if (inst->isop(LIR_negi))
            SH4_neg(operand_reg, result_reg);
        else
            SH4_not(operand_reg, result_reg);

        freeResourcesOf(inst);
    }

    void Assembler::asm_nongp_copy(Register dest_reg, Register src_reg) {
        NanoAssert(IsFpReg(dest_reg) && IsFpReg(src_reg));

        underrunProtect(2 * sizeof(NIns));
        SH4_fmov(src_reg, dest_reg);
        SH4_fmov(src_reg + 1, dest_reg + 1);
    }

    void Assembler::asm_param(LIns *inst) {
        int arg_number = inst->paramArg();
        int kind = inst->paramKind();

        if (kind == 0) {
            // Ordinary parameter.
            if (arg_number < NumArgRegs) {
                // The incoming parameter is in register.
                prepareResultReg(inst, rmask(argRegs[arg_number]));
            }
            else {
                // The incoming parameter is on stack, and FP points
                // nearby. Keep in sync' with genPrologue().
                Register reg = prepareResultReg(inst, GpRegs);
                int offset   = (arg_number - NumArgRegs) * sizeof(intptr_t)
                               + 2 * sizeof(int); /* saved PR and saved SP. */
                asm_load32i(offset, FP, reg);
            }
        }
        else {
            // Saved parameter.
            prepareResultReg(inst, rmask(savedRegs[arg_number]));
            // No code to generate.
        }
        freeResourcesOf(inst);
    }

    // Keep in sync' with SH4_IMMD_NOCHK_SIZE.
    void Assembler::asm_immd_nochk(uint64_t value, Register result_reg) {
            NIns *over_constant = NULL;

            // Sanity check.
            NanoAssert((uintptr_t)_nIns - SH4_IMMD_NOCHK_SIZE >= (uintptr_t)codeStart);
            NanoAssert(result_reg != Rtemp);

            // 8. Load the "double" constant from @Rtemp.
            SH4_fmovs_indRy(Rtemp, result_reg);
            SH4_fmovs_incRy(Rtemp, result_reg + 1);

            // 7. Restore R0 from the stack.
            SH4_movl_incRy(SP, R0);

            // 6. Get the address of the constant
            SH4_mov(R0, Rtemp);
            over_constant = _nIns;

            // 5. align the constant with nop
            if (((uint32_t)_nIns & 0x3) != 0)
                SH4_nop();

            // 4. constant64
            SH4_emit32((uint32_t)(value >> 32));
            SH4_emit32((uint32_t)(value & 0x00000000FFFFFFFFLLU));

            // 3. branch over the constant
            SH4_nop();
            SH4_bra(SH4_LABEL(over_constant));

            // 2. Compute the address of the constant.
            SH4_mova_dispPC_R0(2 * sizeof(NIns));

            // 1. Save R0 onto the stack since we need it soon.
            SH4_movl_decRx(R0, SP);
    }

    void Assembler::asm_immd(uint64_t value, Register result_reg) {
        underrunProtect(SH4_IMMD_NOCHK_SIZE);
        asm_immd_nochk(value, result_reg);
    }

    void Assembler::asm_immd(LIns *inst) {
        Register result_reg = prepareResultReg(inst, FpRegs);
        asm_immd(inst->immDasQ(), result_reg);
        freeResourcesOf(inst);
    }

    bool Assembler::canRemat(LIns* inst) {
        return inst->isImmI() || inst->isop(LIR_allocp);
    }

    void Assembler::asm_restore(LIns *inst, Register reg) {
        if (inst->isop(LIR_allocp)) {
            int offset = FP_RELATIVE(arDisp(inst));

            underrunProtect(2 * sizeof(NIns));
            if (FITS_SH4_add_imm(offset)){
                SH4_add_imm(offset, reg); // 2. reg + offset -> reg
                SH4_mov(FP, reg);         // 1. FP -> reg
            }
            else {
                SH4_add(FP, reg);      // 2. FP + reg -> reg
                asm_immi(offset, reg); // 1. offset   -> reg
            }
        }
        else if (inst->isImmI()) {
            asm_immi(inst->immI(), reg);
        }
        else {
            int offset = findMemFor(inst);
            if (IsGpReg(reg)) {
                asm_load32i(offset, FP, reg);
            }
            else {
                asm_load64d(offset, FP, reg);
            }
        }
    }

    NIns* Assembler::genEpilogue() {
        // Take the opportunity of this epilogue to pre-allocate space
        // for constants right after.
        asm_generate_pool(0);

        underrunProtect(6 * sizeof(NIns));

        // 3. Perform a return to the caller using the return address.
        SH4_nop();
        SH4_rts();

        // 2. Restore the previous frame pointer.
        SH4_movl_incRy(SP, FP);

        // 1. Restore the return address.
        SH4_ldsl_incRx_PR(SP);

        // 0. Restore the stack pointer.
        SH4_add_imm(STATIC_FP, SP);
        SH4_mov(FP, SP);

        return _nIns;
    }

    void Assembler::asm_ret(LIns* inst) {
        genEpilogue();
        releaseRegisters();
        assignSavedRegs();

        LIns *value = inst->oprnd1();

        Register reg = inst->isop(LIR_retd) ? retDregs[0] : retRegs[0];
        findSpecificRegFor(value, reg);
    }

    void Assembler::asm_spill(Register reg, int offset, bool quad) {
        (void)quad;

        if (offset == 0)
            return; // Nothing to spill.

        if (IsGpReg(reg)) {
            NanoAssert(!quad);
            asm_store32i(reg, offset, FP);
        }
        else {
            NanoAssert(quad);
            asm_store64d(reg, offset, FP);
        }
    }

#define MIN_IMM -128
#define MAX_IMM 127

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

    // Keep in sync' with SH4_IMMI_NOCHK_SIZE.
    void Assembler::asm_immi_nochk(int constant, Register reg, bool force) {
        // Build the constant by adding several smaller constants that
        // fit into the [very small] immediate field of SH4 instructions.
        if (!force
            && constant >= 2 * MIN_IMM
            && constant <= 2 * MAX_IMM) {
            int tmp;

            while (constant < MIN_IMM || constant > MAX_IMM) {
                if (constant < 0)
                    tmp = MAX(constant, MIN_IMM);
                else
                    tmp = MIN(constant, MAX_IMM);

                SH4_add_imm(tmp, reg);  // 2. tmp + Rx -> Rx
                constant -= tmp;
            }
            SH4_mov_imm(constant, reg); // 1. tmp -> Rx
        }
        // Otherwise use a constant stored in the instruction stream.
        else {
            NIns *over_constant = _nIns;
            NIns *constant_addr = NULL;

            // Try to [re-]use a slot from the current pool.
            if (!force && asm_load_constant(constant, reg))
                return;

            // 4. align the constant with nop
            if (((uint32_t)_nIns & 0x3) != 0)
                SH4_nop();

            // 3. constant32
            SH4_emit32(constant);
            constant_addr = _nIns;

            // 2. branch over the constant
            SH4_nop();
            SH4_bra(SH4_LABEL(over_constant));

            // 1. @(PC + offset) -> reg
            SH4_movl_PCrel(constant_addr, reg);
        }
    }

#undef MIN_IMM
#undef MAX_IMM

    void Assembler::asm_immi(int constant, Register reg, bool force) {
        underrunProtect(SH4_IMMI_NOCHK_SIZE);
        asm_immi_nochk(constant, reg, force);
    }

    void Assembler::asm_store32i(Register value_reg, int offset, Register base_reg) {
        // Rtemp is mandatory to handle the "worst" case.
        NanoAssert(value_reg != Rtemp && base_reg != Rtemp);

        if (base_reg == FP)
            offset = FP_RELATIVE(offset);

        if (offset == 0) {
            // No displacement.
            underrunProtect(1 * sizeof(NIns));
            SH4_movl_indRx(value_reg, base_reg);
        }
        else if (FITS_SH4_movl_dispRx(offset)) {
            // The displacement fits the constraints of this instruction.
            underrunProtect(1 * sizeof(NIns));
            SH4_movl_dispRx(value_reg, offset, base_reg);
        }
        else {
            // General & worst case.
            underrunProtect(2 * sizeof(NIns));
            SH4_movl_indRx(value_reg, Rtemp); // 3. value       -> @(temp)
            SH4_add(base_reg, Rtemp);         // 2. base + temp -> temp
            asm_immi(offset, Rtemp);          // 1. offset      -> temp
        }
    }

#define gen_asm_storeXXi(size, length)                                  \
    void Assembler::asm_store ## size ## i(Register value_reg, int offset, Register base_reg) { \
        if (base_reg == FP)                                             \
            offset = FP_RELATIVE(offset);                               \
                                                                        \
        if (offset == 0) {                                              \
            /* No displacement. */                                      \
            underrunProtect(1 * sizeof(NIns));                          \
            SH4_mov ## length ## _indRx(value_reg, base_reg);           \
        }                                                               \
        else if (FITS_SH4_mov ## length ## _R0_dispRx(offset)) {        \
            /* The displacement is small enough to fit into the */      \
            /* special #size bits store "R0 -> @(base + offset)". */    \
            if (value_reg == R0) {                                      \
                /* We are lucky, the value is R0. */                    \
                underrunProtect(1 * sizeof(NIns));                      \
                SH4_mov ## length ## _R0_dispRx(offset, base_reg); /* R0 -> @(base + offset) */ \
            } else {                                                    \
                /* We have to save and restore R0. */                   \
                underrunProtect(4 * sizeof(NIns));                      \
                SH4_mov(Rtemp, R0);                                /* 4. Rtemp -> R0            */ \
                SH4_mov ## length ## _R0_dispRx(offset, base_reg); /* 3. R0 -> @(base + offset) */ \
                SH4_mov(value_reg, R0);                            /* 2. value -> R0            */ \
                SH4_mov(R0, Rtemp);                                /* 1. R0 -> Rtemp            */ \
            }                                                           \
        }                                                               \
        else {                                                          \
            /* Worst case. */                                           \
            underrunProtect(2 * sizeof(NIns));                          \
            SH4_mov ## length ## _indRx(value_reg, Rtemp); /* 3. value -> @(temp)    */ \
            SH4_add(base_reg, Rtemp);                      /* 2. base + temp -> temp */ \
            asm_immi(offset, Rtemp);                       /* 1. offset -> temp      */ \
        }                                                               \
    }

    gen_asm_storeXXi(16, w)
    gen_asm_storeXXi(8, b)

    void Assembler::asm_store32(LOpcode opcode, LIns *value, int offset, LIns *base) {
        Register value_reg;
        Register base_reg;

        getBaseReg2(GpRegs, value, value_reg, GpRegs, base, base_reg, offset);

        switch (opcode) {
        case LIR_sti:
            asm_store32i(value_reg, offset, base_reg);
            break;
        case LIR_sti2s:
            asm_store16i(value_reg, offset, base_reg);
            break;
        case LIR_sti2c:
            asm_store8i(value_reg, offset, base_reg);
            break;
        default:
            NanoAssertMsg(0, "asm_store32 should never receive this LIR opcode");
            return;
        }
    }

    void Assembler::asm_store64d(Register value_reg, int offset, Register base_reg) {
        underrunProtect(2 * sizeof(NIns));

        // 2. Store the "double" to @Rtemp.
        SH4_fmovs_decRx(value_reg + 1, Rtemp);
        SH4_fmovs_decRx(value_reg, Rtemp);

        // Adjust the offset since we are using pre-decrementd (by 4) indirect loads.
        offset += 8;

        // 1. base + offset -> Rtemp.
        asm_base_offset(offset, base_reg);
    }

    void Assembler::asm_store32d(Register value_reg, int offset, Register base_reg) {
        underrunProtect(3 * sizeof(NIns));

        // 3. Store the "float" to @Rtemp.
        SH4_fmovs_indRx(Dtemp, Rtemp);

        // 2. Convert back to simple precision.
        SH4_fsts_FPUL(Dtemp);
        SH4_fcnvds_double_FPUL(value_reg);

        // 1. base + offset -> Rtemp.
        asm_base_offset(offset, base_reg);
    }

    void Assembler::asm_store64(LOpcode opcode, LIns *value, int offset, LIns *base) {
        Register value_reg;
        Register base_reg;

        // If "value" is already in a register, use that one.
        value_reg = ( value->isInReg() ? value->getReg() : findRegFor(value, FpRegs) );
        base_reg  = getBaseReg(base, offset, GpRegs);

        switch (opcode) {
        case LIR_std:
            asm_store64d(value_reg, offset, base_reg);
            break;
        case LIR_std2f:
            asm_store32d(value_reg, offset, base_reg);
            break;
        default:
            NanoAssertMsg(0, "asm_store64 should never receive this LIR opcode");
        }
    }

#ifdef NJ_VERBOSE
    void Assembler::asm_inc_m32(uint32_t *counter) {
        NIns *over_constant = NULL;
        NIns *constant_addr = NULL;

        // Increment the 32-bit profiling counter without changing any registers.

        underrunProtect(11 * sizeof(NIns));

        // 6. restore the register used as a pointer to the counter
        SH4_movl_incRy(SP, R0);

        // 5. (*counter)++
        SH4_movl_indRx(Rtemp, R0);
        SH4_add_imm(1, Rtemp);
        SH4_movl_indRy(R0, Rtemp);

        over_constant = _nIns;
        // 4. align the constant with nop
        if (((uint32_t)_nIns & 0x3) != 0)
            SH4_nop();

        // 3. constant (== counter address)
        SH4_emit32((uint32_t)counter);
        constant_addr = _nIns;

        // 2. branch over the constant
        SH4_nop();
        SH4_bra(SH4_LABEL(over_constant));

        // 1. @(PC + offset) -> R0
        SH4_movl_PCrel(constant_addr, R0);

        // 0. R0 will be used as a pointer to the counter
        SH4_movl_decRx(R0, SP);
    }
#endif

    void Assembler::asm_cmp(LOpcode simplified_opcode, LIns *condition) {
        LIns *operand1 = condition->oprnd1();
        LIns *operand2 = condition->oprnd2();

        Register operand1_reg;
        Register operand2_reg;

        if (isCmpDOpcode(simplified_opcode))
            findRegFor2(FpRegs, operand1, operand1_reg, FpRegs, operand2, operand2_reg);
        else
            findRegFor2(GpRegs, operand1, operand1_reg, GpRegs, operand2, operand2_reg);

        underrunProtect(3 * sizeof(NIns));

        switch(simplified_opcode) {
        case LIR_eqi  : SH4_cmpeq(operand2_reg, operand1_reg); break;
        case LIR_gti  : SH4_cmpgt(operand2_reg, operand1_reg); break;
        case LIR_gei  : SH4_cmpge(operand2_reg, operand1_reg); break;
        case LIR_gtui : SH4_cmphi(operand2_reg, operand1_reg); break;
        case LIR_geui : SH4_cmphs(operand2_reg, operand1_reg); break;

        case LIR_eqd : SH4_fcmpeq_double(operand2_reg, operand1_reg); break;
        case LIR_gtd : SH4_fcmpgt_double(operand2_reg, operand1_reg); break;
        case LIR_ltd : SH4_fcmpgt_double(operand1_reg, operand2_reg); break;

        case LIR_ged : { /* (A >= B) <==> ((A == B) || (A > B)) */
            NIns *end_of_test = _nIns;
            SH4_fcmpeq_double(operand2_reg, operand1_reg); // 3. A == B ?
            SH4_bt(SH4_LABEL(end_of_test));                // 2. skip to preserve T-bit.
            SH4_fcmpgt_double(operand2_reg, operand1_reg); // 1. A > B ?
        }
            break;

        case LIR_led : { /* (A <= B) <==> ((A == B) || (B > A)) */
            NIns *end_of_test = _nIns;
            SH4_fcmpeq_double(operand2_reg, operand1_reg); // 2. A == B ?
            SH4_bt(SH4_LABEL(end_of_test));                // 2. skip to preserve T-bit.
            SH4_fcmpgt_double(operand1_reg, operand2_reg); // 1. B > A ?
        }
            break;

        default:
            NanoAssertMsg(0, "asm_cmp should never receive this LIR opcode");
        }
    }

    NIns* Assembler::asm_branch_ov(LOpcode opcode, NIns* target) {
        NIns *patch_target = NULL;

        if (opcode == LIR_mulxovi || opcode == LIR_muljovi) {
            patch_target = asm_branch(false, target);

            underrunProtect(3 * sizeof(NIns));

            // Remember the T-bit is equal to :
            //     sign_of(result_reg) == sign_of(operand1) XOR sign_of(operand2)

            SH4_tst(Rtemp, Rtemp); // 3. test Rtemp == 0, hence asm_branch(false)
            SH4_rotcl(Rtemp);      // 2. Rtemp << 1 + T-Bit -> Rtemp
            SH4_sts_MACH(Rtemp);   // 1. Hi32(MAC) -> Rtemp
        }
        else
            patch_target = asm_branch(true, target);

        return patch_target;
    }

    bool Assembler::simplifyOpcode(LOpcode &opcode) {
        bool negated= false;

        // Those following opcodes do not exist on SH4, so we have
        // to reverse the test:
        switch(opcode) {
        case LIR_lti:
            opcode = LIR_gei;
            negated = true;
            break;

        case LIR_lei:
            opcode = LIR_gti;
            negated = true;
            break;

        case LIR_ltui:
            opcode = LIR_geui;
            negated = true;
            break;

        case LIR_leui:
            opcode = LIR_gtui;
            negated = true;
            break;

        default:
            break;
        }

        return negated;
    }

    NIns *Assembler::asm_branch(bool test, NIns *target) {
        NIns *patch_target = NULL;
        NIns *over_pool = NULL;

        underrunProtect(3 * sizeof(NIns) + SH4_IMMI_NOCHK_SIZE);

        // Try to select the optimal branch code sequence.
        if (target != NULL && FITS_SH4_bt(SH4_LABEL(target))) {
            // The target is close enough to be reached thanks to one
            // instruction only. Note: FITS_SH4_bf <==> FITS_SH4_bt
            if (test == true)
                SH4_bt(SH4_LABEL(target));
            else
                SH4_bf(SH4_LABEL(target));

            return NULL;
        }

        over_pool = _nIns;

        // Take the opportunity of this unconditional branch to
        // pre-allocate space for constants right after.  The size is
        // limited to 50 words because of the limitation of the
        // instruction bf/t: branch at most 256 bytes far away.
        //
        //     pool size          =  50 * sizeof(word) = 200 bytes
        //   + pool alignement    =  ~1 * sizeof(half) =   2 bytes
        //   + instructions below = ~10 * sizeof(NIns) =  20 bytes
        //                                       total = 222 bytes
        //
        // Note asm_generate_pool() prevents any code buffer
        // allocation so there is no need for underrunProtect(),
        // however we still have to reserve space for the instructions
        // below.
        asm_generate_pool(3 * sizeof(NIns) + SH4_IMMI_NOCHK_SIZE, 50);

        // Check if the target is not so far we can reach it with an
        // unconditional branch wrapped around by a reversed branch.
        if (target != NULL &&  FITS_SH4_bra(SH4_LABEL(target + 1 * sizeof(NIns)))) {
            // 2. branch #target
            SH4_nop();
            SH4_bra(SH4_LABEL(target));

            // 1. branch over 2. if true (negated test)
            if (test == true)
                SH4_bf(SH4_LABEL(over_pool));
            else
                SH4_bt(SH4_LABEL(over_pool));

            return NULL;
        }

        // Otherwise it's the worst case.

        // 3. jump @target_reg
        SH4_nop();
        SH4_jmp_indRx(Rtemp);

        // 2. target -> target_reg
        asm_immi_nochk((int)target, Rtemp, true);
        patch_target = _nIns;

        // 1. branch over 3. if true (negated test)
        if (test == true)
            SH4_bf(SH4_LABEL(over_pool));
        else
            SH4_bt(SH4_LABEL(over_pool));

        return patch_target;
    }

    NIns *Assembler::asm_branch(bool branchOnFalse, LIns *condition, NIns *target) {
        NanoAssert(condition->isCmp());

        LOpcode opcode = condition->opcode();
        bool negated  = simplifyOpcode(opcode);

        NIns *patch_target = asm_branch(negated ? branchOnFalse : !branchOnFalse, target);

        asm_cmp(opcode, condition);

        return patch_target;
    }

    void Assembler::nBeginAssembly() {
        max_stack_args = 0;
    }

    void Assembler::nFragExit(LIns *guard) {
        Fragment *target = guard->record()->exit->target;
        GuardRecord *guard_record = NULL;

        // 3. Jump to the target fragment.
        if (target && target->fragEntry)
            JMP(target->fragEntry);
        else {
            // The target fragment doesn't exist yet, so emit a jump to the epilogue.
            // If the target is created later on, the jump will be patched.
            if (!_epilogue)
                _epilogue = genEpilogue();

            // Take the opportunity of this unconditional branch to create
            // a new pool right after the code sequence.
            asm_generate_pool(0);

            underrunProtect(2 * sizeof(NIns));

            // Use a branch code sequence patchable by nPatchBranch(),
            // that is, with a constant pool instead of an immediate.
            SH4_nop();
            SH4_jmp_indRx(Rtemp);
            asm_immi((int)_epilogue, Rtemp, true);

            guard_record = guard->record();
            guard_record->jmp = _nIns;
        }

        // Profiling for the exit.
        verbose_only(
            if (_logc->lcbits & LC_FragProfile)
                asm_inc_m32(&guard->record()->profCount);
        )

        // 2. Restore the stack pointer.
        MR(SP, FP);

        // 1. Set the return value.
        asm_immi((int)guard_record, R0);
    }

    void Assembler::nInit() {
        int fpscr = 0;

        __asm__ __volatile__ ("sts fpscr, %0": "=r" (fpscr));

        // Only the 'double' precision mode is currently supported within Nano/SH4.
        NanoAssert((fpscr & (1 << 19)) != 0);

        // Only the 'simple' transfer size mode is currently supported within Nano/SH4.
        NanoAssert((fpscr & (1 << 20)) == 0);

        // Declare prefered registers for some specific opcodes.
        nHints[LIR_calli]  = rmask(retRegs[0]);
        nHints[LIR_calld]  = rmask(retDregs[0]);
        nHints[LIR_paramp] = PREFER_SPECIAL;
        nHints[LIR_sti2s]   = rmask(R0);
        nHints[LIR_sti2c]   = rmask(R0);
        nHints[LIR_lduc2ui] = rmask(R0);
        nHints[LIR_ldc2i]   = rmask(R0);
        nHints[LIR_ldus2ui] = rmask(R0);
        nHints[LIR_lds2i]   = rmask(R0);
        nHints[LIR_ldi]     = rmask(R0);

        current_pool.nb_slots = 0;
        current_pool.slots = NULL;
    }

    // Keep in sync' with LARGEST_BRANCH_PATCH.
    void Assembler::nPatchBranch(NIns *branch, NIns *target) {
        // Adjust the address to point to the constant (three instructions below).
        branch += 3;

        // Patch the constant.
        *((uint32_t *)(void *)branch) = (uint32_t) target;
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set) {
        Register reg;

        // Find the first register in this set.
        reg = lsReg(set);

        _allocator.free &= ~rmask(reg);

        // Sanity check.
        NanoAssert((rmask(reg) & set) == rmask(reg));

        return reg;
    }

    void Assembler::nRegisterResetAll(RegAlloc& regs) {
        regs.clear();
        regs.free = GpRegs | FpRegs;
    }

    // Keep in sync' with Assembler::asm_param().
    NIns* Assembler::genPrologue() {
        int adjusted_stack_size = max_stack_args + _activation.stackSlotsNeeded() * STACK_GRANULARITY - STATIC_FP;
        adjusted_stack_size = alignUp(adjusted_stack_size, NJ_ALIGN_STACK);

        underrunProtect(SH4_IMMI_NOCHK_SIZE + 5 * sizeof(NIns));

        // 4. Adjust the SP to the expected value.
        if (adjusted_stack_size != 0) {
            adjusted_stack_size = -adjusted_stack_size;

            if (FITS_SH4_add_imm(adjusted_stack_size))
                SH4_add_imm(adjusted_stack_size, SP);
            else {
                // We use R0 since it's not used for regular arguments,
                // and is otherwise clobberred by the call.
                SH4_add(R0, SP);
                asm_immi_nochk(adjusted_stack_size, R0);
            }
        }

        // 3. Move the FP to a "static" point, this allows
        //    positive addressing, more efficient on SH4.
        SH4_mov(SP, FP);
        SH4_add_imm(-STATIC_FP, SP);

        NIns *patchEntry = _nIns;

        // 2. Save the return address.
        SH4_stsl_PR_decRx(SP);

        // 1. Save the previous frame pointer.
        SH4_movl_decRx(FP, SP);

        return patchEntry;
    }

    RegisterMask Assembler::nHint(LIns* inst) {
        RegisterMask prefer = 0;

        NanoAssert(inst->isop(LIR_paramp));
        if (inst->paramKind() == 0)
            if (inst->paramArg() < 4)
                prefer = rmask(argRegs[inst->paramArg()]);

        return prefer;
    }

    void Assembler::swapCodeChunks() {
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));

        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);

        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }

    void Assembler::underrunProtect(int nb_bytes) {
        NanoAssertMsg(nb_bytes <= LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");

        if ((uintptr_t)_nIns - nb_bytes < (uintptr_t)codeStart) {
            NIns* target = _nIns;
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));

            // This jump will call underrunProtect again, but since we're on
            // a new page large enough to host its code, nothing will happen.
            JMP(target, true);
        }
    }

    void Assembler::asm_insert_random_nop() {
        NanoAssert(0); // not supported
    }

#define MOVL_PCREL_RANGE 1020

    // Note: reserved_bytes can be 0 only if the call to
    //       asm_generate_pool() is followed by a call to
    //       underrunProtect() since this former does not call this
    //       latter.
    bool Assembler::asm_generate_pool(int reserved_bytes, int nb_slots) {
        const int instructions_per_constant = 10;
        int i;

        nb_slots = MIN(MAX_NB_SLOTS, nb_slots);

        // Don't pre-allocate a new pool if the number of instructions
        // that can reach the first free slot is lesser than the
        // number of reachable slots multiplied by the density of the
        // constant/instructions:
        //
        //     scalar ]x, y] < scalar [m, n] * instructions_per_constant
        //
        // where x, y, m ,n are defined as below:
        //
        //   start of code buffer ---->  (low addresses)
        //                                     ...
        //                                instruction y  <----+ (not yet emitted)
        //                                     ...            :
        //         current _nIns  +---->  instruction x       |
        //                        :            ...            :
        //                        |       instruction 2       | maximum range
        //     first instruction  +---->  instruction 1       | for movl_dispPC
        //                        |      +-------------+      |
        //                        |      | free slot n |      |
        //                        :      :     ...     :      :
        //       first free slot  +----> | free slot m | <----+
        //                               :     ...     :
        //                               | used slot 2 |
        //                               | used slot 1 |
        //                               +-------------+
        //                                     ...
        //     end of code buffer ---->  (high addresses)
        //
        //
        // The idea is to not allocate a new pool each time to avoid
        // the waste of memory.
        //
        // Note I aligned instruction_x on a 4-word boundary since it
        // is used as the start address of the pool.

        int scalar_mn = 0;
        int scalar_xy = 0;
        uintptr_t instruction_x = (uintptr_t)_nIns & ~0x3;
        uintptr_t instruction_y = instruction_x;
        uintptr_t code_start    = (uintptr_t)codeStart + reserved_bytes;
        uintptr_t code_end      = (uintptr_t)codeEnd;

        // Ensure we don't use a pool from another code buffer.
        if (   (uintptr_t)current_pool.slots < code_start
            || (uintptr_t)current_pool.slots > code_end)
            current_pool.nb_slots = 0;

        // Compute the expression "scalar [m, n]"
        for (i = 0; i < current_pool.nb_slots; i++) {
            if (current_pool.slots[i] == 0) {
                scalar_mn = current_pool.nb_slots - i;
                instruction_y = (uintptr_t)&current_pool.slots[i] - MOVL_PCREL_RANGE;
                break;
            }
        }

        // Ensure we stay within the current code buffer.
        if (   instruction_y < code_start
            || instruction_y > code_end)
            instruction_y = code_start - 1 * sizeof(NIns);

        // Compute the expression "scalar ]x, y]"
        scalar_xy = (ptrdiff_t)(instruction_x - instruction_y) / (int)sizeof(NIns);
        scalar_xy--;

        if (scalar_xy > 0 && scalar_xy <= scalar_mn * instructions_per_constant) {
            verbose_only(verbose_outputf("pool not emitted: 0 < scalar_xy [%d] <= %d * scalar_mn [%d]",
                                         (int)scalar_xy, instructions_per_constant, scalar_mn));
            return false;
        }

        // Shrink the size of the new pool to ensure it will not
        // require an new code buffer.
        while (instruction_x - nb_slots * (sizeof(int) + instructions_per_constant * sizeof(NIns)) < code_start)
            nb_slots--;

        if (nb_slots <= 2) {
            verbose_only(verbose_outputf("pool not emitted: not enough space in the current code buffer."));
            return false;
        }

        // Fill the pool with dummy values.
        current_pool.slots    = (int *)(instruction_x - nb_slots * sizeof(int));
        current_pool.nb_slots = nb_slots;
        memset(current_pool.slots, 0, nb_slots * sizeof(int));

        _nIns = (NIns *)current_pool.slots;

        verbose_only(verbose_outputf("pool emitted: 0 >= scalar_xy [%d] > %d * scalar_mn [%d]; address = %p, size = %d",
                                     (int)scalar_xy, instructions_per_constant, scalar_mn, current_pool.slots, current_pool.nb_slots));
        return true;
    }

    bool Assembler::asm_load_constant(int constant, Register reg) {
        bool has_generated_pool = false;
        int *slot;
        NIns *over_pool;

        // Generate a new pool if there is no current pool.
        if (current_pool.slots == NULL) {
            over_pool = _nIns;

            // reserved_bytes was set to ``2 * sizeof(NIns)`` to
            // ensure there's still enough space after the pool
            // allocation for the instructions below.
            has_generated_pool = asm_generate_pool(2 * sizeof(NIns));
            if (!has_generated_pool)
                return false;

            // No need for underrunProtect() here since it was done by
            // the caller of asm_immi_nochk().
            SH4_nop();
            SH4_bra(SH4_LABEL(over_pool));
        }

    try_again:

        // Search the farther reachable slot in the current pool ...
        uintptr_t farther_slot = MIN((((uintptr_t) _nIns) & ~0x3) + MOVL_PCREL_RANGE,
                                     (uintptr_t) &current_pool.slots[current_pool.nb_slots - 1]);

        verbose_only(verbose_outputf("pool load: _nIns = %p, constant = %d, farther_slot = 0x%x", _nIns, constant, farther_slot));

        // ... search from that point ...
        for (slot = (int *)farther_slot; slot >= current_pool.slots; slot--) {
            // ... if this constant was already stored in the current pool ...
            if (*slot == constant) {
                verbose_only(verbose_outputf("pool load: found same constant in slot @ %p", slot));
                SH4_movl_PCrel(slot, reg);
                return true;
            }
            // ... otherwise, use the first free slot.
            if (*slot == 0) {
                verbose_only(verbose_outputf("pool load: found a free slot @ %p", slot));
                *slot = constant;
                SH4_movl_PCrel(slot, reg);
                return true;
            }
        }

        NanoAssert(!has_generated_pool);

        over_pool = _nIns;

        // reserved_bytes was set to ``1 * sizeof(NIns)`` to ensure
        // there's still enough space after the pool allocation for
        // the SH4_movl_PCrel above (backward goto).
        has_generated_pool = asm_generate_pool(1 * sizeof(NIns));
        if (!has_generated_pool) {
            verbose_only(verbose_outputf("pool load: no slot found", slot));
            return false;
        }
        else {
            SH4_nop();
            SH4_bra(SH4_LABEL(over_pool));
            goto try_again;
        }
    }
}

#endif // FEATURE_NANOJIT && FEATURE_SH4
