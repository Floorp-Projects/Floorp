/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2009, 2010 University of Szeged
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_ARMAssembler_h
#define assembler_assembler_ARMAssembler_h

#include "assembler/wtf/Platform.h"

// Some debug code uses s(n)printf for instruction logging.
#include <stdio.h>

#if ENABLE_ASSEMBLER && WTF_CPU_ARM_TRADITIONAL

#include "assembler/assembler/AssemblerBufferWithConstantPool.h"
#include "assembler/wtf/Assertions.h"

// TODO: We don't print the condition code in our spew lines. Doing this
// is awkward whilst maintaining a consistent field width.
namespace js {
    namespace jit {
        class Assembler;
    }
}

namespace JSC {

    typedef uint32_t ARMWord;

    namespace ARMRegisters {
        typedef enum {
            r0 = 0,
            r1,
            r2,
            r3,
            S0 = r3,
            r4,
            r5,
            r6,
            r7,
            r8,
            S1 = r8,
            r9,
            r10,
            r11,
            r12,
            ip = r12,
            r13,
            sp = r13,
            r14,
            lr = r14,
            r15,
            pc = r15,
            invalid_reg
        } RegisterID;

        typedef enum {
            d0,
            d1,
            d2,
            d3,
            SD0 = d3,
            d4,
            d5,
            d6,
            d7,
            d8,
            d9,
            d10,
            d11,
            d12,
            d13,
            d14,
            d15,
            d16,
            d17,
            d18,
            d19,
            d20,
            d21,
            d22,
            d23,
            d24,
            d25,
            d26,
            d27,
            d28,
            d29,
            d30,
            d31,
            invalid_freg
        } FPRegisterID;

        inline FPRegisterID floatShadow(FPRegisterID s)
        {
            return (FPRegisterID)(s*2);
        }
        inline FPRegisterID doubleShadow(FPRegisterID d)
        {
            return (FPRegisterID)(d / 2);
        }
    } // namespace ARMRegisters
    class ARMAssembler : public GenericAssembler {
    public:

        typedef ARMRegisters::RegisterID RegisterID;
        typedef ARMRegisters::FPRegisterID FPRegisterID;
        typedef AssemblerBufferWithConstantPool<2048, 4, 4, ARMAssembler> ARMBuffer;
        typedef SegmentedVector<int, 64> Jumps;

        unsigned char *buffer() const { return m_buffer.buffer(); }
        bool oom() const { return m_buffer.oom(); }

        // ARM conditional constants
        typedef enum {
            EQ = 0x00000000, // Zero
            NE = 0x10000000, // Non-zero
            CS = 0x20000000,
            CC = 0x30000000,
            MI = 0x40000000,
            PL = 0x50000000,
            VS = 0x60000000,
            VC = 0x70000000,
            HI = 0x80000000,
            LS = 0x90000000,
            GE = 0xa0000000,
            LT = 0xb0000000,
            GT = 0xc0000000,
            LE = 0xd0000000,
            AL = 0xe0000000
        } Condition;

        // ARM instruction constants
        enum {
            AND = (0x0 << 21),
            EOR = (0x1 << 21),
            SUB = (0x2 << 21),
            RSB = (0x3 << 21),
            ADD = (0x4 << 21),
            ADC = (0x5 << 21),
            SBC = (0x6 << 21),
            RSC = (0x7 << 21),
            TST = (0x8 << 21),
            TEQ = (0x9 << 21),
            CMP = (0xa << 21),
            CMN = (0xb << 21),
            ORR = (0xc << 21),
            MOV = (0xd << 21),
            BIC = (0xe << 21),
            MVN = (0xf << 21),
            MUL = 0x00000090,
            MULL = 0x00c00090,
            FCPYD = 0x0eb00b40,
            FADDD = 0x0e300b00,
            FNEGD = 0x0eb10b40,
            FABSD = 0x0eb00bc0,
            FDIVD = 0x0e800b00,
            FSUBD = 0x0e300b40,
            FMULD = 0x0e200b00,
            FCMPD = 0x0eb40b40,
            FSQRTD = 0x0eb10bc0,
            DTR = 0x05000000,
            LDRH = 0x00100090,
            STRH = 0x00000090,
            DTRH = 0x00000090,
            STMDB = 0x09200000,
            LDMIA = 0x08b00000,
            B = 0x0a000000,
            BL = 0x0b000000
#if WTF_ARM_ARCH_VERSION >= 5 || defined(__ARM_ARCH_4T__)
           ,BX = 0x012fff10
#endif
#if WTF_ARM_ARCH_VERSION >= 5
           ,CLZ = 0x016f0f10,
            BKPT = 0xe1200070,
            BLX = 0x012fff30
#endif
#if WTF_ARM_ARCH_VERSION >= 7
           ,MOVW = 0x03000000,
            MOVT = 0x03400000
#endif
        };

        enum {
            OP2_IMM = (1 << 25),
            OP2_IMMh = (1 << 22),
            OP2_INV_IMM = (1 << 26),
            SET_CC = (1 << 20),
            OP2_OFSREG = (1 << 25),
            DT_UP = (1 << 23),
            DT_BYTE = (1 << 22),
            DT_WB = (1 << 21),
            // This flag is inlcuded in LDR and STR
            DT_PRE = (1 << 24),
            // This flag makes switches the instruction between {ld,st}r{,s}h and {ld,st}rsb
            HDT_UH = (1 << 5),
            // if this bit is on, we do a register offset, if it is off, we do an immediate offest.
            HDT_IMM = (1 << 22), 
            // Differentiates half word load/store between signed and unsigned (also enables signed byte loads.)
            HDT_S = (1 << 6),
            DT_LOAD = (1 << 20)
        };

        // Masks of ARM instructions
        enum {
            BRANCH_MASK = 0x00ffffff,
            NONARM = 0xf0000000,
            SDT_MASK = 0x0c000000,
            SDT_OFFSET_MASK = 0xfff
        };

        enum {
            BOFFSET_MIN = -0x00800000,
            BOFFSET_MAX = 0x007fffff,
            SDT = 0x04000000
        };

