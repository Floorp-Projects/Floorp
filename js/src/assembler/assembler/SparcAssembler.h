/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef assembler_assembler_SparcAssembler_h
#define assembler_assembler_SparcAssembler_h

#include <assembler/wtf/Platform.h>

// Some debug code uses s(n)printf for instruction logging.
#include <stdio.h>

#if ENABLE_ASSEMBLER && WTF_CPU_SPARC

#include "AssemblerBufferWithConstantPool.h"
#include <assembler/wtf/Assertions.h>

#include "methodjit/Logging.h"
#define IPFX  "        %s"
#define ISPFX "        "
#ifdef JS_METHODJIT_SPEW
# define MAYBE_PAD (isOOLPath ? ">  " : "")
#else
# define MAYBE_PAD ""
#endif

namespace JSC {

    typedef uint32_t SparcWord;

    namespace SparcRegisters {
        typedef enum {
            g0 = 0, // g0 is always 0
            g1 = 1, // g1 is a scratch register for v8
            g2 = 2,
            g3 = 3,
            g4 = 4,
            g5 = 5, // Reserved for system
            g6 = 6, // Reserved for system
            g7 = 7, // Reserved for system

            o0 = 8,
            o1 = 9,
            o2 = 10,
            o3 = 11,
            o4 = 12,
            o5 = 13,
            o6 = 14, // SP
            o7 = 15,

            l0 = 16,
            l1 = 17,
            l2 = 18,
            l3 = 19,
            l4 = 20,
            l5 = 21,
            l6 = 22,
            l7 = 23,

            i0 = 24,
            i1 = 25,
            i2 = 26,
            i3 = 27,
            i4 = 28,
            i5 = 29,
            i6 = 30, // FP
            i7 = 31,

            sp = o6,
            fp = i6
        } RegisterID;

        typedef enum {
            f0 = 0,
            f1 = 1,
            f2 = 2,
            f3 = 3,
            f4 = 4,
            f5 = 5,
            f6 = 6,
            f7 = 7,
            f8 = 8,
            f9 = 9,
            f10 = 10,
            f11 = 11,
            f12 = 12,
            f13 = 13,
            f14 = 14,
            f15 = 15,
            f16 = 16,
            f17 = 17,
            f18 = 18,
            f19 = 19,
            f20 = 20,
            f21 = 21,
            f22 = 22,
            f23 = 23,
            f24 = 24,
            f25 = 25,
            f26 = 26,
            f27 = 27,
            f28 = 28,
            f29 = 29,
            f30 = 30,
            f31 = 31
        } FPRegisterID;

    } // namespace SparcRegisters

    class SparcAssembler : public GenericAssembler {
    public:
        typedef SparcRegisters::RegisterID RegisterID;
        typedef SparcRegisters::FPRegisterID FPRegisterID;
        AssemblerBuffer m_buffer;
        bool oom() const { return m_buffer.oom(); }

        // Sparc conditional constants
        typedef enum {
            ConditionE   = 0x1, // Zero
            ConditionLE  = 0x2,
            ConditionL   = 0x3,
            ConditionLEU = 0x4,
            ConditionCS  = 0x5,
            ConditionNEG = 0x6,
            ConditionVS  = 0x7,
            ConditionA   = 0x8, // branch_always
            ConditionNE  = 0x9, // Non-zero
            ConditionG   = 0xa,
            ConditionGE  = 0xb,
            ConditionGU  = 0xc,
            ConditionCC  = 0xd,
            ConditionVC  = 0xf
        } Condition;


        typedef enum {
            DoubleConditionNE  = 0x1,
            DoubleConditionUL  = 0x3,
            DoubleConditionL   = 0x4,
            DoubleConditionUG  = 0x5,
            DoubleConditionG   = 0x6,
            DoubleConditionE   = 0x9,
            DoubleConditionUE  = 0xa,
            DoubleConditionGE  = 0xb,
            DoubleConditionUGE = 0xc,
            DoubleConditionLE  = 0xd,
            DoubleConditionULE = 0xe
        } DoubleCondition;

