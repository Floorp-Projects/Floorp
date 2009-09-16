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
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

// uncomment this to enable _vprof/_nvprof macros
//#define DOPROF
#include "../vprof/vprof.h"

#if defined FEATURE_NANOJIT && defined NANOJIT_X64

/*
completion
- 64bit branch offsets
- finish cmov/qcmov with other conditions
- validate asm_cond with other conditions

better code
- put R12 back in play as a base register
- no-disp addr modes (except RBP/R13)
- disp64 branch/call
- spill gp values to xmm registers?
- prefer xmm registers for copies since gprs are in higher demand?
- stack arg doubles
- stack based LIR_param

tracing
- asm_qjoin
- asm_qhi
- nFragExit

*/

namespace nanojit
{
    const Register Assembler::retRegs[] = { RAX };
#ifdef _MSC_VER
    const Register Assembler::argRegs[] = { RCX, RDX, R8, R9 };
    const Register Assembler::savedRegs[] = { RBX, RSI, RDI, R12, R13, R14, R15 };
#else
    const Register Assembler::argRegs[] = { RDI, RSI, RDX, RCX, R8, R9 };
    const Register Assembler::savedRegs[] = { RBX, R12, R13, R14, R15 };
#endif

    const char *regNames[] = {
        "rax",   "rcx",  "rdx",   "rbx",   "rsp",   "rbp",   "rsi",   "rdi",
        "r8",    "r9",   "r10",   "r11",   "r12",   "r13",   "r14",   "r15",
        "xmm0",  "xmm1", "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
        "xmm8",  "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
    };

#ifdef _DEBUG
    #define TODO(x) todo(#x)
    static void todo(const char *s) {
        verbose_only( avmplus::AvmLog("%s",s); )
        NanoAssertMsgf(false, "%s", s);
    }
#else
    #define TODO(x)
#endif

    // MODRM and restrictions:
    // memory access modes != 11 require SIB if base&7 == 4 (RSP or R12)
    // mode 00 with base&7 == 5 means RIP+disp32 (RBP or R13), use mode 01 disp8=0 instead
    // rex prefix required to use RSP-R15 as 8bit registers in mod/rm8 modes.

    // take R12 out of play as a base register because it requires the SIB byte like ESP
    const RegisterMask BaseRegs = GpRegs & ~rmask(R12);

    static inline int oplen(uint64_t op) {
        return op & 255;
    }

    // encode 2-register rex prefix.  dropped if none of its bits are set.
    static inline uint64_t rexrb(uint64_t op, Register r, Register b) {
        int shift = 64 - 8*oplen(op);
        uint64_t rex = ((op >> shift) & 255) | ((r&8)>>1) | ((b&8)>>3);
        return rex != 0x40 ? op | rex << shift : op - 1;
    }

    // encode 2-register rex prefix.  dropped if none of its bits are set, but
    // keep REX if b >= rsp, to allow uniform use of all 16 8bit registers
    static inline uint64_t rexrb8(uint64_t op, Register r, Register b) {
        int shift = 64 - 8*oplen(op);
        uint64_t rex = ((op >> shift) & 255) | ((r&8)>>1) | ((b&8)>>3);
        return ((rex | (b & ~3)) != 0x40) ? (op | (rex << shift)) : op - 1;
    }

    // encode 2-register rex prefix that follows a manditory prefix (66,F2,F3)
    // [prefix][rex][opcode]
    static inline uint64_t rexprb(uint64_t op, Register r, Register b) {
        int shift = 64 - 8*oplen(op) + 8;
        uint64_t rex = ((op >> shift) & 255) | ((r&8)>>1) | ((b&8)>>3);
        // to drop rex, we replace rex with manditory prefix, and decrement length
        return rex != 0x40 ? op | rex << shift :
            ((op & ~(255LL<<shift)) | (op>>(shift-8)&255) << shift) - 1;
    }

    // [rex][opcode][mod-rr]
    static inline uint64_t mod_rr(uint64_t op, Register r, Register b) {
        return op | uint64_t((r&7)<<3 | (b&7))<<56;
    }

    static inline uint64_t mod_disp32(uint64_t op, Register r, Register b, int32_t d) {
        NanoAssert(IsGpReg(r) && IsGpReg(b));
        NanoAssert((b & 7) != 4); // using RSP or R12 as base requires SIB
        if (isS8(d)) {
            // op is:  0x[disp32=0][mod=2:r:b][op][rex][len]
            NanoAssert((((op>>24)&255)>>6) == 2); // disp32 mode
            int len = oplen(op);
            op = (op & ~0xff000000LL) | (0x40 | (r&7)<<3 | (b&7))<<24; // replace mod
            return op<<24 | int64_t(d)<<56 | (len-3); // shrink disp, add disp8
        } else {
            // op is: 0x[disp32][mod][op][rex][len]
            return op | int64_t(d)<<32 | uint64_t((r&7)<<3 | (b&7))<<24;
        }
    }

    #ifdef NJ_VERBOSE
    void Assembler::dis(NIns *p, int bytes) {
        char b[32], *s = b; // room for 8 hex bytes plus null
        *s++ = ' ';
        for (NIns *end = p + bytes; p < end; p++) {
            VMPI_sprintf(s, "%02x ", *p);
            s += 3;
        }
        *s = 0;
        asm_output("%s", b);
    }
    #endif

    void Assembler::emit(uint64_t op) {
        int len = oplen(op);
        // we will only move nIns by -len bytes, but we write 8
        // bytes.  so need to protect 8 so we dont stomp the page
        // header or the end of the preceding page (might segf)
        underrunProtect(8);
        ((int64_t*)_nIns)[-1] = op;
        _nIns -= len; // move pointer by length encoded in opcode
        _nvprof("x64-bytes", len);
        verbose_only( if (_logc->lcbits & LC_Assembly) dis(_nIns, len); )
    }

    void Assembler::emit8(uint64_t op, int64_t v) {
        NanoAssert(isS8(v));
        emit(op | uint64_t(v)<<56);
    }

    void Assembler::emit32(uint64_t op, int64_t v) {
        NanoAssert(isS32(v));
        emit(op | uint64_t(uint32_t(v))<<32);
    }

    // 2-register modrm32 form
    void Assembler::emitrr(uint64_t op, Register r, Register b) {
        emit(rexrb(mod_rr(op, r, b), r, b));
    }

    // 2-register modrm8 form (8 bit operand size)
    void Assembler::emitrr8(uint64_t op, Register r, Register b) {
        emit(rexrb8(mod_rr(op, r, b), r, b));
    }

