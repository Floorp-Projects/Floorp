/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef X86Assembler_h
#define X86Assembler_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && (WTF_CPU_X86 || WTF_CPU_X86_64)

#include "AssemblerBuffer.h"
#include "jsstdint.h"
#include "assembler/wtf/Assertions.h"
#include "jsvector.h"

#include "methodjit/Logging.h"
#define IPFX  "        %s"
#define ISPFX "        "
#ifdef JS_METHODJIT_SPEW
# define MAYBE_PAD (isOOLPath ? ">  " : "")
# define PRETTY_PRINT_OFFSET(os) (((os)<0)?"-":""), (((os)<0)?-(os):(os))
# define FIXME_INSN_PRINTING                                \
    do {                                                    \
        js::JaegerSpew(js::JSpew_Insns,                     \
                       ISPFX "FIXME insn printing %s:%d\n", \
                       __FILE__, __LINE__);                 \
    } while (0)
#else
# define MAYBE_PAD ""
# define FIXME_INSN_PRINTING ((void) 0)
# define PRETTY_PRINT_OFFSET(os) "", 0
#endif


namespace JSC {

inline bool CAN_SIGN_EXTEND_8_32(int32_t value) { return value == (int32_t)(signed char)value; }

namespace X86Registers {
    typedef enum {
        eax,
        ecx,
        edx,
        ebx,
        esp,
        ebp,
        esi,
        edi

#if WTF_CPU_X86_64
       ,r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15
#endif
    } RegisterID;

    typedef enum {
        xmm0,
        xmm1,
        xmm2,
        xmm3,
        xmm4,
        xmm5,
        xmm6,
        xmm7
    } XMMRegisterID;

    static const char* nameFPReg(XMMRegisterID fpreg)
    {
        static const char* xmmnames[8]
          = { "%xmm0", "%xmm1", "%xmm2", "%xmm3",
              "%xmm4", "%xmm5", "%xmm6", "%xmm7" };
        int off = (XMMRegisterID)fpreg - (XMMRegisterID)xmm0;
        return (off < 0 || off > 7) ? "%xmm?" : xmmnames[off];
    }

    static const char* nameIReg(int szB, RegisterID reg)
    {
        static const char* r64names[16]
          = { "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
              "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15" };
        static const char* r32names[16]
          = { "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
              "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d" };
        static const char* r16names[16]
          = { "%ax", "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di",
              "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w" };
        static const char* r8names[16]
          = { "%al", "%cl", "%dl", "%bl", "%ah/spl", "%ch/bpl", "%dh/sil", "%bh/dil",
              "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b" };
        int          off = (RegisterID)reg - (RegisterID)eax;
        const char** tab = r64names;
        switch (szB) {
            case 1: tab = r8names; break;
            case 2: tab = r16names; break;
            case 4: tab = r32names; break;
        }
        return (off < 0 || off > 15) ? "%r???" : tab[off];
    }

    static const char* nameIReg(RegisterID reg)
    {
#       if WTF_CPU_X86_64
        return nameIReg(8, reg);
#       else
        return nameIReg(4, reg);
#       endif
    }

} /* namespace X86Registers */


class X86Assembler {
public:
    typedef X86Registers::RegisterID RegisterID;
    typedef X86Registers::XMMRegisterID XMMRegisterID;
    typedef XMMRegisterID FPRegisterID;

    typedef enum {
        ConditionO,
        ConditionNO,
        ConditionB,
        ConditionAE,
        ConditionE,
        ConditionNE,
        ConditionBE,
        ConditionA,
        ConditionS,
        ConditionNS,
        ConditionP,
        ConditionNP,
        ConditionL,
        ConditionGE,
        ConditionLE,
        ConditionG,

        ConditionC  = ConditionB,
        ConditionNC = ConditionAE
    } Condition;

    static const char* nameCC(Condition cc)
    {
        static const char* names[16]
          = { "o ", "no", "b ", "ae", "e ", "ne", "be", "a ",
              "s ", "ns", "p ", "np", "l ", "ge", "le", "g " };
        int ix = (int)cc;
        return (ix < 0 || ix > 15) ? "??" : names[ix];
    }

private:
    typedef enum {
        OP_ADD_EvGv                     = 0x01,
        OP_ADD_GvEv                     = 0x03,
        OP_OR_EvGv                      = 0x09,
        OP_OR_GvEv                      = 0x0B,
        OP_2BYTE_ESCAPE                 = 0x0F,
        OP_3BYTE_ESCAPE                 = 0x3A,
        OP_AND_EvGv                     = 0x21,
        OP_AND_GvEv                     = 0x23,
        OP_SUB_EvGv                     = 0x29,
        OP_SUB_GvEv                     = 0x2B,
        PRE_PREDICT_BRANCH_NOT_TAKEN    = 0x2E,
        OP_XOR_EvGv                     = 0x31,
        OP_XOR_GvEv                     = 0x33,
        OP_CMP_EvGv                     = 0x39,
        OP_CMP_GvEv                     = 0x3B,
#if WTF_CPU_X86_64
        PRE_REX                         = 0x40,
#endif
        OP_PUSH_EAX                     = 0x50,
        OP_POP_EAX                      = 0x58,
#if WTF_CPU_X86
        OP_PUSHA                        = 0x60,
        OP_POPA                         = 0x61,
#endif
#if WTF_CPU_X86_64
        OP_MOVSXD_GvEv                  = 0x63,
#endif
        PRE_OPERAND_SIZE                = 0x66,
        PRE_SSE_66                      = 0x66,
        OP_PUSH_Iz                      = 0x68,
        OP_IMUL_GvEvIz                  = 0x69,
        OP_GROUP1_EbIb                  = 0x80,
        OP_GROUP1_EvIz                  = 0x81,
        OP_GROUP1_EvIb                  = 0x83,
        OP_TEST_EvGv                    = 0x85,
        OP_XCHG_EvGv                    = 0x87,
        OP_MOV_EbGv                     = 0x88,
        OP_MOV_EvGv                     = 0x89,
        OP_MOV_GvEv                     = 0x8B,
        OP_LEA                          = 0x8D,
        OP_GROUP1A_Ev                   = 0x8F,
        OP_CDQ                          = 0x99,
        OP_MOV_EAXOv                    = 0xA1,
        OP_MOV_OvEAX                    = 0xA3,
        OP_MOV_EAXIv                    = 0xB8,
        OP_GROUP2_EvIb                  = 0xC1,
        OP_RET                          = 0xC3,
        OP_GROUP11_EvIb                 = 0xC6,
        OP_GROUP11_EvIz                 = 0xC7,
        OP_INT3                         = 0xCC,
        OP_GROUP2_Ev1                   = 0xD1,
        OP_GROUP2_EvCL                  = 0xD3,
        OP_CALL_rel32                   = 0xE8,
        OP_JMP_rel32                    = 0xE9,
        PRE_SSE_F2                      = 0xF2,
        PRE_SSE_F3                      = 0xF3,
        OP_HLT                          = 0xF4,
        OP_GROUP3_EbIb                  = 0xF6,
        OP_GROUP3_Ev                    = 0xF7,
        OP_GROUP3_EvIz                  = 0xF7, // OP_GROUP3_Ev has an immediate, when instruction is a test. 
        OP_GROUP5_Ev                    = 0xFF
    } OneByteOpcodeID;

