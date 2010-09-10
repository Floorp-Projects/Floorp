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

    inline void Assembler::SH4_emit16(uint16_t value) {
        NanoAssert(_nIns - codeStart >= 2);
        _nIns--;
        *((uint16_t*)_nIns) = value;
    }

    inline void Assembler::SH4_emit32(uint32_t value) {
        NanoAssert(_nIns - codeStart >= 4);
        _nIns -= 2;
        *((uint32_t *)(void *)_nIns) = value;
    }

    /*************************************/
    /* Start of the auto-generated part. */

#define SH4_CHECK_RANGE_add_imm(imm) ((imm) >= -128 && (imm) <= 127)

#define FITS_SH4_add_imm(imm) (SH4_CHECK_RANGE_add_imm(imm))

    inline void Assembler::SH4_add_imm(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_add_imm(imm));
        SH4_emit16((0x7 << 12) | ((Rx & 0xF) << 8) | ((imm & 0xFF) << 0));
        asm_output("add_imm %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_add(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xC << 0));
        asm_output("add R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_addc(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xE << 0));
        asm_output("addc R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_addv(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("addv R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_and_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_and_imm_R0(imm) (SH4_CHECK_RANGE_and_imm_R0(imm))

    inline void Assembler::SH4_and_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_and_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0x9 << 8) | ((imm & 0xFF) << 0));
        asm_output("and_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_and(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("and R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_andb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_andb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_andb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_andb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_andb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xD << 8) | ((imm & 0xFF) << 0));
        asm_output("andb_imm_dispR0GBR %d", imm);
    }

#define SH4_CHECK_RANGE_bra(imm) ((imm) >= -4096 && (imm) <= 4094)

#define SH4_CHECK_ALIGN_bra(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bra(imm) (SH4_CHECK_RANGE_bra((imm) + 2) && SH4_CHECK_ALIGN_bra((imm) + 2))

    inline void Assembler::SH4_bra(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bra(imm + 2) && SH4_CHECK_ALIGN_bra(imm + 2));
        SH4_emit16((0xA << 12) | (((imm & 0x1FFE) >> 1) << 0));
        asm_output("bra %d", imm);
    }

#define SH4_CHECK_RANGE_bsr(imm) ((imm) >= -4096 && (imm) <= 4094)

#define SH4_CHECK_ALIGN_bsr(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bsr(imm) (SH4_CHECK_RANGE_bsr((imm) + 2) && SH4_CHECK_ALIGN_bsr((imm) + 2))

    inline void Assembler::SH4_bsr(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bsr(imm + 2) && SH4_CHECK_ALIGN_bsr(imm + 2));
        SH4_emit16((0xB << 12) | (((imm & 0x1FFE) >> 1) << 0));
        asm_output("bsr %d", imm);
    }

#define SH4_CHECK_RANGE_bt(imm) ((imm) >= -256 && (imm) <= 254)

#define SH4_CHECK_ALIGN_bt(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bt(imm) (SH4_CHECK_RANGE_bt((imm) + 2) && SH4_CHECK_ALIGN_bt((imm) + 2))

    inline void Assembler::SH4_bt(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bt(imm + 2) && SH4_CHECK_ALIGN_bt(imm + 2));
        SH4_emit16((0x8 << 12) | (0x9 << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("bt %d", imm);
    }

#define SH4_CHECK_RANGE_bf(imm) ((imm) >= -256 && (imm) <= 254)

#define SH4_CHECK_ALIGN_bf(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bf(imm) (SH4_CHECK_RANGE_bf((imm) + 2) && SH4_CHECK_ALIGN_bf((imm) + 2))

    inline void Assembler::SH4_bf(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bf(imm + 2) && SH4_CHECK_ALIGN_bf(imm + 2));
        SH4_emit16((0x8 << 12) | (0xB << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("bf %d", imm);
    }

#define SH4_CHECK_RANGE_bts(imm) ((imm) >= -256 && (imm) <= 254)

#define SH4_CHECK_ALIGN_bts(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bts(imm) (SH4_CHECK_RANGE_bts((imm) + 2) && SH4_CHECK_ALIGN_bts((imm) + 2))

    inline void Assembler::SH4_bts(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bts(imm + 2) && SH4_CHECK_ALIGN_bts(imm + 2));
        SH4_emit16((0x8 << 12) | (0xD << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("bts %d", imm);
    }

#define SH4_CHECK_RANGE_bfs(imm) ((imm) >= -256 && (imm) <= 254)

#define SH4_CHECK_ALIGN_bfs(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_bfs(imm) (SH4_CHECK_RANGE_bfs((imm) + 2) && SH4_CHECK_ALIGN_bfs((imm) + 2))

    inline void Assembler::SH4_bfs(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_bfs(imm + 2) && SH4_CHECK_ALIGN_bfs(imm + 2));
        SH4_emit16((0x8 << 12) | (0xF << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("bfs %d", imm);
    }

    inline void Assembler::SH4_clrmac() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x2 << 4) | (0x8 << 0));
        asm_output("clrmac");
    }

    inline void Assembler::SH4_clrs() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x4 << 4) | (0x8 << 0));
        asm_output("clrs");
    }

    inline void Assembler::SH4_clrt() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x0 << 4) | (0x8 << 0));
        asm_output("clrt");
    }

#define SH4_CHECK_RANGE_cmpeq_imm_R0(imm) ((imm) >= -128 && (imm) <= 127)