    // same as emitrr, but with a prefix byte
    void Assembler::emitprr(uint64_t op, Register r, Register b) {
        emit(rexprb(mod_rr(op, r, b), r, b));
    }

    // disp32 modrm form, when the disp fits in the instruction (opcode is 1-3 bytes)
    void Assembler::emitrm(uint64_t op, Register r, int32_t d, Register b) {
        emit(rexrb(mod_disp32(op, r, b, d), r, b));
    }

    // disp32 modrm form when the disp must be written separately (opcode is 4+ bytes)
    uint64_t Assembler::emit_disp32(uint64_t op, int32_t d) {
        if (isS8(d)) {
            NanoAssert(((op>>56)&0xC0) == 0x80); // make sure mod bits == 2 == disp32 mode
            underrunProtect(1+8);
            *(--_nIns) = (NIns) d;
            _nvprof("x64-bytes", 1);
            op ^= 0xC000000000000000LL; // change mod bits to 1 == disp8 mode
        } else {
            underrunProtect(4+8); // room for displ plus fullsize op
            *((int32_t*)(_nIns -= 4)) = d;
            _nvprof("x64-bytes", 4);
        }
        return op;
    }

    // disp32 modrm form when the disp must be written separately (opcode is 4+ bytes)
    void Assembler::emitrm_wide(uint64_t op, Register r, int32_t d, Register b) {
        op = emit_disp32(op, d);
        emitrr(op, r, b);
    }

    // disp32 modrm form when the disp must be written separately (opcode is 4+ bytes)
    // p = prefix -- opcode must have a 66, F2, or F3 prefix
    void Assembler::emitprm(uint64_t op, Register r, int32_t d, Register b) {
        op = emit_disp32(op, d);
        emitprr(op, r, b);
    }

    void Assembler::emitrr_imm(uint64_t op, Register r, Register b, int32_t imm) {
        NanoAssert(IsGpReg(r) && IsGpReg(b));
        underrunProtect(4+8); // room for imm plus fullsize op
        *((int32_t*)(_nIns -= 4)) = imm;
        _nvprof("x86-bytes", 4);
        emitrr(op, r, b);
    }

    // op = [rex][opcode][modrm][imm8]
    void Assembler::emitr_imm8(uint64_t op, Register b, int32_t imm8) {
        NanoAssert(IsGpReg(b) && isS8(imm8));
        op |= uint64_t(imm8)<<56 | uint64_t(b&7)<<48;  // modrm is 2nd to last byte
        emit(rexrb(op, (Register)0, b));
    }

    void Assembler::MR(Register d, Register s) {
        NanoAssert(IsGpReg(d) && IsGpReg(s));
        emitrr(X64_movqr, d, s);
    }

    void Assembler::JMPl(NIns* target) {
        NanoAssert(!target || isS32(target - _nIns));
        underrunProtect(8); // must do this before calculating offset
        emit32(X64_jmp, target ? target - _nIns : 0);
    }

    void Assembler::JMP(NIns *target) {
        if (!target || isS32(target - _nIns)) {
            underrunProtect(8); // must do this before calculating offset
            if (target && isS8(target - _nIns)) {
                emit8(X64_jmp8, target - _nIns);
            } else {
                emit32(X64_jmp, target ? target - _nIns : 0);
            }
        } else {
            TODO(jmp64);
        }
    }

    // register allocation for 2-address style ops of the form R = R (op) B
    void Assembler::regalloc_binary(LIns *ins, RegisterMask allow, Register &rr, Register &ra, Register &rb) {
#ifdef _DEBUG
        RegisterMask originalAllow = allow;
#endif
        rb = UnknownReg;
        LIns *a = ins->oprnd1();
        LIns *b = ins->oprnd2();
        if (a != b) {
            rb = findRegFor(b, allow);
            allow &= ~rmask(rb);
        }
        rr = prepResultReg(ins, allow);
        Reservation* rA = getresv(a);
        // if this is last use of a in reg, we can re-use result reg
        if (rA == 0 || (ra = rA->reg) == UnknownReg) {
            ra = findSpecificRegFor(a, rr);
        } else if (!(allow & rmask(ra))) {
            // rA already has a register assigned, but it's not valid
            // to make sure floating point operations stay in FPU registers
            // as much as possible, make sure that only a few opcodes are
            // reserving GPRs.
            NanoAssert(a->opcode() == LIR_quad || a->opcode() == LIR_ldq);
            allow &= ~rmask(rr);
            ra = findRegFor(a, allow);
        }
        if (a == b) {
            rb = ra;
        }
        NanoAssert(originalAllow & rmask(rr));
        NanoAssert(originalAllow & rmask(ra));
        NanoAssert(originalAllow & rmask(rb));
    }

    void Assembler::asm_qbinop(LIns *ins) {
        asm_arith(ins);
    }

    void Assembler::asm_shift(LIns *ins) {
        // shift require rcx for shift count
        LIns *b = ins->oprnd2();
        if (b->isconst()) {
            asm_shift_imm(ins);
            return;
        }
        Register rr, ra;
        if (b != ins->oprnd1()) {
            findSpecificRegFor(b, RCX);
            regalloc_unary(ins, GpRegs & ~rmask(RCX), rr, ra);
        } else {
            // a == b means both must be in RCX
            regalloc_unary(ins, rmask(RCX), rr, ra);
        }
        X64Opcode xop;
        switch (ins->opcode()) {
        default:
            TODO(asm_shift);
        case LIR_qursh:     xop = X64_shrq;     break;
        case LIR_qirsh:     xop = X64_sarq;     break;
        case LIR_qilsh:     xop = X64_shlq;     break;
        case LIR_ush:       xop = X64_shr;      break;
        case LIR_rsh:       xop = X64_sar;      break;
        case LIR_lsh:       xop = X64_shl;      break;
        }
        emitr(xop, rr);
        if (rr != ra)
            MR(rr, ra);
    }

    void Assembler::asm_shift_imm(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        X64Opcode xop;
        switch (ins->opcode()) {
        default: TODO(shiftimm);
        case LIR_qursh:     xop = X64_shrqi;    break;
        case LIR_qirsh:     xop = X64_sarqi;    break;
        case LIR_qilsh:     xop = X64_shlqi;    break;
        case LIR_ush:       xop = X64_shri;     break;
        case LIR_rsh:       xop = X64_sari;     break;
        case LIR_lsh:       xop = X64_shli;     break;
        }
        int shift = ins->oprnd2()->imm32() & 255;
        emit8(rexrb(xop | uint64_t(rr&7)<<48, (Register)0, rr), shift);
        if (rr != ra)
            MR(rr, ra);
    }

