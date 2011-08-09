/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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

#ifndef ARMAssembler_h
#define ARMAssembler_h

#include "assembler/wtf/Platform.h"

// Some debug code uses s(n)printf for instruction logging.
#include <stdio.h>

#if ENABLE_ASSEMBLER && WTF_CPU_ARM_TRADITIONAL

#include "AssemblerBufferWithConstantPool.h"
#include "assembler/wtf/Assertions.h"

#include "methodjit/Logging.h"
#define IPFX    "        %s"
#define ISPFX   "        "
#ifdef JS_METHODJIT_SPEW
# define MAYBE_PAD (isOOLPath ? ">  " : "")
# define FIXME_INSN_PRINTING                                \
    do {                                                   \
        js::JaegerSpew(js::JSpew_Insns,                    \
                       IPFX "FIXME insn printing %s:%d\n", \
                       MAYBE_PAD,                          \
                       __FILE__, __LINE__);                \
    } while (0)
#else
# define MAYBE_PAD ""
# define FIXME_INSN_PRINTING ((void) 0)
#endif

// TODO: We don't print the condition code in our JaegerSpew lines. Doing this
// is awkward whilst maintaining a consistent field width.

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
            pc = r15
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
            d31
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

    class ARMAssembler {
    public:
        
#ifdef JS_METHODJIT_SPEW
        bool isOOLPath;
        // Assign a default value to keep Valgrind quiet.
        ARMAssembler() : isOOLPath(false) { }
#else
        ARMAssembler() { }
#endif

        static unsigned int const maxPoolSlots = 512;

        typedef ARMRegisters::RegisterID RegisterID;
        typedef ARMRegisters::FPRegisterID FPRegisterID;
        typedef AssemblerBufferWithConstantPool<maxPoolSlots * sizeof(ARMWord), 4, 4, 4, ARMAssembler> ARMBuffer;
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
            DTR = 0x05000000,
#if WTF_ARM_ARCH_VERSION >= 5
            LDRH = 0x00100090,
            STRH = 0x00000090,
            DTRH = 0x00000090,
#endif
            STMDB = 0x09200000,
            LDMIA = 0x08b00000,
            B = 0x0a000000,
            BL = 0x0b000000,
            NOP = 0x01a00000    // Effective NOP ("MOV r0, r0").
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
        public:
            JmpSrc()
                : m_offset(-1)
            {
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
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX    "%-15s %s, 0x%04x\n", MAYBE_PAD, "movw", nameGpReg(rd), (op2 & 0xfff) | ((op2 >> 4) & 0xf000));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MOVW | RD(rd) | op2);
        }

        void movt_r(int rd, ARMWord op2, Condition cc = AL)
        {
            ASSERT((op2 | 0xf0fff) == 0xf0fff);
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX    "%-15s %s, 0x%04x\n", MAYBE_PAD, "movt", nameGpReg(rd), (op2 & 0xfff) | ((op2 >> 4) & 0xf000));
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
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s, %s\n", MAYBE_PAD, "mull", nameGpReg(rdlo), nameGpReg(rdhi), nameGpReg(rn), nameGpReg(rm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | MULL | RN(rdhi) | RD(rdlo) | RS(rn) | RM(rm));
        }

        // pc relative loads (useful for loading from pools).
        void ldr_imm(int rd, ARMWord imm, Condition cc = AL)
        {
            char mnemonic[16];
            snprintf(mnemonic, 16, "ldr%s", nameCC(cc));
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX    "%-15s %s, =0x%x @ (%d) (reusable pool entry)\n", MAYBE_PAD, mnemonic, nameGpReg(rd), imm, static_cast<int32_t>(imm));
            m_buffer.putIntWithConstantInt(static_cast<ARMWord>(cc) | DTR | DT_LOAD | DT_UP | RN(ARMRegisters::pc) | RD(rd), imm, true);
        }

        void ldr_un_imm(int rd, ARMWord imm, Condition cc = AL)
        {
            char mnemonic[16];
            snprintf(mnemonic, 16, "ldr%s", nameCC(cc));
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX    "%-15s %s, =0x%x @ (%d)\n", MAYBE_PAD, mnemonic, nameGpReg(rd), imm, static_cast<int32_t>(imm));
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
            js::JaegerSpew(js::JSpew_Insns, 
                           IPFX "%sr%s%s, [%s, #%s%u]\n", 
                           MAYBE_PAD, mnemonic_act, mnemonic_sign, mnemonic_size,
                           nameGpReg(rd), nameGpReg(rb), off_sign, offset);
            if (size == 32 || (size == 8 && !isSigned)) {
                /* All (the one) 32 bit ops and the unsigned 8 bit ops use the original encoding.*/
                emitInst(static_cast<ARMWord>(cc) | DTR |
                         (isLoad ? DT_LOAD : 0) |
                         (size == 8 ? DT_BYTE : 0) |
                         (posOffset ? DT_UP : 0), rd, rb, offset);
            } else {
                /* All 16 bit ops and 8 bit unsigned use the newer encoding.*/
                /*these instructions don't exist before ARMv4*/
                ASSERT(WTF_ARM_ARCH_VERSION >= 4);
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
            js::JaegerSpew(js::JSpew_Insns, 
                           IPFX "%sr%s%s, [%s, #%s%s]\n", MAYBE_PAD, mnemonic_act, mnemonic_sign, mnemonic_size,
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
                ASSERT(WTF_ARM_ARCH_VERSION >= 4);
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
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #+%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDR rd, [rb, +rm]
        //  STR rd, [rb, +rm]
        void dtr_ur(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, +%s]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDR rd, [rb, -offset]
        //  STR rd, [rb, -offset]
        void dtr_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #-%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDR rd, [rb, -rm]
        //  STR rd, [rb, -rm]
        void dtr_dr(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldr") : ("str");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, -%s]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, +offset]
        //  STRB rd, [rb, +offset]
        void dtrb_u(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #+%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRSB rd, [rb, +offset]
        //  STRSB rd, [rb, +offset]
        void dtrsb_u(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrsb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #+%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTRH | HDT_S | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, +rm]
        //  STRB rd, [rb, +rm]
        void dtrb_ur(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, +%s]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | DT_UP | OP2_OFSREG, rd, rb, rm);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, #-offset]
        //  STRB rd, [rb, #-offset]
        void dtrb_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #-%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRSB rd, [rb, #-offset]
        //  STRSB rd, [rb, #-offset]
        // TODO: this instruction does not exist on arm v4 and earlier
        void dtrsb_d(bool isLoad, int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            ASSERT(isLoad); /*can only do signed byte loads, not stores*/
            char const * mnemonic = (isLoad) ? ("ldrsb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #-%u]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | DTRH | HDT_S | (isLoad ? DT_LOAD : 0), rd, rb, offset);
        }

        // Data transfers like this:
        //  LDRB rd, [rb, -rm]
        //  STRB rd, [rb, -rm]
        void dtrb_dr(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            char const * mnemonic = (isLoad) ? ("ldrb") : ("strb");
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, -%s]\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | DTR | DT_BYTE | (isLoad ? DT_LOAD : 0) | OP2_OFSREG, rd, rb, rm);
        }

        void ldrh_r(int rd, int rb, int rm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, +%s]\n", MAYBE_PAD, "ldrh", nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rb, rm);
        }

        void ldrh_d(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #-%u]\n", MAYBE_PAD, "ldrh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_PRE, rd, rb, offset);
        }

        void ldrh_u(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #+%u]\n", MAYBE_PAD, "ldrh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rb, offset);
        }

        void ldrsh_d(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #-%u]\n", MAYBE_PAD, "ldrsh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | HDT_S | DT_PRE, rd, rb, offset); 
       }

        void ldrsh_u(int rd, int rb, ARMWord offset, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, #+%u]\n", MAYBE_PAD, "ldrsh", nameGpReg(rd), nameGpReg(rb), offset);
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | HDT_S | DT_UP | DT_PRE, rd, rb, offset);
        }

        void strh_r(int rb, int rm, int rd, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, [%s, +%s]\n", MAYBE_PAD, "strh", nameGpReg(rd), nameGpReg(rb), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | STRH | HDT_UH | DT_UP | DT_PRE, rd, rb, rm);
        }

        void push_r(int reg, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s {%s}\n", MAYBE_PAD, "push", nameGpReg(reg));
            ASSERT(ARMWord(reg) <= 0xf);
            m_buffer.putInt(cc | DTR | DT_WB | RN(ARMRegisters::sp) | RD(reg) | 0x4);
        }

        void pop_r(int reg, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s {%s}\n", MAYBE_PAD, "pop", nameGpReg(reg));
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
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s #0x%04x\n", MAYBE_PAD, "bkpt", value);
            m_buffer.putInt(BKPT | ((value & 0xfff0) << 4) | (value & 0xf));
#else
            // Cannot access to Zero memory address
            dtr_dr(true, ARMRegisters::S0, ARMRegisters::S0, ARMRegisters::S0);
#endif
        }

        void bx(int rm, Condition cc = AL)
        {
#if WTF_ARM_ARCH_VERSION >= 5 || defined(__ARM_ARCH_4T__)
            js::JaegerSpew(
                    js::JSpew_Insns,
                    IPFX    "bx%-13s %s\n", MAYBE_PAD, nameCC(cc), nameGpReg(rm));
            emitInst(static_cast<ARMWord>(cc) | BX, 0, 0, RM(rm));
#else
            mov_r(ARMRegisters::pc, RM(rm), cc);
#endif
        }

        JmpSrc blx(int rm, Condition cc = AL)
        {
#if WTF_CPU_ARM && WTF_ARM_ARCH_VERSION >= 5
            int s = m_buffer.uncheckedSize();
            js::JaegerSpew(
                    js::JSpew_Insns,
                    IPFX    "blx%-12s %s\n", MAYBE_PAD, nameCC(cc), nameGpReg(rm));
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

        int size()
        {
            return m_buffer.size();
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

#ifdef DEBUG
        void allowPoolFlush(bool allowFlush)
        {
            m_buffer.allowPoolFlush(allowFlush);
        }
#endif

        JmpDst label()
        {
            JmpDst label(m_buffer.size());
            js::JaegerSpew(js::JSpew_Insns, IPFX "#label     ((%d))\n", MAYBE_PAD, label.m_offset);
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

        void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool **poolp);
        void executableCopy(void* buffer);
        void fixUpOffsets(void* buffer);

        // Patching helpers

        // Generate a single LDR instruction loading from the specified literal pool
        // slot into rd, with condition cc. It is assumed that the literal pool
        // slot is within range of an LDR instruction.
        static void putLDRLiteral(ARMWord* ldr, Condition cc, int rd, ARMWord* literal)
        {
            ptrdiff_t   offset = getApparentPCOffset(ldr, literal);
            ASSERT((rd >= 0) && (rd <= 15));

            if (offset >= 0) {
                ASSERT(!(offset & ~0xfff));
                *ldr = cc | DT_UP | 0x051f0000 | (rd << 12) | (offset & 0xfff);
            } else {
                offset = -offset;
                ASSERT(!(offset & ~0xfff));
                *ldr = cc |         0x051f0000 | (rd << 12) | (offset & 0xfff);
            }
        }

        // Try to generate a single branch instruction. The insturction address
        // is assumed to be finalized, so the branch offset will be relative to
        // the address of 'insn'.
        static bool tryPutImmediateBranch(ARMWord * insn, Condition cc, ARMWord value, bool pic=false)
        {
            if (value & 0x1) {
                // We can't handle interworking branches here.
                return false;
            }

            if (pic) {
                // We can't generate a position-independent branch to an
                // absolute address using the relative 'B' instruction.
                return false;
            }

            ptrdiff_t offset = getApparentPCOffset(insn, value);

            // ARM instructions are always word-aligned.
            ASSERT(!(offset & 0x3));
            ASSERT(!(value & 0x3));

            if (value == reinterpret_cast<ARMWord>(insn+1)) {
                // The new branch target is the next instruction
                // anyway, so just insert a NOP.
                *insn = cc | NOP;
                return true;
            } else if ((offset >= -33554432) && (offset <= 33554428)) {
                // Use a simple branch instruction (B):
                //          "B(cc)      addr"
                ASSERT(!((offset/4) & 0xff000000) || !((-offset/4) & 0xff000000));
                *insn = cc | B | ((offset/4) & 0x00ffffff);
                return true;
            }

            return false;
        }

        // Try to generate a single insturction to load an immediate.
        //
        // The immediate-load may be a branch (if rd=15). In this case, an
        // appropriate branching instruction will be used.
        //
        // Specify pic=true if the code must be position-independent. For
        // position-independent code, this method will _not_ use ADR to encode
        // a literal.
        static bool tryPutImmediateMove(ARMWord * insn, Condition cc, ARMWord value, int rd, bool pic=false)
        {
            if (rd == 15) {
                // Filter out branches.
                return tryPutImmediateBranch(insn, cc, value, pic);
            }

            ARMWord op2;

#if WTF_ARM_ARCH_VERSION >= 7
            // ---- Try MOVW. ----
            op2 = getImm16Op2(value);
            if (op2 != INVALID_IMM) {
                ASSERT(!(op2 & 0xfff0f000));
                *insn = cc | MOVW | RD(rd) | op2;
                return true;
            }
#endif
            // ---- Try MOV. ----
            op2 = getOp2(value);
            if (op2 != INVALID_IMM) {
                ASSERT((op2 & 0xfffff000) == OP2_IMM);
                *insn = cc | MOV | RD(rd) | op2;
                return true;
            }
            // ---- Try MVN. ----
            op2 = getOp2(~value);
            if (op2 != INVALID_IMM) {
                ASSERT((op2 & 0xfffff000) == OP2_IMM);
                *insn = cc | MVN | RD(rd) | op2;
                return true;
            }
            // ---- Try ADR. ----
            if (!pic) {
                // ---- Try ADR (ADD to PC). ----
                op2 = getOp2(getApparentPCOffset(insn, value));
                if (op2 != INVALID_IMM) {
                    ASSERT((op2 & 0xfffff000) == OP2_IMM);
                    *insn = cc | ADD | RD(rd) | RN(15) | op2;
                    return true;
                }
                // ---- Try ADR (SUB from PC). ----
                op2 = getOp2(-getApparentPCOffset(insn, value));
                if (op2 != INVALID_IMM) {
                    ASSERT((op2 & 0xfffff000) == OP2_IMM);
                    *insn = cc | SUB | RD(rd) | RN(15) | op2;
                    return true;
                }
            }
            
            return false;
        }

        // Try to get the address of a pool slot referenced by the LDR at
        // *insn. If *insn is a BLX <reg> instruction preceeded by an LDR into
        // the same register, that LDR is used instead.
        //
        // This mechanism allows the common LDR-BLX call sequence to be
        // handled, where the patching address ('insn') is the BLX itself.
        static ARMWord* tryGetLdrImmAddress(ARMWord* insn)
        {
#if WTF_CPU_ARM && WTF_ARM_ARCH_VERSION >= 5
            // If *insn is not a load ...
            if ((*insn & 0x0f7f0000) != 0x051f0000) {
                // ... check that it is a BLX ...
                if((*insn & 0x0ffffff0) != 0x012fff30) {
                    return NULL;
                }
                ARMWord blx_reg = *insn & 0xf;
                // *insn is a BLX, so look at the instruction before it. It
                // should be an LDR.
                insn--;
                ARMWord ldr_reg = (*insn >> 12) & 0xf;

                // Check that the BLX calls to the same register that is loaded
                // before it. (We verify that the previous instruction is an
                // LDR in the next step.)
                if (blx_reg != ldr_reg) {
                    return NULL;
                }
            }
#endif
            // *insn is expected to be an LDR. If it isn't, report failure.
            if((*insn & 0x0f7f0000) != 0x051f0000) {
                return NULL;
            }

            // Assume that the load targets an aligned address.
            ASSERT(!(*insn & 0x3));

            // Extract the address from the LDR instruction.
            ARMWord *   addr = getApparentPC(insn);
            ptrdiff_t   offset = (*insn & SDT_OFFSET_MASK) / 4;
            return addr + ((*insn & DT_UP) ? (offset) : (-offset));
        }

        // Get the address of a pool slot referenced by the LDR at *insn. If
        // *insn is a BLX instruction preceeded by an LDR, that LDR is used
        // instead.
        //
        // This mechanism allows the common LDR-BLX call sequence to be
        // handled, where the patching address ('insn') is the BLX itself.
        //
        // An assertion is thrown if the provided instruction is not a
        // recognized LDR or LDR-BLX sequence.
        static ARMWord* getLdrImmAddress(ARMWord* insn)
        {
            ARMWord * addr = tryGetLdrImmAddress(insn);
            ASSERT(addr);
            return addr;
        }

        static ARMWord* getLdrImmAddressOnPool(ARMWord* insn, uint32_t* constPool)
        {
            // Must be an ldr ..., [pc +/- imm]
            ASSERT((*insn & 0x0f7f0000) == 0x051f0000);

            if (*insn & 0x1)
                return reinterpret_cast<ARMWord*>(constPool + ((*insn & SDT_OFFSET_MASK) >> 1));
            return getLdrImmAddress(insn);
        }

        // Return true only if the instruction (*insn) has the following form:
        //  "LDR Rt, [PC, offset]"
        static inline bool checkIsLDRLiteral(ARMWord const * insn)
        {
            return (*insn & 0x0f7f0000) == 0x051f0000;
        }

        static bool checkIsLDRLiteral(ARMWord const * insn, int * rt, Condition * cc)
        {
            *rt = (*insn >> 12) & 0xf;
            *cc = getCondition(*insn);
            return checkIsLDRLiteral(insn);
        }

        // Return true only if the instruction is one that could be emitted
        // from tryPutImmediateMove (or tryPutImmediateBranch).
        static bool checkIsImmediateMoveOrBranch(ARMWord const * insn, int * rd, Condition * cc)
        {
            if (((*insn & 0x0ff00000) == MOVW)                      ||
                ((*insn & 0x0ff00000) == (MOV | OP2_IMM))           ||
                ((*insn & 0x0ff00000) == (MVN | OP2_IMM))           ||
                ((*insn & 0x0fff0000) == (ADD | OP2_IMM | RN(15)))  ||  // ADR (ADD)
                ((*insn & 0x0fff0000) == (SUB | OP2_IMM | RN(15))))     // ADR (SUB)
            {
                // All immediate moves have a similar encoding, so we can
                // handle them in one case. This mechanism also catches
                // branches that may have been generated using the above
                // instructions. (*rd will be set to 15 in that case.)
                *cc = getCondition(*insn);
                *rd = (*insn >> 12) & 0xf;
                return true;
            }

            if ((*insn & 0x0f000000) == B) {
                // Simple 'B' branches.
                *cc = getCondition(*insn);
                *rd = 15;
                return true;
            }

            if ((*insn & 0x0ff00000) == MOV) {
                // Instructions like "MOV  Rd, Rm"
                if (((*insn >> 12) & 0xf) == (*insn & 0xf)) {
                    // If Rd is the same register as Rm, this is a NOP. This
                    // may be used as a branch-to-next instruction.
                    *cc = getCondition(*insn);
                    *rd = 15;
                    return true;
                }
            }

#if WTF_ARM_ARCH_VERSION >= 7
            if ((*insn & 0x0fffffff) == 0x0320f000) {
                // Architectural ARMv7 NOP. This may be used as a
                // branch-to-next instruction.
                *cc = getCondition(*insn);
                *rd = 15;
                return true;
            }
#endif

            return false;
        }

        // --------
        //
        // Patching Constant Pool Loads
        //
        // Constant pool loads (including branches) are optimized when they are
        // patched. This occurs during fixUpOffsets (i.e. at finalization) as
        // well as during late patching, after finalization. The loads are
        // optimized to single-instruction branches or immediate moves as
        // necessary. However, when these optimized instruction are repatched
        // again, they might need to be replaced with the original load
        // instruction as single-instruction branches and moves can only encode
        // a limited subset of possible values. Finding the original pool slot
        // allocated to these instructions is not trivial, but it is necessary
        // to find the original pool slot because LDR instructions have a
        // limited offset range, and if _any_ free slot is used, a future LDR
        // patch might fail.
        //
        // TODO: Since literal pool loads always have positive offsets, it
        // should be possible to simply re-use the highest-addressed reachable
        // free slot in the pool when reinstating an LDR. This would allow a
        // simple bitmap to be used to show free slots, to avoid the expensive
        // linked-list traversal.
        //
        // The following scheme is used:
        //
        // When a load is optimized to a branch (in the case of an LDR into the
        // PC) or an immediate move (in the case of any other LDR), its literal
        // pool slot is no longer needed. It becomes part of a linked list,
        // which enumerates all unused pool slots in each pool. Because the
        // LDR instruction has just 12 bits (plus an up/down indicator bit) to
        // encode an offset, and because pool slots are always 4-byte-aligned,
        // only 11 bits are required to form an offset from one pool slot to
        // any other. The pool slots can form a doubly-linked list with a
        // 10-bit offset to the original LDR instruction:
        //
        //      [31:21]     Offset to previous.
        //      [20:10]     Offset to next.
        //      [ 9: 0]     Reverse offset to LDR.
        //
        // Node offsets are aligned to 4 bytes and are stored with a bias of
        // 1023 words. Thus, 0x000 (the minimum) represents an offset of -4092
        // and 0x7ff (the maximum) represents an offset of 4096.
        //
        // Note: A bias is used rather than a signed field only because it
        // simplifies encoding and decoding of the fields.
        //
        // The LDR offset is stored with the same bias (of 1023 words), but has
        // fewer bits and can only represent negative offsets. Also, note that
        // the PC offset (DefaultPrefetching) is taken into account for the LDR
        // offset, so its range matches that of a PC-relative load.
        //
        // When an LDR is optimized to something else, its pool slot is
        // inserted into the linked list. This is the common case, and is
        // relatively cheap.
        //
        // When an LDR must be reinstated, the linked list must be walked. The
        // reverse LDR offset in each element can be checked to find a matching
        // slot. At this point, the slot is removed from the linked list and is
        // reinstated as a normal literal pool slot.
        //
        // Because the location of the literal pools is not stored anywhere, a
        // mechanism must exist for patching code to find it. Every literal
        // pool begins with a special marker, as in placeConstantPoolMarker.
        // This marker forms the first element of the linked list of empty
        // slots, but always has an offset to previous of 1024 (encoded as
        // 0xffe00000) and an LDR offset of 0 (encoded as 0x000003ff). The
        // 'next' offset points to the first element in the linked list of
        // unused slots. The marker is always an invalid ARM instruction, so it
        // is safe to walk forwards through ARM instructions looking for it.
        //
        // There is a cost associated with finding the literal pool. A memory
        // scan must be performed, and this can be time-consuming. However,
        // these pool look-ups are cached and the overal performance benefit
        // should greatly outweight the cost of these look-ups.
        //
        // This scheme is elaborate and complex, but greatly beneficial, and
        // hopefully worth the extra complexity.
        //
        // Note that the method 'patchLiteral32' is the entry point for
        // this mechanism. Calling code should not need to be aware of the
        // optimizations performed internally.
        //
        // ----

        // Pool slot offset getters.

        static inline ptrdiff_t getPoolSlotOffsetNext(ARMWord slot)
        {
            return ((slot >> 10) & 0x7ff) - 1023;               // Remove bias.
        }

        static inline ptrdiff_t getPoolSlotOffsetPrev(ARMWord slot)
        {
            return ((slot >> 21) & 0x7ff) - 1023;               // Remove bias.
        }

        static inline ptrdiff_t getPoolSlotOffsetLDR(ARMWord slot)
        {
            return (slot & 0x3ff) - 1023 - DefaultPrefetching;  // Remove bias.
        }

        // Pool slot offset setters.

        static inline ARMWord setPoolSlotOffsetNext(ARMWord slot, ptrdiff_t offset)
        {
            offset += 1023;                         // Apply bias.
            ASSERT(!(offset & ~0x7ff));
            return (slot & ~(0x7ff << 10)) | (offset << 10);
        }

        static inline ARMWord setPoolSlotOffsetPrev(ARMWord slot, ptrdiff_t offset)
        {
            offset += 1023;                         // Apply bias.
            ASSERT(!(offset & ~0x7ff));
            return (slot & ~(0x7ff << 21)) | (offset << 21);
        }

        static inline ARMWord setPoolSlotOffsetLDR(ARMWord slot, ptrdiff_t offset)
        {
            offset += 1023 + DefaultPrefetching;    // Apply bias.
            ASSERT(!(offset & ~0x3ff));
            return (slot & ~0x3ff) | offset;
        }

        // Pool slot linked list navigation utilities.

        static inline ARMWord * getNextEmptyPoolSlot(ARMWord * current)
        {
            ptrdiff_t offset = getPoolSlotOffsetNext(*current);
            // If offset is 0 (indicating the end of the list), return NULL.
            return (offset) ? (current + offset) : (NULL);
        }

        static inline ARMWord * getPrevEmptyPoolSlot(ARMWord * current)
        {
            ptrdiff_t offset = getPoolSlotOffsetPrev(*current);
            // Assert that we never back-track beyond the marker slot.
            ASSERT(offset != 1024);
            return current + offset;
        }

        static inline ARMWord * getLDROfPoolSlot(ARMWord * current)
        {
            ptrdiff_t offset = getPoolSlotOffsetLDR(*current);
            // The LDR offset must always be valid, except for the marker slot.
            ASSERT(offset || ((*current & 0xffe003ff) == 0xffe003ff));
            return current + offset;
        }

        // This method scans forward through code looking for a literal pool
        // marker. This is an expensive operation, and should be avoided where
        // possible.
        static ARMWord * findLiteralPool(ARMWord * ldr)
        {
            ARMWord * pool;

            static ARMWord *  lastPool = NULL;  // Cached pool location.
            static ARMWord *  lastLDR = NULL;   // Cache LDR location.

            if (lastPool) {
                // Literal pool loads never jump over literal pools. That is,
                // any pool load will always refer to the next pool, and never
                // the one after it. Therefore, if 'ldr' lies between lastPool
                // and lastLDR, we can guarantee that the last pool base is the
                // right one for 'ldr', and the cost of scanning through code
                // can be avoided.
                if ((ldr >= lastLDR) && (ldr < lastPool)) {
                    // The cached lastPool location is valid for this LDR.
                    // However, don't update lastLDR here. Keep the
                    // lastLDR->lastPool range as large as possible.
                    ASSERT((*lastPool & 0xffe003ff) == 0xffe003ff);
                    return lastPool;
                }
            }

            // Scan to find the start of the literal pool. The marker has a
            // 'prev' offset of 1024 (0xffe00000) and an LDR offset of 0
            // (0x000003ff), but note that the 'next' offset can vary once
            // elements are added to the list.
            for (pool = ldr+1; (*pool & 0xffe003ff) != 0xffe003ff; pool++) {
                // Assert that we don't run off the end of the pool. This
                // should never happen if the marker is correctly placed.
                ASSERT(getApparentPCOffset(ldr, pool) < (ptrdiff_t)(maxPoolSlots * sizeof(ARMWord)));

                if (lastLDR && (pool == lastLDR)) {
                    // If we reach lastLDR, we know that the next pool is after
                    // lastLDR. We can therefore guarantee that lastPool is the
                    // pool referenced by 'ldr', and we can also use 'ldr' to
                    // expand the range of the lastLDR-lastPool cache.
                    lastLDR = ldr;
                    ASSERT((*lastPool & 0xffe003ff) == 0xffe003ff);
                    ASSERT(getApparentPCOffset(ldr, lastPool) < (ptrdiff_t)(maxPoolSlots * sizeof(ARMWord)));
                    return lastPool;
                }
            }

            lastPool = pool;
            lastLDR = ldr;

            ASSERT(pool);
            return pool;
        }

        // Use this when an active pool slot is becoming inactive. The (now
        // inactive) pool slot no longer holds an LDR value and becomes part of
        // the linked list of inactive slots.
        //
        // The previous node ('prev') can be NULL if the location of the pool
        // is not known. In this case, findLiteralPool is invoked to find the
        // marker slot, and 'slot' is inserted immediately after the marker.
        static void setEmptyPoolSlot(ARMWord * slot, ARMWord * prev, ARMWord * ldr)
        {
            // Retrieve and check the prev and next slots. If 'prev' is not
            // specified, point it at the marker slot.
            if (!prev) {
                prev = findLiteralPool(ldr);
            }
            ARMWord * next = getNextEmptyPoolSlot(prev);
            if (!next) {
                // The new slot will be at the end of the list.
                next = slot;
            }

            ASSERT(slot > ldr); // We only use positive LDR offsets.
            ASSERT(prev > ldr); // If this fails, prev is probably a slot in a different pool.

            // Build the new slot and update 'next' and 'prev'.
            ARMWord     prev_scratch = *prev;
            prev_scratch = setPoolSlotOffsetNext(*prev, slot-prev);
            ASSERT((slot-prev) == getPoolSlotOffsetNext(prev_scratch));

            ARMWord     next_scratch = *next;
            next_scratch = setPoolSlotOffsetPrev(*next, slot-next);
            ASSERT((slot-next) == getPoolSlotOffsetPrev(next_scratch));

            ARMWord     slot_scratch = 0;
            slot_scratch = setPoolSlotOffsetPrev(slot_scratch, prev-slot);
            slot_scratch = setPoolSlotOffsetNext(slot_scratch, next-slot);
            slot_scratch = setPoolSlotOffsetLDR(slot_scratch, ldr-slot);

            ASSERT((prev-slot) == getPoolSlotOffsetPrev(slot_scratch));
            ASSERT((next-slot) == getPoolSlotOffsetNext(slot_scratch));
            ASSERT((ldr-slot)  == getPoolSlotOffsetLDR(slot_scratch));

            // Write the new slots out to memory. Ensure that 'next' is written
            // before 'slot'; they are the same if 'slot' is the end of the
            // list.
            *prev = prev_scratch;
            *next = next_scratch;
            *slot = slot_scratch;

            // Assert the validity of the list.
            ASSERT(getNextEmptyPoolSlot(prev) == slot);
            ASSERT(getPrevEmptyPoolSlot(slot) == prev);
            ASSERT(getLDROfPoolSlot(slot) == ldr);
            if (slot != next) {
                ASSERT(getNextEmptyPoolSlot(slot) == next);
                ASSERT(getPrevEmptyPoolSlot(next) == slot);
                ASSERT(getPoolSlotOffsetLDR(*next));
            } else {
                ASSERT(!getNextEmptyPoolSlot(slot));
            }
        }

        // Use this when an inactive pool slot is becoming active. After
        // calling this method, the (now active) pool slot can hold an LDR
        // value and is no longer part of the linked list of inactive slots.
        static inline void clearEmptyPoolSlot(ARMWord * slot)
        {
            ARMWord * prev = getPrevEmptyPoolSlot(slot);
            ARMWord * next = getNextEmptyPoolSlot(slot);

            if (!next) {
                // We're removing the last slot in the list.
                *prev = setPoolSlotOffsetNext(*prev, 0);

                ASSERT(!getNextEmptyPoolSlot(prev));
            } else {
                // Join the two adjacent slots, next and prev.
                *next = setPoolSlotOffsetPrev(*next, prev-next);
                *prev = setPoolSlotOffsetNext(*prev, next-prev);

                ASSERT(getNextEmptyPoolSlot(prev) == next);
                ASSERT(getPrevEmptyPoolSlot(next) == prev);
            }
        }

        // Use this to find an empty pool slot belonging to the specified load
        // site. A base slot can be specified, and this must occur _before_
        // the target slot in the linked list. The marker slot is usually the
        // only safe bet as the list is unlikely to be in any particular order.
        // If base is NULL, findLiteralPool will be used to find it.
        //
        // This method is expensive. It not only calls the already expensive
        // findLiteralPool method, but also walks the linked list. However, it
        // should only be necessary to use this when deoptimizing a
        // single-instruction move or branch into an LDR instruction.
        static ARMWord * findEmptyPoolSlot(ARMWord * base, ARMWord * ldr)
        {
            ARMWord * slot = base;
            if (!slot) {
                slot = findLiteralPool(ldr);
            }
            ASSERT(slot);
            while (getLDROfPoolSlot(slot) != ldr) {
                ARMWord * next = getNextEmptyPoolSlot(slot);
                slot = next;

                // We should never hit the end of the list. If we do, there is
                // no empty slot belonging to the specified LDR site.
                ASSERT(slot);
            }
            return slot;
        }

        // This is the generic patching method for 32-bit literal values. All
        // such values must start off as literal-pool loads, but this method
        // may optimize them into more efficient sequences. Optimized loads are
        // tracked and may be passed into this method again (if 'repatchable'
        // is true).
        //
        // Specify repatchable=false where possible, as this will greatly
        // improve patching time.
        //
        // Specify pic=true if position-independent code is required. Specify
        // this if calling before the code is copied to its final location.
        static void patchLiteral32(ARMWord * from, void* to, bool repatchable=true, bool pic=false)
        {
            ASSERT(!(reinterpret_cast<ARMWord>(from) & 0x3));  // ARM instructions are always word-aligned.

            ARMWord *   slot = tryGetLdrImmAddress(from);

            Condition   cc;
            int         rt;

            if (slot) {
                // ---- Existing code is NOT optimized. ----
                //
                // tryGetLdrImmAddress returned a meaningful address, so 'from'
                // is an LDR instruction (or an LDR-BLX sequence) and has an
                // active literal pool slot.

                if (checkIsLDRLiteral(from, &rt, &cc)) {
                    // Try to optimize the following load:
                    //  from -> "LDR(cc)    rt, [PC, offset]"
                    // If rt is the PC (15), the load is a branch.

                    // Try to emit an optimized move (or branch).
                    if (tryPutImmediateMove(from, cc, reinterpret_cast<ARMWord>(to), rt, pic)) {
                        // Success! Synchonize the caches and update the linked
                        // list of inactive pool slots.
                        ExecutableAllocator::cacheFlush(from, sizeof(ARMWord));
                        if (repatchable) {
                            setEmptyPoolSlot(slot, NULL, from);
                        }
                        return;
                    }
                }

                // Fall back to simply patching the value in the pool slot.
                *slot = reinterpret_cast<ARMWord>(to);
                return;
            } else {
                // ---- Existing code IS optimized. ----
                //
                // tryGetLdrImmAddress did not find a slot, so the instruction
                // is not an LDR (or an LDR-BLX sequence). This can only occur
                // if we've previously optimized an LDR-based sequence to a
                // shorter branch, so this section includes patchers for
                // optimized cases (including de-optimizations where the offset
                // does not fit in the existing optimized case).
                
                // This only occurs on the second patch, so the branch _must_
                // be repatchable if we get here.
                ASSERT(repatchable);

                if (checkIsImmediateMoveOrBranch(from, &rt, &cc)) {
                    // Try to patch optimized immediate loads (where rt<15) or
                    // simple branches (where rt==15). If we can do this
                    // in-place, there is no need to look up the literal pool's
                    // linked list of inactive slots.
                    if (tryPutImmediateMove(from, cc, reinterpret_cast<ARMWord>(to), rt, pic)) {
                        // Success! Synchronize the caches.
                        ExecutableAllocator::cacheFlush(from, sizeof(ARMWord));
                        return;
                    }

                    // Fall back to the slow case:
                    //      "LDR(cc) rt, [PC, offset]"
                    slot = findEmptyPoolSlot(NULL, from);
                    clearEmptyPoolSlot(slot);
                    putLDRLiteral(from, cc, rt, slot);
                    ExecutableAllocator::cacheFlush(from, sizeof(ARMWord));
                    *slot = reinterpret_cast<ARMWord>(to);
                    return;
                }
            }

            // Every possible optimized case (where repatchable was set to
            // true) must be recognized, so this assertion should be
            // unreachable.
            ASSERT_NOT_REACHED();
            return;
        }

        // Patch the LDR instruction ('load') with the specified index, and
        // return the resulting LDR instruction. The result is not a valid LDR
        // instruction. The index simply points into the yet-to-be-flushed
        // literal pool.
        //
        // This method should not be called on any load whose literal pool has
        // already been flushed.
        //
        // As for all literal-pool loads, the offset must be positive.
        static ARMWord patchConstantPoolLoad(ARMWord load, ARMWord index)
        {
            index = (index << 1) + 1;
            ASSERT(!(index & ~0xfff));
            return (load & ~0xfff) | index;
        }

        // Patch an LDR instruction in the instruction stream so that the LDR
        // offset refers to a constant in the constant pool at 'pool'. The
        // existing load instruction at 'load' must contain (as its load
        // offset) an index into the constant pool. This method
        // should be used when flushing the constant pool to the instruction
        // stream. It reads the LDR offset as an index into 'pool' and replaces
        // it with the real offset between the pool slot and 'load'.
        //
        // As for all literal-pool loads, the literal pool must appear _after_
        // the LDR instruction.
        static void patchConstantPoolLoad(void* load, void* pool);

        // Patch pointers

        static void linkPointer(void* code, JmpDst from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##linkPointer     ((%p + %#x)) points to ((%p))\n",
                           code, from.m_offset, to);

            ASSERT(!(from.m_offset & 0x3));
            ARMWord offset = from.m_offset / sizeof(ARMWord);
            patchLiteral32(reinterpret_cast<ARMWord*>(code) + offset, to);
        }

        static void repatchInt32(void* from, int32_t to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##repatchInt32    ((%p)) holds ((%p))\n",
                           from, to);

            patchLiteral32(reinterpret_cast<ARMWord*>(from), reinterpret_cast<void*>(to));
        }

        static void repatchPointer(void* from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##repatchPointer  ((%p)) points to ((%p))\n",
                           from, to);

            patchLiteral32(reinterpret_cast<ARMWord*>(from), to);
        }

        static void repatchLoadPtrToLEA(void* from)
        {
            // On ARM, this is a patch from LDR to ADD. It is restricted conversion,
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

            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "##linkJump         ((%#x)) jumps to ((%#x))\n", MAYBE_PAD,
                           from.m_offset, to.m_offset);

            *addr = to.m_offset;
        }

        static void linkJump(void* code, JmpSrc from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##linkJump        ((%p + %#x)) jumps to ((%p))\n",
                           code, from.m_offset, to);
            ASSERT(!(from.m_offset & 0x3));
            ARMWord offset = from.m_offset / sizeof(ARMWord);
            patchLiteral32(reinterpret_cast<ARMWord*>(code) + offset, to);
        }

        static void relinkJump(void* from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##relinkJump      ((%p)) jumps to ((%p))\n",
                           from, to);

            patchLiteral32(reinterpret_cast<ARMWord*>(from), to);
        }

        static bool canRelinkJump(void* from, void* to)
        {
            return true;
        }

        static void linkCall(void* code, JmpSrc from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##linkCall        ((%p + %#x)) jumps to ((%p))\n",
                           code, from.m_offset, to);

            ASSERT(!(from.m_offset & 0x3));
            ARMWord offset = from.m_offset / sizeof(ARMWord);
            patchLiteral32(reinterpret_cast<ARMWord*>(code) + offset, to);
        }

        static void relinkCall(void* from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##relinkCall      ((%p)) jumps to ((%p))\n",
                           from, to);

            patchLiteral32(reinterpret_cast<ARMWord*>(from), to);
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

        static ARMWord placeConstantPoolMarker()
        {
            // The marker forms the first element in a linked list of empty
            // pool slots. Each unused pool slot has the following structure:
            //
            // Bits:
            //  [31:21]     Offset to previous. (Always +1024 for the marker.)
            //  [20:10]     Offset to next. (Initially 0 for the marker.)
            //  [ 9: 0]     Reverse offset to LDR. (Always 0 for the marker.)
            //
            // Node offsets are aligned to 4 bytes and are stored with a bias
            // of 1023 words. Thus, 0x000 represents an offset of -4092 and
            // 0x7ff (the maximum) represents an offset of 4096.
            //
            // The LDR offset is stored with the same bias, but has fewer bits
            // and can only represent negative offsets. Also, note that the PC
            // offset is taken into account for the LDR offset, so its range
            // matches that of a PC-relative load.
            return 0xffefffff;
        }

    private:
        // pretty-printing functions
        static char const * nameGpReg(int reg)
        {
            static char const * names[] = {
                "r0", "r1", "r2", "r3",
                "r4", "r5", "r6", "r7",
                "r8", "r9", "r10", "r11",
                "ip", "sp", "lr", "pc"
            };
            // Keep GCC's warnings quiet by clamping the subscript range.
            if ((reg >= 0) && (reg <= 15)) {
                return names[reg];
            } else {
                ASSERT(reg <= 15);
                ASSERT(reg >= 0);
                return "!!";
            }
        }

        static char const * nameFpRegD(int reg)
        {
            ASSERT(reg <= 31);
            ASSERT(reg >= 0);
            static char const * names[] = {
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
            static char const * names[] = {
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
            ASSERT(cc >= 0);
            ASSERT((cc & 0x0fffffff) == 0);

            uint32_t    ccIndex = cc >> 28;
            static char const * names[] = {
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
            char    mnemonic[16];
            snprintf(mnemonic, 16, "%s%s", ins, nameCC(cc));

            char    op2_fmt[48];
            fmtOp2(op2_fmt, op2);

            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, mnemonic, nameGpReg(rd), nameGpReg(rn), op2_fmt);
        }

        void spewInsWithOp2(char const * ins, Condition cc, int r, ARMWord op2)
        {
            char    mnemonic[16];
            snprintf(mnemonic, 16, "%s%s", ins, nameCC(cc));

            char    op2_fmt[48];
            fmtOp2(op2_fmt, op2);

            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s\n", MAYBE_PAD, mnemonic, nameGpReg(r), op2_fmt);
        }

        static ARMWord RM(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg;
        }

        static ARMWord RS(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 8;
        }

        static ARMWord RD(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 12;
        }

        static ARMWord RN(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 16;
        }

        static ARMWord DD(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [22,15:12].
            return ((reg << 12) | (reg << 18)) & 0x0040f000;
        }

        static ARMWord DN(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [7,19:16].
            return ((reg << 16) | (reg << 3)) & 0x000f0080;
        }

        static ARMWord DM(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Encoded as bits [5,3:0].
            return ((reg << 1) & 0x20) | (reg & 0xf);
        }

        static ARMWord SD(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Endoded as bits [15:12,22].
            return ((reg << 11) | (reg << 22)) & 0x0040f000;
        }

        static ARMWord SM(int reg)
        {
            ASSERT(reg <= ARMRegisters::d31);
            // Encoded as bits [5,3:0].
            return ((reg << 5) & 0x20) | ((reg >> 1) & 0xf);
        }

        static inline ARMWord getConditionalField(ARMWord i)
        {
            return i & 0xf0000000;
        }

        static inline Condition getCondition(ARMWord i)
        {
            return static_cast<Condition>(getConditionalField(i));
        }

        static inline ARMWord * getApparentPC(ARMWord * insn)
        {
            return insn + DefaultPrefetching;
        }

        static inline ptrdiff_t getApparentPCOffset(ARMWord * insn, ARMWord * target)
        {
            return reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(getApparentPC(insn));
        }

        static inline ptrdiff_t getApparentPCOffset(ARMWord * insn, ARMWord target_addr)
        {
            return getApparentPCOffset(insn, reinterpret_cast<ARMWord*>(target_addr));
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

            FCPYD = 0x0eb00b40,
            FADDD = 0x0e300b00,
            FNEGD = 0x0eb10b40,
            FDIVD = 0x0e800b00,
            FSUBD = 0x0e300b40,
            FMULD = 0x0e200b00,
            FCMPD = 0x0eb40b40,
            FSQRTD = 0x0eb10bc0,
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
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX   "%s%d %s, [%s, #%s%u]\n", MAYBE_PAD, 
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

            js::JaegerSpew(js::JSpew_Insns,
                           IPFX   "vcvt.%s.%-15s, %s,%s\n", MAYBE_PAD, 
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
                JS_NOT_REACHED("Other conversions did not seem useful on 2011/08/04");
            }
        }

        // does r2:r1 -> dn, dn -> r2:r1, r2:r1 -> s2:s1, etc.
        void vmov64 (bool fromFP, bool isDbl, int r1, int r2, int rFP, Condition cc = AL)
        {
            if (fromFP) {
                js::JaegerSpew(js::JSpew_Insns,
                               IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vmov", 
                               nameGpReg(r1), nameGpReg(r2), nameFpRegD(rFP));
            } else {
                js::JaegerSpew(js::JSpew_Insns,
                               IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vmov",
                               nameFpRegD(rFP), nameGpReg(r1), nameGpReg(r2));
            }
            emitVFPInst(static_cast<ARMWord>(cc) | VFP_DXFER | VFP_MOV |
                        (fromFP ? DT_LOAD : 0) |
                        (isDbl ? VFP_DBL : 0), RD(r1), RN(r2), isDbl ? DM(rFP) : SM(rFP));
            
        }

        void fcpyd_r(int dd, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vmov.f64", 
                           nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FCPYD, dd, dd, dm);
        }

        void faddd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vadd.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FADDD, dd, dn, dm);
        }

        void fnegd_r(int dd, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s, %s\n", MAYBE_PAD, "fnegd", nameFpRegD(dd), nameFpRegD(dm));
            m_buffer.putInt(static_cast<ARMWord>(cc) | FNEGD | DD(dd) | DM(dm));
        }

        void fdivd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vdiv.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FDIVD, dd, dn, dm);
        }

        void fsubd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vsub.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FSUBD, dd, dn, dm);
        }

        void fmuld_r(int dd, int dn, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s, %s\n", MAYBE_PAD, "vmul.f64", nameFpRegD(dd), nameFpRegD(dn), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FMULD, dd, dn, dm);
        }

        void fcmpd_r(int dd, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s\n", MAYBE_PAD, "vcmp.f64", nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FCMPD, dd, 0, dm);
        }

        void fsqrtd_r(int dd, int dm, Condition cc = AL)
        {
            js::JaegerSpew(js::JSpew_Insns,
                    IPFX   "%-15s %s, %s\n", MAYBE_PAD, "vsqrt.f64", nameFpRegD(dd), nameFpRegD(dm));
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FSQRTD, dd, 0, dm);
        }

        void fmsr_r(int dd, int rn, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FMSR, rn, dd, 0);
        }

        void fmrs_r(int rd, int dn, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FMRS, rd, dn, 0);
        }

        void fsitod_r(int dd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FSITOD, dd, 0, dm);
        }

        void fuitod_r(int dd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FUITOD, dd, 0, dm);
        }

        void ftosid_r(int fd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FTOSID, fd, 0, dm);
        }

        void ftosizd_r(int fd, int dm, Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            emitInst(static_cast<ARMWord>(cc) | FTOSIZD, fd, 0, dm);
        }

        void fmstat(Condition cc = AL)
        {
            // TODO: emitInst doesn't work for VFP instructions, though it
            // seems to work for current usage.
            m_buffer.putInt(static_cast<ARMWord>(cc) | FMSTAT);
        }
    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)

#endif // ARMAssembler_h