        typedef enum {
            BranchOnCondition,
            BranchOnDoubleCondition
        } BranchType;

        class JmpSrc {
            friend class SparcAssembler;
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
            friend class SparcAssembler;
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

            int m_used : 1;
            signed int m_offset : 31;
        };

        // Instruction formating

        void format_2_1(int rd, int op2, int imm22)
        {
            m_buffer.putInt(rd << 25 | op2 << 22 | (imm22 & 0x3FFFFF));
        }

        void format_2_2(int a, int cond, int op2, int disp22)
        {
            format_2_1((a & 0x1) << 4 | (cond & 0xF), op2, disp22);
        }

        void format_2_3(int a, int cond, int op2, int cc1, int cc0, int p, int disp19)
        {
            format_2_2(a, cond, op2, (cc1 & 0x1) << 21 | (cc0 & 0x1) << 20 | (p & 0x1) << 19 | (disp19 & 0x7FFFF));
        }

        void format_2_4(int a, int rcond, int op2, int d16hi, int p, int rs1, int d16lo)
        {
            format_2_2(a, (rcond & 0x7), op2, (d16hi & 0x3) << 20 | (p & 0x1) << 19 | rs1 << 14 | (d16lo & 0x3FFF));
        }

        void format_3(int op1, int rd, int op3, int bits19)
        {
            m_buffer.putInt(op1 << 30 | rd << 25 | op3 << 19 | (bits19 & 0x7FFFF));
        }

        void format_3_1(int op1, int rd, int op3, int rs1, int bit8, int rs2)
        {
            format_3(op1, rd, op3, rs1 << 14 | (bit8 & 0xFF) << 5 | rs2);
        }

        void format_3_1_imm(int op1, int rd, int op3, int rs1, int simm13)
        {
            format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (simm13 & 0x1FFF));
        }

        void format_3_2(int op1, int rd, int op3, int rs1, int rcond, int rs2)
        {
            format_3(op1, rd, op3, rs1 << 14 | (rcond & 0x3) << 10 | rs2);
        }