    static bool isImm32(LIns *ins) {
        return ins->isconst() || (ins->isconstq() && isS32(ins->imm64()));
    }
    static int32_t getImm32(LIns *ins) {
        return ins->isconst() ? ins->imm32() : int32_t(ins->imm64());
    }

    // binary op, integer regs, rhs is int32 const
    void Assembler::asm_arith_imm(LIns *ins) {
        LIns *b = ins->oprnd2();
        int32_t imm = getImm32(b);
        LOpcode op = ins->opcode();
        Register rr, ra;
        if (op == LIR_mul) {
            // imul has true 3-addr form, it doesn't clobber ra
            rr = prepResultReg(ins, GpRegs);
            LIns *a = ins->oprnd1();
            ra = findRegFor(a, GpRegs);
            emitrr_imm(X64_imuli, rr, ra, imm);
            return;
        }
        regalloc_unary(ins, GpRegs, rr, ra);
        X64Opcode xop;
        if (isS8(imm)) {
            switch (ins->opcode()) {
            default: TODO(arith_imm8);
            case LIR_iaddp:
            case LIR_add:   xop = X64_addlr8;   break;
            case LIR_and:   xop = X64_andlr8;   break;
            case LIR_or:    xop = X64_orlr8;    break;
            case LIR_sub:   xop = X64_sublr8;   break;
            case LIR_xor:   xop = X64_xorlr8;   break;
            case LIR_qiadd:
            case LIR_qaddp: xop = X64_addqr8;   break;
            case LIR_qiand: xop = X64_andqr8;   break;
            case LIR_qior:  xop = X64_orqr8;    break;
            case LIR_qxor:  xop = X64_xorqr8;   break;
            }
            emitr_imm8(xop, rr, imm);
        } else {
            switch (ins->opcode()) {
            default: TODO(arith_imm);
            case LIR_iaddp:
            case LIR_add:   xop = X64_addlri;   break;
            case LIR_and:   xop = X64_andlri;   break;
            case LIR_or:    xop = X64_orlri;    break;
            case LIR_sub:   xop = X64_sublri;   break;
            case LIR_xor:   xop = X64_xorlri;   break;
            case LIR_qiadd:
            case LIR_qaddp: xop = X64_addqri;   break;
            case LIR_qiand: xop = X64_andqri;   break;
            case LIR_qior:  xop = X64_orqri;    break;
            case LIR_qxor:  xop = X64_xorqri;   break;
            }
            emitr_imm(xop, rr, imm);
        }
        if (rr != ra)
            MR(rr, ra);
    }

    // binary op with integer registers
    void Assembler::asm_arith(LIns *ins) {
        Register rr, ra, rb;
        LOpcode op = ins->opcode();
        if ((op & ~LIR64) >= LIR_lsh && (op & ~LIR64) <= LIR_ush) {
            asm_shift(ins);
            return;
        }
        LIns *b = ins->oprnd2();
        if (isImm32(b)) {
            asm_arith_imm(ins);
            return;
        }
        regalloc_binary(ins, GpRegs, rr, ra, rb);
        X64Opcode xop;
        switch (ins->opcode()) {
        default:
            TODO(asm_arith);
        case LIR_or:
            xop = X64_orlrr;
            break;
        case LIR_sub:
            xop = X64_subrr;
            break;
        case LIR_iaddp:
        case LIR_add:
            xop = X64_addrr;
            break;
        case LIR_and:
            xop = X64_andrr;
            break;
        case LIR_xor:
            xop = X64_xorrr;
            break;
        case LIR_mul:
            xop = X64_imul;
            break;
        case LIR_qxor:
            xop = X64_xorqrr;
            break;
        case LIR_qior:
            xop = X64_orqrr;
            break;
        case LIR_qiand:
            xop = X64_andqrr;
            break;
        case LIR_qiadd:
        case LIR_qaddp:
            xop = X64_addqrr;
            break;
        }
        emitrr(xop, rr, rb);
        if (rr != ra)
            MR(rr,ra);
    }

    // binary op with fp registers
    void Assembler::asm_fop(LIns *ins) {
        Register rr, ra, rb;
        regalloc_binary(ins, FpRegs, rr, ra, rb);
        X64Opcode xop;
        switch (ins->opcode()) {
        default:
            TODO(asm_fop);
        case LIR_fdiv:
            xop = X64_divsd;
            break;
        case LIR_fmul:
            xop = X64_mulsd;
            break;
        case LIR_fadd:
            xop = X64_addsd;
            break;
        case LIR_fsub:
            xop = X64_subsd;
            break;
        }
        emitprr(xop, rr, rb);
        if (rr != ra) {
            asm_nongp_copy(rr, ra);
        }
    }

    void Assembler::asm_neg_not(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        NanoAssert(IsGpReg(ra));
        X64Opcode xop;
        if (ins->isop(LIR_not)) {
            xop = X64_not;
        } else {
            xop = X64_neg;
        }
        emitr(xop, rr);
        if (rr != ra)
            MR(rr, ra);
    }

    void Assembler::asm_call(LIns *ins) {
        const CallInfo *call = ins->callInfo();
        ArgSize sizes[MAXARGS];
        int argc = call->get_sizes(sizes);

        bool indirect = call->isIndirect();
        if (!indirect) {
            verbose_only(if (_logc->lcbits & LC_Assembly)
                outputf("        %p:", _nIns);
            )
            NIns *target = (NIns*)call->_address;
            // must do underrunProtect before calculating offset
            underrunProtect(8);
            if (isS32(target - _nIns)) {
                emit32(X64_call, target - _nIns);
            } else {
                // can't reach target from here, load imm64 and do an indirect jump
                emit(X64_callrax);
                emit_quad(RAX, (uint64_t)target);
            }
        } else {
            // Indirect call: we assign the address arg to RAX since it's not
            // used for regular arguments, and is otherwise scratch since it's
            // clobberred by the call.
            asm_regarg(ARGSIZE_P, ins->arg(--argc), RAX);
            emit(X64_callrax);
        }

    #ifdef _MSC_VER
        int stk_used = 32; // always reserve 32byte shadow area
    #else
        int stk_used = 0;
        Register fr = XMM0;
    #endif
        int arg_index = 0;
        for (int i = 0; i < argc; i++) {
            int j = argc - i - 1;
            ArgSize sz = sizes[j];
            LIns* arg = ins->arg(j);
            if ((sz & ARGSIZE_MASK_INT) && arg_index < NumArgRegs) {
                // gp arg
                asm_regarg(sz, arg, argRegs[arg_index]);
                arg_index++;
            }
        #ifdef _MSC_VER
            else if (sz == ARGSIZE_F && arg_index < NumArgRegs) {
                // double goes in XMM reg # based on overall arg_index
                asm_regarg(sz, arg, Register(XMM0+arg_index));
                arg_index++;
            }
        #else
            else if (sz == ARGSIZE_F && fr < XMM8) {
                // double goes in next available XMM register
                asm_regarg(sz, arg, fr);
                fr = nextreg(fr);
            }
        #endif
            else {
                asm_stkarg(sz, arg, stk_used);
                stk_used += sizeof(void*);
            }
        }

        if (stk_used > max_stk_used)
            max_stk_used = stk_used;
    }