    typedef enum {
        OP2_MOVSD_VsdWsd    = 0x10,
        OP2_MOVSD_WsdVsd    = 0x11,
        OP2_UNPCKLPS_VsdWsd = 0x14,
        OP2_CVTSI2SD_VsdEd  = 0x2A,
        OP2_CVTTSD2SI_GdWsd = 0x2C,
        OP2_UCOMISD_VsdWsd  = 0x2E,
        OP2_ADDSD_VsdWsd    = 0x58,
        OP2_MULSD_VsdWsd    = 0x59,
        OP2_CVTSS2SD_VsdEd  = 0x5A,
        OP2_CVTSD2SS_VsdEd  = 0x5A,
        OP2_SUBSD_VsdWsd    = 0x5C,
        OP2_DIVSD_VsdWsd    = 0x5E,
        OP2_SQRTSD_VsdWsd   = 0x51,
        OP2_ANDPD_VpdWpd    = 0x54,
        OP2_XORPD_VpdWpd    = 0x57,
        OP2_MOVD_VdEd       = 0x6E,
        OP2_PSRLDQ_Vd       = 0x73,
        OP2_MOVD_EdVd       = 0x7E,
        OP2_JCC_rel32       = 0x80,
        OP_SETCC            = 0x90,
        OP2_IMUL_GvEv       = 0xAF,
        OP2_MOVSX_GvEb      = 0xBE,
        OP2_MOVSX_GvEw      = 0xBF,
        OP2_MOVZX_GvEb      = 0xB6,
        OP2_MOVZX_GvEw      = 0xB7,
        OP2_PEXTRW_GdUdIb   = 0xC5
    } TwoByteOpcodeID;

    typedef enum {
        OP3_PINSRD_VsdWsd   = 0x22
    } ThreeByteOpcodeID;

    TwoByteOpcodeID jccRel32(Condition cond)
    {
        return (TwoByteOpcodeID)(OP2_JCC_rel32 + cond);
    }

    TwoByteOpcodeID setccOpcode(Condition cond)
    {
        return (TwoByteOpcodeID)(OP_SETCC + cond);
    }

    typedef enum {
        GROUP1_OP_ADD = 0,
        GROUP1_OP_OR  = 1,
        GROUP1_OP_ADC = 2,
        GROUP1_OP_AND = 4,
        GROUP1_OP_SUB = 5,
        GROUP1_OP_XOR = 6,
        GROUP1_OP_CMP = 7,

        GROUP1A_OP_POP = 0,

        GROUP2_OP_SHL = 4,
        GROUP2_OP_SHR = 5,
        GROUP2_OP_SAR = 7,

        GROUP3_OP_TEST = 0,
        GROUP3_OP_NOT  = 2,
        GROUP3_OP_NEG  = 3,
        GROUP3_OP_IDIV = 7,

        GROUP5_OP_CALLN = 2,
        GROUP5_OP_JMPN  = 4,
        GROUP5_OP_PUSH  = 6,

        GROUP11_MOV = 0
    } GroupOpcodeID;
    
    class X86InstructionFormatter;
public:

#ifdef JS_METHODJIT_SPEW
    bool isOOLPath;
#endif

    class JmpSrc {
        friend class X86Assembler;
        friend class X86InstructionFormatter;
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
        friend class X86Assembler;
        friend class X86InstructionFormatter;
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

        signed int m_offset : 31;
        bool m_used : 1;
    };

    X86Assembler()
#ifdef JS_METHODJIT_SPEW
      : isOOLPath(false)
#endif
    {
    }

    size_t size() const { return m_formatter.size(); }
    unsigned char *buffer() const { return m_formatter.buffer(); }
    bool oom() const { return m_formatter.oom(); }

    // Stack operations:

    void push_r(RegisterID reg)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "push       %s\n", MAYBE_PAD, nameIReg(reg));
        m_formatter.oneByteOp(OP_PUSH_EAX, reg);
    }

    void pop_r(RegisterID reg)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "pop        %s\n", MAYBE_PAD, nameIReg(reg));
        m_formatter.oneByteOp(OP_POP_EAX, reg);
    }

    void push_i32(int imm)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "pushl      %s$0x%x\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(imm));
        m_formatter.oneByteOp(OP_PUSH_Iz);
        m_formatter.immediate32(imm);
    }

    void push_m(int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "push       %s0x%x(%s)\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_PUSH, base, offset);
    }

    void pop_m(int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP1A_Ev, GROUP1A_OP_POP, base, offset);
    }

    // Arithmetic operations:

#if !WTF_CPU_X86_64
    void adcl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADC, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADC, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void addl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addl       %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_ADD_EvGv, src, dst);
    }

    void addl_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_ADD_GvEv, dst, base, offset);
    }

    void addl_rm(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_ADD_EvGv, src, base, offset);
    }

    void addl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addl       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, dst);
            m_formatter.immediate32(imm);
        }
    }

    void addl_im(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addl       %d, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(8,base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if WTF_CPU_X86_64
    void addq_rr(RegisterID src, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_ADD_EvGv, src, dst);
    }

    void addq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addq       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(8,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_ADD, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_ADD, dst);
            m_formatter.immediate32(imm);
        }
    }

    void addq_im(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addq       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(8,base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_ADD, base, offset);
            m_formatter.immediate32(imm);
        }
    }
#else
    void addl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_ADD, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_ADD, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void andl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andl       %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_AND_EvGv, src, dst);
    }

    void andl_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_AND_GvEv, dst, base, offset);
    }

    void andl_rm(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_AND_EvGv, src, base, offset);
    }

    void andl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andl       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_AND, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_AND, dst);
            m_formatter.immediate32(imm);
        }
    }

    void andl_im(int imm, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_AND, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_AND, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if WTF_CPU_X86_64
    void andq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andq       %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_AND_EvGv, src, dst);
    }

    void andq_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andq       %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(8,base), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_AND_GvEv, dst, base, offset);
    }

    void orq_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "orq        %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(8,base), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_OR_GvEv, dst, base, offset);
    }

    void andq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andq       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(8,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_AND, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_AND, dst);
            m_formatter.immediate32(imm);
        }
    }
#else
    void andl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_AND, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_AND, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void negl_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "negl       %s\n", MAYBE_PAD, nameIReg(4,dst));
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_NEG, dst);
    }

    void negl_m(int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_NEG, base, offset);
    }

    void notl_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "notl       %s\n", MAYBE_PAD, nameIReg(4,dst));
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_NOT, dst);
    }

    void notl_m(int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_NOT, base, offset);
    }

    void orl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "orl        %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_OR_EvGv, src, dst);
    }

    void orl_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_OR_GvEv, dst, base, offset);
    }

    void orl_rm(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_OR_EvGv, src, base, offset);
    }

    void orl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "orl        $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_OR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_OR, dst);
            m_formatter.immediate32(imm);
        }
    }

    void orl_im(int imm, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_OR, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_OR, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if WTF_CPU_X86_64
    void negq_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "negq       %s\n", MAYBE_PAD, nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_GROUP3_Ev, GROUP3_OP_NEG, dst);
    }

    void orq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "orq        %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_OR_EvGv, src, dst);
    }

    void orq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "orq        $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(8,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_OR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_OR, dst);
            m_formatter.immediate32(imm);
        }
    }

    void notq_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "notq       %s\n", MAYBE_PAD, nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_GROUP3_Ev, GROUP3_OP_NOT, dst);
    }