#define FITS_SH4_cmpeq_imm_R0(imm) (SH4_CHECK_RANGE_cmpeq_imm_R0(imm))

    inline void Assembler::SH4_cmpeq_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_cmpeq_imm_R0(imm));
        SH4_emit16((0x8 << 12) | (0x8 << 8) | ((imm & 0xFF) << 0));
        asm_output("cmpeq_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_cmpeq(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x0 << 0));
        asm_output("cmpeq R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_cmpge(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x3 << 0));
        asm_output("cmpge R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_cmpgt(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x7 << 0));
        asm_output("cmpgt R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_cmphi(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("cmphi R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_cmphs(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x2 << 0));
        asm_output("cmphs R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_cmppl(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x5 << 0));
        asm_output("cmppl R%d", Rx);
    }

    inline void Assembler::SH4_cmppz(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x1 << 0));
        asm_output("cmppz R%d", Rx);
    }

    inline void Assembler::SH4_cmpstr(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xC << 0));
        asm_output("cmpstr R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_div0s(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x7 << 0));
        asm_output("div0s R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_div0u() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x1 << 4) | (0x9 << 0));
        asm_output("div0u");
    }

    inline void Assembler::SH4_div1(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x4 << 0));
        asm_output("div1 R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_extsb(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xE << 0));
        asm_output("extsb R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_extsw(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("extsw R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_extub(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xC << 0));
        asm_output("extub R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_extuw(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xD << 0));
        asm_output("extuw R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_icbi_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xE << 4) | (0x3 << 0));
        asm_output("icbi_indRx R%d", Rx);
    }

    inline void Assembler::SH4_jmp_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0xB << 0));
        asm_output("jmp_indRx R%d", Rx);
    }

    inline void Assembler::SH4_jsr_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0xB << 0));
        asm_output("jsr_indRx R%d", Rx);
    }

    inline void Assembler::SH4_ldc_SR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0xE << 0));
        asm_output("ldc_SR R%d", Rx);
    }

    inline void Assembler::SH4_ldc_GBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0xE << 0));
        asm_output("ldc_GBR R%d", Rx);
    }

    inline void Assembler::SH4_ldc_SGR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0xA << 0));
        asm_output("ldc_SGR R%d", Rx);
    }

    inline void Assembler::SH4_ldc_VBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0xE << 0));
        asm_output("ldc_VBR R%d", Rx);
    }

    inline void Assembler::SH4_ldc_SSR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0xE << 0));
        asm_output("ldc_SSR R%d", Rx);
    }

    inline void Assembler::SH4_ldc_SPC(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x4 << 4) | (0xE << 0));
        asm_output("ldc_SPC R%d", Rx);
    }

    inline void Assembler::SH4_ldc_DBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0xF << 4) | (0xA << 0));
        asm_output("ldc_DBR R%d", Rx);
    }

#define SH4_CHECK_RANGE_ldc_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_ldc_bank(imm) (SH4_CHECK_RANGE_ldc_bank(imm))

    inline void Assembler::SH4_ldc_bank(Register Rx, int imm) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_ldc_bank(imm));
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((imm & 0x7) << 4) | (0xE << 0));
        asm_output("ldc_bank R%d, %d", Rx, imm);
    }

    inline void Assembler::SH4_ldcl_incRx_SR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SR R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_GBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_GBR R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_VBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_VBR R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_SGR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0x6 << 0));
        asm_output("ldcl_incRx_SGR R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_SSR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SSR R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_SPC(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x4 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SPC R%d", Rx);
    }

    inline void Assembler::SH4_ldcl_incRx_DBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0xF << 4) | (0x6 << 0));
        asm_output("ldcl_incRx_DBR R%d", Rx);
    }

#define SH4_CHECK_RANGE_ldcl_incRx_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_ldcl_incRx_bank(imm) (SH4_CHECK_RANGE_ldcl_incRx_bank(imm))

    inline void Assembler::SH4_ldcl_incRx_bank(Register Rx, int imm) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_ldcl_incRx_bank(imm));
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((imm & 0x7) << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_bank R%d, %d", Rx, imm);
    }

    inline void Assembler::SH4_lds_MACH(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0xA << 0));
        asm_output("lds_MACH R%d", Rx);
    }

    inline void Assembler::SH4_lds_MACL(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0xA << 0));
        asm_output("lds_MACL R%d", Rx);
    }

    inline void Assembler::SH4_lds_PR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0xA << 0));
        asm_output("lds_PR R%d", Rx);
    }

    inline void Assembler::SH4_lds_FPUL(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0x5 << 4) | (0xA << 0));
        asm_output("lds_FPUL R%d", Ry);
    }

    inline void Assembler::SH4_lds_FPSCR(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0x6 << 4) | (0xA << 0));
        asm_output("lds_FPSCR R%d", Ry);
    }

    inline void Assembler::SH4_ldsl_incRx_MACH(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_MACH R%d", Rx);
    }

    inline void Assembler::SH4_ldsl_incRx_MACL(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_MACL R%d", Rx);
    }

    inline void Assembler::SH4_ldsl_incRx_PR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_PR R%d", Rx);
    }

    inline void Assembler::SH4_ldsl_incRy_FPUL(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0x5 << 4) | (0x6 << 0));
        asm_output("ldsl_incRy_FPUL R%d", Ry);
    }

    inline void Assembler::SH4_ldsl_incRy_FPSCR(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0x6 << 4) | (0x6 << 0));
        asm_output("ldsl_incRy_FPSCR R%d", Ry);
    }

    inline void Assembler::SH4_ldtlb() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x3 << 4) | (0x8 << 0));
        asm_output("ldtlb");
    }

    inline void Assembler::SH4_macw_incRy_incRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("macw_incRy_incRx R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_mov_imm(imm) ((imm) >= -128 && (imm) <= 127)

#define FITS_SH4_mov_imm(imm) (SH4_CHECK_RANGE_mov_imm(imm))

    inline void Assembler::SH4_mov_imm(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_mov_imm(imm));
        SH4_emit16((0xE << 12) | ((Rx & 0xF) << 8) | ((imm & 0xFF) << 0));
        asm_output("mov_imm %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_mov(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x3 << 0));
        asm_output("mov R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movb_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movb_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movb_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x0 << 0));
        asm_output("movb_indRx R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movb_dispRy_R0(imm) ((imm) >= 0 && (imm) <= 15)

#define FITS_SH4_movb_dispRy_R0(imm) (SH4_CHECK_RANGE_movb_dispRy_R0(imm))

    inline void Assembler::SH4_movb_dispRy_R0(int imm, Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15 && SH4_CHECK_RANGE_movb_dispRy_R0(imm));
        SH4_emit16((0x8 << 12) | (0x4 << 8) | ((Ry & 0xF) << 4) | ((imm & 0xF) << 0));
        asm_output("movb_dispRy_R0 %d, R%d", imm, Ry);
    }

#define SH4_CHECK_RANGE_movb_dispGBR_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_movb_dispGBR_R0(imm) (SH4_CHECK_RANGE_movb_dispGBR_R0(imm))

    inline void Assembler::SH4_movb_dispGBR_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movb_dispGBR_R0(imm));
        SH4_emit16((0xC << 12) | (0x4 << 8) | ((imm & 0xFF) << 0));
        asm_output("movb_dispGBR_R0 %d", imm);
    }

    inline void Assembler::SH4_movb_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xC << 0));
        asm_output("movb_dispR0Ry R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movb_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_incRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movb_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x0 << 0));
        asm_output("movb_indRy R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movb_R0_dispRx(imm) ((imm) >= 0 && (imm) <= 15)

#define FITS_SH4_movb_R0_dispRx(imm) (SH4_CHECK_RANGE_movb_R0_dispRx(imm))

    inline void Assembler::SH4_movb_R0_dispRx(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_movb_R0_dispRx(imm));
        SH4_emit16((0x8 << 12) | (0x0 << 8) | ((Rx & 0xF) << 4) | ((imm & 0xF) << 0));
        asm_output("movb_R0_dispRx %d, R%d", imm, Rx);
    }

#define SH4_CHECK_RANGE_movb_R0_dispGBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_movb_R0_dispGBR(imm) (SH4_CHECK_RANGE_movb_R0_dispGBR(imm))

    inline void Assembler::SH4_movb_R0_dispGBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movb_R0_dispGBR(imm));
        SH4_emit16((0xC << 12) | (0x0 << 8) | ((imm & 0xFF) << 0));
        asm_output("movb_R0_dispGBR %d", imm);
    }

