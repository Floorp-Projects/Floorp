/* THIS FILE IS AUTO-GENERATED */

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

#define SH4_CHECK_RANGE_add_imm(imm) ((imm) >= -128 && (imm) <= 127)

#define FITS_SH4_add_imm(imm) (SH4_CHECK_RANGE_add_imm(imm))

    inline void Assembler::SH4_add_imm(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_add_imm(imm));
        SH4_emit16((0x7 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0xFF) << 0));
        asm_output("add_imm %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_add(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xC << 0));
        asm_output("add R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_addc(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xE << 0));
        asm_output("addc R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_addv(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("addv R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_and_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_and_imm_R0(imm) (SH4_CHECK_RANGE_and_imm_R0(imm))

    inline void Assembler::SH4_and_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_and_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0x9 << 8) | ((imm & 0xFF) << 0));
        asm_output("and_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_and(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("and R%d, R%d", Ry.n, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x0 << 0));
        asm_output("cmpeq R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_cmpge(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x3 << 0));
        asm_output("cmpge R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_cmpgt(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x7 << 0));
        asm_output("cmpgt R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_cmphi(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("cmphi R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_cmphs(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x2 << 0));
        asm_output("cmphs R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_cmppl(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x5 << 0));
        asm_output("cmppl R%d", Rx.n);
    }

    inline void Assembler::SH4_cmppz(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x1 << 0));
        asm_output("cmppz R%d", Rx.n);
    }

    inline void Assembler::SH4_cmpstr(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xC << 0));
        asm_output("cmpstr R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_div0s(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x7 << 0));
        asm_output("div0s R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_div0u() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x1 << 4) | (0x9 << 0));
        asm_output("div0u");
    }

    inline void Assembler::SH4_div1(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x4 << 0));
        asm_output("div1 R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_extsb(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xE << 0));
        asm_output("extsb R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_extsw(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("extsw R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_extub(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xC << 0));
        asm_output("extub R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_extuw(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xD << 0));
        asm_output("extuw R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_icbi_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xE << 4) | (0x3 << 0));
        asm_output("icbi_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_jmp_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0xB << 0));
        asm_output("jmp_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_jsr_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0xB << 0));
        asm_output("jsr_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_SR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0xE << 0));
        asm_output("ldc_SR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_GBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0xE << 0));
        asm_output("ldc_GBR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_SGR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0xA << 0));
        asm_output("ldc_SGR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_VBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0xE << 0));
        asm_output("ldc_VBR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_SSR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0xE << 0));
        asm_output("ldc_SSR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_SPC(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x4 << 4) | (0xE << 0));
        asm_output("ldc_SPC R%d", Rx.n);
    }

    inline void Assembler::SH4_ldc_DBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xF << 4) | (0xA << 0));
        asm_output("ldc_DBR R%d", Rx.n);
    }

#define SH4_CHECK_RANGE_ldc_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_ldc_bank(imm) (SH4_CHECK_RANGE_ldc_bank(imm))

    inline void Assembler::SH4_ldc_bank(Register Rx, int imm) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_ldc_bank(imm));
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0x7) << 4) | (0xE << 0));
        asm_output("ldc_bank R%d, %d", Rx.n, imm);
    }

    inline void Assembler::SH4_ldcl_incRx_SR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_GBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_GBR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_VBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_VBR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_SGR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0x6 << 0));
        asm_output("ldcl_incRx_SGR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_SSR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SSR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_SPC(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x4 << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_SPC R%d", Rx.n);
    }

    inline void Assembler::SH4_ldcl_incRx_DBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xF << 4) | (0x6 << 0));
        asm_output("ldcl_incRx_DBR R%d", Rx.n);
    }

