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
#ifdef _WIN64
    const Register Assembler::argRegs[] = { RCX, RDX, R8, R9 };
    const Register Assembler::savedRegs[] = { RBX, RSI, RDI, R12, R13, R14, R15 };
#else
    const Register Assembler::argRegs[] = { RDI, RSI, RDX, RCX, R8, R9 };
    const Register Assembler::savedRegs[] = { RBX, R12, R13, R14, R15 };
#endif

    const char *regNames[] = {
        "rax",  "rcx",  "rdx",   "rbx",   "rsp",   "rbp",   "rsi",   "rdi",
        "r8",   "r9",   "r10",   "r11",   "r12",   "r13",   "r14",   "r15",
        "xmm0", "xmm1", "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
        "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
    };

    const char *gpRegNames32[] = {
        "eax", "ecx", "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };

    const char *gpRegNames8[] = {
        "al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
        "r8l", "r9l", "r10l", "r11l", "r12l", "r13l", "r14l", "r15l"
    };

    const char *gpRegNames8hi[] = {
        "ah", "ch", "dh", "bh"
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

    // MODRM and SIB restrictions:
    // memory access modes != 11 require SIB if base&7 == 4 (RSP or R12)
    // mode 00 with base == x101 means RIP+disp32 (RBP or R13), use mode 01 disp8=0 instead
    // mode 01 or 11 with base = x101 means disp32 + EBP or R13, not RIP relative
    // base == x100 means SIB byte is present, so using ESP|R12 as base requires SIB
    // rex prefix required to use RSP-R15 as 8bit registers in mod/rm8 modes.

    // take R12 out of play as a base register because using ESP or R12 as base requires the SIB byte
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

    // encode 3-register rex prefix.  dropped if none of its bits are set.
    static inline uint64_t rexrxb(uint64_t op, Register r, Register x, Register b) {
        int shift = 64 - 8*oplen(op);
        uint64_t rex = ((op >> shift) & 255) | ((r&8)>>1) | ((x&8)>>2) | ((b&8)>>3);
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

    // [rex][opcode][modrm=r][sib=xb]
    static inline uint64_t mod_rxb(uint64_t op, Register r, Register x, Register b) {
        return op | /*modrm*/uint64_t((r&7)<<3)<<48 | /*sib*/uint64_t((x&7)<<3|(b&7))<<56;
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

    // All the emit() functions should only be called from within codegen
    // functions PUSHR(), SHR(), etc.

    void Assembler::emit(uint64_t op) {
        int len = oplen(op);
        // we will only move nIns by -len bytes, but we write 8
        // bytes.  so need to protect 8 so we dont stomp the page
        // header or the end of the preceding page (might segf)
        underrunProtect(8);
        ((int64_t*)_nIns)[-1] = op;
        _nIns -= len; // move pointer by length encoded in opcode
        _nvprof("x64-bytes", len);
    }

    void Assembler::emit8(uint64_t op, int64_t v) {
        NanoAssert(isS8(v));
        emit(op | uint64_t(v)<<56);
    }

    void Assembler::emit_target8(size_t underrun, uint64_t op, NIns* target) {
        underrunProtect(underrun); // must do this before calculating offset
        // Nb: see emit_target32() for why we use _nIns here.
        int64_t offset = target - _nIns;
        NanoAssert(isS8(offset));
        emit(op | uint64_t(offset)<<56);
    }

    void Assembler::emit_target32(size_t underrun, uint64_t op, NIns* target) {
        underrunProtect(underrun); // must do this before calculating offset
        // Nb: at this point in time, _nIns points to the most recently
        // written instruction, ie. the jump's successor.  So why do we use it
        // to compute the offset, rather than the jump's address?  Because in
        // x86/x64-64 the offset in a relative jump is not from the jmp itself
        // but from the following instruction.  Eg. 'jmp $0' will jump to the
        // next instruction.
        int64_t offset = target ? target - _nIns : 0;
        NanoAssert(isS32(offset));
        emit(op | uint64_t(uint32_t(offset))<<32);
    }

    // 3-register modrm32+sib form
    void Assembler::emitrxb(uint64_t op, Register r, Register x, Register b) {
        emit(rexrxb(mod_rxb(op, r, x, b), r, x, b));
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

    void Assembler::emitr_imm64(uint64_t op, Register r, uint64_t imm64) {
        underrunProtect(8+8); // imm64 + worst case instr len
        *((uint64_t*)(_nIns -= 8)) = imm64;
        _nvprof("x64-bytes", 8);
        emitr(op, r);
    }

    void Assembler::emitrxb_imm(uint64_t op, Register r, Register x, Register b, int32_t imm) {
        NanoAssert(IsGpReg(r) && IsGpReg(x) && IsGpReg(b));
        underrunProtect(4+8); // room for imm plus fullsize op
        *((int32_t*)(_nIns -= 4)) = imm;
        _nvprof("x86-bytes", 4);
        emitrxb(op, r, x, b);
    }

    // op = [rex][opcode][modrm][imm8]
    void Assembler::emitr_imm8(uint64_t op, Register b, int32_t imm8) {
        NanoAssert(IsGpReg(b) && isS8(imm8));
        op |= uint64_t(imm8)<<56 | uint64_t(b&7)<<48;  // modrm is 2nd to last byte
        emit(rexrb(op, (Register)0, b));
    }

    void Assembler::emitxm_abs(uint64_t op, Register r, int32_t addr32)
    {
        underrunProtect(4+8);
        *((int32_t*)(_nIns -= 4)) = addr32;
        _nvprof("x64-bytes", 4);
        op = op | uint64_t((r&7)<<3)<<48; // put rr[0:2] into mod/rm byte
        op = rexrb(op, r, (Register)0);   // put rr[3] into rex byte
        emit(op);
    }

    void Assembler::emitxm_rel(uint64_t op, Register r, NIns* addr64)
    {
        underrunProtect(4+8);
        int32_t d = (int32_t)(addr64 - _nIns);
        *((int32_t*)(_nIns -= 4)) = d;
        _nvprof("x64-bytes", 4);
        emitrr(op, r, (Register)0);
    }

    // Succeeds if 'target' is within a signed 8-bit offset from the current
    // instruction's address.
    bool Assembler::isTargetWithinS8(NIns* target)
    {
        NanoAssert(target);
        // First call underrunProtect().  Without it, we might compute the
        // difference just before starting a new code chunk.
        underrunProtect(8);
        return isS8(target - _nIns);
    }

    // Like isTargetWithinS8(), but for signed 32-bit offsets.
    bool Assembler::isTargetWithinS32(NIns* target)
    {
        NanoAssert(target);
        underrunProtect(8);
        return isS32(target - _nIns);
    }

#define RB(r)       gpRegNames8[(r)]
#define RBhi(r)     gpRegNames8hi[(r)]
#define RL(r)       gpRegNames32[(r)]
#define RQ(r)       gpn(r)

#define R           Register
#define I           int
#define I32         int32_t
#define U64         uint64_t
#define S           size_t

    void Assembler::PUSHR(R r)  { emitr(X64_pushr,r); asm_output("push %s", RQ(r)); }
    void Assembler::POPR( R r)  { emitr(X64_popr, r); asm_output("pop %s",  RQ(r)); }
    void Assembler::NOT(  R r)  { emitr(X64_not,  r); asm_output("notl %s", RL(r)); }
    void Assembler::NEG(  R r)  { emitr(X64_neg,  r); asm_output("negl %s", RL(r)); }
    void Assembler::IDIV( R r)  { emitr(X64_idiv, r); asm_output("idivl edx:eax, %s",RL(r)); }

    void Assembler::SHR( R r)   { emitr(X64_shr,  r); asm_output("shrl %s, ecx", RL(r)); }
    void Assembler::SAR( R r)   { emitr(X64_sar,  r); asm_output("sarl %s, ecx", RL(r)); }
    void Assembler::SHL( R r)   { emitr(X64_shl,  r); asm_output("shll %s, ecx", RL(r)); }
    void Assembler::SHRQ(R r)   { emitr(X64_shrq, r); asm_output("shrq %s, ecx", RQ(r)); }
    void Assembler::SARQ(R r)   { emitr(X64_sarq, r); asm_output("sarq %s, ecx", RQ(r)); }
    void Assembler::SHLQ(R r)   { emitr(X64_shlq, r); asm_output("shlq %s, ecx", RQ(r)); }

    void Assembler::SHRI( R r, I i)   { emit8(rexrb(X64_shri  | U64(r&7)<<48, (R)0, r), i); asm_output("shrl %s, %d", RL(r), i); }
    void Assembler::SARI( R r, I i)   { emit8(rexrb(X64_sari  | U64(r&7)<<48, (R)0, r), i); asm_output("sarl %s, %d", RL(r), i); }
    void Assembler::SHLI( R r, I i)   { emit8(rexrb(X64_shli  | U64(r&7)<<48, (R)0, r), i); asm_output("shll %s, %d", RL(r), i); }
    void Assembler::SHRQI(R r, I i)   { emit8(rexrb(X64_shrqi | U64(r&7)<<48, (R)0, r), i); asm_output("shrq %s, %d", RQ(r), i); }
    void Assembler::SARQI(R r, I i)   { emit8(rexrb(X64_sarqi | U64(r&7)<<48, (R)0, r), i); asm_output("sarq %s, %d", RQ(r), i); }
    void Assembler::SHLQI(R r, I i)   { emit8(rexrb(X64_shlqi | U64(r&7)<<48, (R)0, r), i); asm_output("shlq %s, %d", RQ(r), i); }

    void Assembler::SETE( R r)  { emitr8(X64_sete, r); asm_output("sete %s", RB(r)); }
    void Assembler::SETL( R r)  { emitr8(X64_setl, r); asm_output("setl %s", RB(r)); }
    void Assembler::SETLE(R r)  { emitr8(X64_setle,r); asm_output("setle %s",RB(r)); }
    void Assembler::SETG( R r)  { emitr8(X64_setg, r); asm_output("setg %s", RB(r)); }
    void Assembler::SETGE(R r)  { emitr8(X64_setge,r); asm_output("setge %s",RB(r)); }
    void Assembler::SETB( R r)  { emitr8(X64_setb, r); asm_output("setb %s", RB(r)); }
    void Assembler::SETBE(R r)  { emitr8(X64_setbe,r); asm_output("setbe %s",RB(r)); }
    void Assembler::SETA( R r)  { emitr8(X64_seta, r); asm_output("seta %s", RB(r)); }
    void Assembler::SETAE(R r)  { emitr8(X64_setae,r); asm_output("setae %s",RB(r)); }
    void Assembler::SETO( R r)  { emitr8(X64_seto, r); asm_output("seto %s", RB(r)); }

    void Assembler::ADDRR(R l, R r)     { emitrr(X64_addrr,l,r); asm_output("addl %s, %s", RL(l),RL(r)); }
    void Assembler::SUBRR(R l, R r)     { emitrr(X64_subrr,l,r); asm_output("subl %s, %s", RL(l),RL(r)); }
    void Assembler::ANDRR(R l, R r)     { emitrr(X64_andrr,l,r); asm_output("andl %s, %s", RL(l),RL(r)); }
    void Assembler::ORLRR(R l, R r)     { emitrr(X64_orlrr,l,r); asm_output("orl %s, %s",  RL(l),RL(r)); }
    void Assembler::XORRR(R l, R r)     { emitrr(X64_xorrr,l,r); asm_output("xorl %s, %s", RL(l),RL(r)); }
    void Assembler::IMUL( R l, R r)     { emitrr(X64_imul, l,r); asm_output("imull %s, %s",RL(l),RL(r)); }
    void Assembler::CMPLR(R l, R r)     { emitrr(X64_cmplr,l,r); asm_output("cmpl %s, %s", RL(l),RL(r)); }
    void Assembler::MOVLR(R l, R r)     { emitrr(X64_movlr,l,r); asm_output("movl %s, %s", RL(l),RL(r)); }

    void Assembler::ADDQRR( R l, R r)   { emitrr(X64_addqrr, l,r); asm_output("addq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::SUBQRR( R l, R r)   { emitrr(X64_subqrr, l,r); asm_output("subq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::ANDQRR( R l, R r)   { emitrr(X64_andqrr, l,r); asm_output("andq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::ORQRR(  R l, R r)   { emitrr(X64_orqrr,  l,r); asm_output("orq %s, %s",   RQ(l),RQ(r)); }
    void Assembler::XORQRR( R l, R r)   { emitrr(X64_xorqrr, l,r); asm_output("xorq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMPQR(  R l, R r)   { emitrr(X64_cmpqr,  l,r); asm_output("cmpq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::MOVQR(  R l, R r)   { emitrr(X64_movqr,  l,r); asm_output("movq %s, %s",  RQ(l),RQ(r)); }
    void Assembler::MOVAPSR(R l, R r)   { emitrr(X64_movapsr,l,r); asm_output("movaps %s, %s",RQ(l),RQ(r)); }

    void Assembler::CMOVNO( R l, R r)   { emitrr(X64_cmovno, l,r); asm_output("cmovlno %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNE( R l, R r)   { emitrr(X64_cmovne, l,r); asm_output("cmovlne %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNL( R l, R r)   { emitrr(X64_cmovnl, l,r); asm_output("cmovlnl %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNLE(R l, R r)   { emitrr(X64_cmovnle,l,r); asm_output("cmovlnle %s, %s", RL(l),RL(r)); }
    void Assembler::CMOVNG( R l, R r)   { emitrr(X64_cmovng, l,r); asm_output("cmovlng %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNGE(R l, R r)   { emitrr(X64_cmovnge,l,r); asm_output("cmovlnge %s, %s", RL(l),RL(r)); }
    void Assembler::CMOVNB( R l, R r)   { emitrr(X64_cmovnb, l,r); asm_output("cmovlnb %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNBE(R l, R r)   { emitrr(X64_cmovnbe,l,r); asm_output("cmovlnbe %s, %s", RL(l),RL(r)); }
    void Assembler::CMOVNA( R l, R r)   { emitrr(X64_cmovna, l,r); asm_output("cmovlna %s, %s",  RL(l),RL(r)); }
    void Assembler::CMOVNAE(R l, R r)   { emitrr(X64_cmovnae,l,r); asm_output("cmovlnae %s, %s", RL(l),RL(r)); }

    void Assembler::CMOVQNO( R l, R r)  { emitrr(X64_cmovqno, l,r); asm_output("cmovqno %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNE( R l, R r)  { emitrr(X64_cmovqne, l,r); asm_output("cmovqne %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNL( R l, R r)  { emitrr(X64_cmovqnl, l,r); asm_output("cmovqnl %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNLE(R l, R r)  { emitrr(X64_cmovqnle,l,r); asm_output("cmovqnle %s, %s", RQ(l),RQ(r)); }
    void Assembler::CMOVQNG( R l, R r)  { emitrr(X64_cmovqng, l,r); asm_output("cmovqng %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNGE(R l, R r)  { emitrr(X64_cmovqnge,l,r); asm_output("cmovqnge %s, %s", RQ(l),RQ(r)); }
    void Assembler::CMOVQNB( R l, R r)  { emitrr(X64_cmovqnb, l,r); asm_output("cmovqnb %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNBE(R l, R r)  { emitrr(X64_cmovqnbe,l,r); asm_output("cmovqnbe %s, %s", RQ(l),RQ(r)); }
    void Assembler::CMOVQNA( R l, R r)  { emitrr(X64_cmovqna, l,r); asm_output("cmovqna %s, %s",  RQ(l),RQ(r)); }
    void Assembler::CMOVQNAE(R l, R r)  { emitrr(X64_cmovqnae,l,r); asm_output("cmovqnae %s, %s", RQ(l),RQ(r)); }

    void Assembler::MOVSXDR(R l, R r)   { emitrr(X64_movsxdr,l,r); asm_output("movsxd %s, %s",RQ(l),RL(r)); }

    void Assembler::MOVZX8(R l, R r)    { emitrr8(X64_movzx8,l,r); asm_output("movzx %s, %s",RQ(l),RB(r)); }

// XORPS is a 4x32f vector operation, we use it instead of the more obvious
// XORPD because it's one byte shorter.  This is ok because it's only used for
// zeroing an XMM register;  hence the single argument.
    void Assembler::XORPS(        R r)  { emitprr(X64_xorps,   r,r); asm_output("xorps %s, %s",   RQ(r),RQ(r)); }
    void Assembler::DIVSD(   R l, R r)  { emitprr(X64_divsd,   l,r); asm_output("divsd %s, %s",   RQ(l),RQ(r)); }
    void Assembler::MULSD(   R l, R r)  { emitprr(X64_mulsd,   l,r); asm_output("mulsd %s, %s",   RQ(l),RQ(r)); }
    void Assembler::ADDSD(   R l, R r)  { emitprr(X64_addsd,   l,r); asm_output("addsd %s, %s",   RQ(l),RQ(r)); }
    void Assembler::SUBSD(   R l, R r)  { emitprr(X64_subsd,   l,r); asm_output("subsd %s, %s",   RQ(l),RQ(r)); }
    void Assembler::CVTSQ2SD(R l, R r)  { emitprr(X64_cvtsq2sd,l,r); asm_output("cvtsq2sd %s, %s",RQ(l),RQ(r)); }
    void Assembler::CVTSI2SD(R l, R r)  { emitprr(X64_cvtsi2sd,l,r); asm_output("cvtsi2sd %s, %s",RQ(l),RL(r)); }
    void Assembler::UCOMISD( R l, R r)  { emitprr(X64_ucomisd, l,r); asm_output("ucomisd %s, %s", RQ(l),RQ(r)); }
    void Assembler::MOVQRX(  R l, R r)  { emitprr(X64_movqrx,  r,l); asm_output("movq %s, %s",    RQ(l),RQ(r)); } // Nb: r and l are deliberately reversed within the emitprr() call.
    void Assembler::MOVQXR(  R l, R r)  { emitprr(X64_movqxr,  l,r); asm_output("movq %s, %s",    RQ(l),RQ(r)); }

// MOVI must not affect condition codes!
    void Assembler::MOVI(  R r, I32 i32)    { emitr_imm(X64_movi,  r,i32); asm_output("movl %s, %d",RL(r),i32); }
    void Assembler::ADDLRI(R r, I32 i32)    { emitr_imm(X64_addlri,r,i32); asm_output("addl %s, %d",RL(r),i32); }
    void Assembler::SUBLRI(R r, I32 i32)    { emitr_imm(X64_sublri,r,i32); asm_output("subl %s, %d",RL(r),i32); }
    void Assembler::ANDLRI(R r, I32 i32)    { emitr_imm(X64_andlri,r,i32); asm_output("andl %s, %d",RL(r),i32); }
    void Assembler::ORLRI( R r, I32 i32)    { emitr_imm(X64_orlri, r,i32); asm_output("orl %s, %d", RL(r),i32); }
    void Assembler::XORLRI(R r, I32 i32)    { emitr_imm(X64_xorlri,r,i32); asm_output("xorl %s, %d",RL(r),i32); }
    void Assembler::CMPLRI(R r, I32 i32)    { emitr_imm(X64_cmplri,r,i32); asm_output("cmpl %s, %d",RL(r),i32); }

    void Assembler::ADDQRI( R r, I32 i32)   { emitr_imm(X64_addqri, r,i32); asm_output("addq %s, %d",   RQ(r),i32); }
    void Assembler::SUBQRI( R r, I32 i32)   { emitr_imm(X64_subqri, r,i32); asm_output("subq %s, %d",   RQ(r),i32); }
    void Assembler::ANDQRI( R r, I32 i32)   { emitr_imm(X64_andqri, r,i32); asm_output("andq %s, %d",   RQ(r),i32); }
    void Assembler::ORQRI(  R r, I32 i32)   { emitr_imm(X64_orqri,  r,i32); asm_output("orq %s, %d",    RQ(r),i32); }
    void Assembler::XORQRI( R r, I32 i32)   { emitr_imm(X64_xorqri, r,i32); asm_output("xorq %s, %d",   RQ(r),i32); }
    void Assembler::CMPQRI( R r, I32 i32)   { emitr_imm(X64_cmpqri, r,i32); asm_output("cmpq %s, %d",   RQ(r),i32); }
    void Assembler::MOVQI32(R r, I32 i32)   { emitr_imm(X64_movqi32,r,i32); asm_output("movqi32 %s, %d",RQ(r),i32); }

    void Assembler::ADDLR8(R r, I32 i8)     { emitr_imm8(X64_addlr8,r,i8); asm_output("addl %s, %d", RL(r),i8); }
    void Assembler::SUBLR8(R r, I32 i8)     { emitr_imm8(X64_sublr8,r,i8); asm_output("subl %s, %d", RL(r),i8); }
    void Assembler::ANDLR8(R r, I32 i8)     { emitr_imm8(X64_andlr8,r,i8); asm_output("andl %s, %d", RL(r),i8); }
    void Assembler::ORLR8( R r, I32 i8)     { emitr_imm8(X64_orlr8, r,i8); asm_output("orl %s, %d",  RL(r),i8); }
    void Assembler::XORLR8(R r, I32 i8)     { emitr_imm8(X64_xorlr8,r,i8); asm_output("xorl %s, %d", RL(r),i8); }
    void Assembler::CMPLR8(R r, I32 i8)     { emitr_imm8(X64_cmplr8,r,i8); asm_output("cmpl %s, %d", RL(r),i8); }

    void Assembler::ADDQR8(R r, I32 i8)     { emitr_imm8(X64_addqr8,r,i8); asm_output("addq %s, %d",RQ(r),i8); }
    void Assembler::SUBQR8(R r, I32 i8)     { emitr_imm8(X64_subqr8,r,i8); asm_output("subq %s, %d",RQ(r),i8); }
    void Assembler::ANDQR8(R r, I32 i8)     { emitr_imm8(X64_andqr8,r,i8); asm_output("andq %s, %d",RQ(r),i8); }
    void Assembler::ORQR8( R r, I32 i8)     { emitr_imm8(X64_orqr8, r,i8); asm_output("orq %s, %d", RQ(r),i8); }
    void Assembler::XORQR8(R r, I32 i8)     { emitr_imm8(X64_xorqr8,r,i8); asm_output("xorq %s, %d",RQ(r),i8); }
    void Assembler::CMPQR8(R r, I32 i8)     { emitr_imm8(X64_cmpqr8,r,i8); asm_output("cmpq %s, %d",RQ(r),i8); }

    void Assembler::IMULI(R l, R r, I32 i32)    { emitrr_imm(X64_imuli,l,r,i32); asm_output("imuli %s, %s, %d",RL(l),RL(r),i32); }

    void Assembler::MOVQI(R r, U64 u64)         { emitr_imm64(X64_movqi,r,u64); asm_output("movq %s, %p",RQ(r),(void*)u64); }

    void Assembler::LEARIP(R r, I32 d)          { emitrm(X64_learip,r,d,(Register)0); asm_output("lea %s, %d(rip)",RQ(r),d); }

    void Assembler::LEAQRM(R r1, I d, R r2)     { emitrm(X64_leaqrm,r1,d,r2); asm_output("leaq %s, %d(%s)",RQ(r1),d,RQ(r2)); }
    void Assembler::MOVLRM(R r1, I d, R r2)     { emitrm(X64_movlrm,r1,d,r2); asm_output("movl %s, %d(%s)",RL(r1),d,RQ(r2)); }
    void Assembler::MOVQRM(R r1, I d, R r2)     { emitrm(X64_movqrm,r1,d,r2); asm_output("movq %s, %d(%s)",RQ(r1),d,RQ(r2)); }
    void Assembler::MOVLMR(R r1, I d, R r2)     { emitrm(X64_movlmr,r1,d,r2); asm_output("movl %d(%s), %s",d,RQ(r1),RL(r2)); }
    void Assembler::MOVQMR(R r1, I d, R r2)     { emitrm(X64_movqmr,r1,d,r2); asm_output("movq %d(%s), %s",d,RQ(r1),RQ(r2)); }

    void Assembler::MOVZX8M( R r1, I d, R r2)   { emitrm_wide(X64_movzx8m, r1,d,r2); asm_output("movzxb %s, %d(%s)",RQ(r1),d,RQ(r2)); }
    void Assembler::MOVZX16M(R r1, I d, R r2)   { emitrm_wide(X64_movzx16m,r1,d,r2); asm_output("movzxs %s, %d(%s)",RQ(r1),d,RQ(r2)); }

    void Assembler::MOVSDRM(R r1, I d, R r2)    { emitprm(X64_movsdrm,r1,d,r2); asm_output("movsd %s, %d(%s)",RQ(r1),d,RQ(r2)); }
    void Assembler::MOVSDMR(R r1, I d, R r2)    { emitprm(X64_movsdmr,r1,d,r2); asm_output("movsd %d(%s), %s",d,RQ(r1),RQ(r2)); }

    void Assembler::JMP8( S n, NIns* t)    { emit_target8(n, X64_jmp8,t); asm_output("jmp %p", t); }

    void Assembler::JMP32(S n, NIns* t)    { emit_target32(n,X64_jmp, t); asm_output("jmp %p", t); }

    void Assembler::JMPX(R indexreg, NIns** table)  { emitrxb_imm(X64_jmpx, (R)0, indexreg, (Register)5, (int32_t)(uintptr_t)table); asm_output("jmpq [%s*8 + %p]", RQ(indexreg), (void*)table); }

    void Assembler::JMPXB(R indexreg, R tablereg)   { emitxb(X64_jmpxb, indexreg, tablereg); asm_output("jmp [%s*8 + %s]", RQ(indexreg), RQ(tablereg)); }

    void Assembler::JO( S n, NIns* t)      { emit_target32(n,X64_jo,  t); asm_output("jo %p", t); }
    void Assembler::JE( S n, NIns* t)      { emit_target32(n,X64_je,  t); asm_output("je %p", t); }
    void Assembler::JL( S n, NIns* t)      { emit_target32(n,X64_jl,  t); asm_output("jl %p", t); }
    void Assembler::JLE(S n, NIns* t)      { emit_target32(n,X64_jle, t); asm_output("jle %p",t); }
    void Assembler::JG( S n, NIns* t)      { emit_target32(n,X64_jg,  t); asm_output("jg %p", t); }
    void Assembler::JGE(S n, NIns* t)      { emit_target32(n,X64_jge, t); asm_output("jge %p",t); }
    void Assembler::JB( S n, NIns* t)      { emit_target32(n,X64_jb,  t); asm_output("jb %p", t); }
    void Assembler::JBE(S n, NIns* t)      { emit_target32(n,X64_jbe, t); asm_output("jbe %p",t); }
    void Assembler::JA( S n, NIns* t)      { emit_target32(n,X64_ja,  t); asm_output("ja %p", t); }
    void Assembler::JAE(S n, NIns* t)      { emit_target32(n,X64_jae, t); asm_output("jae %p",t); }
    void Assembler::JP( S n, NIns* t)      { emit_target32(n,X64_jp,  t); asm_output("jp  %p",t); }

    void Assembler::JNO( S n, NIns* t)     { emit_target32(n,X64_jo ^X64_jneg, t); asm_output("jno %p", t); }
    void Assembler::JNE( S n, NIns* t)     { emit_target32(n,X64_je ^X64_jneg, t); asm_output("jne %p", t); }
    void Assembler::JNL( S n, NIns* t)     { emit_target32(n,X64_jl ^X64_jneg, t); asm_output("jnl %p", t); }
    void Assembler::JNLE(S n, NIns* t)     { emit_target32(n,X64_jle^X64_jneg, t); asm_output("jnle %p",t); }
    void Assembler::JNG( S n, NIns* t)     { emit_target32(n,X64_jg ^X64_jneg, t); asm_output("jng %p", t); }
    void Assembler::JNGE(S n, NIns* t)     { emit_target32(n,X64_jge^X64_jneg, t); asm_output("jnge %p",t); }
    void Assembler::JNB( S n, NIns* t)     { emit_target32(n,X64_jb ^X64_jneg, t); asm_output("jnb %p", t); }
    void Assembler::JNBE(S n, NIns* t)     { emit_target32(n,X64_jbe^X64_jneg, t); asm_output("jnbe %p",t); }
    void Assembler::JNA( S n, NIns* t)     { emit_target32(n,X64_ja ^X64_jneg, t); asm_output("jna %p", t); }
    void Assembler::JNAE(S n, NIns* t)     { emit_target32(n,X64_jae^X64_jneg, t); asm_output("jnae %p",t); }

    void Assembler::JO8( S n, NIns* t)     { emit_target8(n,X64_jo8,  t); asm_output("jo %p", t); }
    void Assembler::JE8( S n, NIns* t)     { emit_target8(n,X64_je8,  t); asm_output("je %p", t); }
    void Assembler::JL8( S n, NIns* t)     { emit_target8(n,X64_jl8,  t); asm_output("jl %p", t); }
    void Assembler::JLE8(S n, NIns* t)     { emit_target8(n,X64_jle8, t); asm_output("jle %p",t); }
    void Assembler::JG8( S n, NIns* t)     { emit_target8(n,X64_jg8,  t); asm_output("jg %p", t); }
    void Assembler::JGE8(S n, NIns* t)     { emit_target8(n,X64_jge8, t); asm_output("jge %p",t); }
    void Assembler::JB8( S n, NIns* t)     { emit_target8(n,X64_jb8,  t); asm_output("jb %p", t); }
    void Assembler::JBE8(S n, NIns* t)     { emit_target8(n,X64_jbe8, t); asm_output("jbe %p",t); }
    void Assembler::JA8( S n, NIns* t)     { emit_target8(n,X64_ja8,  t); asm_output("ja %p", t); }
    void Assembler::JAE8(S n, NIns* t)     { emit_target8(n,X64_jae8, t); asm_output("jae %p",t); }
    void Assembler::JP8( S n, NIns* t)     { emit_target8(n,X64_jp8,  t); asm_output("jp  %p",t); }

    void Assembler::JNO8( S n, NIns* t)    { emit_target8(n,X64_jo8 ^X64_jneg8, t); asm_output("jno %p", t); }
    void Assembler::JNE8( S n, NIns* t)    { emit_target8(n,X64_je8 ^X64_jneg8, t); asm_output("jne %p", t); }
    void Assembler::JNL8( S n, NIns* t)    { emit_target8(n,X64_jl8 ^X64_jneg8, t); asm_output("jnl %p", t); }
    void Assembler::JNLE8(S n, NIns* t)    { emit_target8(n,X64_jle8^X64_jneg8, t); asm_output("jnle %p",t); }
    void Assembler::JNG8( S n, NIns* t)    { emit_target8(n,X64_jg8 ^X64_jneg8, t); asm_output("jng %p", t); }
    void Assembler::JNGE8(S n, NIns* t)    { emit_target8(n,X64_jge8^X64_jneg8, t); asm_output("jnge %p",t); }
    void Assembler::JNB8( S n, NIns* t)    { emit_target8(n,X64_jb8 ^X64_jneg8, t); asm_output("jnb %p", t); }
    void Assembler::JNBE8(S n, NIns* t)    { emit_target8(n,X64_jbe8^X64_jneg8, t); asm_output("jnbe %p",t); }
    void Assembler::JNA8( S n, NIns* t)    { emit_target8(n,X64_ja8 ^X64_jneg8, t); asm_output("jna %p", t); }
    void Assembler::JNAE8(S n, NIns* t)    { emit_target8(n,X64_jae8^X64_jneg8, t); asm_output("jnae %p",t); }

    void Assembler::CALL( S n, NIns* t)    { emit_target32(n,X64_call,t); asm_output("call %p",t); }

    void Assembler::CALLRAX()       { emit(X64_callrax); asm_output("call (rax)"); }
    void Assembler::RET()           { emit(X64_ret);     asm_output("ret");        }

    void Assembler::MOVQSPR(I d, R r)   { emit(X64_movqspr | U64(d) << 56 | U64((r&7)<<3) << 40 | U64((r&8)>>1) << 24); asm_output("movq %d(rsp), %s", d, RQ(r)); }    // insert r into mod/rm and rex bytes

    void Assembler::XORPSA(R r, I32 i32)    { emitxm_abs(X64_xorpsa, r, i32); asm_output("xorps %s, (0x%x)",RQ(r), i32); }
    void Assembler::XORPSM(R r, NIns* a64)  { emitxm_rel(X64_xorpsm, r, a64); asm_output("xorps %s, (%p)",  RQ(r), a64); }

    void Assembler::X86_AND8R(R r)  { emit(X86_and8r | U64(r<<3|(r|4))<<56); asm_output("andb %s, %s", RB(r), RBhi(r)); }
    void Assembler::X86_SETNP(R r)  { emit(X86_setnp | U64(r|4)<<56); asm_output("setnp %s", RBhi(r)); }
    void Assembler::X86_SETE(R r)   { emit(X86_sete  | U64(r)<<56);   asm_output("sete %s", RB(r)); }

#undef R
#undef I
#undef I32
#undef U64
#undef S

    void Assembler::MR(Register d, Register s) {
        NanoAssert(IsGpReg(d) && IsGpReg(s));
        MOVQR(d, s);
    }

    // This is needed for guards;  we must be able to patch the jmp later and
    // we cannot do that if an 8-bit relative jump is used, so we can't use
    // JMP().
    void Assembler::JMPl(NIns* target) {
        JMP32(8, target);
    }

    void Assembler::JMP(NIns *target) {
        if (!target || isTargetWithinS32(target)) {
            if (target && isTargetWithinS8(target)) {
                JMP8(8, target);
            } else {
                JMP32(8, target);
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
        // if this is last use of a in reg, we can re-use result reg
        if (a->isUnusedOrHasUnknownReg()) {
            ra = findSpecificRegForUnallocated(a, rr);
        } else if (!(allow & rmask(a->getReg()))) {
            // 'a' already has a register assigned, but it's not valid.
            // To make sure floating point operations stay in FPU registers
            // as much as possible, make sure that only a few opcodes are
            // reserving GPRs.
            NanoAssert(a->isop(LIR_quad) || a->isop(LIR_ldq) || a->isop(LIR_ldqc)|| a->isop(LIR_u2f) || a->isop(LIR_float));
            allow &= ~rmask(rr);
            ra = findRegFor(a, allow);
        } else {
            ra = a->getReg();
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
        switch (ins->opcode()) {
        default:
            TODO(asm_shift);
        case LIR_qursh: SHRQ(rr);   break;
        case LIR_qirsh: SARQ(rr);   break;
        case LIR_qilsh: SHLQ(rr);   break;
        case LIR_ush:   SHR( rr);   break;
        case LIR_rsh:   SAR( rr);   break;
        case LIR_lsh:   SHL( rr);   break;
        }
        if (rr != ra)
            MR(rr, ra);
    }

    void Assembler::asm_shift_imm(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        int shift = ins->oprnd2()->imm32() & 63;
        switch (ins->opcode()) {
        default: TODO(shiftimm);
        case LIR_qursh: SHRQI(rr, shift);   break;
        case LIR_qirsh: SARQI(rr, shift);   break;
        case LIR_qilsh: SHLQI(rr, shift);   break;
        case LIR_ush:   SHRI( rr, shift);   break;
        case LIR_rsh:   SARI( rr, shift);   break;
        case LIR_lsh:   SHLI( rr, shift);   break;
        }
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
            IMULI(rr, ra, imm);
            return;
        }
        regalloc_unary(ins, GpRegs, rr, ra);
        if (isS8(imm)) {
            switch (ins->opcode()) {
            default: TODO(arith_imm8);
            case LIR_iaddp:
            case LIR_add:   ADDLR8(rr, imm);   break;
            case LIR_and:   ANDLR8(rr, imm);   break;
            case LIR_or:    ORLR8( rr, imm);   break;
            case LIR_sub:   SUBLR8(rr, imm);   break;
            case LIR_xor:   XORLR8(rr, imm);   break;
            case LIR_qiadd:
            case LIR_qaddp: ADDQR8(rr, imm);   break;
            case LIR_qiand: ANDQR8(rr, imm);   break;
            case LIR_qior:  ORQR8( rr, imm);   break;
            case LIR_qxor:  XORQR8(rr, imm);   break;
            }
        } else {
            switch (ins->opcode()) {
            default: TODO(arith_imm);
            case LIR_iaddp:
            case LIR_add:   ADDLRI(rr, imm);   break;
            case LIR_and:   ANDLRI(rr, imm);   break;
            case LIR_or:    ORLRI( rr, imm);   break;
            case LIR_sub:   SUBLRI(rr, imm);   break;
            case LIR_xor:   XORLRI(rr, imm);   break;
            case LIR_qiadd:
            case LIR_qaddp: ADDQRI(rr, imm);   break;
            case LIR_qiand: ANDQRI(rr, imm);   break;
            case LIR_qior:  ORQRI( rr, imm);   break;
            case LIR_qxor:  XORQRI(rr, imm);   break;
            }
        }
        if (rr != ra)
            MR(rr, ra);
    }

    void Assembler::asm_div_mod(LIns *ins) {
        LIns *div;
        if (ins->opcode() == LIR_mod) {
            // LIR_mod expects the LIR_div to be near
            div = ins->oprnd1();
            prepResultReg(ins, rmask(RDX));
        } else {
            div = ins;
            evictIfActive(RDX);
        }

        NanoAssert(div->isop(LIR_div));

        LIns *lhs = div->oprnd1();
        LIns *rhs = div->oprnd2();

        prepResultReg(div, rmask(RAX));

        Register rhsReg = findRegFor(rhs, (GpRegs ^ (rmask(RAX)|rmask(RDX))));
        Register lhsReg = lhs->isUnusedOrHasUnknownReg()
                          ? findSpecificRegForUnallocated(lhs, RAX)
                          : lhs->getReg();
        IDIV(rhsReg);
        SARI(RDX, 31);
        MR(RDX, RAX);
        if (RAX != lhsReg)
            MR(RAX, lhsReg);
    }

    // binary op with integer registers
    void Assembler::asm_arith(LIns *ins) {
        Register rr, ra, rb;

        switch (ins->opcode() & ~LIR64) {
        case LIR_lsh:
        case LIR_rsh:
        case LIR_ush:
            asm_shift(ins);
            return;
        case LIR_mod:
        case LIR_div:
            asm_div_mod(ins);
            return;
        default:
            break;
        }

        LIns *b = ins->oprnd2();
        if (isImm32(b)) {
            asm_arith_imm(ins);
            return;
        }
        regalloc_binary(ins, GpRegs, rr, ra, rb);
        switch (ins->opcode()) {
        default:        TODO(asm_arith);
        case LIR_or:    ORLRR(rr, rb);  break;
        case LIR_sub:   SUBRR(rr, rb);  break;
        case LIR_iaddp:
        case LIR_add:   ADDRR(rr, rb);  break;
        case LIR_and:   ANDRR(rr, rb);  break;
        case LIR_xor:   XORRR(rr, rb);  break;
        case LIR_mul:   IMUL(rr, rb);   break;
        case LIR_qxor:  XORQRR(rr, rb); break;
        case LIR_qior:  ORQRR(rr, rb);  break;
        case LIR_qiand: ANDQRR(rr, rb); break;
        case LIR_qiadd:
        case LIR_qaddp: ADDQRR(rr, rb); break;
        }
        if (rr != ra)
            MR(rr,ra);
    }

    // binary op with fp registers
    void Assembler::asm_fop(LIns *ins) {
        Register rr, ra, rb;
        regalloc_binary(ins, FpRegs, rr, ra, rb);
        switch (ins->opcode()) {
        default:       TODO(asm_fop);
        case LIR_fdiv: DIVSD(rr, rb); break;
        case LIR_fmul: MULSD(rr, rb); break;
        case LIR_fadd: ADDSD(rr, rb); break;
        case LIR_fsub: SUBSD(rr, rb); break;
        }
        if (rr != ra) {
            asm_nongp_copy(rr, ra);
        }
    }

    void Assembler::asm_neg_not(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        NanoAssert(IsGpReg(ra));
        if (ins->isop(LIR_not))
            NOT(rr);
        else
            NEG(rr);
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
            if (isTargetWithinS32(target)) {
                CALL(8, target);
            } else {
                // can't reach target from here, load imm64 and do an indirect jump
                CALLRAX();
                asm_quad(RAX, (uint64_t)target);
            }
        } else {
            // Indirect call: we assign the address arg to RAX since it's not
            // used for regular arguments, and is otherwise scratch since it's
            // clobberred by the call.
            asm_regarg(ARGSIZE_P, ins->arg(--argc), RAX);
            CALLRAX();
        }

    #ifdef _WIN64
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
        #ifdef _WIN64
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
                asm_quad(r, int64_t(p->imm32()));
                return;
            }
            // sign extend int32 to int64
            MOVSXDR(r, r);
        } else if (sz == ARGSIZE_U) {
            NanoAssert(!p->isQuad());
            if (p->isconst()) {
                asm_quad(r, uint64_t(uint32_t(p->imm32())));
                return;
            }
            // zero extend with 32bit mov, auto-zeros upper 32bits
            MOVLR(r, r);
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
            MOVQSPR(stk_off, r);    // movq [rsp+d8], r
            if (sz == ARGSIZE_I) {
                // extend int32 to int64
                NanoAssert(!p->isQuad());
                MOVSXDR(r, r);
            } else if (sz == ARGSIZE_U) {
                // extend uint32 to uint64
                NanoAssert(!p->isQuad());
                MOVLR(r, r);
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
            MOVLR(rr, ra);      // 32bit mov zeros the upper 32bits of the target
        } else {
            NanoAssert(ins->isop(LIR_i2q));
            MOVSXDR(rr, ra);    // sign extend 32->64
        }
    }

    // the CVTSI2SD instruction only writes to the low 64bits of the target
    // XMM register, which hinders register renaming and makes dependence
    // chains longer.  So we precede with XORPS to clear the target register.

    void Assembler::asm_i2f(LIns *ins) {
        Register r = prepResultReg(ins, FpRegs);
        Register b = findRegFor(ins->oprnd1(), GpRegs);
        CVTSI2SD(r, b);     // cvtsi2sd xmmr, b  only writes xmm:0:64
        XORPS(r);           // xorps xmmr,xmmr to break dependency chains
    }

    void Assembler::asm_u2f(LIns *ins) {
        Register r = prepResultReg(ins, FpRegs);
        Register b = findRegFor(ins->oprnd1(), GpRegs);
        NanoAssert(!ins->oprnd1()->isQuad());
        // since oprnd1 value is 32bit, its okay to zero-extend the value without worrying about clobbering.
        CVTSQ2SD(r, b);     // convert int64 to double
        XORPS(r);           // xorps xmmr,xmmr to break dependency chains
        MOVLR(b, b);        // zero extend u32 to int64
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

        LOpcode condop = cond->opcode();
        if (ins->opcode() == LIR_cmov) {
            switch (condop & ~LIR64) {
            case LIR_ov:  CMOVNO( rr, rf);  break;
            case LIR_eq:  CMOVNE( rr, rf);  break;
            case LIR_lt:  CMOVNL( rr, rf);  break;
            case LIR_gt:  CMOVNG( rr, rf);  break;
            case LIR_le:  CMOVNLE(rr, rf);  break;
            case LIR_ge:  CMOVNGE(rr, rf);  break;
            case LIR_ult: CMOVNB( rr, rf);  break;
            case LIR_ugt: CMOVNA( rr, rf);  break;
            case LIR_ule: CMOVNBE(rr, rf);  break;
            case LIR_uge: CMOVNAE(rr, rf);  break;
            default:      NanoAssert(0);    break;
            }
        } else {
            switch (condop & ~LIR64) {
            case LIR_ov:  CMOVQNO( rr, rf); break;
            case LIR_eq:  CMOVQNE( rr, rf); break;
            case LIR_lt:  CMOVQNL( rr, rf); break;
            case LIR_gt:  CMOVQNG( rr, rf); break;
            case LIR_le:  CMOVQNLE(rr, rf); break;
            case LIR_ge:  CMOVQNGE(rr, rf); break;
            case LIR_ult: CMOVQNB( rr, rf); break;
            case LIR_ugt: CMOVQNA( rr, rf); break;
            case LIR_ule: CMOVQNBE(rr, rf); break;
            case LIR_uge: CMOVQNAE(rr, rf); break;
            default:      NanoAssert(0);    break;
            }
        }
        /*const Register rt =*/ findSpecificRegFor(iftrue, rr);
        asm_cmp(cond);
    }

    NIns* Assembler::asm_branch(bool onFalse, LIns *cond, NIns *target) {
        LOpcode condop = cond->opcode();
        if (condop >= LIR_feq && condop <= LIR_fge)
            return asm_fbranch(onFalse, cond, target);

        // we must ensure there's room for the instr before calculating
        // the offset.  and the offset, determines the opcode (8bit or 32bit)
        NanoAssert((condop & ~LIR64) >= LIR_ov);
        NanoAssert((condop & ~LIR64) <= LIR_uge);
        if (target && isTargetWithinS8(target)) {
            if (onFalse) {
                switch (condop & ~LIR64) {
                case LIR_ov:  JNO8( 8, target); break;
                case LIR_eq:  JNE8( 8, target); break;
                case LIR_lt:  JNL8( 8, target); break;
                case LIR_gt:  JNG8( 8, target); break;
                case LIR_le:  JNLE8(8, target); break;
                case LIR_ge:  JNGE8(8, target); break;
                case LIR_ult: JNB8( 8, target); break;
                case LIR_ugt: JNA8( 8, target); break;
                case LIR_ule: JNBE8(8, target); break;
                case LIR_uge: JNAE8(8, target); break;
                default:      NanoAssert(0);    break;
                }
            } else {
                switch (condop & ~LIR64) {
                case LIR_ov:  JO8( 8, target);  break;
                case LIR_eq:  JE8( 8, target);  break;
                case LIR_lt:  JL8( 8, target);  break;
                case LIR_gt:  JG8( 8, target);  break;
                case LIR_le:  JLE8(8, target);  break;
                case LIR_ge:  JGE8(8, target);  break;
                case LIR_ult: JB8( 8, target);  break;
                case LIR_ugt: JA8( 8, target);  break;
                case LIR_ule: JBE8(8, target);  break;
                case LIR_uge: JAE8(8, target);  break;
                default:      NanoAssert(0);    break;
                }
            }
        } else {
            if (onFalse) {
                switch (condop & ~LIR64) {
                case LIR_ov:  JNO( 8, target);  break;
                case LIR_eq:  JNE( 8, target);  break;
                case LIR_lt:  JNL( 8, target);  break;
                case LIR_gt:  JNG( 8, target);  break;
                case LIR_le:  JNLE(8, target);  break;
                case LIR_ge:  JNGE(8, target);  break;
                case LIR_ult: JNB( 8, target);  break;
                case LIR_ugt: JNA( 8, target);  break;
                case LIR_ule: JNBE(8, target);  break;
                case LIR_uge: JNAE(8, target);  break;
                default:      NanoAssert(0);    break;
                }
            } else {
                switch (condop & ~LIR64) {
                case LIR_ov:  JO( 8, target);   break;
                case LIR_eq:  JE( 8, target);   break;
                case LIR_lt:  JL( 8, target);   break;
                case LIR_gt:  JG( 8, target);   break;
                case LIR_le:  JLE(8, target);   break;
                case LIR_ge:  JGE(8, target);   break;
                case LIR_ult: JB( 8, target);   break;
                case LIR_ugt: JA( 8, target);   break;
                case LIR_ule: JBE(8, target);   break;
                case LIR_uge: JAE(8, target);   break;
                default:      NanoAssert(0);    break;
                }
            }
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
            findRegFor2(GpRegs, a, ra, b, rb);
        } else {
            // optimize-me: this will produce a const result!
            ra = rb = findRegFor(a, GpRegs);
        }

        LOpcode condop = cond->opcode();
        if (condop & LIR64)
            CMPQR(ra, rb);
        else
            CMPLR(ra, rb);
    }

    void Assembler::asm_cmp_imm(LIns *cond) {
        LIns *a = cond->oprnd1();
        LIns *b = cond->oprnd2();
        Register ra = findRegFor(a, GpRegs);
        int32_t imm = getImm32(b);
        if (isS8(imm)) {
            if (cond->opcode() & LIR64)
                CMPQR8(ra, imm);
            else 
                CMPLR8(ra, imm);
        } else {
            if (cond->opcode() & LIR64)
                CMPQRI(ra, imm);
            else
                CMPLRI(ra, imm);
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
                JP(16, target);     // underrun of 12 needed, round up for overhang --> 16
                JNE(0, target);     // no underrun needed, previous was enough
                patch = _nIns;
            } else {
                // jp skip (2byte)
                // jeq target
                // skip: ...
                underrunProtect(16); // underrun of 7 needed but we write 2 instr --> 16
                NIns *skip = _nIns;
                JE(0, target);      // no underrun needed, previous was enough
                patch = _nIns;
                JP8(0, skip);       // ditto
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
            if (condop == LIR_fgt) {
                if (onFalse)
                    JBE(8, target);
                else
                    JA(8, target);
            } else { // LIR_fge
                if (onFalse)
                    JB(8, target);
                else
                    JAE(8, target);
            }
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
            MOVZX8(r, r);       // movzx8   r,rl     r[8:63] = 0
            X86_AND8R(r);       // and      rl,rh    rl &= rh
            X86_SETNP(r);       // setnp    rh       rh = !PF
            X86_SETE(r);        // sete     rl       rl = ZF
        } else {
            if (op == LIR_flt) {
                op = LIR_fgt;
                LIns *t = a; a = b; b = t;
            } else if (op == LIR_fle) {
                op = LIR_fge;
                LIns *t = a; a = b; b = t;
            }
            Register r = prepResultReg(ins, GpRegs); // x64 can use any GPR as setcc target
            MOVZX8(r, r);
            if (op == LIR_fgt) 
                SETA(r);
            else
                SETAE(r);
        }
        fcmp(a, b);
    }

    void Assembler::fcmp(LIns *a, LIns *b) {
        Register ra, rb;
        findRegFor2(FpRegs, a, ra, b, rb);
        UCOMISD(ra, rb);
    }

    void Assembler::asm_restore(LIns *ins, Register r) {
        if (ins->isop(LIR_alloc)) {
            int d = disp(ins);
            LEAQRM(r, d, FP);
        }
        else if (ins->isconst()) {
            if (!ins->getArIndex()) {
                ins->markAsClear();
            }
            // unsafe to use xor r,r for zero because it changes cc's
            MOVI(r, ins->imm32());
        }
        else if (ins->isconstq() && IsGpReg(r)) {
            if (!ins->getArIndex()) {
                ins->markAsClear();
            }
            // unsafe to use xor r,r for zero because it changes cc's
            asm_quad(r, ins->imm64());
        }
        else {
            int d = findMemFor(ins);
            if (IsFpReg(r)) {
                NanoAssert(ins->isQuad());
                // load 64bits into XMM.  don't know if double or int64, assume double.
                MOVSDRM(r, d, FP);
            } else if (ins->isQuad()) {
                MOVQRM(r, d, FP);
            } else {
                MOVLRM(r, d, FP);
            }
        }
    }

    void Assembler::asm_cond(LIns *ins) {
        LOpcode op = ins->opcode();
        // unlike x86-32, with a rex prefix we can use any GP register as an 8bit target
        Register r = prepResultReg(ins, GpRegs);
        // SETcc only sets low 8 bits, so extend
        MOVZX8(r, r);
        switch (op) {
        default:
            TODO(cond);
        case LIR_qeq:
        case LIR_eq:    SETE(r);    break;
        case LIR_qlt:
        case LIR_lt:    SETL(r);    break;
        case LIR_qle:
        case LIR_le:    SETLE(r);   break;
        case LIR_qgt:
        case LIR_gt:    SETG(r);    break;
        case LIR_qge:
        case LIR_ge:    SETGE(r);   break;
        case LIR_qult:
        case LIR_ult:   SETB(r);    break;
        case LIR_qule:
        case LIR_ule:   SETBE(r);   break;
        case LIR_qugt:
        case LIR_ugt:   SETA(r);    break;
        case LIR_quge:
        case LIR_uge:   SETAE(r);   break;
        case LIR_ov:    SETO(r);    break;
        }
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
            MOVQRX(d, s);
        } else if (IsFpReg(d) && IsFpReg(s)) {
            // xmm <- xmm: use movaps. movsd r,r causes partial register stall
            MOVAPSR(d, s);
        } else {
            // xmm <- gpr: use movq xmm, r/m64 (66 REX.W 0F 6E /r)
            MOVQXR(d, s);
        }
    }

    void Assembler::regalloc_load(LIns *ins, Register &rr, int32_t &dr, Register &rb) {
        dr = ins->disp();
        LIns *base = ins->oprnd1();
        rb = getBaseReg(ins->opcode(), base, dr, BaseRegs);
        if (ins->isUnusedOrHasUnknownReg()) {
            // use a gpr in case we're copying a non-double
            rr = prepResultReg(ins, GpRegs & ~rmask(rb));
        } else {
            // keep already assigned register
            rr = ins->getReg();
            freeRsrcOf(ins, false);
        }
    }

    void Assembler::asm_load64(LIns *ins) {
        Register rr, rb;
        int32_t dr;
        regalloc_load(ins, rr, dr, rb);
        if (IsGpReg(rr)) {
            // general 64bit load, 32bit const displacement
            MOVQRM(rr, dr, rb);
        } else {
            // load 64bits into XMM.  don't know if double or int64, assume double.
            MOVSDRM(rr, dr, rb);
        }
    }

    void Assembler::asm_ld(LIns *ins) {
        NanoAssert(!ins->isQuad());
        Register r, b;
        int32_t d;
        regalloc_load(ins, r, d, b);
        LOpcode op = ins->opcode();
        switch (op) {
        case LIR_ldcb: MOVZX8M( r, d, b);   break;
        case LIR_ldcs: MOVZX16M(r, d, b);   break;
        default:       MOVLRM(  r, d, b);   break;
        }
    }

    void Assembler::asm_store64(LIns *value, int d, LIns *base) {
        NanoAssert(value->isQuad());
        Register b = getBaseReg(LIR_stqi, base, d, BaseRegs);

        // if we have to choose a register, use a GPR, but not the base reg
        Register r;
        if (value->isUnusedOrHasUnknownReg()) {
            RegisterMask allow;
            // XXX: isFloat doesn't cover float/fmod!  see bug 520208.
            if (value->isFloat() || value->isop(LIR_float) || value->isop(LIR_fmod)) {
                allow = FpRegs;
            } else {
                allow = GpRegs;
            }
            r = findRegFor(value, allow & ~rmask(b));
        } else {
            r = value->getReg();
        }

        if (IsGpReg(r)) {
            // gpr store
            MOVQMR(r, d, b);
        }
        else {
            // xmm store
            MOVSDMR(r, d, b);
        }
    }

    void Assembler::asm_store32(LIns *value, int d, LIns *base) {
        NanoAssert(!value->isQuad());
        Register b = getBaseReg(LIR_sti, base, d, BaseRegs);
        Register r = findRegFor(value, GpRegs & ~rmask(b));

        // store 32bits to 64bit addr.  use rex so we can use all 16 regs
        MOVLMR(r, d, b);
    }

    // generate a 64bit constant, must not affect condition codes!
    void Assembler::asm_quad(Register r, uint64_t v) {
        NanoAssert(IsGpReg(r));
        if (isU32(v)) {
            MOVI(r, int32_t(v));
        } else if (isS32(v)) {
            // safe for sign-extension 32->64
            MOVQI32(r, int32_t(v));
        } else if (isTargetWithinS32((NIns*)v)) {
            // value is with +/- 2GB from RIP, can use LEA with RIP-relative disp32
            int32_t d = int32_t(int64_t(v)-int64_t(_nIns));
            LEARIP(r, d);
        } else {
            MOVQI(r, v);
        }
    }

    void Assembler::asm_int(LIns *ins) {
        Register r = prepResultReg(ins, GpRegs);
        int32_t v = ins->imm32();
        if (v == 0) {
            // special case for zero
            XORRR(r, r);
        } else {
            MOVI(r, v);
        }
    }

    void Assembler::asm_quad(LIns *ins) {
        uint64_t v = ins->imm64();
        RegisterMask allow = v == 0 ? GpRegs|FpRegs : GpRegs;
        Register r = prepResultReg(ins, allow);
        if (v == 0) {
            if (IsGpReg(r)) {
                // special case for zero
                XORRR(r, r);
            } else {
                // xorps for xmm
                XORPS(r);
            }
        } else {
            asm_quad(r, v);
        }
    }

    void Assembler::asm_qjoin(LIns*) {
        TODO(asm_qjoin);
    }

    Register Assembler::asm_prep_fcall(LIns *ins) {
        return prepResultReg(ins, rmask(XMM0));
    }

    void Assembler::asm_param(LIns *ins) {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        if (kind == 0) {
            // ordinary param
            // first four or six args always in registers for x86_64 ABI
            if (a < (uint32_t)NumArgRegs) {
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
        // if this is last use of a in reg, we can re-use result reg
        if (a->isUnusedOrHasUnknownReg()) {
            ra = findSpecificRegForUnallocated(a, rr);
        } else {
            // 'a' already has a register assigned.  Caller must emit a copy
            // to rr once instr code is generated.  (ie  mov rr,ra ; op rr)
            ra = a->getReg();
        }
        NanoAssert(allow & rmask(rr));
    }

    static const AVMPLUS_ALIGN16(int64_t) negateMask[] = {0x8000000000000000LL,0};

    void Assembler::asm_fneg(LIns *ins) {
        Register rr, ra;
        if (isS32((uintptr_t)negateMask) || isTargetWithinS32((NIns*)negateMask)) {
            regalloc_unary(ins, FpRegs, rr, ra);
            if (isS32((uintptr_t)negateMask)) {
                // builtin code is in bottom or top 2GB addr space, use absolute addressing
                XORPSA(rr, (int32_t)(uintptr_t)negateMask);
            } else {
                // jit code is within +/-2GB of builtin code, use rip-relative
                XORPSM(rr, (NIns*)negateMask);
            }
            if (ra != rr)
                asm_nongp_copy(rr,ra);
        } else {
            // this is just hideous - can't use RIP-relative load, can't use
            // absolute-address load, and cant move imm64 const to XMM.
            // so do it all in a GPR.  hrmph.
            rr = prepResultReg(ins, GpRegs);
            ra = findRegFor(ins->oprnd1(), GpRegs & ~rmask(rr));
            XORQRR(rr, ra);                     // xor rr, ra
            asm_quad(rr, negateMask[0]);        // mov rr, 0x8000000000000000
        }
    }

    void Assembler::asm_qhi(LIns*) {
        TODO(asm_qhi);
    }

    void Assembler::asm_qlo(LIns *ins) {
        Register rr, ra;
        regalloc_unary(ins, GpRegs, rr, ra);
        NanoAssert(IsGpReg(ra));
        MOVLR(rr, ra);  // 32bit mov zeros the upper 32bits of the target
    }

    void Assembler::asm_spill(Register rr, int d, bool /*pop*/, bool quad) {
        if (d) {
            if (!IsFpReg(rr)) {
                if (quad)
                    MOVQMR(rr, d, FP);
                else
                    MOVLMR(rr, d, FP);
            } else {
                // store 64bits from XMM to memory
                NanoAssert(quad);
                MOVSDMR(rr, d, FP);
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
                SUBQR8(RSP, amt);
            else
                SUBQRI(RSP, amt);
        }

        verbose_only( outputAddr=true; asm_output("[patch entry]"); )
        NIns *patchEntry = _nIns;
        MR(FP, RSP);    // Establish our own FP.
        PUSHR(FP);      // Save caller's FP.

        return patchEntry;
    }

    NIns* Assembler::genEpilogue() {
        // pop rbp
        // ret
        RET();
        POPR(RBP);
        return _nIns;
    }

    void Assembler::nRegisterResetAll(RegAlloc &a) {
        // add scratch registers to our free list for the allocator
        a.clear();
#ifdef _WIN64
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
    #if defined _MSC_VER
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
        asm_quad(RAX, uintptr_t(lr));
    }

    void Assembler::nInit(AvmCore*) {
    }

    void Assembler::nBeginAssembly() {
        max_stk_used = 0;
    }

    // This should only be called from within emit() et al.
    void Assembler::underrunProtect(ptrdiff_t bytes) {
        NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");
        NIns *pc = _nIns;
        NIns *top = codeStart;  // this may be in a normal code chunk or an exit code chunk

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
                // This may be in a normal code chunk or an exit code chunk.
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
            // This may be in a normal code chunk or an exit code chunk.
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            // This jump will call underrunProtect again, but since we're on a new
            // page, nothing will happen.
            JMP(pc);
        }
    #endif
    }

    RegisterMask Assembler::hint(LIns *, RegisterMask allow) {
        return allow;
    }

    void Assembler::nativePageSetup() {
        NanoAssert(!_inExit);
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
    void Assembler::asm_inc_m32(uint32_t* /*pCtr*/)
    {
        // todo: implement this
    }
    )

    void Assembler::asm_jtbl(LIns* ins, NIns** table)
    {
        // exclude R12 because ESP and R12 cannot be used as an index
        // (index=100 in SIB means "none")
        Register indexreg = findRegFor(ins->oprnd1(), GpRegs & ~rmask(R12));
        if (isS32((intptr_t)table)) {
            // table is in low 2GB or high 2GB, can use absolute addressing
            // jmpq [indexreg*8 + table]
            JMPX(indexreg, table);
        } else {
            // don't use R13 for base because we want to use mod=00, i.e. [index*8+base + 0]
            Register tablereg = registerAllocTmp(GpRegs & ~(rmask(indexreg)|rmask(R13)));
            // jmp [indexreg*8 + tablereg]
            JMPXB(indexreg, tablereg);
            // tablereg <- #table
            asm_quad(tablereg, (uint64_t)table);
        }
    }

    void Assembler::swapCodeChunks() {
        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);
        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }

} // namespace nanojit

#endif // FEATURE_NANOJIT && NANOJIT_X64