        void format_3_2_imm(int op1, int rd, int op3, int rs1, int rcond, int simm10)
        {
            format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (rcond & 0x3) << 10 | (simm10 & 0x1FFF));
        }

        void format_3_3(int op1, int rd, int op3, int rs1, int cmask, int mmask)
        {
            format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (cmask & 0x7) << 5 | (mmask & 0xF));
        }
        void format_3_4(int op1, int rd, int op3, int bits19)
        {
            format_3(op1, rd, op3, bits19);
        }

        void format_3_5(int op1, int rd, int op3, int rs1, int x, int rs2)
        {
            format_3(op1, rd, op3, rs1 << 14 | (x & 0x1) << 12 | rs2);
        }

        void format_3_6(int op1, int rd, int op3, int rs1, int shcnt32)
        {
            format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | (shcnt32 & 0x1F));
        }

        void format_3_7(int op1, int rd, int op3, int rs1, int shcnt64)
        {
            format_3(op1, rd, op3, rs1 << 14 | 1 << 13 | 1 << 12 | (shcnt64 & 0x3F));
        }

        void format_3_8(int op1, int rd, int op3, int rs1, int bits9, int rs2)
        {
            format_3(op1, rd, op3, rs1 << 14 | (bits9 & 0x1FF) << 5 | rs2);
        }

        void format_3_9(int op1, int cc1, int cc0, int op3, int rs1, int bits9, int rs2)
        {
            format_3(op1, (cc1 & 0x1) << 1 | (cc0 & 0x1), op3, rs1 << 14 | (bits9 & 0x1FF) << 5 | rs2);
        }

        void format_4_1(int rd, int op3, int rs1, int cc1, int cc0, int rs2)
        {
            format_3(2, rd, op3, rs1 << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | rs2);
        }

        void format_4_1_imm(int rd, int op3, int rs1, int cc1, int cc0, int simm11)
        {
            format_3(2, rd, op3, rs1 << 14 | (cc1 & 0x1) << 12 | 1 << 13 |(cc0 & 0x1) << 11 | (simm11 & 0x7FF));
        }

        void format_4_2(int rd, int op3, int cc2, int cond, int cc1, int cc0, int rs2)
        {
            format_3(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | rs2);
        }

        void format_4_2_imm(int rd, int op3, int cc2, int cond, int cc1, int cc0, int simm11)
        {
            format_3(2, rd, op3, (cc2 & 0x1) << 18 | (cond & 0xF) << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | (simm11 & 0x7FF));
        }

        void format_4_3(int rd, int op3, int rs1, int cc1, int cc0, int swap_trap)
        {
            format_3(2, rd, op3, rs1 << 14 | 1 << 13 | (cc1 & 0x1) << 12 | (cc0 & 0x1) << 11 | (swap_trap & 0x7F));
        }

        void format_4_4(int rd, int op3, int rs1, int rcond, int opf_low, int rs2)
        {
            format_3(2, rd, op3, rs1 << 14 | (rcond & 0x7) << 10 | (opf_low & 0x1F) << 5 | rs2);
        }

        void format_4_5(int rd, int op3, int cond, int opf_cc, int opf_low, int rs2)
        {
            format_3(2, rd, op3, (cond & 0xF) << 14 | (opf_cc & 0x7) << 11 | (opf_low & 0x3F) << 5 | rs2);
        }

        void addcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "addcc       %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x10, rs1, 0, rs2);
        }

        void addcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "addcc       %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x10, rs1, simm13);
        }

        void add_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "add         %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0, rs1, 0, rs2);
        }

        void add_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "add         %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0, rs1, simm13);
        }

        void andcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "andcc       %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x11, rs1, 0, rs2);
        }

        void andcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "andcc       %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x11, rs1, simm13);
        }

        void or_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "or          %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x2, rs1, 0, rs2);
        }

        void or_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "or          %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x2, rs1, simm13);
        }

        // sethi %hi(imm22) rd
        void sethi(int imm22, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sethi       %%hi(0x%x), %s\n", MAYBE_PAD,
                           imm22, nameGpReg(rd));
            format_2_1(rd, 0x4, (imm22 >> 10));
        }

        void sll_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sll         %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_5(2, rd, 0x25, rs1, 0, rs2);
        }

        void sll_imm(int rs1, int shcnt32, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sll         %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), shcnt32, nameGpReg(rd));
            format_3_6(2, rd, 0x25, rs1, shcnt32);
        }

        void sra_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sra         %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_5(2, rd, 0x27, rs1, 0, rs2);
        }

        void sra_imm(int rs1, int shcnt32, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sra         %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), shcnt32, nameGpReg(rd));
            format_3_6(2, rd, 0x27, rs1, shcnt32);
        }

        void srl_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "srl         %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_5(2, rd, 0x26, rs1, 0, rs2);
        }

        void srl_imm(int rs1, int shcnt32, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "srl         %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), shcnt32, nameGpReg(rd));
            format_3_6(2, rd, 0x26, rs1, shcnt32);
        }

        void subcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "subcc       %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x14, rs1, 0, rs2);
        }

        void subcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "subcc       %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x14, rs1, simm13);
        }

        void orcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "orcc        %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x12, rs1, 0, rs2);
        }

        void orcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "orcc        %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x12, rs1, simm13);
        }

        void xorcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "xorcc       %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x13, rs1, 0, rs2);
        }

        void xorcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "xorcc       %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x13, rs1, simm13);
        }

        void xnorcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "xnorcc      %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x17, rs1, 0, rs2);
        }

        void xnorcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "xnorcc      %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x17, rs1, simm13);
        }

        void smulcc_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "smulcc      %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x1b, rs1, 0, rs2);
        }

        void smulcc_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "smulcc      %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x1b, rs1, simm13);
        }

        void ldsb_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldsb        [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0x9, rs1, 0, rs2);
        }

        void ldsb_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldsb        [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(3, rd, 0x9, rs1, simm13);
        }

        void ldub_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldub        [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0x1, rs1, 0, rs2);
        }

        void ldub_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldub        [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(3, rd, 0x1, rs1, simm13);
        }

        void lduw_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "lduw        [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0x0, rs1, 0, rs2);
        }

        void lduwa_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "lduwa       [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0x10, rs1, 0x82, rs2);
        }

        void lduw_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "lduw        [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(3, rd, 0x0, rs1, simm13);
        }

        void ldsh_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldsh        [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0xa, rs1, 0, rs2);
        }

        void ldsh_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ldsh        [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(3, rd, 0xa, rs1, simm13);
        }

        void lduh_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "lduh        [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(3, rd, 0x2, rs1, 0, rs2);
        }

        void lduh_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "lduh        [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(3, rd, 0x2, rs1, simm13);
        }

        void stb_r(int rd, int rs2, int rs1)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "stb         %s, [%s + %s]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), nameGpReg(rs2));
            format_3_1(3, rd, 0x5, rs1, 0, rs2);
        }

        void stb_imm(int rd, int rs1, int simm13)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "stb         %s, [%s + %d]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), simm13);
            format_3_1_imm(3, rd, 0x5, rs1, simm13);
        }

        void sth_r(int rd, int rs2, int rs1)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sth         %s, [%s + %s]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), nameGpReg(rs2));
            format_3_1(3, rd, 0x6, rs1, 0, rs2);
        }

        void sth_imm(int rd, int rs1, int simm13)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "sth         %s, [%s + %d]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), simm13);
            format_3_1_imm(3, rd, 0x6, rs1, simm13);
        }

        void stw_r(int rd, int rs2, int rs1)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "stw         %s, [%s + %s]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), nameGpReg(rs2));
            format_3_1(3, rd, 0x4, rs1, 0, rs2);
        }

        void stw_imm(int rd, int rs1, int simm13)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "stw         %s, [%s + %d]\n", MAYBE_PAD,
                           nameGpReg(rd), nameGpReg(rs1), simm13);
            format_3_1_imm(3, rd, 0x4, rs1, simm13);
        }

        void ldf_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ld          [%s + %s], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameFpReg(rd));
            format_3_1(3, rd, 0x20, rs1, 0, rs2);
        }

        void ldf_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ld          [%s + %d], %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameFpReg(rd));
            format_3_1_imm(3, rd, 0x20, rs1, simm13);
        }

        void stf_r(int rd, int rs2, int rs1)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "st          %s, [%s + %s]\n", MAYBE_PAD,
                           nameFpReg(rd), nameGpReg(rs1), nameGpReg(rs2));
            format_3_1(3, rd, 0x24, rs1, 0, rs2);
        }

        void stf_imm(int rd, int rs1, int simm13)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "st          %s, [%s + %d]\n", MAYBE_PAD,
                           nameFpReg(rd), nameGpReg(rs1), simm13);
            format_3_1_imm(3, rd, 0x24, rs1, simm13);
        }

        void fmovd_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fmovd       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0x2, rs2);
        }

        void fcmpd_r(int rs1, int rs2)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fcmpd       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs1), nameFpReg(rs2));
            format_3_9(2, 0, 0, 0x35, rs1, 0x52, rs2);
        }

        void nop()
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "nop\n", MAYBE_PAD);
            format_2_1(0, 0x4, 0);
        }

        void branch_con(Condition cond, int target)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "b%s         0x%x\n", MAYBE_PAD,
                           nameICC(cond), target);
            format_2_2(0, cond, 0x2, target);
        }

        void fbranch_con(DoubleCondition cond, int target)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fb%s        0x%x\n", MAYBE_PAD,
                           nameFCC(cond), target);
            format_2_2(0, cond, 0x6, target);
        }

        void rdy(int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "rdy         %s\n", MAYBE_PAD,
                           nameFpReg(rd));
            format_3_1(2, rd, 0x28, 0, 0, 0);
        }

        void rdpc(int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "rdpc        %s\n", MAYBE_PAD,
                           nameGpReg(rd));
            format_3_1(2, rd, 0x28, 5, 0, 0);
        }
        void jmpl_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "jmpl        %s + %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x38, rs1, 0, rs2);
        }

        void jmpl_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "jmpl        %s + %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x38, rs1, simm13);
        }

        void save_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "save        %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x3c, rs1, 0, rs2);
        }

        void save_imm(int rs1, int simm13, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "save        %s, %d, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), simm13, nameGpReg(rd));
            format_3_1_imm(2, rd, 0x3c, rs1, simm13);
        }

        void restore_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "restore     %s, %s, %s\n", MAYBE_PAD,
                           nameGpReg(rs1), nameGpReg(rs2), nameGpReg(rd));
            format_3_1(2, rd, 0x3d, rs1, 0, rs2);
        }

        void ta_imm(int swap_trap)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "ta          %d\n", MAYBE_PAD,
                           swap_trap);
            format_4_3(0x8, 0xa, 0, 0, 0, swap_trap);
        }

        void movcc_imm(int simm11, int rd, Condition cond)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "mov%s       %d, %s\n", MAYBE_PAD,
                           nameICC(cond), simm11, nameGpReg(rd));
            format_4_2_imm(rd, 0x2c, 1, cond, 0, 0, simm11);
        }

        void fabss_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fabss       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0x9, rs2);
        }

        void faddd_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "faddd       %s, %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs1), nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, rs1, 0x42, rs2);
        }

        void fsubd_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fsubd       %s, %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs1), nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, rs1, 0x46, rs2);
        }

        void fmuld_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fmuld       %s, %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs1), nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, rs1, 0x4a, rs2);
        }

        void fdivd_r(int rs1, int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fdivd       %s, %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs1), nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, rs1, 0x4e, rs2);
        }

        void fsqrtd_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fsqartd     %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0x2a, rs2);
        }

        void fabsd_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fabsd     %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0x0a, rs2);
        }

        void fnegd_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fnegd       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0x06, rs2);
        }

        void fitod_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fitod       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0xc8, rs2);
        }

        void fdtoi_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fdtoi       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0xd2, rs2);
        }

        void fdtos_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fdtos       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0xc6, rs2);
        }

        void fstod_r(int rs2, int rd)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "fstod       %s, %s\n", MAYBE_PAD,
                           nameFpReg(rs2), nameFpReg(rd));
            format_3_8(2, rd, 0x34, 0, 0xc9, rs2);
        }

        static bool isimm13(int imm)
        {
            return (imm) <= 0xfff && (imm) >= -0x1000;
        }

        static bool isimm22(int imm)
        {
            return (imm) <= 0x1fffff && (imm) >= -0x200000;
        }

        void move_nocheck(int imm_v, RegisterID dest)
        {
            sethi(imm_v, dest);
            or_imm(dest, imm_v & 0x3FF, dest);
        }

        JmpSrc call()
        {
            JmpSrc r = JmpSrc(m_buffer.size());
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "call        %d\n", MAYBE_PAD,
                           r.m_offset);
            m_buffer.putInt(0x40000000);
            nop();
            return r;
        }

        JmpSrc jump_common(BranchType branchtype, int cond)
        {
            if (branchtype == BranchOnCondition)
                branch_con(Condition(cond), 0);
            else
                fbranch_con(DoubleCondition(cond), 0);

            nop();
            branch_con(ConditionA, 7);
            nop();
            move_nocheck(0, SparcRegisters::g2);
            rdpc(SparcRegisters::g3);
            jmpl_r(SparcRegisters::g2, SparcRegisters::g3, SparcRegisters::g0);
            nop();
            return JmpSrc(m_buffer.size());
        }

        JmpSrc branch(Condition cond)
        {
            return jump_common(BranchOnCondition, cond);
        }

        JmpSrc fbranch(DoubleCondition cond)
        {
            return jump_common(BranchOnDoubleCondition, cond);
        }

        JmpSrc jmp()
        {
            return jump_common(BranchOnCondition, ConditionA);
        }

        // Assembler admin methods:

        JmpDst label()
        {
            JmpDst r = JmpDst(m_buffer.size());
            js::JaegerSpew(js::JSpew_Insns,
                           IPFX "#label     ((%d))\n", MAYBE_PAD, r.m_offset);
            return r;
        }

        // General helpers

        size_t size() const { return m_buffer.size(); }
        unsigned char *buffer() const { return m_buffer.buffer(); }

        static int getDifferenceBetweenLabels(JmpDst src, JmpDst dst)
        {
            return dst.m_offset - src.m_offset;
        }
    
        static int getDifferenceBetweenLabels(JmpDst src, JmpSrc dst)
        {
            return dst.m_offset - src.m_offset;
        }
    
        static int getDifferenceBetweenLabels(JmpSrc src, JmpDst dst)
        {
            return dst.m_offset - src.m_offset;
        }

        static unsigned getCallReturnOffset(JmpSrc call)
        {
            return call.m_offset + 20;
        }

        static void* getRelocatedAddress(void* code, JmpSrc jump)
        {
            ASSERT(jump.m_offset != -1);

            return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + jump.m_offset);
        }
    
        static void* getRelocatedAddress(void* code, JmpDst destination)
        {
            ASSERT(destination.m_offset != -1);

            return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + destination.m_offset);
        }

        void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool **poolp, CodeKind kind)
        {
            return m_buffer.executableAllocAndCopy(allocator, poolp, kind);
        }

        void* executableCopy(void* buffer)
        {
            return memcpy(buffer, m_buffer.buffer(), size());
        }

        static void patchPointerInternal(void* where, int value)
        {
            // Patch move_nocheck.
            uint32_t *branch = (uint32_t*) where;
            branch[0] &= 0xFFC00000;
            branch[0] |= (value >> 10) & 0x3FFFFF;
            branch[1] &= 0xFFFFFC00;
            branch[1] |= value & 0x3FF;
            ExecutableAllocator::cacheFlush(where, 8);
        }

        static void patchbranch(void* where, int value)
        {
            uint32_t *branch = (uint32_t*) where;
            branch[0] &= 0xFFC00000;
            branch[0] |= value & 0x3FFFFF;
            ExecutableAllocator::cacheFlush(where, 4);
        }

        static bool canRelinkJump(void* from, void* to)
        {
            return true;
        }

        static void relinkJump(void* from, void* to)
        {
            from = (void *)((int)from - 36);
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##link     ((%p)) jumps to ((%p))\n",
                           from, to);

            int value = ((int)to - (int)from) / 4;
            if (isimm22(value)) 
                patchbranch(from, value);
            else {
                patchbranch(from, 4);
                from = (void *)((intptr_t)from + 16);
                patchPointerInternal(from, (int)(value * 4 - 24));
            }
        }

        void linkJump(JmpSrc from, JmpDst to)
        {
            ASSERT(from.m_offset != -1);
            ASSERT(to.m_offset != -1);
            intptr_t code = (intptr_t)(m_buffer.data());
            void *where = (void *)((intptr_t)code + from.m_offset);
            void *target = (void *)((intptr_t)code + to.m_offset);
            relinkJump(where, target);
        }

        static void linkJump(void* code, JmpSrc from, void* to)
        {
            ASSERT(from.m_offset != -1);
            void *where = (void *)((intptr_t)code + from.m_offset);
            relinkJump(where, to);
        }

        static void relinkCall(void* from, void* to)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##relinkCall ((from=%p)) ((to=%p))\n",
                           from, to);

            void * where= (void *)((intptr_t)from - 20);
            patchPointerInternal(where, (int)to);
            ExecutableAllocator::cacheFlush(where, 8);
        }

        static void linkCall(void* code, JmpSrc where, void* to)
        {
            void *from = (void *)((intptr_t)code + where.m_offset);
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##linkCall ((from=%p)) ((to=%p))\n",
                           from, to);
            int disp = ((int)to - (int)from)/4;
            *(uint32_t *)((int)from) &= 0x40000000;
            *(uint32_t *)((int)from) |= disp & 0x3fffffff;
            ExecutableAllocator::cacheFlush(from, 4);
        }

        static void linkPointer(void* code, JmpDst where, void* value)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##linkPointer     ((%p + %#x)) points to ((%p))\n",
                           code, where.m_offset, value);

            void *from = (void *)((intptr_t)code + where.m_offset);
            patchPointerInternal(from, (int)value);
        }

        static void repatchInt32(void* where, int value)
        {
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##repatchInt32 ((where=%p)) holds ((value=%d))\n",
                           where, value);

            patchPointerInternal(where, value);
        }

        static void repatchPointer(void* where, void* value)
        { 
            js::JaegerSpew(js::JSpew_Insns,
                           ISPFX "##repatchPointer ((where = %p)) points to ((%p))\n",
                           where, value);

            patchPointerInternal(where, (int)value);
        }

        static void repatchLoadPtrToLEA(void* where)
        {
            // sethi is used. The offset is in a register
            if (*(uint32_t *)((int)where) & 0x01000000)
                where = (void *)((intptr_t)where + 8);

            *(uint32_t *)((int)where) &= 0x3fffffff;
            *(uint32_t *)((int)where) |= 0x80000000;
            ExecutableAllocator::cacheFlush(where, 4);
        }

        static void repatchLEAToLoadPtr(void* where)
        {
            // sethi is used. The offset is in a register
            if (*(uint32_t *)((int)where) & 0x01000000)
                where = (void *)((intptr_t)where + 8);

            *(uint32_t *)((int)where) &= 0x3fffffff;
            *(uint32_t *)((int)where) |= 0xc0000000;
            ExecutableAllocator::cacheFlush(where, 4);
        }

    private:
        static char const * nameGpReg(int reg)
        {
            ASSERT(reg <= 31);
            ASSERT(reg >= 0);
            static char const * const names[] = {
                "%g0", "%g1", "%g2", "%g3",
                "%g4", "%g5", "%g6", "%g7",
                "%o0", "%o1", "%o2", "%o3",
                "%o4", "%o5", "%sp", "%o7",
                "%l0", "%l1", "%l2", "%l3",
                "%l4", "%l5", "%l6", "%l7",
                "%i0", "%i1", "%i2", "%i3",
                "%i4", "%i5", "%fp", "%i7"
            };
            return names[reg];
        }

        static char const * nameFpReg(int reg)
        {
            ASSERT(reg <= 31);
            ASSERT(reg >= 0);
            static char const * const names[] = {
                "%f0",   "%f1",   "%f2",   "%f3",
                "%f4",   "%f5",   "%f6",   "%f7",
                "%f8",   "%f9",  "%f10",  "%f11",
                "%f12",  "%f13",  "%f14",  "%f15",
                "%f16",  "%f17",  "%f18",  "%f19",
                "%f20",  "%f21",  "%f22",  "%f23",
                "%f24",  "%f25",  "%f26",  "%f27",
                "%f28",  "%f29",  "%f30",  "%f31"
            };
            return names[reg];
        }

        static char const * nameICC(Condition cc)
        {
            ASSERT(cc <= ConditionVC);
            ASSERT(cc >= 0);

            uint32_t    ccIndex = cc;
            static char const * const inames[] = {
                "   ", "e  ",
                "le ", "l  ",
                "leu", "cs ",
                "neg", "vs ",
                "a  ", "ne ",
                "g  ", "ge ",
                "gu ", "cc ",
                "   ", "vc "
            };
            return inames[ccIndex];
        }

        static char const * nameFCC(DoubleCondition cc)
        {
            ASSERT(cc <= DoubleConditionULE);
            ASSERT(cc >= 0);

            uint32_t    ccIndex = cc;
            static char const * const fnames[] = {
                "   ", "ne ",
                "   ", "ul ",
                "l  ", "ug ",
                "g  ", "   ",
                "   ", "e  ",
                "ue ", "ge ",
                "ugu", "le ",
                "ule", "   "
            };
            return fnames[ccIndex];
        }


    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(SPARC)

#endif /* assembler_assembler_SparcAssembler_h */