#else
    void orl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_OR, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_OR, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void subl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subl       %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_SUB_EvGv, src, dst);
    }

    void subl_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_SUB_GvEv, dst, base, offset);
    }

    void subl_rm(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_SUB_EvGv, src, base, offset);
    }

    void subl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subl       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4, dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, dst);
            m_formatter.immediate32(imm);
        }
    }
    
    void subl_im(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subl       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(4, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, base, offset);
            m_formatter.immediate32(imm);
        }
    }

#if WTF_CPU_X86_64
    void subq_rr(RegisterID src, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_SUB_EvGv, src, dst);
    }

    void subq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subq       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(8,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_SUB, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_SUB, dst);
            m_formatter.immediate32(imm);
        }
    }
#else
    void subl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_SUB, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_SUB, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void xorl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xorl       %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_XOR_EvGv, src, dst);
    }

    void xorl_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_XOR_GvEv, dst, base, offset);
    }

    void xorl_rm(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_XOR_EvGv, src, base, offset);
    }

    void xorl_im(int imm, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_XOR, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_XOR, base, offset);
            m_formatter.immediate32(imm);
        }
    }

    void xorl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xorl       %d, %s\n", MAYBE_PAD,
                       imm, nameIReg(4,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_XOR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_XOR, dst);
            m_formatter.immediate32(imm);
        }
    }

#if WTF_CPU_X86_64
    void xorq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xorq       %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8, dst));
        m_formatter.oneByteOp64(OP_XOR_EvGv, src, dst);
    }

    void xorq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xorq       %d, %s\n", MAYBE_PAD,
                       imm, nameIReg(8,dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_XOR, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_XOR, dst);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void sarl_i8r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "sarl       $%d, %s\n", MAYBE_PAD, imm, nameIReg(4, dst));
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, GROUP2_OP_SAR, dst);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, GROUP2_OP_SAR, dst);
            m_formatter.immediate8(imm);
        }
    }

    void sarl_CLr(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "sarl       %%cl, %s\n", MAYBE_PAD, nameIReg(4, dst));
        m_formatter.oneByteOp(OP_GROUP2_EvCL, GROUP2_OP_SAR, dst);
    }
    
    void shrl_i8r(int imm, RegisterID dst)
    {
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, GROUP2_OP_SHR, dst);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, GROUP2_OP_SHR, dst);
            m_formatter.immediate8(imm);
        }
    }
    
    void shrl_CLr(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP2_EvCL, GROUP2_OP_SHR, dst);
    }

    void shll_i8r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "shll       $%d, %s\n", MAYBE_PAD, imm, nameIReg(4, dst));
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, GROUP2_OP_SHL, dst);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, GROUP2_OP_SHL, dst);
            m_formatter.immediate8(imm);
        }
    }

    void shll_CLr(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "shll       %%cl, %s\n", MAYBE_PAD, nameIReg(4, dst));
        m_formatter.oneByteOp(OP_GROUP2_EvCL, GROUP2_OP_SHL, dst);
    }

#if WTF_CPU_X86_64
    void sarq_CLr(RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_GROUP2_EvCL, GROUP2_OP_SAR, dst);
    }

    void sarq_i8r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "sarq       $%d, %s\n", MAYBE_PAD, imm, nameIReg(8, dst));
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, GROUP2_OP_SAR, dst);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, GROUP2_OP_SAR, dst);
            m_formatter.immediate8(imm);
        }
    }

    void shlq_i8r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "shlq       $%d, %s\n", MAYBE_PAD, imm, nameIReg(8, dst));
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, GROUP2_OP_SHL, dst);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, GROUP2_OP_SHL, dst);
            m_formatter.immediate8(imm);
        }
    }
#endif

    void imull_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "imull       %s, %s\n", MAYBE_PAD, nameIReg(4,src), nameIReg(4, dst));
        m_formatter.twoByteOp(OP2_IMUL_GvEv, dst, src);
    }

    void imull_mr(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(OP2_IMUL_GvEv, dst, base, offset);
    }

    void imull_i32r(RegisterID src, int32_t value, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_IMUL_GvEvIz, dst, src);
        m_formatter.immediate32(value);
    }

    void idivl_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "idivl      %s\n", MAYBE_PAD, 
                       nameIReg(4, dst));
        m_formatter.oneByteOp(OP_GROUP3_Ev, GROUP3_OP_IDIV, dst);
    }

    // Comparisons:

    void cmpl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       %s, %s\n", MAYBE_PAD, 
                       nameIReg(4, src), nameIReg(4, dst));
        m_formatter.oneByteOp(OP_CMP_EvGv, src, dst);
    }

    void cmpl_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       %s, %s0x%x(%s)\n", MAYBE_PAD, 
                       nameIReg(4, src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_CMP_EvGv, src, base, offset);
    }

    void cmpl_mr(int offset, RegisterID base, RegisterID src)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       %s0x%x(%s), %s\n", MAYBE_PAD, 
                       PRETTY_PRINT_OFFSET(offset), nameIReg(4, base), nameIReg(src));
        m_formatter.oneByteOp(OP_CMP_GvEv, src, base, offset);
    }

    void cmpl_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4, dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
            m_formatter.immediate32(imm);
        }
    }

    void cmpl_ir_force32(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       $0x%x, %s\n", MAYBE_PAD, imm, nameIReg(4, dst));
        m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
        m_formatter.immediate32(imm);
    }
    
    void cmpl_im(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(4,base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate32(imm);
        }
    }
    
    void cmpb_im(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP1_EbIb, GROUP1_OP_CMP, base, offset);
        m_formatter.immediate8(imm);
    }
    
    void cmpb_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp(OP_GROUP1_EbIb, GROUP1_OP_CMP, base, index, scale, offset);
        m_formatter.immediate8(imm);
    }

    void cmpl_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpl       %d, %d(%s,%s,%d)\n", MAYBE_PAD,
                       imm, offset, nameIReg(4,base), nameIReg(4,index), scale);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate32(imm);
        }
    }

    void cmpl_im_force32(int imm, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
        m_formatter.immediate32(imm);
    }

#if WTF_CPU_X86_64
    void cmpq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpq       %s, %s\n", MAYBE_PAD,
                       nameIReg(8, src), nameIReg(8, dst));
        m_formatter.oneByteOp64(OP_CMP_EvGv, src, dst);
    }

    void cmpq_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpq       %s, %d(%s)\n", MAYBE_PAD,
                       nameIReg(8, src), offset, nameIReg(8, base));
        m_formatter.oneByteOp64(OP_CMP_EvGv, src, base, offset);
    }

    void cmpq_mr(int offset, RegisterID base, RegisterID src)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpq       %d(%s), %s\n", MAYBE_PAD,
                       offset, nameIReg(8, base), nameIReg(8, src));
        m_formatter.oneByteOp64(OP_CMP_GvEv, src, base, offset);
    }

    void cmpq_ir(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cmpq       %d, %s\n", MAYBE_PAD,
                       imm, nameIReg(8, dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, dst);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, dst);
            m_formatter.immediate32(imm);
        }
    }

    void cmpq_im(int imm, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, offset);
            m_formatter.immediate32(imm);
        }
    }

    void cmpq_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate32(imm);
        }
    }