#define SH4_CHECK_RANGE_movl_dispRx(imm) ((imm) >= 0 && (imm) <= 60)

#define SH4_CHECK_ALIGN_movl_dispRx(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_dispRx(imm) (SH4_CHECK_RANGE_movl_dispRx(imm) && SH4_CHECK_ALIGN_movl_dispRx(imm))

    inline void Assembler::SH4_movl_dispRx(Register Ry, int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15 && SH4_CHECK_RANGE_movl_dispRx(imm) && SH4_CHECK_ALIGN_movl_dispRx(imm));
        SH4_emit16((0x1 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (((imm & 0x3C) >> 2) << 0));
        asm_output("movl_dispRx R%d, %d, R%d", Ry, imm, Rx);
    }

    inline void Assembler::SH4_movl_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movl_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movl_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x2 << 0));
        asm_output("movl_indRx R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movl_dispRy(imm) ((imm) >= 0 && (imm) <= 60)

#define SH4_CHECK_ALIGN_movl_dispRy(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_dispRy(imm) (SH4_CHECK_RANGE_movl_dispRy(imm) && SH4_CHECK_ALIGN_movl_dispRy(imm))

    inline void Assembler::SH4_movl_dispRy(int imm, Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15 && SH4_CHECK_RANGE_movl_dispRy(imm) && SH4_CHECK_ALIGN_movl_dispRy(imm));
        SH4_emit16((0x5 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (((imm & 0x3C) >> 2) << 0));
        asm_output("movl_dispRy %d, R%d, R%d", imm, Ry, Rx);
    }

#define SH4_CHECK_RANGE_movl_dispGBR_R0(imm) ((imm) >= 0 && (imm) <= 1020)

#define SH4_CHECK_ALIGN_movl_dispGBR_R0(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_dispGBR_R0(imm) (SH4_CHECK_RANGE_movl_dispGBR_R0(imm) && SH4_CHECK_ALIGN_movl_dispGBR_R0(imm))

    inline void Assembler::SH4_movl_dispGBR_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movl_dispGBR_R0(imm) && SH4_CHECK_ALIGN_movl_dispGBR_R0(imm));
        SH4_emit16((0xC << 12) | (0x6 << 8) | (((imm & 0x3FC) >> 2) << 0));
        asm_output("movl_dispGBR_R0 %d", imm);
    }

#define SH4_CHECK_RANGE_movl_dispPC(imm) ((imm) >= 0 && (imm) <= 1020)

#define SH4_CHECK_ALIGN_movl_dispPC(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_dispPC(imm) (SH4_CHECK_RANGE_movl_dispPC(imm) && SH4_CHECK_ALIGN_movl_dispPC(imm))

    inline void Assembler::SH4_movl_dispPC(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_movl_dispPC(imm) && SH4_CHECK_ALIGN_movl_dispPC(imm));
        SH4_emit16((0xD << 12) | ((Rx & 0xF) << 8) | (((imm & 0x3FC) >> 2) << 0));
        asm_output("movl_dispPC %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_movl_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xE << 0));
        asm_output("movl_dispR0Ry R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movl_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_incRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movl_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x2 << 0));
        asm_output("movl_indRy R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movl_R0_dispGBR(imm) ((imm) >= 0 && (imm) <= 1020)

#define SH4_CHECK_ALIGN_movl_R0_dispGBR(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_R0_dispGBR(imm) (SH4_CHECK_RANGE_movl_R0_dispGBR(imm) && SH4_CHECK_ALIGN_movl_R0_dispGBR(imm))

    inline void Assembler::SH4_movl_R0_dispGBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movl_R0_dispGBR(imm) && SH4_CHECK_ALIGN_movl_R0_dispGBR(imm));
        SH4_emit16((0xC << 12) | (0x2 << 8) | (((imm & 0x3FC) >> 2) << 0));
        asm_output("movl_R0_dispGBR %d", imm);
    }

    inline void Assembler::SH4_movw_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movw_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movw_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x1 << 0));
        asm_output("movw_indRx R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movw_dispRy_R0(imm) ((imm) >= 0 && (imm) <= 30)

#define SH4_CHECK_ALIGN_movw_dispRy_R0(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_dispRy_R0(imm) (SH4_CHECK_RANGE_movw_dispRy_R0(imm) && SH4_CHECK_ALIGN_movw_dispRy_R0(imm))

    inline void Assembler::SH4_movw_dispRy_R0(int imm, Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15 && SH4_CHECK_RANGE_movw_dispRy_R0(imm) && SH4_CHECK_ALIGN_movw_dispRy_R0(imm));
        SH4_emit16((0x8 << 12) | (0x5 << 8) | ((Ry & 0xF) << 4) | (((imm & 0x1E) >> 1) << 0));
        asm_output("movw_dispRy_R0 %d, R%d", imm, Ry);
    }

#define SH4_CHECK_RANGE_movw_dispGBR_R0(imm) ((imm) >= 0 && (imm) <= 510)