        enum {
            padForAlign8  = (int)0x00,
            padForAlign16 = (int)0x0000,
            padForAlign32 = (int)0xe12fff7f  // 'bkpt 0xffff'
        };

        typedef enum {
            LSL = 0,
            LSR = 1,
            ASR = 2,
            ROR = 3
        } Shift;

        static const ARMWord INVALID_IMM = 0xf0000000;
        static const ARMWord InvalidBranchTarget = 0xffffffff;
        static const int DefaultPrefetching = 2;

        class JmpSrc {
            friend class ARMAssembler;
            friend class js::jit::Assembler;
        public:
            JmpSrc()
                : m_offset(-1)
            {
            }
            int offset() {return m_offset;}

            bool isSet() const {
                return m_offset != -1;
            }

        private:
            JmpSrc(int offset)
                : m_offset(offset)
            {
            }

            int m_offset;
        };

        class JmpDst {
            friend class ARMAssembler;
            friend class js::jit::Assembler;
        public:
            JmpDst()
                : m_offset(-1)
                , m_used(false)
            {
            }

            bool isUsed() const { return m_used; }
            void used() { m_used = true; }
            bool isValid() const { return m_offset != -1; }
        private:
            JmpDst(int offset)
                : m_offset(offset)
                , m_used(false)
            {
                ASSERT(m_offset == offset);
            }

            int m_offset : 31;
            bool m_used : 1;
        };

        // Instruction formating

        void emitInst(ARMWord op, int rd, int rn, ARMWord op2)
        {
            ASSERT ( ((op2 & ~OP2_IMM) <= 0xfff) || (((op2 & ~OP2_IMMh) <= 0xfff)) );
            m_buffer.putInt(op | RN(rn) | RD(rd) | op2);
        }

        // Work out the pre-shifted constant necessary to encode the specified
        // logical shift left for op2 immediates. Only even shifts can be
        // applied.
        //
        // Input validity is asserted in debug builds.
        ARMWord getOp2RotLSL(int lsl)
        {
            ASSERT((lsl >= 0) && (lsl <= 24));
            ASSERT(!(lsl % 2));

            return (-(lsl/2) & 0xf) << 8;
        }

        void and_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("and", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | AND, rd, rn, op2);
        }

        void ands_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("ands", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | AND | SET_CC, rd, rn, op2);
        }

        void eor_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("eor", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | EOR, rd, rn, op2);
        }

        void eors_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("eors", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | EOR | SET_CC, rd, rn, op2);
        }

        void sub_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("sub", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | SUB, rd, rn, op2);
        }

        void subs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("subs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | SUB | SET_CC, rd, rn, op2);
        }

        void rsb_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("rsb", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | RSB, rd, rn, op2);
        }

        void rsbs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("rsbs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | RSB | SET_CC, rd, rn, op2);
        }

        void add_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("add", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ADD, rd, rn, op2);
        }

        void adds_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("adds", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ADD | SET_CC, rd, rn, op2);
        }

        void adc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("adc", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ADC, rd, rn, op2);
        }

        void adcs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("adcs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ADC | SET_CC, rd, rn, op2);
        }

        void sbc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("sbc", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | SBC, rd, rn, op2);
        }

        void sbcs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("sbcs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | SBC | SET_CC, rd, rn, op2);
        }

        void rsc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("rsc", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | RSC, rd, rn, op2);
        }

        void rscs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("rscs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | RSC | SET_CC, rd, rn, op2);
        }

        void tst_r(int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("tst", cc, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | TST | SET_CC, 0, rn, op2);
        }

        void teq_r(int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("teq", cc, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | TEQ | SET_CC, 0, rn, op2);
        }

        void cmp_r(int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("cmp", cc, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | CMP | SET_CC, 0, rn, op2);
        }

        void cmn_r(int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("cmn", cc, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | CMN | SET_CC, 0, rn, op2);
        }

        void orr_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("orr", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ORR, rd, rn, op2);
        }

        void orrs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("orrs", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | ORR | SET_CC, rd, rn, op2);
        }

        void mov_r(int rd, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("mov", cc, rd, op2);
            emitInst(static_cast<ARMWord>(cc) | MOV, rd, ARMRegisters::r0, op2);
        }

#if WTF_ARM_ARCH_VERSION >= 7
        void movw_r(int rd, ARMWord op2, Condition cc = AL)
        {
            ASSERT((op2 | 0xf0fff) == 0xf0fff);
            spew("%-15s %s, 0x%04x", "movw", nameGpReg(rd), (op2 & 0xfff) | ((op2 >> 4) & 0xf000));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MOVW | RD(rd) | op2);
        }

        void movt_r(int rd, ARMWord op2, Condition cc = AL)
        {
            ASSERT((op2 | 0xf0fff) == 0xf0fff);
            spew("%-15s %s, 0x%04x", "movt", nameGpReg(rd), (op2 & 0xfff) | ((op2 >> 4) & 0xf000));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MOVT | RD(rd) | op2);
        }