#else
    void cmpl_rm(RegisterID reg, void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_CMP_EvGv, reg, addr);
    }

    void cmpl_im(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, addr);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, addr);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void cmpw_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        FIXME_INSN_PRINTING;
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, src, base, index, scale, offset);
    }

    void cmpw_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        FIXME_INSN_PRINTING;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.prefix(PRE_OPERAND_SIZE);
            m_formatter.oneByteOp(OP_GROUP1_EvIb, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate8(imm);
        } else {
            m_formatter.prefix(PRE_OPERAND_SIZE);
            m_formatter.oneByteOp(OP_GROUP1_EvIz, GROUP1_OP_CMP, base, index, scale, offset);
            m_formatter.immediate16(imm);
        }
    }

    void testl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testl      %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_TEST_EvGv, src, dst);
    }
    
    void testl_i32r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testl      $0x%x, %s\n", MAYBE_PAD,
                       imm, nameIReg(dst));
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, dst);
        m_formatter.immediate32(imm);
    }

    void testl_i32m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testl      $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, offset);
        m_formatter.immediate32(imm);
    }
    
    void testb_im(int imm, int offset, RegisterID base)
    {
        m_formatter.oneByteOp(OP_GROUP3_EbIb, GROUP3_OP_TEST, base, offset);
        m_formatter.immediate8(imm);
    }
    
    void testb_im(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        m_formatter.oneByteOp(OP_GROUP3_EbIb, GROUP3_OP_TEST, base, index, scale, offset);
        m_formatter.immediate8(imm);
    }

    void testl_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }

#if WTF_CPU_X86_64
    void testq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testq      %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_TEST_EvGv, src, dst);
    }

    void testq_i32r(int imm, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, dst);
        m_formatter.immediate32(imm);
    }

    void testq_i32m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testq      $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, offset);
        m_formatter.immediate32(imm);
    }

    void testq_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, GROUP3_OP_TEST, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }
#endif 

    void testw_rr(RegisterID src, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_TEST_EvGv, src, dst);
    }
    
    void testb_i8r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "testb      $0x%x, %s\n", MAYBE_PAD,
                       imm, nameIReg(1,dst));
        m_formatter.oneByteOp8(OP_GROUP3_EbIb, GROUP3_OP_TEST, dst);
        m_formatter.immediate8(imm);
    }

    void setCC_r(Condition cond, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "set%s      %s\n", MAYBE_PAD,
                       nameCC(cond), nameIReg(1,dst));
        m_formatter.twoByteOp8(setccOpcode(cond), (GroupOpcodeID)0, dst);
    }

    void sete_r(RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp8(setccOpcode(ConditionE), (GroupOpcodeID)0, dst);
    }

    void setz_r(RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        sete_r(dst);
    }

    void setne_r(RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp8(setccOpcode(ConditionNE), (GroupOpcodeID)0, dst);
    }

    void setnz_r(RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        setne_r(dst);
    }

    // Various move ops:

    void cdq()
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cdq              \n", MAYBE_PAD);
        m_formatter.oneByteOp(OP_CDQ);
    }

    void xchgl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xchgl      %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_XCHG_EvGv, src, dst);
    }

#if WTF_CPU_X86_64
    void xchgq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xchgq      %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_XCHG_EvGv, src, dst);
    }
#endif

    void movl_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_MOV_EvGv, src, dst);
    }

    void movw_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movw       %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameIReg(2,src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, offset);
    }
    
    void movl_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameIReg(4,src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, offset);
    }

    void movl_rm_disp32(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp_disp32(OP_MOV_EvGv, src, base, offset);
    }

    void movw_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movw       %s, %d(%s,%s,%d)\n", MAYBE_PAD, 
                       nameIReg(2, src), offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, index, scale, offset);
    }

    void movl_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       %s, %d(%s,%s,%d)\n", MAYBE_PAD, 
                       nameIReg(4, src), offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.oneByteOp(OP_MOV_EvGv, src, base, index, scale, offset);
    }
    
    void movl_mEAX(void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_MOV_EAXOv);
#if WTF_CPU_X86_64
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int>(addr));
#endif
    }

    void movl_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4, dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, dst, base, offset);
    }

    void movl_mr_disp32(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp_disp32(OP_MOV_GvEv, dst, base, offset);
    }

    void movl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(4, dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, dst, base, index, scale, offset);
    }

    void movl_i32r(int imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       $0x%x, %s\n", MAYBE_PAD,
                       imm, nameIReg(dst));
        m_formatter.oneByteOp(OP_MOV_EAXIv, dst);
        m_formatter.immediate32(imm);
    }

    void movb_i8m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movb       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_GROUP11_EvIb, GROUP11_MOV, base, offset);
        m_formatter.immediate8(imm);
    }

    void movb_i8m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movb       $0x%x, %d(%s,%s,%d)\n", MAYBE_PAD,
                       imm, offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.oneByteOp(OP_GROUP11_EvIb, GROUP11_MOV, base, index, scale, offset);
        m_formatter.immediate8(imm);
    }

    void movw_i16m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movw       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, base, offset);
        m_formatter.immediate16(imm);
    }

    void movl_i32m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       $0x%x, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, base, offset);
        m_formatter.immediate32(imm);
    }

    void movw_i16m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movw       $0x%x, %d(%s,%s,%d)\n", MAYBE_PAD,
                       imm, offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, base, index, scale, offset);
        m_formatter.immediate16(imm);
    }

    void movl_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       $0x%x, %d(%s,%s,%d)\n", MAYBE_PAD,
                       imm, offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }

    void movl_EAXm(void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_MOV_OvEAX);
#if WTF_CPU_X86_64
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int>(addr));
#endif
    }

#if WTF_CPU_X86_64
    void movq_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %s, %s\n", MAYBE_PAD,
                       nameIReg(8,src), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, dst);
    }

    void movq_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameIReg(8,src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, base, offset);
    }

    void movq_rm_disp32(RegisterID src, int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64_disp32(OP_MOV_EvGv, src, base, offset);
    }

    void movq_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameIReg(8,src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp64(OP_MOV_EvGv, src, base, index, scale, offset);
    }

    void movq_mEAX(void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_MOV_EAXOv);
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
    }

    void movq_EAXm(void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64(OP_MOV_OvEAX);
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
    }

    void movq_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX  "movq       %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, dst, base, offset);
    }

    void movq_mr_disp32(int offset, RegisterID base, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp64_disp32(OP_MOV_GvEv, dst, base, offset);
    }

    void movq_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, dst, base, index, scale, offset);
    }

    void leaq_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "leaq       %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(8,dst)),
        m_formatter.oneByteOp64(OP_LEA, dst, base, index, scale, offset);
    }

    void movq_i32m(int imm, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX  "movq       $%d, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, GROUP11_MOV, base, offset);
        m_formatter.immediate32(imm);
    }

    void movq_i32m(int imm, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX  "movq       $%d, %s0x%x(%s)\n", MAYBE_PAD,
                       imm, PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, GROUP11_MOV, base, index, scale, offset);
        m_formatter.immediate32(imm);
    }

    void movq_i64r(int64_t imm, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movabsq    $0x%llx, %s\n", MAYBE_PAD,
                       (unsigned long long int)imm, nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_MOV_EAXIv, dst);
        m_formatter.immediate64(imm);
    }
    
    void movsxd_rr(RegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsxd     %s, %s\n", MAYBE_PAD,
                       nameIReg(4, src), nameIReg(8, dst));
        m_formatter.oneByteOp64(OP_MOVSXD_GvEv, dst, src);
    }
    
    