#define SH4_CHECK_ALIGN_movw_dispGBR_R0(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_dispGBR_R0(imm) (SH4_CHECK_RANGE_movw_dispGBR_R0(imm) && SH4_CHECK_ALIGN_movw_dispGBR_R0(imm))

    inline void Assembler::SH4_movw_dispGBR_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movw_dispGBR_R0(imm) && SH4_CHECK_ALIGN_movw_dispGBR_R0(imm));
        SH4_emit16((0xC << 12) | (0x5 << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("movw_dispGBR_R0 %d", imm);
    }

#define SH4_CHECK_RANGE_movw_dispPC(imm) ((imm) >= 0 && (imm) <= 510)

#define SH4_CHECK_ALIGN_movw_dispPC(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_dispPC(imm) (SH4_CHECK_RANGE_movw_dispPC(imm) && SH4_CHECK_ALIGN_movw_dispPC(imm))

    inline void Assembler::SH4_movw_dispPC(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_movw_dispPC(imm) && SH4_CHECK_ALIGN_movw_dispPC(imm));
        SH4_emit16((0x9 << 12) | ((Rx & 0xF) << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("movw_dispPC %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_movw_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xD << 0));
        asm_output("movw_dispR0Ry R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movw_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_incRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_movw_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x1 << 0));
        asm_output("movw_indRy R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_movw_R0_dispRx(imm) ((imm) >= 0 && (imm) <= 30)

#define SH4_CHECK_ALIGN_movw_R0_dispRx(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_R0_dispRx(imm) (SH4_CHECK_RANGE_movw_R0_dispRx(imm) && SH4_CHECK_ALIGN_movw_R0_dispRx(imm))

    inline void Assembler::SH4_movw_R0_dispRx(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_movw_R0_dispRx(imm) && SH4_CHECK_ALIGN_movw_R0_dispRx(imm));
        SH4_emit16((0x8 << 12) | (0x1 << 8) | ((Rx & 0xF) << 4) | (((imm & 0x1E) >> 1) << 0));
        asm_output("movw_R0_dispRx %d, R%d", imm, Rx);
    }

#define SH4_CHECK_RANGE_movw_R0_dispGBR(imm) ((imm) >= 0 && (imm) <= 510)

#define SH4_CHECK_ALIGN_movw_R0_dispGBR(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_R0_dispGBR(imm) (SH4_CHECK_RANGE_movw_R0_dispGBR(imm) && SH4_CHECK_ALIGN_movw_R0_dispGBR(imm))

    inline void Assembler::SH4_movw_R0_dispGBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movw_R0_dispGBR(imm) && SH4_CHECK_ALIGN_movw_R0_dispGBR(imm));
        SH4_emit16((0xC << 12) | (0x1 << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("movw_R0_dispGBR %d", imm);
    }

#define SH4_CHECK_RANGE_mova_dispPC_R0(imm) ((imm) >= 0 && (imm) <= 1020)

#define SH4_CHECK_ALIGN_mova_dispPC_R0(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_mova_dispPC_R0(imm) (SH4_CHECK_RANGE_mova_dispPC_R0(imm) && SH4_CHECK_ALIGN_mova_dispPC_R0(imm))

    inline void Assembler::SH4_mova_dispPC_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_mova_dispPC_R0(imm) && SH4_CHECK_ALIGN_mova_dispPC_R0(imm));
        SH4_emit16((0xC << 12) | (0x7 << 8) | (((imm & 0x3FC) >> 2) << 0));
        asm_output("mova_dispPC_R0 %d", imm);
    }

    inline void Assembler::SH4_movcal_R0_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xC << 4) | (0x3 << 0));
        asm_output("movcal_R0_indRx R%d", Rx);
    }

    inline void Assembler::SH4_movcol_R0_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x7 << 4) | (0x3 << 0));
        asm_output("movcol_R0_indRx R%d", Rx);
    }

    inline void Assembler::SH4_movlil_indRy_R0(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Ry & 0xF) << 8) | (0x6 << 4) | (0x3 << 0));
        asm_output("movlil_indRy_R0 R%d", Ry);
    }

    inline void Assembler::SH4_movt(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x9 << 0));
        asm_output("movt R%d", Rx);
    }

    inline void Assembler::SH4_movual_indRy_R0(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0xA << 4) | (0x9 << 0));
        asm_output("movual_indRy_R0 R%d", Ry);
    }

    inline void Assembler::SH4_movual_incRy_R0(Register Ry) {
        NanoAssert(1 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Ry & 0xF) << 8) | (0xE << 4) | (0x9 << 0));
        asm_output("movual_incRy_R0 R%d", Ry);
    }

    inline void Assembler::SH4_mulsw(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("mulsw R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_muls(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("muls R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_mull(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x7 << 0));
        asm_output("mull R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_muluw(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xE << 0));
        asm_output("muluw R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_mulu(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xE << 0));
        asm_output("mulu R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_neg(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xB << 0));
        asm_output("neg R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_negc(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xA << 0));
        asm_output("negc R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_nop() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x0 << 4) | (0x9 << 0));
        asm_output("nop");
    }

    inline void Assembler::SH4_not(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x7 << 0));
        asm_output("not R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_ocbi_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x9 << 4) | (0x3 << 0));
        asm_output("ocbi_indRx R%d", Rx);
    }

    inline void Assembler::SH4_ocbp_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xA << 4) | (0x3 << 0));
        asm_output("ocbp_indRx R%d", Rx);
    }

    inline void Assembler::SH4_ocbwb_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xB << 4) | (0x3 << 0));
        asm_output("ocbwb_indRx R%d", Rx);
    }

#define SH4_CHECK_RANGE_or_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_or_imm_R0(imm) (SH4_CHECK_RANGE_or_imm_R0(imm))

    inline void Assembler::SH4_or_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_or_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0xB << 8) | ((imm & 0xFF) << 0));
        asm_output("or_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_or(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xB << 0));
        asm_output("or R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_orb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_orb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xF << 8) | ((imm & 0xFF) << 0));
        asm_output("orb_imm_dispR0GBR %d", imm);
    }

    inline void Assembler::SH4_pref_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x8 << 4) | (0x3 << 0));
        asm_output("pref_indRx R%d", Rx);
    }

    inline void Assembler::SH4_prefi_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xD << 4) | (0x3 << 0));
        asm_output("prefi_indRx R%d", Rx);
    }

    inline void Assembler::SH4_rotcl(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x4 << 0));
        asm_output("rotcl R%d", Rx);
    }

    inline void Assembler::SH4_rotcr(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x5 << 0));
        asm_output("rotcr R%d", Rx);
    }

    inline void Assembler::SH4_rotl(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x4 << 0));
        asm_output("rotl R%d", Rx);
    }

    inline void Assembler::SH4_rotr(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x5 << 0));
        asm_output("rotr R%d", Rx);
    }

    inline void Assembler::SH4_rte() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x2 << 4) | (0xB << 0));
        asm_output("rte");
    }

    inline void Assembler::SH4_rts() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x0 << 4) | (0xB << 0));
        asm_output("rts");
    }

    inline void Assembler::SH4_sets() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x5 << 4) | (0x8 << 0));
        asm_output("sets");
    }

    inline void Assembler::SH4_sett() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x1 << 4) | (0x8 << 0));
        asm_output("sett");
    }

    inline void Assembler::SH4_shad(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xC << 0));
        asm_output("shad R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_shld(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xD << 0));
        asm_output("shld R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_shal(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x0 << 0));
        asm_output("shal R%d", Rx);
    }

    inline void Assembler::SH4_shar(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x1 << 0));
        asm_output("shar R%d", Rx);
    }

    inline void Assembler::SH4_shll(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x0 << 0));
        asm_output("shll R%d", Rx);
    }

    inline void Assembler::SH4_shll16(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x8 << 0));
        asm_output("shll16 R%d", Rx);
    }

    inline void Assembler::SH4_shll2(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x8 << 0));
        asm_output("shll2 R%d", Rx);
    }

    inline void Assembler::SH4_shll8(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x8 << 0));
        asm_output("shll8 R%d", Rx);
    }

    inline void Assembler::SH4_shlr(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x1 << 0));
        asm_output("shlr R%d", Rx);
    }

    inline void Assembler::SH4_shlr16(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x9 << 0));
        asm_output("shlr16 R%d", Rx);
    }

    inline void Assembler::SH4_shlr2(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x9 << 0));
        asm_output("shlr2 R%d", Rx);
    }

    inline void Assembler::SH4_shlr8(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x9 << 0));
        asm_output("shlr8 R%d", Rx);
    }

    inline void Assembler::SH4_sleep() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x1 << 4) | (0xB << 0));
        asm_output("sleep");
    }

    inline void Assembler::SH4_stc_SR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x2 << 0));
        asm_output("stc_SR R%d", Rx);
    }

    inline void Assembler::SH4_stc_GBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x2 << 0));
        asm_output("stc_GBR R%d", Rx);
    }

    inline void Assembler::SH4_stc_VBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x2 << 0));
        asm_output("stc_VBR R%d", Rx);
    }

    inline void Assembler::SH4_stc_SSR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0x2 << 0));
        asm_output("stc_SSR R%d", Rx);
    }

    inline void Assembler::SH4_stc_SPC(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x4 << 4) | (0x2 << 0));
        asm_output("stc_SPC R%d", Rx);
    }

    inline void Assembler::SH4_stc_SGR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0xA << 0));
        asm_output("stc_SGR R%d", Rx);
    }

    inline void Assembler::SH4_stc_DBR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0xF << 4) | (0xA << 0));
        asm_output("stc_DBR R%d", Rx);
    }