    void Assembler::asm_regarg(ArgSize sz, LIns *p, Register r) {
        if (sz == ARGSIZE_I) {
            NanoAssert(!p->isQuad());
            if (p->isconst()) {
                emit_quad(r, int64_t(p->imm32()));
                return;
            }
            // sign extend int32 to int64
            emitrr(X64_movsxdr, r, r);
        } else if (sz == ARGSIZE_U) {
            NanoAssert(!p->isQuad());
            if (p->isconst()) {
                emit_quad(r, uint64_t(uint32_t(p->imm32())));
                return;
            }
            // zero extend with 32bit mov, auto-zeros upper 32bits
            emitrr(X64_movlr, r, r);
        }
        /* there is no point in folding an immediate here, because
         * the argument register must be a scratch register and we're
         * just before a call.  Just reserving the register will cause
         * the constant to be rematerialized nearby in asm_restore(),
         * which is the same instruction we would otherwise emit right
         * here, and moving it earlier in the stream provides more scheduling
         * freedom to the cpu. */
        findSpecificRegFor(p, r);
    }

    void Assembler::asm_stkarg(ArgSize sz, LIns *p, int stk_off) {
        NanoAssert(isS8(stk_off));
        if (sz & ARGSIZE_MASK_INT) {
            Register r = findRegFor(p, GpRegs);
            uint64_t xop = X64_movqspr | uint64_t(stk_off) << 56;         // movq [rsp+d8], r
            xop |= uint64_t((r&7)<<3) << 40 | uint64_t((r&8)>>1) << 24;   // insert r into mod/rm and rex bytes
            emit(xop);
            if (sz == ARGSIZE_I) {
                // extend int32 to int64
                NanoAssert(!p->isQuad());
                emitrr(X64_movsxdr, r, r);
            } else if (sz == ARGSIZE_U) {
                // extend uint32 to uint64
                NanoAssert(!p->isQuad());
                emitrr(X64_movlr, r, r);
            }
        } else {
            TODO(asm_stkarg_non_int);
        }
    }

    void Assembler::asm_promote(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        NanoAssert(IsGpReg(ra));
        if (ins->isop(LIR_u2q)) {
            emitrr(X64_movlr, rr, ra); // 32bit mov zeros the upper 32bits of the target
        } else {
            NanoAssert(ins->isop(LIR_i2q));
            emitrr(X64_movsxdr, rr, ra); // sign extend 32->64
        }
    }

    // the CVTSI2SD instruction only writes to the low 64bits of the target
    // XMM register, which hinders register renaming and makes dependence
    // chains longer.  So we precede with XORPS to clear the target register.

    void Assembler::asm_i2f(LIns *ins) {
        Register r = prepResultReg(ins, FpRegs);
        Register b = findRegFor(ins->oprnd1(), GpRegs);
        emitprr(X64_cvtsi2sd, r, b);    // cvtsi2sd xmmr, b  only writes xmm:0:64
        emitprr(X64_xorps, r, r);       // xorpd xmmr,xmmr to break dependency chains
    }

    void Assembler::asm_u2f(LIns *ins) {
        Register r = prepResultReg(ins, FpRegs);
        Register b = findRegFor(ins->oprnd1(), GpRegs);
        NanoAssert(!ins->oprnd1()->isQuad());
        // since oprnd1 value is 32bit, its okay to zero-extend the value without worrying about clobbering.
        emitprr(X64_cvtsq2sd, r, b);    // convert int64 to double
        emitprr(X64_xorps, r, r);       // xorpd xmmr,xmmr to break dependency chains
        emitrr(X64_movlr, b, b);        // zero extend u32 to int64
    }

    void Assembler::asm_cmov(LIns *ins) {
        LIns* cond    = ins->oprnd1();
        LIns* iftrue  = ins->oprnd2();
        LIns* iffalse = ins->oprnd3();
        NanoAssert(cond->isCmp());
        NanoAssert((ins->isop(LIR_qcmov) && iftrue->isQuad() && iffalse->isQuad()) ||
                   (ins->isop(LIR_cmov) && !iftrue->isQuad() && !iffalse->isQuad()));

        // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
        // (This is true on Intel, is it true on all architectures?)
        const Register rr = prepResultReg(ins, GpRegs);
        const Register rf = findRegFor(iffalse, GpRegs & ~rmask(rr));

        int condop = (cond->opcode() & ~LIR64) - LIR_ov;
        static const X64Opcode cmov[] = {
            X64_cmovno, X64_cmovne,                       // ov, eq
            X64_cmovge, X64_cmovle, X64_cmovg, X64_cmovl, // lt,  gt,  le,  ge
            X64_cmovae, X64_cmovbe, X64_cmova, X64_cmovb  // ult, ugt, ule, uge
        };
        NanoAssert(condop >= 0 && condop < int(sizeof(cmov) / sizeof(cmov[0])));
        NanoStaticAssert(sizeof(cmov) / sizeof(cmov[0]) == size_t(LIR_uge - LIR_ov + 1));
        uint64_t xop = cmov[condop];
        if (ins->opcode() == LIR_qcmov)
            xop |= (uint64_t)X64_cmov_64;
        emitrr(xop, rr, rf);
        /*const Register rt =*/ findSpecificRegFor(iftrue, rr);
        asm_cmp(cond);
    }