#else
    void movl_rm(RegisterID src, void* addr)
    {
        FIXME_INSN_PRINTING;
        if (src == X86Registers::eax)
            movl_EAXm(addr);
        else 
            m_formatter.oneByteOp(OP_MOV_EvGv, src, addr);
    }
    
    void movl_mr(void* addr, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movl       0(%p), %s\n", MAYBE_PAD,
                       addr, nameIReg(4, dst));
        if (dst == X86Registers::eax)
            movl_mEAX(addr);
        else
            m_formatter.oneByteOp(OP_MOV_GvEv, dst, addr);
    }

    void movl_i32m(int imm, void* addr)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP11_EvIz, GROUP11_MOV, addr);
        m_formatter.immediate32(imm);
    }
#endif

    void movb_rm(RegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movb       %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameIReg(1, src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.oneByteOp8(OP_MOV_EbGv, src, base, offset);
    }

    void movb_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movb       %s, %d(%s,%s,%d)\n", MAYBE_PAD,
                       nameIReg(1, src), offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.oneByteOp8(OP_MOV_EbGv, src, base, index, scale, offset);
    }

    void movzbl_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movzbl     %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4, dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEb, dst, base, offset);
    }

    void movzbl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movzbl     %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEb, dst, base, index, scale, offset);
    }

    void movxbl_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movxbl     %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4, dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEb, dst, base, offset);
    }

    void movxbl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movxbl     %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEb, dst, base, index, scale, offset);
    }

    void movzwl_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movzwl     %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4, dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, dst, base, offset);
    }

    void movzwl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movzwl     %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, dst, base, index, scale, offset);
    }

    void movxwl_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movxwl     %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4, dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, dst, base, offset);
    }

    void movxwl_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movxwl     %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, dst, base, index, scale, offset);
    }

    void movzbl_rr(RegisterID src, RegisterID dst)
    {
        // In 64-bit, this may cause an unnecessary REX to be planted (if the dst register
        // is in the range ESP-EDI, and the src would not have required a REX).  Unneeded
        // REX prefixes are defined to be silently ignored by the processor.
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movzbl      %s, %s\n", MAYBE_PAD,
                       nameIReg(4,src), nameIReg(4,dst));
        m_formatter.twoByteOp8(OP2_MOVZX_GvEb, dst, src);
    }

    void leal_mr(int offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "leal       %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameIReg(dst));
        m_formatter.oneByteOp(OP_LEA, dst, base, index, scale, offset);
    }

    void leal_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "leal       %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(4,dst));
        m_formatter.oneByteOp(OP_LEA, dst, base, offset);
    }
#if WTF_CPU_X86_64
    void leaq_mr(int offset, RegisterID base, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "leaq       %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameIReg(8,dst));
        m_formatter.oneByteOp64(OP_LEA, dst, base, offset);
    }
#endif

    // Flow control:

    JmpSrc call()
    {
        m_formatter.oneByteOp(OP_CALL_rel32);
        JmpSrc r = m_formatter.immediateRel32();
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "call       ((%d))\n", MAYBE_PAD, r.m_offset);
        return r;
    }
    
    JmpSrc call(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_CALLN, dst);
        JmpSrc r = JmpSrc(m_formatter.size());
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "call       *%s\n", MAYBE_PAD, nameIReg(dst));
        return r;
    }
    
    void call_m(int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_CALLN, base, offset);
    }

    JmpSrc jmp()
    {
        m_formatter.oneByteOp(OP_JMP_rel32);
        JmpSrc r = m_formatter.immediateRel32();
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "jmp        ((%d))\n", MAYBE_PAD, r.m_offset);
        return r;
    }
    
    // Return a JmpSrc so we have a label to the jump, so we can use this
    // To make a tail recursive call on x86-64.  The MacroAssembler
    // really shouldn't wrap this as a Jump, since it can't be linked. :-/
    JmpSrc jmp_r(RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "jmp        ((%s))\n", MAYBE_PAD,
           nameIReg(dst));
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_JMPN, dst);
        return JmpSrc(m_formatter.size());
    }
    
    void jmp_m(int offset, RegisterID base)
    {
        FIXME_INSN_PRINTING;
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_JMPN, base, offset);
    }

    void jmp_m(int offset, RegisterID base, RegisterID index, int scale) {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "jmp       ((%d(%s,%s,%d)))\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.oneByteOp(OP_GROUP5_Ev, GROUP5_OP_JMPN, base, index, scale, offset);
    }

    JmpSrc jne()
    {
        return jCC(ConditionNE);
    }
    
    JmpSrc jnz()
    {
        // printing done by jne()
        return jne();
    }

    JmpSrc je()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionE));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jz()
    {
        // printing done by je()
        return je();
    }

    JmpSrc jl()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionL));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jb()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionB));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jle()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionLE));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jbe()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionBE));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jge()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionGE));
        return m_formatter.immediateRel32();
    }

    JmpSrc jg()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionG));
        return m_formatter.immediateRel32();
    }

    JmpSrc ja()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionA));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jae()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionAE));
        return m_formatter.immediateRel32();
    }
    
    JmpSrc jo()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionO));
        return m_formatter.immediateRel32();
    }

    JmpSrc jp()
    {
        return jCC(ConditionP);
    }
    
    JmpSrc js()
    {
        FIXME_INSN_PRINTING;
        m_formatter.twoByteOp(jccRel32(ConditionS));
        return m_formatter.immediateRel32();
    }

    JmpSrc jCC(Condition cond)
    {
        m_formatter.twoByteOp(jccRel32(cond));
        JmpSrc r = m_formatter.immediateRel32();
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "j%s        ((%d))\n", MAYBE_PAD,
                       nameCC(cond), r.m_offset);
        return r;
    }

    // SSE operations:

    void addsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addsd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_ADDSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void addsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "addsd      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_ADDSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void cvtss2sd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtps2pd   %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F3);
        m_formatter.twoByteOp(OP2_CVTSS2SD_VsdEd, (RegisterID)dst, (RegisterID)src);
    }

    void cvtsd2ss_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtps2pd   %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTSD2SS_VsdEd, (RegisterID)dst, (RegisterID)src);
    }

    void cvtsi2sd_rr(RegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtsi2sd   %s, %s\n", MAYBE_PAD,
                       nameIReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTSI2SD_VsdEd, (RegisterID)dst, src);
    }

#if WTF_CPU_X86_64
    void cvtsq2sd_rr(RegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtsq2sd   %s, %s\n", MAYBE_PAD,
                       nameIReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp64(OP2_CVTSI2SD_VsdEd, (RegisterID)dst, src);
    }
#endif

    void cvtsi2sd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtsi2sd   %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTSI2SD_VsdEd, (RegisterID)dst, base, offset);
    }

#if !WTF_CPU_X86_64
    void cvtsi2sd_mr(void* address, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvtsi2sd   %p, %s\n", MAYBE_PAD,
                       address, nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTSI2SD_VsdEd, (RegisterID)dst, address);
    }