#define SH4_CHECK_RANGE_stc_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_stc_bank(imm) (SH4_CHECK_RANGE_stc_bank(imm))

    inline void Assembler::SH4_stc_bank(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_stc_bank(imm));
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((imm & 0x7) << 4) | (0x2 << 0));
        asm_output("stc_bank %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_stcl_SR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x3 << 0));
        asm_output("stcl_SR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_VBR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x3 << 0));
        asm_output("stcl_VBR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_SSR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0x3 << 0));
        asm_output("stcl_SSR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_SPC_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x4 << 4) | (0x3 << 0));
        asm_output("stcl_SPC_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_GBR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x3 << 0));
        asm_output("stcl_GBR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_SGR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x3 << 4) | (0x2 << 0));
        asm_output("stcl_SGR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stcl_DBR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0xF << 4) | (0x2 << 0));
        asm_output("stcl_DBR_decRx R%d", Rx);
    }

#define SH4_CHECK_RANGE_stcl_bank_decRx(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_stcl_bank_decRx(imm) (SH4_CHECK_RANGE_stcl_bank_decRx(imm))

    inline void Assembler::SH4_stcl_bank_decRx(int imm, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && SH4_CHECK_RANGE_stcl_bank_decRx(imm));
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | ((imm & 0x7) << 4) | (0x3 << 0));
        asm_output("stcl_bank_decRx %d, R%d", imm, Rx);
    }

    inline void Assembler::SH4_sts_MACH(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0xA << 0));
        asm_output("sts_MACH R%d", Rx);
    }

    inline void Assembler::SH4_sts_MACL(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0xA << 0));
        asm_output("sts_MACL R%d", Rx);
    }

    inline void Assembler::SH4_sts_PR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0xA << 0));
        asm_output("sts_PR R%d", Rx);
    }

    inline void Assembler::SH4_sts_FPUL(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x5 << 4) | (0xA << 0));
        asm_output("sts_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_sts_FPSCR(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x6 << 4) | (0xA << 0));
        asm_output("sts_FPSCR R%d", Rx);
    }

    inline void Assembler::SH4_stsl_MACH_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x2 << 0));
        asm_output("stsl_MACH_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stsl_MACL_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x2 << 0));
        asm_output("stsl_MACL_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stsl_PR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x2 << 0));
        asm_output("stsl_PR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stsl_FPUL_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x5 << 4) | (0x2 << 0));
        asm_output("stsl_FPUL_decRx R%d", Rx);
    }

    inline void Assembler::SH4_stsl_FPSCR_decRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x6 << 4) | (0x2 << 0));
        asm_output("stsl_FPSCR_decRx R%d", Rx);
    }

    inline void Assembler::SH4_sub(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("sub R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_subc(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xA << 0));
        asm_output("subc R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_subv(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xB << 0));
        asm_output("subv R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_swapb(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("swapb R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_swapw(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x6 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("swapw R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_synco() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0xA << 4) | (0xB << 0));
        asm_output("synco");
    }

    inline void Assembler::SH4_tasb_indRx(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0xB << 0));
        asm_output("tasb_indRx R%d", Rx);
    }

#define SH4_CHECK_RANGE_trapa_imm(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_trapa_imm(imm) (SH4_CHECK_RANGE_trapa_imm(imm))

    inline void Assembler::SH4_trapa_imm(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_trapa_imm(imm));
        SH4_emit16((0xC << 12) | (0x3 << 8) | ((imm & 0xFF) << 0));
        asm_output("trapa_imm %d", imm);
    }

#define SH4_CHECK_RANGE_tst_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_tst_imm_R0(imm) (SH4_CHECK_RANGE_tst_imm_R0(imm))

    inline void Assembler::SH4_tst_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_tst_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0x8 << 8) | ((imm & 0xFF) << 0));
        asm_output("tst_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_tst(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("tst R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_tstb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_tstb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_tstb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_tstb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_tstb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xC << 8) | ((imm & 0xFF) << 0));
        asm_output("tstb_imm_dispR0GBR %d", imm);
    }

#define SH4_CHECK_RANGE_xor_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_xor_imm_R0(imm) (SH4_CHECK_RANGE_xor_imm_R0(imm))

    inline void Assembler::SH4_xor_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_xor_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0xA << 8) | ((imm & 0xFF) << 0));
        asm_output("xor_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_xor(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xA << 0));
        asm_output("xor R%d, R%d", Ry, Rx);
    }