    NIns* Assembler::asm_branch(bool onFalse, LIns *cond, NIns *target) {
        LOpcode condop = cond->opcode();
        if (condop >= LIR_feq && condop <= LIR_fge)
            return asm_fbranch(onFalse, cond, target);

        // we must ensure there's room for the instr before calculating
        // the offset.  and the offset, determines the opcode (8bit or 32bit)
        underrunProtect(8);
        NanoAssert((condop & ~LIR64) >= LIR_ov);
        NanoAssert((condop & ~LIR64) <= LIR_uge);
        if (target && isS8(target - _nIns)) {
            static const X64Opcode j8[] = {
                X64_jo8, X64_je8,                     // ov, eq
                X64_jl8, X64_jg8, X64_jle8, X64_jge8, // lt,  gt,  le,  ge
                X64_jb8, X64_ja8, X64_jbe8, X64_jae8  // ult, ugt, ule, uge
            };
            NanoStaticAssert(sizeof(j8) / sizeof(j8[0]) == LIR_uge - LIR_ov + 1);
            uint64_t xop = j8[(condop & ~LIR64) - LIR_ov];
            xop ^= onFalse ? (uint64_t)X64_jneg8 : 0;
            emit8(xop, target - _nIns);
        } else {
            static const X64Opcode j32[] = {
                X64_jo, X64_je,                   // ov, eq
                X64_jl, X64_jg, X64_jle, X64_jge, // lt,  gt,  le,  ge
                X64_jb, X64_ja, X64_jbe, X64_jae  // ult, ugt, ule, uge
            };
            NanoStaticAssert(sizeof(j32) / sizeof(j32[0]) == LIR_uge - LIR_ov + 1);
            uint64_t xop = j32[(condop & ~LIR64) - LIR_ov];
            xop ^= onFalse ? (uint64_t)X64_jneg : 0;
            emit32(xop, target ? target - _nIns : 0);
        }
        NIns *patch = _nIns;            // addr of instr to patch
        asm_cmp(cond);
        return patch;
    }

    void Assembler::asm_cmp(LIns *cond) {
        // LIR_ov recycles the flags set by arithmetic ops
        if (cond->opcode() == LIR_ov)
            return;
        LIns *b = cond->oprnd2();
        if (isImm32(b)) {
            asm_cmp_imm(cond);
            return;
        }
        LIns *a = cond->oprnd1();
        Register ra, rb;
        if (a != b) {
            Reservation *resva, *resvb;
            findRegFor2(GpRegs, a, resva, b, resvb);
            ra = resva->reg;
            rb = resvb->reg;
        } else {
            // optimize-me: this will produce a const result!
            ra = rb = findRegFor(a, GpRegs);
        }

        LOpcode condop = cond->opcode();
        emitrr(condop & LIR64 ? X64_cmpqr : X64_cmplr, ra, rb);
    }

    void Assembler::asm_cmp_imm(LIns *cond) {
        LIns *a = cond->oprnd1();
        LIns *b = cond->oprnd2();
        Register ra = findRegFor(a, GpRegs);
        int32_t imm = getImm32(b);
        if (isS8(imm)) {
            X64Opcode xop = (cond->opcode() & LIR64) ? X64_cmpqr8 : X64_cmplr8;
            emitr_imm8(xop, ra, imm);
        } else {
            X64Opcode xop = (cond->opcode() & LIR64) ? X64_cmpqri : X64_cmplri;
            emitr_imm(xop, ra, imm);
        }
    }

    // compiling floating point branches
    // discussion in https://bugzilla.mozilla.org/show_bug.cgi?id=443886
    //
    //  fucom/p/pp: c3 c2 c0   jae ja    jbe jb je jne
    //  ucomisd:     Z  P  C   !C  !C&!Z C|Z C  Z  !Z
    //              -- -- --   --  ----- --- -- -- --
    //  unordered    1  1  1             T   T  T
    //  greater >    0  0  0   T   T               T
    //  less    <    0  0  1             T   T     T
    //  equal   =    1  0  0   T         T      T
    //
    //  here's the cases, using conditionals:
    //
    //  branch  >=  >   <=       <        =
    //  ------  --- --- ---      ---      ---
    //  LIR_jt  jae ja  swap+jae swap+ja  jp over je
    //  LIR_jf  jb  jbe swap+jb  swap+jbe jne+jp

    NIns* Assembler::asm_fbranch(bool onFalse, LIns *cond, NIns *target) {
        LOpcode condop = cond->opcode();
        NIns *patch;
        LIns *a = cond->oprnd1();
        LIns *b = cond->oprnd2();
        if (condop == LIR_feq) {
            if (onFalse) {
                // branch if unordered or !=
                underrunProtect(16); // 12 needed, round up for overhang
                emit32(X64_jp, target ? target - _nIns : 0);
                emit32(X64_jne, target ? target - _nIns : 0);
                patch = _nIns;
            } else {
                // jp skip (2byte)
                // jeq target
                // skip: ...
                underrunProtect(16); // 7 needed but we write 2 instr
                NIns *skip = _nIns;
                emit32(X64_je, target ? target - _nIns : 0);
                patch = _nIns;
                emit8(X64_jp8, skip - _nIns);
            }
        }
        else {
            if (condop == LIR_flt) {
                condop = LIR_fgt;
                LIns *t = a; a = b; b = t;
            } else if (condop == LIR_fle) {
                condop = LIR_fge;
                LIns *t = a; a = b; b = t;
            }
            X64Opcode xop;
            if (condop == LIR_fgt)
                xop = onFalse ? X64_jbe : X64_ja;
            else // LIR_fge
                xop = onFalse ? X64_jb : X64_jae;
            underrunProtect(8);
            emit32(xop, target ? target - _nIns : 0);
            patch = _nIns;
        }
        fcmp(a, b);
        return patch;
    }

    void Assembler::asm_fcond(LIns *ins) {
        LOpcode op = ins->opcode();
        LIns *a = ins->oprnd1();
        LIns *b = ins->oprnd2();
        if (op == LIR_feq) {
            // result = ZF & !PF, must do logic on flags
            // r = al|bl|cl|dl, can only use rh without rex prefix
            Register r = prepResultReg(ins, 1<<RAX|1<<RCX|1<<RDX|1<<RBX);
            emitrr8(X64_movzx8, r, r);                  // movzx8   r,rl     r[8:63] = 0
            emit(X86_and8r | uint64_t(r<<3|(r|4))<<56); // and      rl,rh    rl &= rh
            emit(X86_setnp | uint64_t(r|4)<<56);        // setnp    rh       rh = !PF
            emit(X86_sete  | uint64_t(r)<<56);          // sete     rl       rl = ZF
        } else {
            if (op == LIR_flt) {
                op = LIR_fgt;
                LIns *t = a; a = b; b = t;
            } else if (op == LIR_fle) {
                op = LIR_fge;
                LIns *t = a; a = b; b = t;
            }
            Register r = prepResultReg(ins, GpRegs); // x64 can use any GPR as setcc target
            emitrr8(X64_movzx8, r, r);
            emitr8(op == LIR_fgt ? X64_seta : X64_setae, r);
        }
        fcmp(a, b);
    }