#endif

    void cvttsd2si_rr(XMMRegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "cvttsd2si  %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameIReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_CVTTSD2SI_GdWsd, dst, (RegisterID)src);
    }

    void unpcklps_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "unpcklps   %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.twoByteOp(OP2_UNPCKLPS_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void movd_rr(RegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movd       %s, %s\n", MAYBE_PAD,
                       nameIReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_MOVD_VdEd, (RegisterID)dst, src);
    }

    void psrldq_rr(XMMRegisterID dest, int shift)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "pslldq     %s, %d\n", MAYBE_PAD,
                       nameFPReg(dest), shift);
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_PSRLDQ_Vd, (RegisterID)3, (RegisterID)dest);
        m_formatter.immediate8(shift);
    }

    void movd_rr(XMMRegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movd       %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameIReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_MOVD_EdVd, (RegisterID)src, dst);
    }

#if WTF_CPU_X86_64
    void movq_rr(XMMRegisterID src, RegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameIReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp64(OP2_MOVD_EdVd, (RegisterID)src, dst);
    }

    void movq_rr(RegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movq       %s, %s\n", MAYBE_PAD,
                       nameIReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp64(OP2_MOVD_VdEd, (RegisterID)dst, src);
    }
#endif

    void movsd_rm(XMMRegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd      %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameFPReg(src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_WsdVsd, (RegisterID)src, base, offset);
    }

    void movss_rm(XMMRegisterID src, int offset, RegisterID base)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movss      %s, %s0x%x(%s)\n", MAYBE_PAD,
                       nameFPReg(src), PRETTY_PRINT_OFFSET(offset), nameIReg(base));
        m_formatter.prefix(PRE_SSE_F3);
        m_formatter.twoByteOp(OP2_MOVSD_WsdVsd, (RegisterID)src, base, offset);
    }

    void movss_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movss      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F3);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void movsd_rm(XMMRegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd       %s, %d(%s,%s,%d)\n", MAYBE_PAD, 
                       nameFPReg(src), offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_WsdVsd, (RegisterID)src, base, index, scale, offset);
    }

    void movss_rm(XMMRegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movss       %s, %d(%s,%s,%d)\n", MAYBE_PAD, 
                       nameFPReg(src), offset, nameIReg(base), nameIReg(index), scale);
        m_formatter.prefix(PRE_SSE_F3);
        m_formatter.twoByteOp(OP2_MOVSD_WsdVsd, (RegisterID)src, base, index, scale, offset);
    }

    void movss_mr(int offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movss      %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F3);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, base, index, scale, offset);
    }

    void movsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void movsd_mr(int offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd      %d(%s,%s,%d), %s\n", MAYBE_PAD,
                       offset, nameIReg(base), nameIReg(index), scale, nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, base, index, scale, offset);
    }

    void movsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

#if !WTF_CPU_X86_64
    void movsd_mr(const void* address, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "movsd      %p, %s\n", MAYBE_PAD,
                       address, nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MOVSD_VsdWsd, (RegisterID)dst, address);
    }
#endif

    void mulsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "mulsd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MULSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void mulsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "mulsd      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_MULSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void pextrw_irr(int whichWord, XMMRegisterID src, RegisterID dst)
    {
        FIXME_INSN_PRINTING;
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_PEXTRW_GdUdIb, (RegisterID)dst, (RegisterID)src);
        m_formatter.immediate8(whichWord);
    }

    void subsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subsd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_SUBSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void subsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "subsd      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_SUBSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void ucomisd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "ucomisd    %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_UCOMISD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void ucomisd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "ucomisd    %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_UCOMISD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void divsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "divsd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_DIVSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void divsd_mr(int offset, RegisterID base, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "divsd      %s0x%x(%s), %s\n", MAYBE_PAD,
                       PRETTY_PRINT_OFFSET(offset), nameIReg(base), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_DIVSD_VsdWsd, (RegisterID)dst, base, offset);
    }

    void xorpd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "xorpd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_XORPD_VpdWpd, (RegisterID)dst, (RegisterID)src);
    }

    void andpd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "andpd      %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.twoByteOp(OP2_ANDPD_VpdWpd, (RegisterID)dst, (RegisterID)src);
    }

    void sqrtsd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "sqrtsd     %s, %s\n", MAYBE_PAD,
                       nameFPReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_F2);
        m_formatter.twoByteOp(OP2_SQRTSD_VsdWsd, (RegisterID)dst, (RegisterID)src);
    }

    void pinsrd_rr(RegisterID src, XMMRegisterID dst)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "pinsrd     $1, %s, %s\n", MAYBE_PAD,
                       nameIReg(src), nameFPReg(dst));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.threeByteOp(OP3_PINSRD_VsdWsd, (RegisterID)dst, (RegisterID)src);
        m_formatter.immediate8(0x01); // the $1
    }

    // Misc instructions:

    void int3()
    {
        js::JaegerSpew(js::JSpew_Insns, IPFX "int3\n", MAYBE_PAD);
        m_formatter.oneByteOp(OP_INT3);
    }
    
    void ret()
    {
        js::JaegerSpew(js::JSpew_Insns, IPFX "ret\n", MAYBE_PAD);
        m_formatter.oneByteOp(OP_RET);
    }

    void predictNotTaken()
    {
        FIXME_INSN_PRINTING;
        m_formatter.prefix(PRE_PREDICT_BRANCH_NOT_TAKEN);
    }

#if WTF_CPU_X86
    void pusha()
    {
        js::JaegerSpew(js::JSpew_Insns, IPFX "pusha\n", MAYBE_PAD);
        m_formatter.oneByteOp(OP_PUSHA);
    }

    void popa()
    {
        js::JaegerSpew(js::JSpew_Insns, IPFX "popa\n", MAYBE_PAD);
        m_formatter.oneByteOp(OP_POPA);
    }
#endif

    // Assembler admin methods:

    JmpDst label()
    {
        JmpDst r = JmpDst(m_formatter.size());
        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "#label     ((%d))\n", MAYBE_PAD, r.m_offset);
        return r;
    }
    
    static JmpDst labelFor(JmpSrc jump, intptr_t offset = 0)
    {
        FIXME_INSN_PRINTING;
        return JmpDst(jump.m_offset + offset);
    }
    
    JmpDst align(int alignment)
    {
        FIXME_INSN_PRINTING;
        while (!m_formatter.isAligned(alignment))
            m_formatter.oneByteOp(OP_HLT);

        return label();
    }

    // Linking & patching:
    //
    // 'link' and 'patch' methods are for use on unprotected code - such as the code
    // within the AssemblerBuffer, and code being patched by the patch buffer.  Once
    // code has been finalized it is (platform support permitting) within a non-
    // writable region of memory; to modify the code in an execute-only execuable
    // pool the 'repatch' and 'relink' methods should be used.

    void linkJump(JmpSrc from, JmpDst to)
    {
        ASSERT(from.m_offset != -1);
        ASSERT(to.m_offset != -1);

        js::JaegerSpew(js::JSpew_Insns,
                       IPFX "##link     ((%d)) jumps to ((%d))\n", MAYBE_PAD,
                       from.m_offset, to.m_offset);
        char* code = reinterpret_cast<char*>(m_formatter.data());
        setRel32(code + from.m_offset, code + to.m_offset);
    }
    
    static void linkJump(void* code, JmpSrc from, void* to)
    {
        ASSERT(from.m_offset != -1);

        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##link     ((%d)) jumps to ((%p))\n",
                       from.m_offset, to);
        setRel32(reinterpret_cast<char*>(code) + from.m_offset, to);
    }

    static void linkCall(void* code, JmpSrc from, void* to)
    {
        ASSERT(from.m_offset != -1);

        FIXME_INSN_PRINTING;
        setRel32(reinterpret_cast<char*>(code) + from.m_offset, to);
    }

    static void linkPointer(void* code, JmpDst where, void* value)
    {
        ASSERT(where.m_offset != -1);

        FIXME_INSN_PRINTING;
        setPointer(reinterpret_cast<char*>(code) + where.m_offset, value);
    }

    static void relinkJump(void* from, void* to)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##relinkJump ((from=%p)) ((to=%p))\n",
                       from, to);
        setRel32(from, to);
    }

    static bool canRelinkJump(void* from, void* to)
    {
        intptr_t offset = reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from);
        return (offset == static_cast<int32_t>(offset));
    }
    
    static void relinkCall(void* from, void* to)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##relinkCall ((from=%p)) ((to=%p))\n",
                       from, to);
        setRel32(from, to);
    }

    static void repatchInt32(void* where, int32_t value)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##relinkInt32 ((where=%p)) ((value=%d))\n",
                       where, value);
        setInt32(where, value);
    }

    static void repatchPointer(void* where, void* value)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##repatchPtr ((where=%p)) ((value=%p))\n",
                       where, value);
        setPointer(where, value);
    }

    static void repatchLoadPtrToLEA(void* where)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##repatchLoadPtrToLEA ((where=%p))\n",
                       where);

