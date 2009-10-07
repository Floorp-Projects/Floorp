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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __nanojit_NativeX64__
#define __nanojit_NativeX64__

#ifndef NANOJIT_64BIT
#error "NANOJIT_64BIT must be defined for X64 backend"
#endif

#ifdef PERFM
#define DOPROF
#include "../vprof/vprof.h"
#define count_instr() _nvprof("x64",1)
#define count_prolog() _nvprof("x64-prolog",1); count_instr();
#define count_imt() _nvprof("x64-imt",1) count_instr()
#else
#define count_instr()
#define count_prolog()
#define count_imt()
#endif

namespace nanojit
{
#define NJ_MAX_STACK_ENTRY              256
#define NJ_ALIGN_STACK                  16

    enum Register {
        RAX = 0, // 1st int return, # of sse varargs
        RCX = 1, // 4th int arg
        RDX = 2, // 3rd int arg 2nd return
        RBX = 3, // saved
        RSP = 4, // stack ptr
        RBP = 5, // frame ptr, saved, sib reqd
        RSI = 6, // 2nd int arg
        RDI = 7, // 1st int arg
        R8  = 8, // 5th int arg
        R9  = 9, // 6th int arg
        R10 = 10, // scratch
        R11 = 11, // scratch
        R12 = 12, // saved
        R13 = 13, // saved, sib reqd like rbp
        R14 = 14, // saved
        R15 = 15, // saved

        XMM0  = 16, // 1st double arg, return
        XMM1  = 17, // 2nd double arg, return
        XMM2  = 18, // 3rd double arg
        XMM3  = 19, // 4th double arg
        XMM4  = 20, // 5th double arg
        XMM5  = 21, // 6th double arg
        XMM6  = 22, // 7th double arg
        XMM7  = 23, // 8th double arg
        XMM8  = 24, // scratch
        XMM9  = 25, // scratch
        XMM10 = 26, // scratch
        XMM11 = 27, // scratch
        XMM12 = 28, // scratch
        XMM13 = 29, // scratch
        XMM14 = 30, // scratch
        XMM15 = 31, // scratch

        FP = RBP,
        UnknownReg = 32,
        FirstReg = RAX,
        LastReg = XMM15
    };

/*
 * Micro-templating variable-length opcodes, idea first
 * describe by Mike Pall of Luajit.
 *
 * X86-64 opcode encodings:  LSB encodes the length of the
 * opcode in bytes, remaining bytes are encoded as 1-7 bytes
 * in a single uint64_t value.  The value is written as a single
 * store into the code stream, and the code pointer is decremented
 * by the length.  each successive instruction partially overlaps
 * the previous one.
 *
 * emit methods below are able to encode mod/rm, sib, rex, and
 * register and small immediate values into these opcode values
 * without much branchy code.
 *
 * these opcodes encapsulate all the const parts of the instruction.
 * for example, the alu-immediate opcodes (add, sub, etc) encode
 * part of their opcode in the R field of the mod/rm byte;  this
 * hardcoded value is in the constant below, and the R argument
 * to emitrr() is 0.  In a few cases, a whole instruction is encoded
 * this way (eg callrax).
 *
 * when a disp32, imm32, or imm64 suffix can't fit in an 8-byte
 * opcode, then it is written into the code separately and not counted
 * in the opcode length.
 */