    void Assembler::fcmp(LIns *a, LIns *b) {
        Reservation *resva, *resvb;
        findRegFor2(FpRegs, a, resva, b, resvb);
        emitprr(X64_ucomisd, resva->reg, resvb->reg);
    }

    void Assembler::asm_restore(LIns *ins, Reservation *resv, Register r) {
        (void) r;
        if (ins->isop(LIR_alloc)) {
            int d = disp(resv);
            emitrm(X64_leaqrm, r, d, FP);
        }
        else if (ins->isconst()) {
            if (!resv->arIndex) {
                ins->resv()->clear();
            }
            // unsafe to use xor r,r for zero because it changes cc's
            emit_int(r, ins->imm32());
        }
        else if (ins->isconstq() && IsGpReg(r)) {
            if (!resv->arIndex) {
                ins->resv()->clear();
            }
            // unsafe to use xor r,r for zero because it changes cc's
            emit_quad(r, ins->imm64());
        }
        else {
            int d = findMemFor(ins);
            if (IsFpReg(r)) {
                NanoAssert(ins->isQuad());
                // load 64bits into XMM.  don't know if double or int64, assume double.
                emitprm(X64_movsdrm, r, d, FP);
            } else if (ins->isQuad()) {
                emitrm(X64_movqrm, r, d, FP);
            } else {
                emitrm(X64_movlrm, r, d, FP);
            }
        }
        verbose_only( if (_logc->lcbits & LC_RegAlloc) {
                        outputForEOL("  <= restore %s",
                        _thisfrag->lirbuf->names->formatRef(ins)); } )
    }

    void Assembler::asm_cond(LIns *ins) {
        LOpcode op = ins->opcode();
        // unlike x86-32, with a rex prefix we can use any GP register as an 8bit target
        Register r = prepResultReg(ins, GpRegs);
        // SETcc only sets low 8 bits, so extend
        emitrr8(X64_movzx8, r, r);
        X64Opcode xop;
        switch (op) {
        default:
            TODO(cond);
        case LIR_qeq:
        case LIR_eq:    xop = X64_sete;     break;
        case LIR_qlt:
        case LIR_lt:    xop = X64_setl;     break;
        case LIR_qle:
        case LIR_le:    xop = X64_setle;    break;
        case LIR_qgt:
        case LIR_gt:    xop = X64_setg;     break;
        case LIR_qge:
        case LIR_ge:    xop = X64_setge;    break;
        case LIR_qult:
        case LIR_ult:   xop = X64_setb;     break;
        case LIR_qule:
        case LIR_ule:   xop = X64_setbe;    break;
        case LIR_qugt:
        case LIR_ugt:   xop = X64_seta;     break;
        case LIR_quge:
        case LIR_uge:   xop = X64_setae;    break;
        case LIR_ov:    xop = X64_seto;     break;
        }
        emitr8(xop, r);
        asm_cmp(ins);
    }

    void Assembler::asm_ret(LIns *ins) {
        genEpilogue();

        // Restore RSP from RBP, undoing SUB(RSP,amt) in the prologue
        MR(RSP,FP);

        assignSavedRegs();
        LIns *value = ins->oprnd1();
        Register r = ins->isop(LIR_ret) ? RAX : XMM0;
        findSpecificRegFor(value, r);
    }

    void Assembler::asm_nongp_copy(Register d, Register s) {
        if (!IsFpReg(d) && IsFpReg(s)) {
            // gpr <- xmm: use movq r/m64, xmm (66 REX.W 0F 7E /r)
            emitprr(X64_movqrx, s, d);
        } else if (IsFpReg(d) && IsFpReg(s)) {
            // xmm <- xmm: use movaps. movsd r,r causes partial register stall
            emitrr(X64_movapsr, d, s);
        } else {
            // xmm <- gpr: use movq xmm, r/m64 (66 REX.W 0F 6E /r)
            emitprr(X64_movqxr, d, s);
        }
    }

    void Assembler::regalloc_load(LIns *ins, Register &rr, int32_t &dr, Register &rb) {
        dr = ins->disp();
        LIns *base = ins->oprnd1();
        rb = getBaseReg(base, dr, BaseRegs);
        Reservation *resv = getresv(ins);
        if (resv && (rr = resv->reg) != UnknownReg) {
            // keep already assigned register
            freeRsrcOf(ins, false);
        } else {
            // use a gpr in case we're copying a non-double
            rr = prepResultReg(ins, GpRegs & ~rmask(rb));
        }
    }

    void Assembler::asm_load64(LIns *ins) {
        Register rr, rb;
        int32_t dr;
        regalloc_load(ins, rr, dr, rb);
        if (IsGpReg(rr)) {
            // general 64bit load, 32bit const displacement
            emitrm(X64_movqrm, rr, dr, rb);
        } else {
            // load 64bits into XMM.  don't know if double or int64, assume double.
            emitprm(X64_movsdrm, rr, dr, rb);
        }
    }

    void Assembler::asm_ld(LIns *ins) {
        NanoAssert(!ins->isQuad());
        Register r, b;
        int32_t d;
        regalloc_load(ins, r, d, b);
        LOpcode op = ins->opcode();
        switch (op) {
        case LIR_ldcb:
            emitrm_wide(X64_movzx8m, r, d, b);
            break;
        case LIR_ldcs:
            emitrm_wide(X64_movzx16m, r, d, b);
            break;
        default:
            emitrm(X64_movlrm, r, d, b);
            break;
        }
    }