#if WTF_CPU_X86_64
        // On x86-64 pointer memory accesses require a 64-bit operand, and as such a REX prefix.
        // Skip over the prefix byte.
        where = reinterpret_cast<char*>(where) + 1;
#endif
        *reinterpret_cast<unsigned char*>(where) = static_cast<unsigned char>(OP_LEA);
    }
    
    static void repatchLEAToLoadPtr(void* where)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##repatchLEAToLoadPtr ((where=%p))\n",
                       where);
#if WTF_CPU_X86_64
        // On x86-64 pointer memory accesses require a 64-bit operand, and as such a REX prefix.
        // Skip over the prefix byte.
        where = reinterpret_cast<char*>(where) + 1;
#endif
        *reinterpret_cast<unsigned char*>(where) = static_cast<unsigned char>(OP_MOV_GvEv);
    }

    static unsigned getCallReturnOffset(JmpSrc call)
    {
        ASSERT(call.m_offset >= 0);
        return call.m_offset;
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
    
    void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool **poolp, CodeKind kind)
    {
        return m_formatter.executableAllocAndCopy(allocator, poolp, kind);
    }

    void executableCopy(void* buffer)
    {
        memcpy(buffer, m_formatter.buffer(), size());
    }

private:

    static void setPointer(void* where, void* value)
    {
        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##setPtr     ((where=%p)) ((value=%p))\n", where, value);
        reinterpret_cast<void**>(where)[-1] = value;
    }

    static void setInt32(void* where, int32_t value)
    {
        reinterpret_cast<int32_t*>(where)[-1] = value;
    }

    static void setRel32(void* from, void* to)
    {
        intptr_t offset = reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from);
        ASSERT(offset == static_cast<int32_t>(offset));
#define JS_CRASH(x) *(int *)x = 0
        if (offset != static_cast<int32_t>(offset))
            JS_CRASH(0xC0DE);
#undef JS_CRASH

        js::JaegerSpew(js::JSpew_Insns,
                       ISPFX "##setRel32 ((from=%p)) ((to=%p))\n", from, to);
        setInt32(from, offset);
    }

    class X86InstructionFormatter {

        static const int maxInstructionSize = 16;

    public:

        // Legacy prefix bytes:
        //
        // These are emmitted prior to the instruction.

        void prefix(OneByteOpcodeID pre)
        {
            m_buffer.putByte(pre);
        }

        // Word-sized operands / no operand instruction formatters.
        //
        // In addition to the opcode, the following operand permutations are supported:
        //   * None - instruction takes no operands.
        //   * One register - the low three bits of the RegisterID are added into the opcode.
        //   * Two registers - encode a register form ModRm (for all ModRm formats, the reg field is passed first, and a GroupOpcodeID may be passed in its place).
        //   * Three argument ModRM - a register, and a register and an offset describing a memory operand.
        //   * Five argument ModRM - a register, and a base register, an index, scale, and offset describing a memory operand.
        //
        // For 32-bit x86 targets, the address operand may also be provided as a void*.
        // On 64-bit targets REX prefixes will be planted as necessary, where high numbered registers are used.
        //
        // The twoByteOp methods plant two-byte Intel instructions sequences (first opcode byte 0x0F).

        void oneByteOp(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void oneByteOp_disp32(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(reg, base, offset);
        }

        void oneByteOp(OneByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

#if !WTF_CPU_X86_64
        void oneByteOp(OneByteOpcodeID opcode, int reg, void* address)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, address);
        }
#endif

        void twoByteOp(TwoByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

#if !WTF_CPU_X86_64
        void twoByteOp(TwoByteOpcodeID opcode, int reg, const void* address)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, address);
        }