    enum X64Opcode
#if defined(_MSC_VER) && _MSC_VER >= 1400
#pragma warning(disable:4480) // nonstandard extension used: specifying underlying type for enum
          : uint64_t
#endif
    {
        // 64bit opcode constants
        //              msb        lsb len
        X64_addqrr  = 0xC003480000000003LL, // 64bit add r += b
        X64_addqri  = 0xC081480000000003LL, // 64bit add r += int64(imm32)
        X64_addqr8  = 0x00C0834800000004LL, // 64bit add r += int64(imm8)
        X64_andqri  = 0xE081480000000003LL, // 64bit and r &= int64(imm32)
        X64_andqr8  = 0x00E0834800000004LL, // 64bit and r &= int64(imm8)
        X64_orqri   = 0xC881480000000003LL, // 64bit or  r |= int64(imm32)
        X64_orqr8   = 0x00C8834800000004LL, // 64bit or  r |= int64(imm8)
        X64_xorqri  = 0xF081480000000003LL, // 64bit xor r ^= int64(imm32)
        X64_xorqr8  = 0x00F0834800000004LL, // 64bit xor r ^= int64(imm8)
        X64_addlri  = 0xC081400000000003LL, // 32bit add r += imm32
        X64_addlr8  = 0x00C0834000000004LL, // 32bit add r += imm8
        X64_andlri  = 0xE081400000000003LL, // 32bit and r &= imm32
        X64_andlr8  = 0x00E0834000000004LL, // 32bit and r &= imm8
        X64_orlri   = 0xC881400000000003LL, // 32bit or  r |= imm32
        X64_orlr8   = 0x00C8834000000004LL, // 32bit or  r |= imm8
        X64_sublri  = 0xE881400000000003LL, // 32bit sub r -= imm32
        X64_sublr8  = 0x00E8834000000004LL, // 32bit sub r -= imm8
        X64_xorlri  = 0xF081400000000003LL, // 32bit xor r ^= imm32
        X64_xorlr8  = 0x00F0834000000004LL, // 32bit xor r ^= imm8
        X64_addrr   = 0xC003400000000003LL, // 32bit add r += b
        X64_andqrr  = 0xC023480000000003LL, // 64bit and r &= b
        X64_andrr   = 0xC023400000000003LL, // 32bit and r &= b
        X64_call    = 0x00000000E8000005LL, // near call
        X64_callrax = 0xD0FF000000000002LL, // indirect call to addr in rax (no REX)
        X64_cmovqno = 0xC0410F4800000004LL, // 64bit conditional mov if (no overflow) r = b
        X64_cmovqb  = 0xC0420F4800000004LL, // 64bit conditional mov if (uint <) r = b
        X64_cmovqae = 0xC0430F4800000004LL, // 64bit conditional mov if (uint >=) r = b
        X64_cmovqne = 0xC0450F4800000004LL, // 64bit conditional mov if (c) r = b
        X64_cmovqbe = 0xC0460F4800000004LL, // 64bit conditional mov if (uint <=) r = b
        X64_cmovqa  = 0xC0470F4800000004LL, // 64bit conditional mov if (uint >) r = b
        X64_cmovql  = 0xC04C0F4800000004LL, // 64bit conditional mov if (int <) r = b
        X64_cmovqge = 0xC04D0F4800000004LL, // 64bit conditional mov if (int >=) r = b
        X64_cmovqle = 0xC04E0F4800000004LL, // 64bit conditional mov if (int <=) r = b
        X64_cmovqg  = 0xC04F0F4800000004LL, // 64bit conditional mov if (int >) r = b
        X64_cmovno  = 0xC0410F4000000004LL, // 32bit conditional mov if (no overflow) r = b
        X64_cmovb   = 0xC0420F4000000004LL, // 32bit conditional mov if (uint <) r = b
        X64_cmovae  = 0xC0430F4000000004LL, // 32bit conditional mov if (uint >=) r = b
        X64_cmovne  = 0xC0450F4000000004LL, // 32bit conditional mov if (c) r = b
        X64_cmovbe  = 0xC0460F4000000004LL, // 32bit conditional mov if (uint <=) r = b
        X64_cmova   = 0xC0470F4000000004LL, // 32bit conditional mov if (uint >) r = b
        X64_cmovl   = 0xC04C0F4000000004LL, // 32bit conditional mov if (int <) r = b
        X64_cmovge  = 0xC04D0F4000000004LL, // 32bit conditional mov if (int >=) r = b
        X64_cmovle  = 0xC04E0F4000000004LL, // 32bit conditional mov if (int <=) r = b
        X64_cmovg   = 0xC04F0F4000000004LL, // 32bit conditional mov if (int >) r = b
        X64_cmov_64 = 0x0000000800000000LL, // OR with 32-bit cmov to promote to 64-bit
        X64_cmplr   = 0xC03B400000000003LL, // 32bit compare r,b
        X64_cmpqr   = 0xC03B480000000003LL, // 64bit compare r,b
        X64_cmplri  = 0xF881400000000003LL, // 32bit compare r,imm32
        X64_cmpqri  = 0xF881480000000003LL, // 64bit compare r,int64(imm32)
        X64_cmplr8  = 0x00F8834000000004LL, // 32bit compare r,imm8
        X64_cmpqr8  = 0x00F8834800000004LL, // 64bit compare r,int64(imm8)
        X64_cvtsi2sd= 0xC02A0F40F2000005LL, // convert int32 to double r = (double) b
        X64_cvtsq2sd= 0xC02A0F48F2000005LL, // convert int64 to double r = (double) b
        X64_divsd   = 0xC05E0F40F2000005LL, // divide scalar double r /= b
        X64_mulsd   = 0xC0590F40F2000005LL, // multiply scalar double r *= b
        X64_addsd   = 0xC0580F40F2000005LL, // add scalar double r += b
        X64_idiv    = 0xF8F7400000000003LL, // 32bit signed div (rax = rdx:rax/r, rdx=rdx:rax%r)
        X64_imul    = 0xC0AF0F4000000004LL, // 32bit signed mul r *= b
        X64_imuli   = 0xC069400000000003LL, // 32bit signed mul r = b * imm32
        X64_imul8   = 0x00C06B4000000004LL, // 32bit signed mul r = b * imm8
        X64_jmp     = 0x00000000E9000005LL, // jump near rel32
        X64_jmp8    = 0x00EB000000000002LL, // jump near rel8
        X64_jo      = 0x00000000800F0006LL, // jump near if overflow
        X64_jb      = 0x00000000820F0006LL, // jump near if below (uint <)
        X64_jae     = 0x00000000830F0006LL, // jump near if above or equal (uint >=)
        X64_ja      = 0x00000000870F0006LL, // jump near if above (uint >)
        X64_jbe     = 0x00000000860F0006LL, // jump near if below or equal (uint <=)
        X64_je      = 0x00000000840F0006LL, // near jump if equal
        X64_jne     = 0x00000000850F0006LL, // jump near if not equal
        X64_jl      = 0x000000008C0F0006LL, // jump near if less (int <)
        X64_jge     = 0x000000008D0F0006LL, // jump near if greater or equal (int >=)
        X64_jg      = 0x000000008F0F0006LL, // jump near if greater (int >)
        X64_jle     = 0x000000008E0F0006LL, // jump near if less or equal (int <=)
        X64_jp      = 0x000000008A0F0006LL, // jump near if parity (PF == 1)
        X64_jnp     = 0x000000008B0F0006LL, // jump near if not parity (PF == 0)
        X64_jneg    = 0x0000000001000000LL, // xor with this mask to negate the condition
        X64_jo8     = 0x0070000000000002LL, // jump near if overflow
        X64_jb8     = 0x0072000000000002LL, // jump near if below (uint <)
        X64_jae8    = 0x0073000000000002LL, // jump near if above or equal (uint >=)
        X64_ja8     = 0x0077000000000002LL, // jump near if above (uint >)
        X64_jbe8    = 0x0076000000000002LL, // jump near if below or equal (uint <=)
        X64_je8     = 0x0074000000000002LL, // near jump if equal
        X64_jne8    = 0x0075000000000002LL, // jump near if not equal
        X64_jl8     = 0x007C000000000002LL, // jump near if less (int <)
        X64_jge8    = 0x007D000000000002LL, // jump near if greater or equal (int >=)
        X64_jg8     = 0x007F000000000002LL, // jump near if greater (int >)
        X64_jle8    = 0x007E000000000002LL, // jump near if less or equal (int <=)
        X64_jp8     = 0x007A000000000002LL, // jump near if parity (PF == 1)
        X64_jnp8    = 0x007B000000000002LL, // jump near if not parity (PF == 0)
        X64_jneg8   = 0x0001000000000000LL, // xor with this mask to negate the condition
        X64_leaqrm  = 0x00000000808D4807LL, // 64bit load effective addr reg <- disp32+base
        X64_learm   = 0x00000000808D4007LL, // 32bit load effective addr reg <- disp32+base
        X64_movlr   = 0xC08B400000000003LL, // 32bit mov r <- b
        X64_movlmr  = 0x0000000080894007LL, // 32bit store r -> [b+d32]
        X64_movlrm  = 0x00000000808B4007LL, // 32bit load r <- [b+d32]
        X64_movqmr  = 0x0000000080894807LL, // 64bit store gpr -> [b+d32]
        X64_movqspr = 0x0024448948000005LL, // 64bit store gpr -> [rsp+d32] (sib required)
        X64_movqr   = 0xC08B480000000003LL, // 64bit mov r <- b
        X64_movqi   = 0xB848000000000002LL, // 64bit mov r <- imm64
        X64_movi    = 0xB840000000000002LL, // 32bit mov r <- imm32
        X64_movqi32 = 0xC0C7480000000003LL, // 64bit mov r <- int64(imm32)
        X64_movapsr = 0xC0280F4000000004LL, // 128bit mov xmm <- xmm
        X64_movqrx  = 0xC07E0F4866000005LL, // 64bit mov b <- xmm-r
        X64_movqxr  = 0xC06E0F4866000005LL, // 64bit mov b -> xmm-r
        X64_movqrm  = 0x00000000808B4807LL, // 64bit load r <- [b+d32]
        X64_movsdrr = 0xC0100F40F2000005LL, // 64bit mov xmm-r <- xmm-b (upper 64bits unchanged)
        X64_movsdrm = 0x80100F40F2000005LL, // 64bit load xmm-r <- [b+d32] (upper 64 cleared)
        X64_movsdmr = 0x80110F40F2000005LL, // 64bit store xmm-r -> [b+d32]
        X64_movsxdr = 0xC063480000000003LL, // sign extend i32 to i64 r = (int64)(int32) b
        X64_movzx8  = 0xC0B60F4000000004LL, // zero extend i8 to i64 r = (uint64)(uint8) b
        X64_movzx8m = 0x80B60F4000000004LL, // zero extend i8 load to i32 r <- [b+d32]
        X64_movzx16m= 0x80B70F4000000004LL, // zero extend i16 load to i32 r <- [b+d32]
        X64_neg     = 0xD8F7400000000003LL, // 32bit two's compliment b = -b
        X64_nop1    = 0x9000000000000001LL, // one byte NOP
        X64_nop2    = 0x9066000000000002LL, // two byte NOP
        X64_nop3    = 0x001F0F0000000003LL, // three byte NOP
        X64_nop4    = 0x00401F0F00000004LL, // four byte NOP
        X64_nop5    = 0x0000441F0F000005LL, // five byte NOP
        X64_nop6    = 0x0000441F0F660006LL, // six byte NOP
        X64_nop7    = 0x00000000801F0F07LL, // seven byte NOP
        X64_not     = 0xD0F7400000000003LL, // 32bit ones compliment b = ~b
        X64_orlrr   = 0xC00B400000000003LL, // 32bit or r |= b
        X64_orqrr   = 0xC00B480000000003LL, // 64bit or r |= b
        X64_popr    = 0x5840000000000002LL, // 64bit pop r <- [rsp++]
        X64_pushr   = 0x5040000000000002LL, // 64bit push r -> [--rsp]
        X64_pxor    = 0xC0EF0F4066000005LL, // 128bit xor xmm-r ^= xmm-b
        X64_ret     = 0xC300000000000001LL, // near return from called procedure
        X64_sete    = 0xC0940F4000000004LL, // set byte if equal (ZF == 1)
        X64_seto    = 0xC0900F4000000004LL, // set byte if overflow (OF == 1)
        X64_setc    = 0xC0920F4000000004LL, // set byte if carry (CF == 1)
        X64_setl    = 0xC09C0F4000000004LL, // set byte if less (int <) (SF != OF)
        X64_setle   = 0xC09E0F4000000004LL, // set byte if less or equal (int <=) (ZF == 1 || SF != OF)
        X64_setg    = 0xC09F0F4000000004LL, // set byte if greater (int >) (ZF == 0 && SF == OF)
        X64_setge   = 0xC09D0F4000000004LL, // set byte if greater or equal (int >=) (SF == OF)
        X64_seta    = 0xC0970F4000000004LL, // set byte if above (uint >) (CF == 0 && ZF == 0)
        X64_setae   = 0xC0930F4000000004LL, // set byte if above or equal (uint >=) (CF == 0)
        X64_setb    = 0xC0920F4000000004LL, // set byte if below (uint <) (CF == 1)
        X64_setbe   = 0xC0960F4000000004LL, // set byte if below or equal (uint <=) (ZF == 1 || CF == 1)
        X64_subsd   = 0xC05C0F40F2000005LL, // subtract scalar double r -= b
        X64_shl     = 0xE0D3400000000003LL, // 32bit left shift r <<= rcx
        X64_shlq    = 0xE0D3480000000003LL, // 64bit left shift r <<= rcx
        X64_shr     = 0xE8D3400000000003LL, // 32bit uint right shift r >>= rcx
        X64_shrq    = 0xE8D3480000000003LL, // 64bit uint right shift r >>= rcx
        X64_sar     = 0xF8D3400000000003LL, // 32bit int right shift r >>= rcx
        X64_sarq    = 0xF8D3480000000003LL, // 64bit int right shift r >>= rcx
        X64_shli    = 0x00E0C14000000004LL, // 32bit left shift r <<= imm8
        X64_shlqi   = 0x00E0C14800000004LL, // 64bit left shift r <<= imm8
        X64_sari    = 0x00F8C14000000004LL, // 32bit int right shift r >>= imm8
        X64_sarqi   = 0x00F8C14800000004LL, // 64bit int right shift r >>= imm8
        X64_shri    = 0x00E8C14000000004LL, // 32bit uint right shift r >>= imm8
        X64_shrqi   = 0x00E8C14800000004LL, // 64bit uint right shift r >>= imm8
        X64_subqrr  = 0xC02B480000000003LL, // 64bit sub r -= b
        X64_subrr   = 0xC02B400000000003LL, // 32bit sub r -= b
        X64_subqri  = 0xE881480000000003LL, // 64bit sub r -= int64(imm32)
        X64_subqr8  = 0x00E8834800000004LL, // 64bit sub r -= int64(imm8)
        X64_ucomisd = 0xC02E0F4066000005LL, // unordered compare scalar double
        X64_xorqrr  = 0xC033480000000003LL, // 64bit xor r &= b
        X64_xorrr   = 0xC033400000000003LL, // 32bit xor r &= b
        X64_xorpd   = 0xC0570F4066000005LL, // 128bit xor xmm (two packed doubles)
        X64_xorps   = 0xC0570F4000000004LL, // 128bit xor xmm (four packed singles), one byte shorter
        X64_xorpsm  = 0x05570F4000000004LL, // 128bit xor xmm, [rip+disp32]
        X64_xorpsa  = 0x2504570F40000005LL, // 128bit xor xmm, [disp32]

        X86_and8r   = 0xC022000000000002LL, // and rl,rh
        X86_sete    = 0xC0940F0000000003LL, // no-rex version of X64_sete
        X86_setnp   = 0xC09B0F0000000003LL  // no-rex set byte if odd parity (ordered fcmp result) (PF == 0)
    };