#define SH4_CHECK_RANGE_ldcl_incRx_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_ldcl_incRx_bank(imm) (SH4_CHECK_RANGE_ldcl_incRx_bank(imm))

    inline void Assembler::SH4_ldcl_incRx_bank(Register Rx, int imm) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_ldcl_incRx_bank(imm));
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0x7) << 4) | (0x7 << 0));
        asm_output("ldcl_incRx_bank R%d, %d", Rx.n, imm);
    }

    inline void Assembler::SH4_lds_MACH(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0xA << 0));
        asm_output("lds_MACH R%d", Rx.n);
    }

    inline void Assembler::SH4_lds_MACL(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0xA << 0));
        asm_output("lds_MACL R%d", Rx.n);
    }

    inline void Assembler::SH4_lds_PR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0xA << 0));
        asm_output("lds_PR R%d", Rx.n);
    }

    inline void Assembler::SH4_lds_FPUL(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0x5 << 4) | (0xA << 0));
        asm_output("lds_FPUL R%d", Ry.n);
    }

    inline void Assembler::SH4_lds_FPSCR(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0x6 << 4) | (0xA << 0));
        asm_output("lds_FPSCR R%d", Ry.n);
    }

    inline void Assembler::SH4_ldsl_incRx_MACH(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_MACH R%d", Rx.n);
    }

    inline void Assembler::SH4_ldsl_incRx_MACL(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_MACL R%d", Rx.n);
    }

    inline void Assembler::SH4_ldsl_incRx_PR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x6 << 0));
        asm_output("ldsl_incRx_PR R%d", Rx.n);
    }

    inline void Assembler::SH4_ldsl_incRy_FPUL(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0x5 << 4) | (0x6 << 0));
        asm_output("ldsl_incRy_FPUL R%d", Ry.n);
    }

    inline void Assembler::SH4_ldsl_incRy_FPSCR(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0x6 << 4) | (0x6 << 0));
        asm_output("ldsl_incRy_FPSCR R%d", Ry.n);
    }

    inline void Assembler::SH4_ldtlb() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x3 << 4) | (0x8 << 0));
        asm_output("ldtlb");
    }

    inline void Assembler::SH4_macw_incRy_incRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("macw_incRy_incRx R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_mov_imm(imm) ((imm) >= -128 && (imm) <= 127)

#define FITS_SH4_mov_imm(imm) (SH4_CHECK_RANGE_mov_imm(imm))

    inline void Assembler::SH4_mov_imm(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_mov_imm(imm));
        SH4_emit16((0xE << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0xFF) << 0));
        asm_output("mov_imm %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_mov(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x3 << 0));
        asm_output("mov R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movb_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movb_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movb_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x0 << 0));
        asm_output("movb_indRx R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_movb_dispRy_R0(imm) ((imm) >= 0 && (imm) <= 15)

#define FITS_SH4_movb_dispRy_R0(imm) (SH4_CHECK_RANGE_movb_dispRy_R0(imm))

    inline void Assembler::SH4_movb_dispRy_R0(int imm, Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15 && SH4_CHECK_RANGE_movb_dispRy_R0(imm));
        SH4_emit16((0x8 << 12) | (0x4 << 8) | ((REGNUM(Ry) & 0xF) << 4) | ((imm & 0xF) << 0));
        asm_output("movb_dispRy_R0 %d, R%d", imm, Ry.n);
    }

#define SH4_CHECK_RANGE_movb_dispGBR_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_movb_dispGBR_R0(imm) (SH4_CHECK_RANGE_movb_dispGBR_R0(imm))

    inline void Assembler::SH4_movb_dispGBR_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_movb_dispGBR_R0(imm));
        SH4_emit16((0xC << 12) | (0x4 << 8) | ((imm & 0xFF) << 0));
        asm_output("movb_dispGBR_R0 %d", imm);
    }

    inline void Assembler::SH4_movb_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xC << 0));
        asm_output("movb_dispR0Ry R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movb_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x4 << 0));
        asm_output("movb_incRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movb_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x0 << 0));
        asm_output("movb_indRy R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_movb_R0_dispRx(imm) ((imm) >= 0 && (imm) <= 15)

#define FITS_SH4_movb_R0_dispRx(imm) (SH4_CHECK_RANGE_movb_R0_dispRx(imm))

    inline void Assembler::SH4_movb_R0_dispRx(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_movb_R0_dispRx(imm));
        SH4_emit16((0x8 << 12) | (0x0 << 8) | ((REGNUM(Rx) & 0xF) << 4) | ((imm & 0xF) << 0));
        asm_output("movb_R0_dispRx %d, R%d", imm, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15 && SH4_CHECK_RANGE_movl_dispRx(imm) && SH4_CHECK_ALIGN_movl_dispRx(imm));
        SH4_emit16((0x1 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (((imm & 0x3C) >> 2) << 0));
        asm_output("movl_dispRx R%d, %d, R%d", Ry.n, imm, Rx.n);
    }

    inline void Assembler::SH4_movl_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movl_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movl_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x2 << 0));
        asm_output("movl_indRx R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_movl_dispRy(imm) ((imm) >= 0 && (imm) <= 60)

#define SH4_CHECK_ALIGN_movl_dispRy(imm) (((imm) & 0x3) == 0)

#define FITS_SH4_movl_dispRy(imm) (SH4_CHECK_RANGE_movl_dispRy(imm) && SH4_CHECK_ALIGN_movl_dispRy(imm))

    inline void Assembler::SH4_movl_dispRy(int imm, Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15 && SH4_CHECK_RANGE_movl_dispRy(imm) && SH4_CHECK_ALIGN_movl_dispRy(imm));
        SH4_emit16((0x5 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (((imm & 0x3C) >> 2) << 0));
        asm_output("movl_dispRy %d, R%d, R%d", imm, Ry.n, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_movl_dispPC(imm) && SH4_CHECK_ALIGN_movl_dispPC(imm));
        SH4_emit16((0xD << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((imm & 0x3FC) >> 2) << 0));
        asm_output("movl_dispPC %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_movl_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xE << 0));
        asm_output("movl_dispR0Ry R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movl_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("movl_incRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movl_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x2 << 0));
        asm_output("movl_indRy R%d, R%d", Ry.n, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movw_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movw_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x1 << 0));
        asm_output("movw_indRx R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_movw_dispRy_R0(imm) ((imm) >= 0 && (imm) <= 30)

#define SH4_CHECK_ALIGN_movw_dispRy_R0(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_dispRy_R0(imm) (SH4_CHECK_RANGE_movw_dispRy_R0(imm) && SH4_CHECK_ALIGN_movw_dispRy_R0(imm))

    inline void Assembler::SH4_movw_dispRy_R0(int imm, Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15 && SH4_CHECK_RANGE_movw_dispRy_R0(imm) && SH4_CHECK_ALIGN_movw_dispRy_R0(imm));
        SH4_emit16((0x8 << 12) | (0x5 << 8) | ((REGNUM(Ry) & 0xF) << 4) | (((imm & 0x1E) >> 1) << 0));
        asm_output("movw_dispRy_R0 %d, R%d", imm, Ry.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_movw_dispPC(imm) && SH4_CHECK_ALIGN_movw_dispPC(imm));
        SH4_emit16((0x9 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((imm & 0x1FE) >> 1) << 0));
        asm_output("movw_dispPC %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_movw_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xD << 0));
        asm_output("movw_dispR0Ry R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movw_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x5 << 0));
        asm_output("movw_incRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_movw_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x1 << 0));
        asm_output("movw_indRy R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_movw_R0_dispRx(imm) ((imm) >= 0 && (imm) <= 30)

#define SH4_CHECK_ALIGN_movw_R0_dispRx(imm) (((imm) & 0x1) == 0)

#define FITS_SH4_movw_R0_dispRx(imm) (SH4_CHECK_RANGE_movw_R0_dispRx(imm) && SH4_CHECK_ALIGN_movw_R0_dispRx(imm))

    inline void Assembler::SH4_movw_R0_dispRx(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_movw_R0_dispRx(imm) && SH4_CHECK_ALIGN_movw_R0_dispRx(imm));
        SH4_emit16((0x8 << 12) | (0x1 << 8) | ((REGNUM(Rx) & 0xF) << 4) | (((imm & 0x1E) >> 1) << 0));
        asm_output("movw_R0_dispRx %d, R%d", imm, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xC << 4) | (0x3 << 0));
        asm_output("movcal_R0_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_movcol_R0_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x7 << 4) | (0x3 << 0));
        asm_output("movcol_R0_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_movlil_indRy_R0(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0x6 << 4) | (0x3 << 0));
        asm_output("movlil_indRy_R0 R%d", Ry.n);
    }

    inline void Assembler::SH4_movt(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x9 << 0));
        asm_output("movt R%d", Rx.n);
    }

    inline void Assembler::SH4_movual_indRy_R0(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0xA << 4) | (0x9 << 0));
        asm_output("movual_indRy_R0 R%d", Ry.n);
    }

    inline void Assembler::SH4_movual_incRy_R0(Register Ry) {
        NanoAssert(1 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Ry) & 0xF) << 8) | (0xE << 4) | (0x9 << 0));
        asm_output("movual_incRy_R0 R%d", Ry.n);
    }

    inline void Assembler::SH4_mulsw(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("mulsw R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_muls(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("muls R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_mull(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x7 << 0));
        asm_output("mull R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_muluw(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xE << 0));
        asm_output("muluw R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_mulu(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xE << 0));
        asm_output("mulu R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_neg(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xB << 0));
        asm_output("neg R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_negc(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xA << 0));
        asm_output("negc R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_nop() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x0 << 4) | (0x9 << 0));
        asm_output("nop");
    }

    inline void Assembler::SH4_not(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x7 << 0));
        asm_output("not R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_ocbi_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x9 << 4) | (0x3 << 0));
        asm_output("ocbi_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_ocbp_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xA << 4) | (0x3 << 0));
        asm_output("ocbp_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_ocbwb_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xB << 4) | (0x3 << 0));
        asm_output("ocbwb_indRx R%d", Rx.n);
    }

#define SH4_CHECK_RANGE_or_imm_R0(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_or_imm_R0(imm) (SH4_CHECK_RANGE_or_imm_R0(imm))

    inline void Assembler::SH4_or_imm_R0(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_or_imm_R0(imm));
        SH4_emit16((0xC << 12) | (0xB << 8) | ((imm & 0xFF) << 0));
        asm_output("or_imm_R0 %d", imm);
    }

    inline void Assembler::SH4_or(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xB << 0));
        asm_output("or R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_orb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_orb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_orb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xF << 8) | ((imm & 0xFF) << 0));
        asm_output("orb_imm_dispR0GBR %d", imm);
    }

    inline void Assembler::SH4_pref_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x8 << 4) | (0x3 << 0));
        asm_output("pref_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_prefi_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xD << 4) | (0x3 << 0));
        asm_output("prefi_indRx R%d", Rx.n);
    }

    inline void Assembler::SH4_rotcl(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x4 << 0));
        asm_output("rotcl R%d", Rx.n);
    }

    inline void Assembler::SH4_rotcr(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x5 << 0));
        asm_output("rotcr R%d", Rx.n);
    }

    inline void Assembler::SH4_rotl(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x4 << 0));
        asm_output("rotl R%d", Rx.n);
    }

    inline void Assembler::SH4_rotr(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x5 << 0));
        asm_output("rotr R%d", Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xC << 0));
        asm_output("shad R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_shld(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xD << 0));
        asm_output("shld R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_shal(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x0 << 0));
        asm_output("shal R%d", Rx.n);
    }

    inline void Assembler::SH4_shar(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x1 << 0));
        asm_output("shar R%d", Rx.n);
    }

    inline void Assembler::SH4_shll(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x0 << 0));
        asm_output("shll R%d", Rx.n);
    }

    inline void Assembler::SH4_shll16(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x8 << 0));
        asm_output("shll16 R%d", Rx.n);
    }

    inline void Assembler::SH4_shll2(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x8 << 0));
        asm_output("shll2 R%d", Rx.n);
    }

    inline void Assembler::SH4_shll8(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x8 << 0));
        asm_output("shll8 R%d", Rx.n);
    }

    inline void Assembler::SH4_shlr(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x1 << 0));
        asm_output("shlr R%d", Rx.n);
    }

    inline void Assembler::SH4_shlr16(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x9 << 0));
        asm_output("shlr16 R%d", Rx.n);
    }

    inline void Assembler::SH4_shlr2(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x9 << 0));
        asm_output("shlr2 R%d", Rx.n);
    }

    inline void Assembler::SH4_shlr8(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x9 << 0));
        asm_output("shlr8 R%d", Rx.n);
    }

    inline void Assembler::SH4_sleep() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0x1 << 4) | (0xB << 0));
        asm_output("sleep");
    }

    inline void Assembler::SH4_stc_SR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x2 << 0));
        asm_output("stc_SR R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_GBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x2 << 0));
        asm_output("stc_GBR R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_VBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x2 << 0));
        asm_output("stc_VBR R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_SSR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0x2 << 0));
        asm_output("stc_SSR R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_SPC(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x4 << 4) | (0x2 << 0));
        asm_output("stc_SPC R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_SGR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0xA << 0));
        asm_output("stc_SGR R%d", Rx.n);
    }

    inline void Assembler::SH4_stc_DBR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xF << 4) | (0xA << 0));
        asm_output("stc_DBR R%d", Rx.n);
    }

#define SH4_CHECK_RANGE_stc_bank(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_stc_bank(imm) (SH4_CHECK_RANGE_stc_bank(imm))

    inline void Assembler::SH4_stc_bank(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_stc_bank(imm));
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0x7) << 4) | (0x2 << 0));
        asm_output("stc_bank %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_stcl_SR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x3 << 0));
        asm_output("stcl_SR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_VBR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x3 << 0));
        asm_output("stcl_VBR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_SSR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0x3 << 0));
        asm_output("stcl_SSR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_SPC_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x4 << 4) | (0x3 << 0));
        asm_output("stcl_SPC_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_GBR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x3 << 0));
        asm_output("stcl_GBR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_SGR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x3 << 4) | (0x2 << 0));
        asm_output("stcl_SGR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stcl_DBR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0xF << 4) | (0x2 << 0));
        asm_output("stcl_DBR_decRx R%d", Rx.n);
    }

#define SH4_CHECK_RANGE_stcl_bank_decRx(imm) ((imm) >= 0 && (imm) <= 7)

#define FITS_SH4_stcl_bank_decRx(imm) (SH4_CHECK_RANGE_stcl_bank_decRx(imm))

    inline void Assembler::SH4_stcl_bank_decRx(int imm, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && SH4_CHECK_RANGE_stcl_bank_decRx(imm));
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((imm & 0x7) << 4) | (0x3 << 0));
        asm_output("stcl_bank_decRx %d, R%d", imm, Rx.n);
    }

    inline void Assembler::SH4_sts_MACH(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0xA << 0));
        asm_output("sts_MACH R%d", Rx.n);
    }

    inline void Assembler::SH4_sts_MACL(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0xA << 0));
        asm_output("sts_MACL R%d", Rx.n);
    }

    inline void Assembler::SH4_sts_PR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0xA << 0));
        asm_output("sts_PR R%d", Rx.n);
    }

    inline void Assembler::SH4_sts_FPUL(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x5 << 4) | (0xA << 0));
        asm_output("sts_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_sts_FPSCR(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x6 << 4) | (0xA << 0));
        asm_output("sts_FPSCR R%d", Rx.n);
    }

    inline void Assembler::SH4_stsl_MACH_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x2 << 0));
        asm_output("stsl_MACH_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stsl_MACL_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x2 << 0));
        asm_output("stsl_MACL_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stsl_PR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x2 << 0));
        asm_output("stsl_PR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stsl_FPUL_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x5 << 4) | (0x2 << 0));
        asm_output("stsl_FPUL_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_stsl_FPSCR_decRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x6 << 4) | (0x2 << 0));
        asm_output("stsl_FPSCR_decRx R%d", Rx.n);
    }

    inline void Assembler::SH4_sub(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("sub R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_subc(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xA << 0));
        asm_output("subc R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_subv(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xB << 0));
        asm_output("subv R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_swapb(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("swapb R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_swapw(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x6 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("swapw R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_synco() {
        NanoAssert(1);
        SH4_emit16((0x0 << 12) | (0x0 << 8) | (0xA << 4) | (0xB << 0));
        asm_output("synco");
    }

    inline void Assembler::SH4_tasb_indRx(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0xB << 0));
        asm_output("tasb_indRx R%d", Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("tst R%d, R%d", Ry.n, Rx.n);
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
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xA << 0));
        asm_output("xor R%d, R%d", Ry.n, Rx.n);
    }

#define SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm) ((imm) >= 0 && (imm) <= 255)

#define FITS_SH4_xorb_imm_dispR0GBR(imm) (SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm))

    inline void Assembler::SH4_xorb_imm_dispR0GBR(int imm) {
        NanoAssert(1 && SH4_CHECK_RANGE_xorb_imm_dispR0GBR(imm));
        SH4_emit16((0xC << 12) | (0xE << 8) | ((imm & 0xFF) << 0));
        asm_output("xorb_imm_dispR0GBR %d", imm);
    }

    inline void Assembler::SH4_xtrct(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x2 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xD << 0));
        asm_output("xtrct R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_dt(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x4 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x1 << 4) | (0x0 << 0));
        asm_output("dt R%d", Rx.n);
    }

    inline void Assembler::SH4_dmulsl(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xD << 0));
        asm_output("dmulsl R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_dmulul(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x3 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x5 << 0));
        asm_output("dmulul R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_macl_incRy_incRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0xF << 0));
        asm_output("macl_incRy_incRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_braf(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x2 << 4) | (0x3 << 0));
        asm_output("braf R%d", Rx.n);
    }

    inline void Assembler::SH4_bsrf(Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15);
        SH4_emit16((0x0 << 12) | ((REGNUM(Rx) & 0xF) << 8) | (0x0 << 4) | (0x3 << 0));
        asm_output("bsrf R%d", Rx.n);
    }

    inline void Assembler::SH4_fabs(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x5 << 4) | (0xD << 0));
        asm_output("fabs R%d", Rx.n);
    }

    inline void Assembler::SH4_fabs_double(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x5 << 4) | (0xD << 0));
        asm_output("fabs_double R%d", Rx.n);
    }

    inline void Assembler::SH4_fadd(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x0 << 0));
        asm_output("fadd R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fadd_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x0 << 0));
        asm_output("fadd_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fcmpeq(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x4 << 0));
        asm_output("fcmpeq R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fcmpeq_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x4 << 0));
        asm_output("fcmpeq_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fcmpgt(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x5 << 0));
        asm_output("fcmpgt R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fcmpgt_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x5 << 0));
        asm_output("fcmpgt_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fcnvds_double_FPUL(Register Rx) {
        NanoAssert(1 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0xB << 4) | (0xD << 0));
        asm_output("fcnvds_double_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_fcnvsd_FPUL_double(Register Rx) {
        NanoAssert(1 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0xA << 4) | (0xD << 0));
        asm_output("fcnvsd_FPUL_double R%d", Rx.n);
    }

    inline void Assembler::SH4_fdiv(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x3 << 0));
        asm_output("fdiv R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fdiv_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x3 << 0));
        asm_output("fdiv_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fipr(Register Ry, Register Rx) {
        NanoAssert(1 && !(((REGNUM(Rx) - 16) & 0x3) || ((REGNUM(Ry) - 16) & 0x3)));
        SH4_emit16((0xF << 12) | (((((REGNUM(Rx) - 16) & 0xF) << 2) | (((REGNUM(Ry) - 16) & 0xF) >> 2)) << 8) | (0xE << 4) | (0xD << 0));
        asm_output("fipr R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fldi0(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x8 << 4) | (0xD << 0));
        asm_output("fldi0 R%d", Rx.n);
    }

    inline void Assembler::SH4_fldi1(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x9 << 4) | (0xD << 0));
        asm_output("fldi1 R%d", Rx.n);
    }

    inline void Assembler::SH4_flds_FPUL(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x1 << 4) | (0xD << 0));
        asm_output("flds_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_float_FPUL(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x2 << 4) | (0xD << 0));
        asm_output("float_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_float_FPUL_double(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x2 << 4) | (0xD << 0));
        asm_output("float_FPUL_double R%d", Rx.n);
    }

    inline void Assembler::SH4_fmac(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xE << 0));
        asm_output("fmac R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xC << 0));
        asm_output("fmov R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_Xdouble_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xC << 0));
        asm_output("fmov_Xdouble_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("fmov_indRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_indRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("fmov_indRy_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmov_indRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_Xdouble_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmov_Xdouble_indRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("fmov_incRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_incRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("fmov_incRy_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmov_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_Xdouble_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmov_Xdouble_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("fmov_dispR0Ry R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_dispR0Ry_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("fmov_dispR0Ry_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmov_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmov_Xdouble_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmov_Xdouble_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_indRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("fmovd_indRy_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_Xdouble_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmovd_Xdouble_indRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_incRy_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("fmovd_incRy_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_Xdouble_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmovd_Xdouble_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_dispR0Ry_Xdouble(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("fmovd_dispR0Ry_Xdouble R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovd_Xdouble_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmovd_Xdouble_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_indRy(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x8 << 0));
        asm_output("fmovs_indRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_indRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xA << 0));
        asm_output("fmovs_indRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_incRy(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x9 << 0));
        asm_output("fmovs_incRy R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_decRx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0xB << 0));
        asm_output("fmovs_decRx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_dispR0Ry(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && REGNUM(Ry) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | ((REGNUM(Ry) & 0xF) << 4) | (0x6 << 0));
        asm_output("fmovs_dispR0Ry R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmovs_dispR0Rx(Register Ry, Register Rx) {
        NanoAssert(1 && REGNUM(Rx) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | ((REGNUM(Rx) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x7 << 0));
        asm_output("fmovs_dispR0Rx R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmul(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x2 << 0));
        asm_output("fmul R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fmul_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x2 << 0));
        asm_output("fmul_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fneg(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x4 << 4) | (0xD << 0));
        asm_output("fneg R%d", Rx.n);
    }

    inline void Assembler::SH4_fneg_double(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x4 << 4) | (0xD << 0));
        asm_output("fneg_double R%d", Rx.n);
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
        NanoAssert(1 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0xF << 4) | (0xD << 0));
        asm_output("fsca_FPUL_double R%d", Rx.n);
    }

    inline void Assembler::SH4_fschg() {
        NanoAssert(1);
        SH4_emit16((0xF << 12) | (0x3 << 8) | (0xF << 4) | (0xD << 0));
        asm_output("fschg");
    }

    inline void Assembler::SH4_fsqrt(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x6 << 4) | (0xD << 0));
        asm_output("fsqrt R%d", Rx.n);
    }

    inline void Assembler::SH4_fsqrt_double(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x6 << 4) | (0xD << 0));
        asm_output("fsqrt_double R%d", Rx.n);
    }

    inline void Assembler::SH4_fsrra(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x7 << 4) | (0xD << 0));
        asm_output("fsrra R%d", Rx.n);
    }

    inline void Assembler::SH4_fsts_FPUL(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x0 << 4) | (0xD << 0));
        asm_output("fsts_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_fsub(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && (REGNUM(Ry) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x1 << 0));
        asm_output("fsub R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_fsub_double(Register Ry, Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1) && (REGNUM(Ry) - 16) <= 15 && !((REGNUM(Ry) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (((REGNUM(Ry) - 16) & 0xF) << 4) | (0x1 << 0));
        asm_output("fsub_double R%d, R%d", Ry.n, Rx.n);
    }

    inline void Assembler::SH4_ftrc_FPUL(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15);
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x3 << 4) | (0xD << 0));
        asm_output("ftrc_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_ftrc_double_FPUL(Register Rx) {
        NanoAssert(1 && (REGNUM(Rx) - 16) <= 15 && !((REGNUM(Rx) - 16) & 0x1));
        SH4_emit16((0xF << 12) | (((REGNUM(Rx) - 16) & 0xF) << 8) | (0x3 << 4) | (0xD << 0));
        asm_output("ftrc_double_FPUL R%d", Rx.n);
    }

    inline void Assembler::SH4_ftrv(Register Rx) {
        NanoAssert(1 && !((REGNUM(Rx) - 16) & 0x3));
        SH4_emit16((0xF << 12) | (((((REGNUM(Rx) - 16) & 0xF) << 2) | 0x1) << 8) | (0xF << 4) | (0xD << 0));
        asm_output("ftrv R%d", Rx.n);
    }