#endif

        void threeByteOp(ThreeByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(OP_3BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

#if WTF_CPU_X86_64
        // Quad-word-sized operands:
        //
        // Used to format 64-bit operantions, planting a REX.w prefix.
        // When planting d64 or f64 instructions, not requiring a REX.w prefix,
        // the normal (non-'64'-postfixed) formatters should be used.

        void oneByteOp64(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(0, 0, 0);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp64(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void oneByteOp64_disp32(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(reg, base, offset);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, int reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }
#endif

        // Byte-operands:
        //
        // These methods format byte operations.  Byte operations differ from the normal
        // formatters in the circumstances under which they will decide to emit REX prefixes.
        // These should be used where any register operand signifies a byte register.
        //
        // The disctinction is due to the handling of register numbers in the range 4..7 on
        // x86-64.  These register numbers may either represent the second byte of the first
        // four registers (ah..bh) or the first byte of the second four registers (spl..dil).
        //
        // Since ah..bh cannot be used in all permutations of operands (specifically cannot
        // be accessed where a REX prefix is present), these are likely best treated as
        // deprecated.  In order to ensure the correct registers spl..dil are selected a
        // REX prefix will be emitted for any byte register operand in the range 4..15.
        //
        // These formatters may be used in instructions where a mix of operand sizes, in which
        // case an unnecessary REX will be emitted, for example:
        //     movzbl %al, %edi
        // In this case a REX will be planted since edi is 7 (and were this a byte operand
        // a REX would be required to specify dil instead of bh).  Unneeded REX prefixes will
        // be silently ignored by the processor.
        //
        // Address operands should still be checked using regRequiresRex(), while byteRegRequiresRex()
        // is provided to check byte register operands.

        void oneByteOp8(OneByteOpcodeID opcode, GroupOpcodeID groupOp, RegisterID rm)
        {
#if !WTF_CPU_X86_64
            ASSERT(!byteRegRequiresRex(rm));
#endif
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(groupOp, rm);
        }

        void oneByteOp8(OneByteOpcodeID opcode, int reg, RegisterID base, int offset)
        {
#if !WTF_CPU_X86_64
            ASSERT(!byteRegRequiresRex(reg));
#endif
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, offset);
        }

        void oneByteOp8(OneByteOpcodeID opcode, int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
#if !WTF_CPU_X86_64
            ASSERT(!byteRegRequiresRex(reg));
#endif
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(reg, base, index, scale, offset);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, RegisterID reg, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg)|byteRegRequiresRex(rm), reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(reg, rm);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, GroupOpcodeID groupOp, RegisterID rm)
        {
            m_buffer.ensureSpace(maxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(groupOp, rm);
        }

        // Immediates:
        //
        // An immedaite should be appended where appropriate after an op has been emitted.
        // The writes are unchecked since the opcode formatters above will have ensured space.

        void immediate8(int imm)
        {
            m_buffer.putByteUnchecked(imm);
        }

        void immediate16(int imm)
        {
            m_buffer.putShortUnchecked(imm);
        }

        void immediate32(int imm)
        {
            m_buffer.putIntUnchecked(imm);
        }

        void immediate64(int64_t imm)
        {
            m_buffer.putInt64Unchecked(imm);
        }

        JmpSrc immediateRel32()
        {
            m_buffer.putIntUnchecked(0);
            return JmpSrc(m_buffer.size());
        }

        // Administrative methods:

        size_t size() const { return m_buffer.size(); }
        unsigned char *buffer() const { return m_buffer.buffer(); }
        bool oom() const { return m_buffer.oom(); }
        bool isAligned(int alignment) const { return m_buffer.isAligned(alignment); }
        void* data() const { return m_buffer.data(); }
        void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool** poolp, CodeKind kind) {
            return m_buffer.executableAllocAndCopy(allocator, poolp, kind);
        }

    private:

        // Internals; ModRm and REX formatters.

        // Byte operand register spl & above require a REX prefix (to prevent the 'H' registers be accessed).
        inline bool byteRegRequiresRex(int reg)
        {
            return (reg >= X86Registers::esp);
        }

        static const RegisterID noBase = X86Registers::ebp;
        static const RegisterID hasSib = X86Registers::esp;
        static const RegisterID noIndex = X86Registers::esp;
#if WTF_CPU_X86_64
        static const RegisterID noBase2 = X86Registers::r13;
        static const RegisterID hasSib2 = X86Registers::r12;

        // Registers r8 & above require a REX prefixe.
        inline bool regRequiresRex(int reg)
        {
            return (reg >= X86Registers::r8);
        }

        // Format a REX prefix byte.
        inline void emitRex(bool w, int r, int x, int b)
        {
            m_buffer.putByteUnchecked(PRE_REX | ((int)w << 3) | ((r>>3)<<2) | ((x>>3)<<1) | (b>>3));
        }

        // Used to plant a REX byte with REX.w set (for 64-bit operations).
        inline void emitRexW(int r, int x, int b)
        {
            emitRex(true, r, x, b);
        }

        // Used for operations with byte operands - use byteRegRequiresRex() to check register operands,
        // regRequiresRex() to check other registers (i.e. address base & index).
        // 
        // NB: WebKit's use of emitRexIf() is limited such that the reqRequiresRex() checks are
        // not needed. SpiderMonkey extends oneByteOp8 functionality such that r, x, and b can
        // all be used.
        inline void emitRexIf(bool condition, int r, int x, int b)
        {
            if (condition || regRequiresRex(r) || regRequiresRex(x) || regRequiresRex(b))
                emitRex(false, r, x, b);
        }

        // Used for word sized operations, will plant a REX prefix if necessary (if any register is r8 or above).
        inline void emitRexIfNeeded(int r, int x, int b)
        {
            emitRexIf(regRequiresRex(r) || regRequiresRex(x) || regRequiresRex(b), r, x, b);
        }
#else
        // No REX prefix bytes on 32-bit x86.
        inline bool regRequiresRex(int) { return false; }
        inline void emitRexIf(bool, int, int, int) {}
        inline void emitRexIfNeeded(int, int, int) {}
#endif

        enum ModRmMode {
            ModRmMemoryNoDisp,
            ModRmMemoryDisp8,
            ModRmMemoryDisp32,
            ModRmRegister
        };

        void putModRm(ModRmMode mode, int reg, RegisterID rm)
        {
            m_buffer.putByteUnchecked((mode << 6) | ((reg & 7) << 3) | (rm & 7));
        }

        void putModRmSib(ModRmMode mode, int reg, RegisterID base, RegisterID index, int scale)
        {
            ASSERT(mode != ModRmRegister);

            putModRm(mode, reg, hasSib);
            m_buffer.putByteUnchecked((scale << 6) | ((index & 7) << 3) | (base & 7));
        }

        void registerModRM(int reg, RegisterID rm)
        {
            putModRm(ModRmRegister, reg, rm);
        }

        void memoryModRM(int reg, RegisterID base, int offset)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a sib with no index & put the base in there.
#if WTF_CPU_X86_64
            if ((base == hasSib) || (base == hasSib2)) {
#else
            if (base == hasSib) {
#endif
                if (!offset) // No need to check if the base is noBase, since we know it is hasSib!
                    putModRmSib(ModRmMemoryNoDisp, reg, base, noIndex, 0);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRmSib(ModRmMemoryDisp8, reg, base, noIndex, 0);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRmSib(ModRmMemoryDisp32, reg, base, noIndex, 0);
                    m_buffer.putIntUnchecked(offset);
                }
            } else {
#if WTF_CPU_X86_64
                if (!offset && (base != noBase) && (base != noBase2))
#else
                if (!offset && (base != noBase))
#endif
                    putModRm(ModRmMemoryNoDisp, reg, base);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRm(ModRmMemoryDisp8, reg, base);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRm(ModRmMemoryDisp32, reg, base);
                    m_buffer.putIntUnchecked(offset);
                }
            }
        }
    
        void memoryModRM_disp32(int reg, RegisterID base, int offset)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a sib with no index & put the base in there.
#if WTF_CPU_X86_64
            if ((base == hasSib) || (base == hasSib2)) {
#else
            if (base == hasSib) {
#endif
                putModRmSib(ModRmMemoryDisp32, reg, base, noIndex, 0);
                m_buffer.putIntUnchecked(offset);
            } else {
                putModRm(ModRmMemoryDisp32, reg, base);
                m_buffer.putIntUnchecked(offset);
            }
        }
    
        void memoryModRM(int reg, RegisterID base, RegisterID index, int scale, int offset)
        {
            ASSERT(index != noIndex);

#if WTF_CPU_X86_64
            if (!offset && (base != noBase) && (base != noBase2))
#else
            if (!offset && (base != noBase))
#endif
                putModRmSib(ModRmMemoryNoDisp, reg, base, index, scale);
            else if (CAN_SIGN_EXTEND_8_32(offset)) {
                putModRmSib(ModRmMemoryDisp8, reg, base, index, scale);
                m_buffer.putByteUnchecked(offset);
            } else {
                putModRmSib(ModRmMemoryDisp32, reg, base, index, scale);
                m_buffer.putIntUnchecked(offset);
            }
        }

#if !WTF_CPU_X86_64
        void memoryModRM(int reg, const void* address)
        {
            // noBase + ModRmMemoryNoDisp means noBase + ModRmMemoryDisp32!
            putModRm(ModRmMemoryNoDisp, reg, noBase);
            m_buffer.putIntUnchecked(reinterpret_cast<int32_t>(address));
        }
#endif

        AssemblerBuffer m_buffer;
    } m_formatter;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(X86)

#endif // X86Assembler_h