    void Assembler::asm_store64(LIns *value, int d, LIns *base) {
        NanoAssert(value->isQuad());
        Register b = getBaseReg(base, d, BaseRegs);

        // if we have to choose a register, use a GPR, but not the base reg
        Reservation *resv = getresv(value);
        Register r;
        if (!resv || (r = resv->reg) == UnknownReg) {
            RegisterMask allow;
            LOpcode op = value->opcode();
            if ((op >= LIR_fneg && op <= LIR_fmod) || op == LIR_fcall) {
                allow = FpRegs;
            } else {
                allow = GpRegs;
            }
            r = findRegFor(value, allow & ~rmask(b));
        }

        if (IsGpReg(r)) {
            // gpr store
            emitrm(X64_movqmr, r, d, b);
        }
        else {
            // xmm store
            emitprm(X64_movsdmr, r, d, b);
        }
    }

    void Assembler::asm_store32(LIns *value, int d, LIns *base) {
        NanoAssert(!value->isQuad());
        Register b = getBaseReg(base, d, BaseRegs);
        Register r = findRegFor(value, GpRegs & ~rmask(b));

        // store 32bits to 64bit addr.  use rex so we can use all 16 regs
        emitrm(X64_movlmr, r, d, b);
    }

    // generate a 32bit constant, must not affect condition codes!
    void Assembler::emit_int(Register r, int32_t v) {
        NanoAssert(IsGpReg(r));
        emitr_imm(X64_movi, r, v);
    }

    // generate a 64bit constant, must not affect condition codes!
    void Assembler::emit_quad(Register r, uint64_t v) {
        NanoAssert(IsGpReg(r));
        if (isU32(v)) {
            emit_int(r, int32_t(v));
            return;
        }
        if (isS32(v)) {
            // safe for sign-extension 32->64
            emitr_imm(X64_movqi32, r, int32_t(v));
            return;
        }
        underrunProtect(8+8); // imm64 + worst case instr len
        ((uint64_t*)_nIns)[-1] = v;
        _nIns -= 8;
        _nvprof("x64-bytes", 8);
        emitr(X64_movqi, r);
    }

    void Assembler::asm_int(LIns *ins) {
        Register r = prepResultReg(ins, GpRegs);
        int32_t v = ins->imm32();
        if (v == 0) {
            // special case for zero
            emitrr(X64_xorrr, r, r);
            return;
        }
        emit_int(r, v);
    }

    void Assembler::asm_quad(LIns *ins) {
        uint64_t v = ins->imm64();
        RegisterMask allow = v == 0 ? GpRegs|FpRegs : GpRegs;
        Register r = prepResultReg(ins, allow);
        if (v == 0) {
            if (IsGpReg(r)) {
                // special case for zero
                emitrr(X64_xorrr, r, r);
            } else {
                // xorps for xmm
                emitprr(X64_xorps, r, r);
            }
        } else {
            emit_quad(r, v);
        }
    }

    void Assembler::asm_qjoin(LIns*) {
        TODO(asm_qjoin);
    }

    Register Assembler::asm_prep_fcall(Reservation*, LIns *ins) {
        return prepResultReg(ins, rmask(XMM0));
    }

    void Assembler::asm_param(LIns *ins) {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        if (kind == 0) {
            // ordinary param
            // first six args always in registers for mac x64
            if (a < 6) {
                // incoming arg in register
                prepResultReg(ins, rmask(argRegs[a]));
            } else {
                // todo: support stack based args, arg 0 is at [FP+off] where off
                // is the # of regs to be pushed in genProlog()
                TODO(asm_param_stk);
            }
        }
        else {
            // saved param
            prepResultReg(ins, rmask(savedRegs[a]));
        }
    }

    // register allocation for 2-address style unary ops of the form R = (op) R
    void Assembler::regalloc_unary(LIns *ins, RegisterMask allow, Register &rr, Register &ra) {
        LIns *a = ins->oprnd1();
        rr = prepResultReg(ins, allow);
        Reservation* rA = getresv(a);
        // if this is last use of a in reg, we can re-use result reg
        if (rA == 0 || (ra = rA->reg) == UnknownReg) {
            ra = findSpecificRegFor(a, rr);
        } else {
            // rA already has a register assigned.  caller must emit a copy
            // to rr once instr code is generated.  (ie  mov rr,ra ; op rr)
        }
        NanoAssert(allow & rmask(rr));
    }

    static const AVMPLUS_ALIGN16(int64_t) negateMask[] = {0x8000000000000000LL,0};

    void Assembler::asm_fneg(LIns *ins) {
        Register rr, ra;
        if (isS32((uintptr_t)negateMask) || isS32((NIns*)negateMask - _nIns)) {
            regalloc_unary(ins, FpRegs, rr, ra);
            if (isS32((uintptr_t)negateMask)) {
                // builtin code is in bottom or top 2GB addr space, use absolute addressing
                underrunProtect(4+8);
                *((int32_t*)(_nIns -= 4)) = (int32_t)(uintptr_t)negateMask;
                _nvprof("x64-bytes", 4);
                uint64_t xop = X64_xorpsa | uint64_t((rr&7)<<3)<<48; // put rr[0:2] into mod/rm byte
                xop = rexrb(xop, rr, (Register)0);  // put rr[3] into rex byte
                emit(xop);
            } else {
                // jit code is within +/-2GB of builtin code, use rip-relative
                underrunProtect(4+8);
                int32_t d = (int32_t) ((NIns*)negateMask - _nIns);
                *((int32_t*)(_nIns -= 4)) = d;
                _nvprof("x64-bytes", 4);
                emitrr(X64_xorpsm, rr, (Register)0);
            }
            if (ra != rr)
                asm_nongp_copy(rr,ra);
        } else {
            // this is just hideous - can't use RIP-relative load, can't use
            // absolute-address load, and cant move imm64 const to XMM.
            // so do it all in a GPR.  hrmph.
            rr = prepResultReg(ins, GpRegs);
            ra = findRegFor(ins->oprnd1(), GpRegs & ~rmask(rr));
            emitrr(X64_xorqrr, rr, ra);         // xor rr, ra
            emit_quad(rr, negateMask[0]);       // mov rr, 0x8000000000000000
        }
    }

    void Assembler::asm_qhi(LIns*) {
        TODO(asm_qhi);
    }

    void Assembler::asm_qlo(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        NanoAssert(IsGpReg(ra));
        emitrr(X64_movlr, rr, ra); // 32bit mov zeros the upper 32bits of the target
    }

    void Assembler::asm_spill(Register rr, int d, bool /*pop*/, bool quad) {
        if (d) {
            if (!IsFpReg(rr)) {
                X64Opcode xop = quad ? X64_movqmr : X64_movlmr;
                emitrm(xop, rr, d, FP);
            } else {
                // store 64bits from XMM to memory
                NanoAssert(quad);
                emitprm(X64_movsdmr, rr, d, FP);
            }
        }
    }