    typedef uint32_t RegisterMask;

    static const RegisterMask GpRegs = 0xffff;
    static const RegisterMask FpRegs = 0xffff0000;
    static const bool CalleeRegsNeedExplicitSaving = true;
#ifdef _MSC_VER
    static const RegisterMask SavedRegs = 1<<RBX | 1<<RSI | 1<<RDI | 1<<R12 | 1<<R13 | 1<<R14 | 1<<R15;
    static const int NumSavedRegs = 7; // rbx, rsi, rdi, r12-15
    static const int NumArgRegs = 4;
#else
    static const RegisterMask SavedRegs = 1<<RBX | 1<<R12 | 1<<R13 | 1<<R14 | 1<<R15;
    static const int NumSavedRegs = 5; // rbx, r12-15
    static const int NumArgRegs = 6;
#endif

    static inline bool isValidDisplacement(int32_t d) {
        return true;
    }
    static inline bool IsFpReg(Register r) {
        return ((1<<r) & FpRegs) != 0;
    }
    static inline bool IsGpReg(Register r) {
        return ((1<<r) & GpRegs) != 0;
    }

    verbose_only( extern const char* regNames[]; )

    #define DECLARE_PLATFORM_STATS()
    #define DECLARE_PLATFORM_REGALLOC()

    #define DECLARE_PLATFORM_ASSEMBLER()                                    \
        const static Register argRegs[NumArgRegs], retRegs[1];              \
        void underrunProtect(ptrdiff_t bytes);                              \
        void nativePageReset();                                             \
        void nativePageSetup();                                             \
        void asm_qbinop(LIns*);                                             \
        void MR(Register, Register);\
        void JMP(NIns*);\
        void JMPl(NIns*);\
        void emit(uint64_t op);\
        void emit8(uint64_t op, int64_t val);\
        void emit32(uint64_t op, int64_t val);\
        void emitrr(uint64_t op, Register r, Register b);\
        void emitrr8(uint64_t op, Register r, Register b);\
        void emitr(uint64_t op, Register b) { emitrr(op, (Register)0, b); }\
        void emitr8(uint64_t op, Register b) { emitrr8(op, (Register)0, b); }\
        void emitprr(uint64_t op, Register r, Register b);\
        void emitrm(uint64_t op, Register r, int32_t d, Register b);\
        void emitrm_wide(uint64_t op, Register r, int32_t d, Register b);\
        uint64_t emit_disp32(uint64_t op, int32_t d);\
        void emitprm(uint64_t op, Register r, int32_t d, Register b);\
        void emitrr_imm(uint64_t op, Register r, Register b, int32_t imm);\
        void emitr_imm(uint64_t op, Register r, int32_t imm) { emitrr_imm(op, (Register)0, r, imm); }\
        void emitr_imm8(uint64_t op, Register b, int32_t imm8);\
        void emit_int(Register r, int32_t v);\
        void emit_quad(Register r, uint64_t v);\
        void asm_regarg(ArgSize, LIns*, Register);\
        void asm_stkarg(ArgSize, LIns*, int);\
        void asm_shift(LIns*);\
        void asm_shift_imm(LIns*);\
        void asm_arith_imm(LIns*);\
        void regalloc_unary(LIns *ins, RegisterMask allow, Register &rr, Register &ra);\
        void regalloc_binary(LIns *ins, RegisterMask allow, Register &rr, Register &ra, Register &rb);\
        void regalloc_load(LIns *ins, Register &rr, int32_t &d, Register &rb);\
        void dis(NIns *p, int bytes);\
        void asm_cmp(LIns*);\
        void asm_cmp_imm(LIns*);\
        void fcmp(LIns*, LIns*);\
        NIns* asm_fbranch(bool, LIns*, NIns*);\
        void asm_div_mod(LIns *i);\
        int max_stk_used;

    #define swapptrs()  { NIns* _tins = _nIns; _nIns=_nExitIns; _nExitIns=_tins; }

    const int LARGEST_UNDERRUN_PROT = 32;  // largest value passed to underrunProtect

    typedef uint8_t NIns;

    inline Register nextreg(Register r) {
        return Register(r+1);
    }

} // namespace nanojit

#endif // __nanojit_NativeX64__