#endif

        void movs_r(int rd, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("movs", cc, rd, op2);
            emitInst(static_cast<ARMWord>(cc) | MOV | SET_CC, rd, ARMRegisters::r0, op2);
        }

        void bic_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("bic", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | BIC, rd, rn, op2);
        }

        void bics_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("bics", cc, rd, rn, op2);
            emitInst(static_cast<ARMWord>(cc) | BIC | SET_CC, rd, rn, op2);
        }

        void mvn_r(int rd, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("mvn", cc, rd, op2);
            emitInst(static_cast<ARMWord>(cc) | MVN, rd, ARMRegisters::r0, op2);
        }

        void mvns_r(int rd, ARMWord op2, Condition cc = AL)
        {
            spewInsWithOp2("mvns", cc, rd, op2);
            emitInst(static_cast<ARMWord>(cc) | MVN | SET_CC, rd, ARMRegisters::r0, op2);
        }

        void mul_r(int rd, int rn, int rm, Condition cc = AL)
        {
            spewInsWithOp2("mul", cc, rd, rn, static_cast<ARMWord>(rm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MUL | RN(rd) | RS(rn) | RM(rm));
        }

        void muls_r(int rd, int rn, int rm, Condition cc = AL)
        {
            spewInsWithOp2("muls", cc, rd, rn, static_cast<ARMWord>(rm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MUL | SET_CC | RN(rd) | RS(rn) | RM(rm));
        }

        void mull_r(int rdhi, int rdlo, int rn, int rm, Condition cc = AL)
        {
            spew("%-15s %s, %s, %s, %s", "mull", nameGpReg(rdlo), nameGpReg(rdhi), nameGpReg(rn), nameGpReg(rm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MULL | RN(rdhi) | RD(rdlo) | RS(rn) | RM(rm));
        }

        // pc relative loads (useful for loading from pools).
        void ldr_imm(int rd, ARMWord imm, Condition cc = AL)
        {
#if defined(JS_METHODJIT_SPEW)
            char mnemonic[16];
            snprintf(mnemonic, 16, "ldr%s", nameCC(cc));
            spew("%-15s %s, =0x%x @ (%d) (reusable pool entry)", mnemonic, nameGpReg(rd), imm, static_cast<int32_t>(imm));
#endif
            m_buffer.putIntWithConstantInt(static_cast<ARMWord>(cc) | DTR | DT_LOAD | DT_UP | RN(ARMRegisters::pc) | RD(rd), imm, true);
        }

        void ldr_un_imm(int rd, ARMWord imm, Condition cc = AL)
        {
#if defined(JS_METHODJIT_SPEW)
            char mnemonic[16];
            snprintf(mnemonic, 16, "ldr%s", nameCC(cc));
            spew("%-15s %s, =0x%x @ (%d)", mnemonic, nameGpReg(rd), imm, static_cast<int32_t>(imm));
#endif
            m_buffer.putIntWithConstantInt(static_cast<ARMWord>(cc) | DTR | DT_LOAD | DT_UP | RN(ARMRegisters::pc) | RD(rd), imm);
        }

        void mem_imm_off(bool isLoad, bool isSigned, int size, bool posOffset,
                         int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            ASSERT(size == 8 || size == 16 || size == 32);
            char const * mnemonic_act = (isLoad) ? ("ld") : ("st");
            char const * mnemonic_sign = (isSigned) ? ("s") : ("");
            
            char const * mnemonic_size = NULL;
            switch (size / 8) {
            case 1:
                mnemonic_size = "b";
                break;
            case 2:
                mnemonic_size = "h";
                break;
            case 4:
                mnemonic_size = "";
                break;
            }
            char const * off_sign = (posOffset) ? ("+") : ("-");
            spew("%sr%s%s %s, [%s, #%s%u]",
                 mnemonic_act, mnemonic_sign, mnemonic_size,
                 nameGpReg(rd), nameGpReg(rb), off_sign, offset);
            if (size == 32 || (size == 8 && !isSigned)) {
                /* All (the one) 32 bit ops and the unsigned 8 bit ops use the original encoding.*/
                emitInst(static_cast<ARMWord>(cc) | DTR |
                         (isLoad ? DT_LOAD : 0) |
                         (size == 8 ? DT_BYTE : 0) |
                         (posOffset ? DT_UP : 0), rd, rb, offset);
            } else {
                /* All 16 bit ops and 8 bit unsigned use the newer encoding.*/
                emitInst(static_cast<ARMWord>(cc) | DTRH | HDT_IMM | DT_PRE |
                         (isLoad ? DT_LOAD : 0) |
                         (size == 16 ? HDT_UH : 0) |
                         (isSigned ? HDT_S : 0) |
                         (posOffset ? DT_UP : 0), rd, rb, offset);
            }
        }

        void mem_reg_off(bool isLoad, bool isSigned, int size, bool posOffset, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic_act = (isLoad) ? ("ld") : ("st");
            char const * mnemonic_sign = (isSigned) ? ("s") : ("");

            char const * mnemonic_size = NULL;
            switch (size / 8) {
            case 1:
                mnemonic_size = "b";
                break;
            case 2:
                mnemonic_size = "h";
                break;
            case 4:
                mnemonic_size = "";
                break;
            }
            char const * off_sign = (posOffset) ? ("+") : ("-");
            spew("%sr%s%s %s, [%s, #%s%s]", mnemonic_act, mnemonic_sign, mnemonic_size,
                 nameGpReg(rd), nameGpReg(rb), off_sign, nameGpReg(rm));
            if (size == 32 || (size == 8 && !isSigned)) {
                /* All (the one) 32 bit ops and the signed 8 bit ops use the original encoding.*/
                emitInst(static_cast<ARMWord>(cc) | DTR |
                         (isLoad ? DT_LOAD : 0) |
                         (size == 8 ? DT_BYTE : 0) |
                         (posOffset ? DT_UP : 0) |
                         OP2_OFSREG, rd, rb, rm);
            } else {
                /* All 16 bit ops and 8 bit unsigned use the newer encoding.*/
                emitInst(static_cast<ARMWord>(cc) | DTRH | DT_PRE |
                         (isLoad ? DT_LOAD : 0) |
                         (size == 16 ? HDT_UH : 0) |
                         (isSigned ? HDT_S : 0) |
                         (posOffset ? DT_UP : 0), rd, rb, rm);
            }
        }

        // Data transfers like this:
        //  LDR rd, [rb, +offset]
        //  STR rd, [rb, +offset]
        void dtr_u(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            spew("%-15s %s, [%s, #+%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDR rd, [rb, +rm]
        //  STR rd, [rb, +rm]
        void dtr_ur(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            spew("%-15s %s, [%s, +%s]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDR rd, [rb, -offset]
        //  STR rd, [rb, -offset]
        void dtr_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            spew("%-15s %s, [%s, #-%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDR rd, [rb, -rm]
        //  STR rd, [rb, -rm]
        void dtr_dr(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            spew("%-15s %s, [%s, -%s]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, +offset]
        //  STRB rd, [rb, +offset]
        void dtrb_u(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            spew("%-15s %s, [%s, #+%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRSB rd, [rb, +offset]
        //  STRSB rd, [rb, +offset]
        void dtrsb_u(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrsb") : ("strb");
            spew("%-15s %s, [%s, #+%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTRH | HDT_S | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, +rm]
        //  STRB rd, [rb, +rm]
        void dtrb_ur(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            spew("%-15s %s, [%s, +%s]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | DT_UP | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, #-offset]
        //  STRB rd, [rb, #-offset]
        void dtrb_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            spew("%-15s %s, [%s, #-%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRSB rd, [rb, #-offset]
        //  STRSB rd, [rb, #-offset]
        void dtrsb_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            ASSERT(isLoad); /*can only do signed byte loads, not stores*/
            char const * mnemonic = (isLoad) ? ("ldrsb") : ("strb");
            spew("%-15s %s, [%s, #-%u]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTRH | HDT_S | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, -rm]
        //  STRB rd, [rb, -rm]
        void dtrb_dr(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            spew("%-15s %s, [%s, -%s]",
                 mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | OP2_OFSREG, rd, rb, rm);
        }

        void ldrh_r(int rd, int rb, int rm, Condition cc = AL)
        {
            spew("%-15s %s, [%s, +%s]",
                 "ldrh", nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rb, rm);
        }

        void ldrh_d(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            spew("%-15s %s, [%s, #-%u]",
                 "ldrh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_PRE, rd, rb, offset);
        }

        void ldrh_u(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            spew("%-15s %s, [%s, #+%u]",
                 "ldrh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rb, offset);
        }

        void ldrsh_d(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            spew("%-15s %s, [%s, #-%u]",
                 "ldrsh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | HDT_S | DT_PRE, rd, rb, offset); 
       }

        void ldrsh_u(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            spew("%-15s %s, [%s, #+%u]",
                 "ldrsh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | HDT_S | DT_UP | DT_PRE, rd, rb, offset);
        }

        void strh_r(int rb, int rm, int rd, Condition cc = AL)
        {
            spew("%-15s %s, [%s, +%s]",
                 "strh", nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | STRH | HDT_UH | DT_UP | DT_PRE, rd, rb, rm);
        }

        void push_r(int reg, Condition cc = AL)
        {
            spew("%-15s {%s}",
                 "push", nameGpReg(reg));
            ASSERT(ARMWord(reg) <= 0xf);
            m_buffer.putInt(cc | DTR | DT_WB | RN(ARMRegisters::sp) | RD(reg) | 0x4);
        }

        void pop_r(int reg, Condition cc = AL)
        {
            spew("%-15s {%s}",
                 "pop", nameGpReg(reg));
            ASSERT(ARMWord(reg) <= 0xf);
            m_buffer.putInt(cc | (DTR ^ DT_PRE) | DT_LOAD | DT_UP | RN(ARMRegisters::sp) | RD(reg) | 0x4);
        }

        inline void poke_r(int reg, Condition cc = AL)
        {
            dtr_d(false, ARMRegisters::sp, 0, reg, cc);
        }

        inline void peek_r(int reg, Condition cc = AL)
        {
            dtr_u(true, reg, ARMRegisters::sp, 0, cc);
        }



#if WTF_ARM_ARCH_VERSION >= 5
        void clz_r(int rd, int rm, Condition cc = AL)
        {
            spewInsWithOp2("clz", cc, rd, static_cast<ARMWord>(rm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | CLZ | RD(rd) | RM(rm));
        }
#endif

        void bkpt(ARMWord value)
        {
#if WTF_ARM_ARCH_VERSION >= 5
            spew("%-15s #0x%04x", "bkpt", value);
            m_buffer.putInt(BKPT | ((value & 0xfff0) << 4) | (value & 0xf));
#else
            // Cannot access to Zero memory address
            dtr_dr(true, ARMRegisters::S0, ARMRegisters::S0, ARMRegisters::S0);
#endif
        }

        void bx(int rm, Condition cc = AL)
        {
#if WTF_ARM_ARCH_VERSION >= 5 || defined(__ARM_ARCH_4T__)
            spew("bx%-13s %s", nameCC(cc), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | BX, 0, 0, RM(rm));
#else
            mov_r(ARMRegisters::pc, RM(rm), cc);
#endif
        }

        JmpSrc blx(int rm, Condition cc = AL)
        {
#if WTF_CPU_ARM && WTF_ARM_ARCH_VERSION >= 5
            int s = m_buffer.uncheckedSize();
            spew("blx%-12s %s", nameCC(cc), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | BLX, 0, 0, RM(rm));
#else
            ASSERT(rm != 14);
            ensureSpace(2 * sizeof(ARMWord), 0);
            mov_r(ARMRegisters::lr, ARMRegisters::pc, cc);
            int s = m_buffer.uncheckedSize();
            bx(rm, cc);
#endif
            return JmpSrc(s);
        }

        static ARMWord lsl(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | (LSL << 5);
        }

        static ARMWord lsr(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | (LSR << 5);
        }

        static ARMWord asr(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | (ASR << 5);
        }

        static ARMWord lsl_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | (LSL << 5) | 0x10;
        }

        static ARMWord lsr_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | (LSR << 5) | 0x10;
        }

        static ARMWord asr_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | (ASR << 5) | 0x10;
        }

        // General helpers

        void forceFlushConstantPool()
        {
            m_buffer.flushWithoutBarrier(true);
        }

        size_t size() const
        {
            return m_buffer.uncheckedSize();
        }

        size_t allocSize() const
        {
            return m_buffer.allocSize();
        }

        void ensureSpace(int insnSpace, int constSpace)
        {
            m_buffer.ensureSpace(insnSpace, constSpace);
        }

        void ensureSpace(int space)
        {
            m_buffer.ensureSpace(space);
        }

        int sizeOfConstantPool()
        {
            return m_buffer.sizeOfConstantPool();
        }

        int flushCount()
        {
            return m_buffer.flushCount();
        }

        JmpDst label()
        {
            JmpDst label(m_buffer.size());
            spew("#label     ((%d))", label.m_offset);
            return label;
        }

        JmpDst align(int alignment)
        {
            while (!m_buffer.isAligned(alignment))
                mov_r(ARMRegisters::r0, ARMRegisters::r0);

            return label();
        }

        JmpSrc loadBranchTarget(int rd, Condition cc = AL, int useConstantPool = 0)
        {
            // The 'useConstantPool' flag really just indicates where we have
            // to use the constant pool, for repatching. We might still use it,
            // so ensure there's space for a pool constant irrespective of
            // 'useConstantPool'.
            ensureSpace(sizeof(ARMWord), sizeof(ARMWord));
            int s = m_buffer.uncheckedSize();
            ldr_un_imm(rd, InvalidBranchTarget, cc);
            m_jumps.append(s | (useConstantPool & 0x1));
            return JmpSrc(s);
        }

        JmpSrc jmp(Condition cc = AL, int useConstantPool = 0)
        {
            return loadBranchTarget(ARMRegisters::pc, cc, useConstantPool);
        }

        void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool **poolp, CodeKind kind);
        void executableCopy(void* buffer);
        void fixUpOffsets(void* buffer);

        // Patching helpers

        static ARMWord* getLdrImmAddress(ARMWord* insn)
        {
#if WTF_CPU_ARM && WTF_ARM_ARCH_VERSION >= 5
            // Check for call
            if ((*insn & 0x0f7f0000) != 0x051f0000) {
                // Must be BLX
                ASSERT((*insn & 0x012fff30) == 0x012fff30);
                insn--;
            }
#endif
            // Must be an ldr ..., [pc +/- imm]
            ASSERT((*insn & 0x0f7f0000) == 0x051f0000);

            ARMWord addr = reinterpret_cast<ARMWord>(insn) + DefaultPrefetching * sizeof(ARMWord);
            if (*insn & DT_UP)
                return reinterpret_cast<ARMWord*>(addr + (*insn & SDT_OFFSET_MASK));
            return reinterpret_cast<ARMWord*>(addr - (*insn & SDT_OFFSET_MASK));
        }

        static ARMWord* getLdrImmAddressOnPool(ARMWord* insn, uint32_t* constPool)
        {
            // Must be an ldr ..., [pc +/- imm]
            ASSERT((*insn & 0x0f7f0000) == 0x051f0000);

            if (*insn & 0x1)
                return reinterpret_cast<ARMWord*>(constPool + ((*insn & SDT_OFFSET_MASK) >> 1));
            return getLdrImmAddress(insn);
        }

        static void patchPointerInternal(intptr_t from, void* to)
        {
            ARMWord* insn = reinterpret_cast<ARMWord*>(from);
            ARMWord* addr = getLdrImmAddress(insn);
            *addr = reinterpret_cast<ARMWord>(to);
        }

        static ARMWord patchConstantPoolLoad(ARMWord load, ARMWord value)
        {
            value = (value << 1) + 1;
            ASSERT(!(value & ~0xfff));
            return (load & ~0xfff) | value;
        }

        static void patchConstantPoolLoad(void* loadAddr, void* constPoolAddr);

        // Patch pointers

        static void linkPointer(void* code, JmpDst from, void* to)
        {
            staticSpew("##linkPointer     ((%p + %#x)) points to ((%p))",
                       code, from.m_offset, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(code) + from.m_offset, to);
        }

        static void repatchInt32(void* from, int32_t to)
        {
            staticSpew("##repatchInt32    ((%p)) holds ((%#x))",
                       from, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(from), reinterpret_cast<void*>(to));
        }

        static void repatchPointer(void* from, void* to)
        {
            staticSpew("##repatchPointer  ((%p)) points to ((%p))",
                       from, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(from), to);
        }

        static void repatchLoadPtrToLEA(void* from)
        {
            // On arm, this is a patch from LDR to ADD. It is restricted conversion,
            // from special case to special case, altough enough for its purpose
            ARMWord* insn = reinterpret_cast<ARMWord*>(from);
            ASSERT((*insn & 0x0ff00f00) == 0x05900000);

            *insn = (*insn & 0xf00ff0ff) | 0x02800000;
            ExecutableAllocator::cacheFlush(insn, sizeof(ARMWord));
        }

        static void repatchLEAToLoadPtr(void* from)
        {
	    // Like repatchLoadPtrToLEA, this is specialized for our purpose.
            ARMWord* insn = reinterpret_cast<ARMWord*>(from);
	    if ((*insn & 0x0ff00f00) == 0x05900000)
		return; // Valid ldr instruction
            ASSERT((*insn & 0x0ff00000) == 0x02800000); // Valid add instruction
            ASSERT((*insn & 0x00000f00) == 0x00000000); // Simple-to-handle immediates (no rotate)

            *insn = (*insn &  0xf00ff0ff) | 0x05900000;
            ExecutableAllocator::cacheFlush(insn, sizeof(ARMWord));
        }

        // Linkers

        void linkJump(JmpSrc from, JmpDst to)
        {
            ARMWord  code = reinterpret_cast<ARMWord>(m_buffer.data());
            ARMWord* insn = reinterpret_cast<ARMWord*>(code + from.m_offset);
            ARMWord* addr = getLdrImmAddressOnPool(insn, m_buffer.poolAddress());

            spew("##linkJump         ((%#x)) jumps to ((%#x))",
                 from.m_offset, to.m_offset);

            *addr = to.m_offset;
        }

        static void linkJump(void* code, JmpSrc from, void* to)
        {
            staticSpew("##linkJump        ((%p + %#x)) jumps to ((%p))",
                       code, from.m_offset, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(code) + from.m_offset, to);
        }

        static void relinkJump(void* from, void* to)
        {
            staticSpew("##relinkJump      ((%p)) jumps to ((%p))",
                       from, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(from), to);
        }

        static bool canRelinkJump(void* from, void* to)
        {
            return true;
        }

        static void linkCall(void* code, JmpSrc from, void* to)
        {
            staticSpew("##linkCall        ((%p + %#x)) jumps to ((%p))",
                       code, from.m_offset, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(code) + from.m_offset, to);
        }

        static void relinkCall(void* from, void* to)
        {
            staticSpew("##relinkCall      ((%p)) jumps to ((%p))",
                       from, to);

            patchPointerInternal(reinterpret_cast<intptr_t>(from), to);
        }

        // Address operations

        static void* getRelocatedAddress(void* code, JmpSrc jump)
        {
            return reinterpret_cast<void*>(reinterpret_cast<ARMWord*>(code) + jump.m_offset / sizeof(ARMWord));
        }

        static void* getRelocatedAddress(void* code, JmpDst label)
        {
            return reinterpret_cast<void*>(reinterpret_cast<ARMWord*>(code) + label.m_offset / sizeof(ARMWord));
        }

        // Address differences

        static int getDifferenceBetweenLabels(JmpDst from, JmpSrc to)
        {
            return to.m_offset - from.m_offset;
        }

        static int getDifferenceBetweenLabels(JmpDst from, JmpDst to)
        {
            return to.m_offset - from.m_offset;
        }

        static unsigned getCallReturnOffset(JmpSrc call)
        {
            return call.m_offset + sizeof(ARMWord);
        }

        // Handle immediates

        static ARMWord getOp2Byte(ARMWord imm)
        {
            ASSERT(imm <= 0xff);
            return OP2_IMMh | (imm & 0x0f) | ((imm & 0xf0) << 4) ;
        }

        static ARMWord getOp2(ARMWord imm);

        // Get an operand-2 field for immediate-shifted-registers in arithmetic
        // instructions.
        static ARMWord getOp2RegScale(RegisterID reg, ARMWord scale);

#if WTF_ARM_ARCH_VERSION >= 7
        static ARMWord getImm16Op2(ARMWord imm)
        {
            if (imm <= 0xffff)
                return (imm & 0xf000) << 4 | (imm & 0xfff);
            return INVALID_IMM;
        }
#endif
        ARMWord getImm(ARMWord imm, int tmpReg, bool invert = false);
        void moveImm(ARMWord imm, int dest);
        ARMWord encodeComplexImm(ARMWord imm, int dest);

        ARMWord getOffsetForHalfwordDataTransfer(ARMWord imm, int tmpReg)
        {
            // Encode immediate data in the instruction if it is possible
            if (imm <= 0xff)
                return getOp2Byte(imm);
            // Otherwise, store the data in a temporary register
            return encodeComplexImm(imm, tmpReg);
        }

        // Memory load/store helpers
        void dataTransferN(bool isLoad, bool isSigned, int size, RegisterID srcDst, RegisterID base, int32_t offset);

        void dataTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, int32_t offset);
        void dataTransfer8(bool isLoad, RegisterID srcDst, RegisterID base, int32_t offset, bool isSigned);
        void baseIndexTransferN(bool isLoad, bool isSigned, int size, RegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset);
        void baseIndexTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset);
        void doubleTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset);
        void doubleTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset, RegisterID index, int32_t scale);

        void floatTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset);
        /**/
        void baseIndexFloatTransfer(bool isLoad, bool isDouble, FPRegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset);

        // Constant pool hnadlers

        static ARMWord placeConstantPoolBarrier(int offset)
        {
            offset = (offset - sizeof(ARMWord)) >> 2;
            ASSERT((offset <= BOFFSET_MAX && offset >= BOFFSET_MIN));
            return AL | B | (offset & BRANCH_MASK);
        }

        // pretty-printing functions
        static char const * nameGpReg(int reg)
        {
            ASSERT(reg <= 16);
            ASSERT(reg >= 0);
            static char const * const names[] = {
                "r0", "r1", "r2", "r3",
                "r4", "r5", "r6", "r7",
                "r8", "r9", "r10", "r11",
                "ip", "sp", "lr", "pc"
            };
            return names[reg];
        }

        static char const * nameFpRegD(int reg)
        {
            ASSERT(reg <= 31);
            ASSERT(reg >= 0);
            static char const * const names[] = {
                 "d0",   "d1",   "d2",   "d3",
                 "d4",   "d5",   "d6",   "d7",
                 "d8",   "d9",  "d10",  "d11",
                "d12",  "d13",  "d14",  "d15",
                "d16",  "d17",  "d18",  "d19",
                "d20",  "d21",  "d22",  "d23",
                "d24",  "d25",  "d26",  "d27",
                "d28",  "d29",  "d30",  "d31"
            };
            return names[reg];
        }
        static char const * nameFpRegS(int reg)
        {
            ASSERT(reg <= 31);
            ASSERT(reg >= 0);
            static char const * const names[] = {
                 "s0",   "s1",   "s2",   "s3",
                 "s4",   "s5",   "s6",   "s7",
                 "s8",   "s9",  "s10",  "s11",
                "s12",  "s13",  "s14",  "s15",
                "s16",  "s17",  "s18",  "s19",
                "s20",  "s21",  "s22",  "s23",
                "s24",  "s25",  "s26",  "s27",
                "s28",  "s29",  "s30",  "s31"
            };
            return names[reg];
        }

        static char const * nameCC(Condition cc)
        {
            ASSERT(cc <= AL);
            ASSERT((cc & 0x0fffffff) == 0);

            uint32_t    ccIndex = cc >> 28;
            static char const * const names[] = {
                "eq", "ne",
                "cs", "cc",
                "mi", "pl",
                "vs", "vc",
                "hi", "ls",
                "ge", "lt",
                "gt", "le",
                "  "        // AL is the default, so don't show it.
            };
            return names[ccIndex];
        }

    private:
        // Decodes operand 2 immediate values (for debug output and assertions).
        inline uint32_t decOp2Imm(uint32_t op2)
        {
            ASSERT((op2 & ~0xfff) == 0);

            uint32_t    imm8 = op2 & 0xff;
            uint32_t    rot = ((op2 >> 7) & 0x1e);

            // 'rot' is a right-rotate count.

            uint32_t    imm = (imm8 >> rot);
            if (rot > 0) {
                imm |= (imm8 << (32-rot));
            }

            return imm;
        }

        // Format the operand 2 argument for debug spew. The operand can be
        // either an immediate or a register specifier.
        void fmtOp2(char * out, ARMWord op2)
        {
            static char const * const shifts[4] = {"LSL", "LSR", "ASR", "ROR"};

            if ((op2 & OP2_IMM) || (op2 & OP2_IMMh)) {
                // Immediate values.
                
                uint32_t    imm = decOp2Imm(op2 & ~(OP2_IMM | OP2_IMMh));
                sprintf(out, "#0x%x @ (%d)", imm, static_cast<int32_t>(imm));
            } else {
                // Register values.

                char const *    rm = nameGpReg(op2 & 0xf);
                Shift           type = static_cast<Shift>((op2 >> 5) & 0x3);

                // Bit 4 specifies barrel-shifter parameters in operand 2.
                if (op2 & (1<<4)) {
                    // Register-shifted register.
                    // Example: "r0, LSL r6"
                    char const *    rs = nameGpReg((op2 >> 8) & 0xf);
                    sprintf(out, "%s, %s %s", rm, shifts[type], rs);
                } else {
                    // Immediate-shifted register.
                    // Example: "r0, ASR #31"
                    uint32_t        imm = (op2 >> 7) & 0x1f;
                    
                    // Deal with special encodings.
                    if ((type == LSL) && (imm == 0)) {
                        // "LSL #0" doesn't shift at all (and is the default).
                        sprintf(out, "%s", rm);
                        return;
                    }

                    if ((type == ROR) && (imm == 0)) {
                        // "ROR #0" is a special case ("RRX").
                        sprintf(out, "%s, RRX", rm);
                        return;
                    }

                    if (((type == LSR) || (type == ASR)) && (imm == 0)) {
                        // Both LSR and ASR have a range of 1-32, with 32
                        // encoded as 0.                  
                        imm = 32;
                    }

                    // Print the result.

                    sprintf(out, "%s, %s #%u", rm, shifts[type], imm);
                }
            }
        }

        void spewInsWithOp2(char const * ins, Condition cc, int rd, int rn, ARMWord op2)
        {
#if defined(JS_METHODJIT_SPEW)
            char    mnemonic[16];
            snprintf(mnemonic, 16, "%s%s", ins, nameCC(cc));

            char    op2_fmt[48];
            fmtOp2(op2_fmt, op2);

            spew("%-15s %s, %s, %s", mnemonic, nameGpReg(rd), nameGpReg(rn), op2_fmt);
#endif
        }

        void spewInsWithOp2(char const * ins, Condition cc, int r, ARMWord op2)
        {
#if defined(JS_METHODJIT_SPEW)
            char    mnemonic[16];
            snprintf(mnemonic, 16, "%s%s", ins, nameCC(cc));

            char    op2_fmt[48];
            fmtOp2(op2_fmt, op2);

            spew("%-15s %s, %s", mnemonic, nameGpReg(r), op2_fmt);
#endif
        }

        ARMWord RM(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg;
        }

        ARMWord RS(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 8;
        }

        ARMWord RD(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 12;
        }

        ARMWord RN(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 16;
        }

        ARMWord DD(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [22,15:12].
            return ((reg << 12) | (reg << 18)) & 0x0040f000;
        }

        ARMWord DN(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [7,19:16].
            return ((reg << 16) | (reg << 3)) & 0x000f0080;
        }

        ARMWord DM(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Encoded as bits [5,3:0].
            return ((reg << 1) & 0x20) | (reg & 0xf);
        }

        ARMWord SD(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [15:12,22].
            return ((reg << 11) | (reg << 22)) & 0x0040f000;
        }

        ARMWord SM(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Encoded as bits [5,3:0].
            return ((reg << 5) & 0x20) | ((reg >> 1) & 0xf);
        }
        ARMWord SN(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Encoded as bits [19:16,7].
            return ((reg << 15) & 0xf0000) | ((reg & 1) << 7);
        }
        static ARMWord getConditionalField(ARMWord i)
        {
            return i & 0xf0000000;
        }

        int genInt(int reg, ARMWord imm, bool positive);

        ARMBuffer m_buffer;
        Jumps m_jumps;
    public:
        // VFP instruction constants
        enum {
            VFP_DATA  = 0x0E000A00,
            VFP_EXT   = 0x0C000A00,
            VFP_XFER  = 0x0E000A08,
            VFP_DXFER = 0x0C400A00,

            VFP_DBL   = (1<<8),
            
            /*integer conversions*/
            VFP_ICVT  = 0x00B80040,
            VFP_FPCVT = 0x00B700C0,
            
            VFP_DTR   = 0x01000000,
            VFP_MOV     = 0x00000010,

            FMSR = 0x0e000a10,
            FMRS = 0x0e100a10,
            FSITOD = 0x0eb80bc0,
            FUITOD = 0x0eb80b40,
            FTOSID = 0x0ebd0b40,
            FTOSIZD = 0x0ebd0bc0,
            FMSTAT = 0x0ef1fa10,
            FDTR = 0x0d000b00

        };
        enum RegType {
            SIntReg32,
            UIntReg32,
            FloatReg32,
            FloatReg64
        };

        const char * nameType(RegType t)
        {
            const char * const name[4] =
                {"S32", "U32", "F32", "F64"};
            return name[t];
        }

        const char * nameTypedReg(RegType t, int reg)
        {
            switch(t) {
            case SIntReg32:
            case UIntReg32:
                return nameGpReg(reg);
            case FloatReg32:
                return nameFpRegS(reg);
            case FloatReg64:
                return nameFpRegD(reg);
            }
            return "";
        }

        bool isFloatType(RegType rt)
        {
            if (rt == FloatReg32 || rt == FloatReg64)
                return true;
            return false;
        }

        bool isIntType(RegType rt)
        {
            if (rt == FloatReg32 || rt == FloatReg64)
                return false;
            return true;
        }

        // ********************************************************************
        // *                            VFP Code:
        //*********************************************************************
        /* this is horrible. There needs to be some sane way of distinguishing D from S from R*/
        void emitVFPInst(ARMWord op, ARMWord rd, ARMWord rn, ARMWord op2)
        {
            m_buffer.putInt(op | rn | rd | op2);
        }

        // NOTE: offset is the actual value that is going to be encoded.  It is the offset in words, NOT in bytes.
        void fmem_imm_off(bool isLoad, bool isDouble, bool isUp, int dest, int rn, ARMWord offset, Condition cc = AL)
        {
            char const * ins = isLoad ? "vldr.f" : "vstr.f";
            spew("%s%d %s, [%s, #%s%u]", 
                 ins, (isDouble ? 64 : 32), (isDouble ? nameFpRegD(dest) : nameFpRegS(dest)),
                 nameGpReg(rn), (isUp ? "+" : "-"), offset);
            ASSERT(offset <= 0xff);
            emitVFPInst(static_cast<ARMWord>(cc) | 
                        VFP_EXT | VFP_DTR | 
                        (isDouble ? VFP_DBL : 0) |
                        (isUp ? DT_UP : 0) | 
                        (isLoad ? DT_LOAD : 0), isDouble ? DD(dest) : SD(dest), RN(rn), offset);
            
        }

        // WARNING: even for an int -> float conversion, all registers used
        // are VFP registers.
        void vcvt(RegType srcType, RegType dstType, int src, int dest, Condition cc = AL)
        {
            ASSERT(srcType != dstType);
            ASSERT(isFloatType(srcType) || isFloatType(dstType));

            spew("vcvt.%s.%-15s, %s,%s", 
                 nameType(dstType), nameType(srcType),
                 nameTypedReg(dstType,dest), nameTypedReg(srcType,src));
            
            if (isFloatType(srcType) && isFloatType (dstType)) {
                // doing a float -> float conversion
                bool dblToFloat = srcType == FloatReg64;
                emitVFPInst(static_cast<ARMWord>(cc) | VFP_DATA | VFP_FPCVT |
                            (dblToFloat ? VFP_DBL : 0),
                            dblToFloat ? SD(dest) : DD(dest),
                            dblToFloat ? DM(src) : SM(src), 0);
            } else {
                MOZ_ASSUME_UNREACHABLE("Other conversions did not seem useful on 2011/08/04");
            }
        }

        // does r2:r1 -> dn, dn -> r2:r1, r2:r1 -> s2:s1, etc.
        void vmov64 (bool fromFP, bool isDbl, int r1, int r2, int rFP, Condition cc = AL)
        {
            if (fromFP) {
                spew("%-15s %s, %s, %s", "vmov", 
                     nameGpReg(r1), nameGpReg(r2), nameFpRegD(rFP));
            } else {
                spew("%-15s %s, %s, %s", "vmov",
                     nameFpRegD(rFP), nameGpReg(r1), nameGpReg(r2));
            }
            emitVFPInst(static_cast<ARMWord>(cc) | VFP_DXFER | VFP_MOV |
                        (fromFP ? DT_LOAD : 0) |
                        (isDbl ? VFP_DBL : 0), RD(r1), RN(r2), isDbl ? DM(rFP) : SM(rFP));
        }

        void fcpyd_r(int dd, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s", "vmov.f64",
                 nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FCPYD, DD(dd), DM(dm), 0);
        }

        void faddd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s, %s", "vadd.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FADDD, DD(dd), DN(dn), DM(dm));
        }

        void fnegd_r(int dd, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s", "fnegd", nameFpRegD(dd), nameFpRegD(dm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | FNEGD | DD(dd) | DM(dm));
        }

        void fdivd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s, %s", "vdiv.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FDIVD, DD(dd), DN(dn), DM(dm));
        }

        void fsubd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s, %s", "vsub.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FSUBD, DD(dd), DN(dn), DM(dm));
        }

        void fabsd_r(int dd, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s", "fabsd", nameFpRegD(dd), nameFpRegD(dm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | FABSD | DD(dd) | DM(dm));
        }

        void fmuld_r(int dd, int dn, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s, %s", "vmul.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FMULD, DD(dd), DN(dn), DM(dm));
        }

        void fcmpd_r(int dd, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s", "vcmp.f64", nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FCMPD, DD(dd), 0, DM(dm));
        }

        void fsqrtd_r(int dd, int dm, Condition cc = AL)
        {
            spew("%-15s %s, %s", "vsqrt.f64", nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FSQRTD, DD(dd), 0, DM(dm));
        }

        void fmsr_r(int dd, int rn, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FMSR, RD(rn), SN(dd), 0);
        }

        void fmrs_r(int rd, int dn, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FMRS, RD(rd), SN(dn), 0);
        }

        // dear god :(
        // integer registers ar encoded the same as single registers
        void fsitod_r(int dd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FSITOD, DD(dd), 0, SM(dm));
        }

        void fuitod_r(int dd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitVFPInst(static_cast<ARMWord>(cc) | FUITOD, DD(dd), 0, SM(dm));
        }

        void ftosid_r(int fd, int dm, Condition cc = AL)
        {
            // TODO: I don't actually know what the encoding is i'm guessing SD and DM.
            emitVFPInst(static_cast<ARMWord>(cc) | FTOSID, SD(fd), 0, DM(dm));
        }

        void ftosizd_r(int fd, int dm, Condition cc = AL)
        {
            // TODO: I don't actually know what the encoding is i'm guessing SD and DM.
            emitVFPInst(static_cast<ARMWord>(cc) | FTOSIZD, SD(fd), 0, DM(dm));
        }

        void fmstat(Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            m_buffer.putInt(static_cast<ARMWord>(cc) | FMSTAT);
        }


        // things added to make IONMONKEY happy!
        // what is the offset (from the beginning of the buffer) to the address
        // of the next instruction
        int nextOffset() {
            return m_buffer.uncheckedSize();
        }
        void putInst32(uint32_t data) {
            m_buffer.putInt(data);
        }
        uint32_t *editSrc(JmpSrc src) {
            return (uint32_t*)(((char*)m_buffer.data()) + src.offset());
        }
    }; // ARMAssembler

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)

#endif /* assembler_assembler_ARMAssembler_h */