#define SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_xorb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_xorb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xE << 8) | ((imm & 0xFF) << 0));
        asm_output("xorb_imm_dispR0GBR %d", imm);
    }

    inline void Assembler::SH4_xtrct(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x2 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xD << 0));
        asm_output("xtrct R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_dt(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x4 << 12) | ((Rx & 0xF) << 8) | (0x1 << 4) | (0x0 << 0));
        asm_output("dt R%d", Rx);
    }

    inline void Assembler::SH4_dmulsl(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xD << 0));
        asm_output("dmulsl R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_dmulul(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x3 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x5 << 0));
        asm_output("dmulul R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_macl_incRy_incRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | ((Ry & 0xF) << 4) | (0xF << 0));
        asm_output("macl_incRy_incRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_braf(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x2 << 4) | (0x3 << 0));
        asm_output("braf R%d", Rx);
    }

    inline void Assembler::SH4_bsrf(Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15);
        SH4_emit16((0x0 << 12) | ((Rx & 0xF) << 8) | (0x0 << 4) | (0x3 << 0));
        asm_output("bsrf R%d", Rx);
    }

    inline void Assembler::SH4_fabs(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x5 << 4) | (0xD << 0));
        asm_output("fabs R%d", Rx);
    }

    inline void Assembler::SH4_fabs_double(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x5 << 4) | (0xD << 0));
        asm_output("fabs_double R%d", Rx);
    }

    inline void Assembler::SH4_fadd(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x0 << 0));
        asm_output("fadd R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fadd_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x0 << 0));
        asm_output("fadd_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fcmpeq(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x4 << 0));
        asm_output("fcmpeq R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fcmpeq_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x4 << 0));
        asm_output("fcmpeq_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fcmpgt(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x5 << 0));
        asm_output("fcmpgt R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fcmpgt_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x5 << 0));
        asm_output("fcmpgt_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fcnvds_double_FPUL(Register Rx) {
        NanoAssert(1 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0xB << 4) | (0xD << 0));
        asm_output("fcnvds_double_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_fcnvsd_FPUL_double(Register Rx) {
        NanoAssert(1 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0xA << 4) | (0xD << 0));
        asm_output("fcnvsd_FPUL_double R%d", Rx);
    }

    inline void Assembler::SH4_fdiv(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x3 << 0));
        asm_output("fdiv R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fdiv_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x3 << 0));
        asm_output("fdiv_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fipr(Register Ry, Register Rx) {
        NanoAssert(1 && !(((Rx - 16) & 0x3) || ((Ry - 16) & 0x3)));
        SH4_emit16((0xF << 12) | (((((Rx - 16) & 0xF) << 2) | (((Ry - 16) & 0xF) >> 2)) << 8) | (0xE << 4) | (0xD << 0));
        asm_output("fipr R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fldi0(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x8 << 4) | (0xD << 0));
        asm_output("fldi0 R%d", Rx);
    }

    inline void Assembler::SH4_fldi1(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x9 << 4) | (0xD << 0));
        asm_output("fldi1 R%d", Rx);
    }

    inline void Assembler::SH4_flds_FPUL(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x1 << 4) | (0xD << 0));
        asm_output("flds_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_float_FPUL(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x2 << 4) | (0xD << 0));
        asm_output("float_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_float_FPUL_double(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x2 << 4) | (0xD << 0));
        asm_output("float_FPUL_double R%d", Rx);
    }

    inline void Assembler::SH4_fmac(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xE << 0));
        asm_output("fmac R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xC << 0));
        asm_output("fmov R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_Xdouble_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xC << 0));
        asm_output("fmov_Xdouble_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("fmov_indRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_indRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("fmov_indRy_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmov_indRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_Xdouble_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmov_Xdouble_indRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("fmov_incRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_incRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("fmov_incRy_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmov_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_Xdouble_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmov_Xdouble_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("fmov_dispR0Ry R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_dispR0Ry_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("fmov_dispR0Ry_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmov_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmov_Xdouble_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmov_Xdouble_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_indRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("fmovd_indRy_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_Xdouble_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmovd_Xdouble_indRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_incRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("fmovd_incRy_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_Xdouble_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmovd_Xdouble_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_dispR0Ry_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("fmovd_dispR0Ry_Xdouble R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovd_Xdouble_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmovd_Xdouble_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x8 << 0));
        asm_output("fmovs_indRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmovs_indRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x9 << 0));
        asm_output("fmovs_incRy R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmovs_decRx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && Ry >= 0 && Ry <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | ((Ry & 0xF) << 4) | (0x6 << 0));
        asm_output("fmovs_dispR0Ry R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmovs_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && Rx >= 0 && Rx <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | ((Rx & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmovs_dispR0Rx R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmul(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x2 << 0));
        asm_output("fmul R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fmul_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x2 << 0));
        asm_output("fmul_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fneg(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x4 << 4) | (0xD << 0));
        asm_output("fneg R%d", Rx);
    }

    inline void Assembler::SH4_fneg_double(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x4 << 4) | (0xD << 0));
        asm_output("fneg_double R%d", Rx);
    }

    inline void Assembler::SH4_fpchg() {
        NanoAssert(1);
        SH4_emit16((0xF << 12) | (0x7 << 8) | (0xF << 4) | (0xD << 0));
        asm_output("fpchg");
    }

    inline void Assembler::SH4_frchg() {
        NanoAssert(1);
        SH4_emit16((0xF << 12) | (0xB << 8) | (0xF << 4) | (0xD << 0));
        asm_output("frchg");
    }

    inline void Assembler::SH4_fsca_FPUL_double(Register Rx) {
        NanoAssert(1 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0xF << 4) | (0xD << 0));
        asm_output("fsca_FPUL_double R%d", Rx);
    }

    inline void Assembler::SH4_fschg() {
        NanoAssert(1);
        SH4_emit16((0xF << 12) | (0x3 << 8) | (0xF << 4) | (0xD << 0));
        asm_output("fschg");
    }

    inline void Assembler::SH4_fsqrt(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x6 << 4) | (0xD << 0));
        asm_output("fsqrt R%d", Rx);
    }

    inline void Assembler::SH4_fsqrt_double(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x6 << 4) | (0xD << 0));
        asm_output("fsqrt_double R%d", Rx);
    }

    inline void Assembler::SH4_fsrra(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x7 << 4) | (0xD << 0));
        asm_output("fsrra R%d", Rx);
    }

    inline void Assembler::SH4_fsts_FPUL(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x0 << 4) | (0xD << 0));
        asm_output("fsts_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_fsub(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && (Ry - 16) >= 0 && (Ry - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x1 << 0));
        asm_output("fsub R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_fsub_double(Register Ry, Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1) && (Ry - 16) >= 0 && (Ry - 16) <= 15 && !((Ry - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (((Ry - 16) & 0xF) << 4) | (0x1 << 0));
        asm_output("fsub_double R%d, R%d", Ry, Rx);
    }

    inline void Assembler::SH4_ftrc_FPUL(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15);
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x3 << 4) | (0xD << 0));
        asm_output("ftrc_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_ftrc_double_FPUL(Register Rx) {
        NanoAssert(1 && (Rx - 16) >= 0 && (Rx - 16) <= 15 && !((Rx - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((Rx - 16) & 0xF) << 8) | (0x3 << 4) | (0xD << 0));
        asm_output("ftrc_double_FPUL R%d", Rx);
    }

    inline void Assembler::SH4_ftrv(Register Rx) {
        NanoAssert(1 && !((Rx - 16) & 0x3));
        SH4_emit16((0xF << 12) | (((((Rx - 16) & 0xF) << 2) | 0x1) << 8) | (0xF << 4) | (0xD << 0));
        asm_output("ftrv R%d", Rx);
    }

    /* End of the auto-generated part. */
    /***********************************/

#define SH4_movl_PCrel(address, reg)                                    \
    SH4_movl_dispPC(((uint32_t)(address) - (((uint32_t)_nIns) & ~0x3)), reg)

#define SH4_LABEL(target) (int32_t)((uint32_t)(target) - (uint32_t)(_nIns) - 2)

    void Assembler::nativePageReset() { }

    void Assembler::nativePageSetup() {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
    }

    void Assembler::MR(Register dest, Register src) {
        underrunProtect(1 * sizeof(NIns));
        SH4_mov(src, dest);
    }

    void Assembler::JMP(NIns *target, bool from_underrunProtect) {
        underrunProtect(2 * sizeof(NIns));
        if (target != NULL && FITS_SH4_bra(SH4_LABEL(target + 1 * sizeof(NIns)))) {
            // We can reach the target with a "bra".
            SH4_nop();
            SH4_bra(SH4_LABEL(target));
        }
        else if (from_underrunProtect) {
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
            // TODO: do this only when needed.

            SH4_movl_incRy(SP, Rtemp);
            SH4_jmp_indRx(Rtemp);

            asm_immi((int)target, Rtemp, (target == NULL));

            SH4_movl_decRx(Rtemp, SP);
        }
        else {
            // General case.
            SH4_nop();
            SH4_jmp_indRx(Rtemp);
            asm_immi((int)target, Rtemp, (target == NULL));
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
                SH4_fmov((Register)(arg->getReg() + 1), (Register)(reg + 1));
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
            SH4_fmov(Register(src_true_reg + 1), Register(dest_reg + 1));
        }
        else {
            SH4_mov(src_true_reg, dest_reg);
        }        

        // 2. If the test is "false", copy the "false" source register
        // then jump over the "mov if true".
        NIns *after_mov_false = _nIns;

        SH4_nop();
        SH4_bra(SH4_LABEL(after_mov_true));

        if (inst->isop(LIR_cmovd)) {
            SH4_fmov(src_false_reg, dest_reg);
            SH4_fmov(Register(src_false_reg + 1), Register(dest_reg + 1));
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
            SH4_fmov(Register(operand_reg + 1), Register(result_reg + 1));
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
            SH4_fmov(Register(operand1_reg + 1), Register(result_reg + 1));
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
        SH4_fmovs_incRy(Rtemp, Register(result_reg + 1));

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
            if (false // TODO: test if R0 is unused.
                && base_reg != R0) {
                NanoAssertMsg(0, "not yet tested.");
                // Optimize when R0 is free and not used as the base.

                underrunProtect(1 * sizeof(NIns));
                SH4_movl_dispR0Ry(base_reg, result_reg); // 2. @(R0 + base) -> result
                asm_immi(offset, R0);                    // 1. offset         -> R0
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
        if (offset == 0) {                                              \
            /* No displacement. */                                      \
            SH4_mov ## length ## _indRy(base_reg, result_reg);          \
        }                                                               \
        else if (FITS_SH4_mov ## length ## _dispRy_R0(offset)) {        \
            /* The displacement is small enough to fit into the */      \
            /* special #size bits load "@(base + offset) -> R0". */     \
            if (result_reg == R0) {                                     \
                /* We are lucky, the result is R0. */                   \
                NanoAssertMsg(0, "not yet tested.");                    \
                SH4_mov ## length ## _dispRy_R0(offset, base_reg); /* @(base + offset) -> R0 */ \
            }                                                           \
            else {                                                      \
                /* We have to save and restore R0. */                   \
                NanoAssertMsg(0, "not yet tested.");                    \
                SH4_mov(Rtemp, R0);                                /* 4. Rtemp -> R0            */ \
                SH4_mov(Rtemp, result_reg);                        /* 3. R0 -> result           */ \
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
        SH4_fmov(Register(src_reg + 1), Register(dest_reg + 1));
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
            NanoAssert(_nIns - SH4_IMMD_NOCHK_SIZE >= codeStart);
            NanoAssert(result_reg != Rtemp);

            // 8. Load the "double" constant from @Rtemp.
            SH4_fmovs_indRy(Rtemp, result_reg);
            SH4_fmovs_incRy(Rtemp, Register(result_reg + 1));

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

#define SIGNIFICAND_MASK 0x000FFFFFFFFFFFFFLLU
#define EXPONENT_MASK    0x7FF0000000000000LLU
#define SIGN_MASK        0x8000000000000000LLU
#define SIGNIFICAND_OFFSET 0
#define EXPONENT_OFFSET    52
#define SIGN_OFFSET        63

    void Assembler::asm_immd(uint64_t value, Register result_reg) {
        int exponent = ((value & EXPONENT_MASK ) >> EXPONENT_OFFSET) - 1023;
        int sign     = (value & SIGN_MASK)       >> SIGN_OFFSET;

        NanoAssert(result_reg != Rtemp);

        if ((value & SIGNIFICAND_MASK) == 0
            && exponent == -1023) {
            // This doube is either 0 or -0.
            underrunProtect(4 * sizeof(NIns));

            if (sign != 0)
                SH4_fneg_double(result_reg);   // 4. -Dreg -> Dreg

            SH4_float_FPUL_double(result_reg); // 3. FPUL  -> Dreg, it's magic !
            SH4_lds_FPUL(Rtemp);               // 2. Rtemp -> FPUL
            SH4_mov_imm(0, Rtemp);             // 1. 0x0   -> Rtemp
        }
        else if ((value & SIGNIFICAND_MASK) == 0
                 && exponent >= 0
                 && exponent < 31) {
            // This double is an interger, so load it in an optimized way.
            // TODO: currently it only supports power-of-2 integers :-/

            int immI = (1 << exponent) * (sign ? -1 : 1);

            underrunProtect(3 * sizeof(NIns));
            SH4_float_FPUL_double(result_reg); // 3. FPUL  -> Dreg, it's magic !
            SH4_lds_FPUL(Rtemp);               // 2. Rtemp -> FPUL
            asm_immi(immI, Rtemp);             // 1. integer -> Rtemp
        }
        else {
            // Generic case.
            underrunProtect(SH4_IMMD_NOCHK_SIZE);
            asm_immd_nochk(value, result_reg);
        }
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
            int offset = arDisp(inst);
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
        underrunProtect(5 * sizeof(NIns));

        // 3. Perform a return to the caller using the return address.
        SH4_nop();
        SH4_rts();

        // 2. Restore the previous frame pointer.
        SH4_movl_incRy(SP, FP);

        // 1. Restore the return address.
        SH4_ldsl_incRx_PR(SP);

        // 0. Restore the stack pointer.
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

    NIns *Assembler::asm_immi(int constant, Register reg, bool force) {
        NIns *constant_addr = NULL;

        if (FITS_SH4_mov_imm(constant) && !force) {
            // Best case.
            underrunProtect(1 * sizeof(NIns));
            SH4_mov_imm(constant, reg);
        }
        else if (false && !force) {
            // TODO: use several "add" if it produces less instructions than worst solution.
            NanoAssertMsg(0, "not yet tested.");
        }
        else {
            // Worst case, TODO: use a constant-pool manager.
            underrunProtect(6 * sizeof(NIns));
            NIns *over_constant = _nIns;

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

        return constant_addr;
    }

    void Assembler::asm_store32i(Register value_reg, int offset, Register base_reg) {
        // Rtemp is mandatory to handle the "worst" case.
        NanoAssert(value_reg != Rtemp && base_reg != Rtemp);

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
            // General case.
            if (false // TODO: test if R0 is unused.
                && base_reg != R0) {
                NanoAssertMsg(0, "not yet tested.");
                // Optimize when R0 is free and not used as the base.

                underrunProtect(1 * sizeof(NIns));
                SH4_movl_dispR0Rx(value_reg, base_reg); // 2. value  -> @(R0 + base)
                asm_immi(offset, R0);                   // 1. offset -> R0
            }
            else {
                // Worst case.
                underrunProtect(2 * sizeof(NIns));
                SH4_movl_indRx(value_reg, Rtemp); // 3. value       -> @(temp)
                SH4_add(base_reg, Rtemp);         // 2. base + temp -> temp
                asm_immi(offset, Rtemp);          // 1. offset      -> temp
            }
        }
    }

#define gen_asm_storeXXi(size, length)                                  \
    void Assembler::asm_store ## size ## i(Register value_reg, int offset, Register base_reg) { \
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
                NanoAssertMsg(0, "not yet tested.");                    \
                                                                        \
                underrunProtect(1 * sizeof(NIns));                      \
                SH4_mov ## length ## _R0_dispRx(offset, base_reg); /* R0 -> @(base + offset) */ \
            } else {                                                    \
                /* We have to save and restore R0. */                   \
                NanoAssertMsg(0, "not yet tested.");                    \
                                                                        \
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
        SH4_fmovs_decRx(Register(value_reg + 1), Rtemp);
        SH4_fmovs_decRx(value_reg, Rtemp);

        // Adjust the offset since we are using a post decrement (by 4) indirect loads.
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

        underrunProtect(3 * sizeof(NIns));

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
        else if (false && target != NULL &&  FITS_SH4_bra(SH4_LABEL(target))) {
            // TODO: The target is not so far we can reach it with a unconditional
            // branch wrapped around by a reversed branch.
            NanoAssert(0); // TBD
            return NULL;
        }

        // Otherwise it's the worst case.

        NIns *over_jump = _nIns;
        // 3. branch @target_reg
        SH4_nop();
        SH4_jmp_indRx(Rtemp);

        // TODO: Reverse 1. and 2. to avoid an unconditional load.
        //       When doing this, be careful with underrunProtect() and asm_immi()!

        // 2. branch over 3. if true (negated test)
        if (test == true)
            SH4_bf(SH4_LABEL(over_jump));
        else
            SH4_bt(SH4_LABEL(over_jump));

        // 1. target -> target_reg
        asm_immi((int)target, Rtemp, true);
        patch_target = _nIns;

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

        // Jump tables are not yet supported.
        NanoAssert(!guard->isop(LIR_xtbl));

        // 3. Jump to the target fragment.
        if (target && target->fragEntry)
            JMP(target->fragEntry);
        else {
            // The target fragment doesn't exist yet, so emit a jump to the epilogue.
            // If the target is created later on, the jump will be patched.
            if (!_epilogue)
                _epilogue = genEpilogue();

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

    void Assembler::nInit(avmplus::AvmCore*) {
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
    }

    // Keep in sync' with LARGEST_BRANCH_PATCH.
    void Assembler::nPatchBranch(NIns *branch, NIns *target) {
        // TODO: Try to relax this branch sequence:
        //
        //    mov.l   @(PC, 6), rX
        //    bra     +6
        //    nop
        //    .word tar-
        //    .word -get
        //    nop
        //
        // with:
        //
        //    bra    @(PC - target)
        //    nop
        //    nop
        //    nop
        //    nop
        //    nop

        // Adjust the address to point to the constant (three instructions below).
        branch += 3;

        // Patch the constant.
        *((uint32_t *)(void *)branch) = (uint32_t) target;
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set) {
        int reg = 0;

        // Find the first register in this set.
        reg = lsReg(set);

        _allocator.free &= ~rmask((Register)reg);

        // Sanity check.
        NanoAssert((rmask((Register)reg) & set) == rmask((Register)reg));

        return (Register)reg;
    }

    void Assembler::nRegisterResetAll(RegAlloc& regs) {
        regs.clear();
        regs.free = GpRegs | FpRegs;
    }

    // Keep in sync' with Assembler::asm_param().
    NIns* Assembler::genPrologue() {
        int frame_size = max_stack_args + _activation.stackSlotsNeeded() * STACK_GRANULARITY;
        frame_size = -alignUp(frame_size, NJ_ALIGN_STACK);

        // 5. Allocate enough room on stack for our busyness.
        if (frame_size != 0) {
            underrunProtect(1 * sizeof(NIns));
            if (FITS_SH4_add_imm(frame_size))
                SH4_add_imm(frame_size, SP);
            else {
                // We use R0 since it's not used for regular arguments,
                // and is otherwise clobberred by the call.
                SH4_add(R0, SP);
                asm_immi(frame_size, R0);
            }
        }

        underrunProtect(3 * sizeof(NIns));
        NIns *patchEntry = _nIns;

        // 3. Set the frame pointer.
        SH4_mov(SP, FP);

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

        NIns *pc = _nIns;
        if (_nIns - nb_bytes < codeStart) {
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));

            // This jump will call underrunProtect again, but since we're on
            // a new page large enough to host its code, nothing will happen.
            JMP(pc, true);
        }
    }

    void Assembler::asm_insert_random_nop() {
        NanoAssert(0); // not supported
    }

}
#endif // FEATURE_NANOJIT && FEATURE_SH4