    NIns* Assembler::genPrologue() {
        // activation frame is 4 bytes per entry even on 64bit machines
        uint32_t stackNeeded = max_stk_used + _activation.tos * 4;

        uint32_t stackPushed =
            sizeof(void*) + // returnaddr
            sizeof(void*); // ebp
        uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
        uint32_t amt = aligned - stackPushed;

        // Reserve stackNeeded bytes, padded
        // to preserve NJ_ALIGN_STACK-byte alignment.
        if (amt) {
            if (isS8(amt))
                emitr_imm8(X64_subqr8, RSP, amt);
            else
                emitr_imm(X64_subqri, RSP, amt);
        }

        verbose_only( outputAddr=true; asm_output("[patch entry]"); )
        NIns *patchEntry = _nIns;
        MR(FP, RSP);            // Establish our own FP.
        emitr(X64_pushr, FP);   // Save caller's FP.

        return patchEntry;
    }

    NIns* Assembler::genEpilogue() {
        // pop rbp
        // ret
        emit(X64_ret);
        emitr(X64_popr, RBP);
        return _nIns;
    }

    void Assembler::nRegisterResetAll(RegAlloc &a) {
        // add scratch registers to our free list for the allocator
        a.clear();
#ifdef _MSC_VER
        a.free = 0x001fffcf; // rax-rbx, rsi, rdi, r8-r15, xmm0-xmm5
#else
        a.free = 0xffffffff & ~(1<<RSP | 1<<RBP);
#endif
        debug_only( a.managed = a.free; )
    }

    void Assembler::nPatchBranch(NIns *patch, NIns *target) {
        NIns *next = 0;
        if (patch[0] == 0xE9) {
            // jmp disp32
            next = patch+5;
        } else if (patch[0] == 0x0F && (patch[1] & 0xF0) == 0x80) {
            // jcc disp32
            next = patch+6;
        } else {
            next = 0;
            TODO(unknown_patch);
        }
        // Guards can result in a valid branch being patched again later, so don't assert
        // that the old value is poison.
        NanoAssert(isS32(target - next));
        ((int32_t*)next)[-1] = int32_t(target - next);
        if (next[0] == 0x0F && next[1] == 0x8A) {
            // code is jne<target>,jp<target>, for LIR_jf(feq)
            // we just patched the jne, now patch the jp.
            next += 6;
            NanoAssert(((int32_t*)next)[-1] == 0);
            NanoAssert(isS32(target - next));
            ((int32_t*)next)[-1] = int32_t(target - next);
        }
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set) {
    #if defined _WIN64
        DWORD tr;
        _BitScanForward(&tr, set);
        _allocator.free &= ~rmask((Register)tr);
        return (Register) tr;
    #else
        // gcc asm syntax
        Register r;
        asm("bsf    %1, %%eax\n\t"
            "btr    %%eax, %2\n\t"
            "movl   %%eax, %0\n\t"
            : "=m"(r) : "m"(set), "m"(_allocator.free) : "%eax", "memory");
        (void)set;
        return r;
    #endif
    }

    void Assembler::nFragExit(LIns *guard) {
        SideExit *exit = guard->record()->exit;
        Fragment *frag = exit->target;
        GuardRecord *lr = 0;
        bool destKnown = (frag && frag->fragEntry);
        // Generate jump to epilog and initialize lr.
        // If the guard is LIR_xtbl, use a jump table with epilog in every entry
        if (guard->isop(LIR_xtbl)) {
            NanoAssert(!guard->isop(LIR_xtbl));
        } else {
            // If the guard already exists, use a simple jump.
            if (destKnown) {
                JMP(frag->fragEntry);
                lr = 0;
            } else {  // target doesn't exist. Use 0 jump offset and patch later
                if (!_epilogue)
                    _epilogue = genEpilogue();
                lr = guard->record();
                JMPl(_epilogue);
                lr->jmp = _nIns;
            }
        }

        MR(RSP, RBP);

        // return value is GuardRecord*
        emit_quad(RAX, uintptr_t(lr));
    }

    void Assembler::nInit(AvmCore*) {
    }

    void Assembler::nBeginAssembly() {
        max_stk_used = 0;
    }

    void Assembler::underrunProtect(ptrdiff_t bytes) {
        NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");
        NIns *pc = _nIns;
        NIns *top = _inExit ? this->exitStart : this->codeStart;

    #if PEDANTIC
        // pedanticTop is based on the last call to underrunProtect; any time we call
        // underrunProtect and would use more than what's already protected, then insert
        // a page break jump.  Sometimes, this will be to a new page, usually it's just
        // the next instruction

        NanoAssert(pedanticTop >= top);
        if (pc - bytes < pedanticTop) {
            // no page break required, but insert a far branch anyway just to be difficult
            const int br_size = 8; // opcode + 32bit addr
            if (pc - bytes - br_size < top) {
                // really do need a page break
                verbose_only(if (_logc->lcbits & LC_Assembly) outputf("newpage %p:", pc);)
                if (_inExit)
                    codeAlloc(exitStart, exitEnd, _nIns verbose_only(, exitBytes));
                else
                    codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            }
            // now emit the jump, but make sure we won't need another page break.
            // we're pedantic, but not *that* pedantic.
            pedanticTop = _nIns - br_size;
            JMP(pc);
            pedanticTop = _nIns - bytes;
        }
    #else
        if (pc - bytes < top) {
            verbose_only(if (_logc->lcbits & LC_Assembly) outputf("newpage %p:", pc);)
            if (_inExit)
                codeAlloc(exitStart, exitEnd, _nIns verbose_only(, exitBytes));
            else
                codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            // this jump will call underrunProtect again, but since we're on a new
            // page, nothing will happen.
            JMP(pc);
        }
    #endif
    }

    RegisterMask Assembler::hint(LIns *, RegisterMask allow) {
        return allow;
    }

    void Assembler::nativePageSetup() {
        if (!_nIns) {
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            IF_PEDANTIC( pedanticTop = _nIns; )
        }
        if (!_nExitIns) {
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
        }
    }

    void Assembler::nativePageReset()
    {}

    // Increment the 32-bit profiling counter at pCtr, without
    // changing any registers.
    verbose_only(
    void Assembler::asm_inc_m32(uint32_t* pCtr)
    {
    }
    )

} // namespace nanojit

#endif // FEATURE_NANOJIT && NANOJIT_X64
