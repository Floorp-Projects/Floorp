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
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifdef AVMPLUS_PORTING_API
#include "portapi_nanojit.h"
#endif

#ifdef UNDER_CE
#include <cmnintrin.h>
#endif

#if defined(AVMPLUS_LINUX)
#include <signal.h>
#include <setjmp.h>
#include <asm/unistd.h>
extern "C" void __clear_cache(void *BEG, void *END);
#endif

// assume EABI, except under CE
#ifdef UNDER_CE
#undef NJ_ARM_EABI
#else
#define NJ_ARM_EABI
#endif

#ifdef FEATURE_NANOJIT

namespace nanojit
{

#ifdef NJ_VERBOSE
const char* regNames[] = {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","fp","ip","sp","lr","pc",
                          "d0","d1","d2","d3","d4","d5","d6","d7","s14"};
const char* condNames[] = {"eq","ne","cs","cc","mi","pl","vs","vc","hi","ls","ge","lt","gt","le",""/*al*/,"nv"};
const char* shiftNames[] = { "lsl", "lsl", "lsr", "lsr", "asr", "asr", "ror", "ror" };
#endif

const Register Assembler::argRegs[] = { R0, R1, R2, R3 };
const Register Assembler::retRegs[] = { R0, R1 };
const Register Assembler::savedRegs[] = { R4, R5, R6, R7, R8, R9, R10 };

// --------------------------------
// ARM-specific utility functions.
// --------------------------------

#ifdef DEBUG
// Return true if enc is a valid Operand 2 encoding and thus can be used as-is
// in an ARM arithmetic operation that accepts such encoding.
//
// This utility does not know (or determine) the actual value that the encoded
// value represents, and thus cannot be used to ensure the correct operation of
// encOp2Imm, but it does ensure that the encoded value can be used to encode a
// valid ARM instruction. decOp2Imm can be used if you also need to check that
// a literal is correctly encoded (and thus that encOp2Imm is working
// correctly).
inline bool
Assembler::isOp2Imm(uint32_t enc)
{
    return ((enc & 0xfff) == enc);
}

// Decodes operand 2 immediate values (for debug output and assertions).
inline uint32_t
Assembler::decOp2Imm(uint32_t enc)
{
    NanoAssert(isOp2Imm(enc));

    uint32_t    imm8 = enc & 0xff;
    uint32_t    rot = 32 - ((enc >> 7) & 0x1e);

    return imm8 << (rot & 0x1f);
}
#endif

// Calculate the number of leading zeroes in data.
inline uint32_t
Assembler::CountLeadingZeroes(uint32_t data)
{
    uint32_t    leading_zeroes;

    // We can't do CLZ on anything earlier than ARMv5. Architectures as early
    // as that aren't supported, but assert that we aren't running on one
    // anyway.
    // If ARMv4 support is required in the future for some reason, we can do a
    // run-time check on config.arch and fall back to the C routine, but for
    // now we can avoid the cost of the check as we don't intend to support
    // ARMv4 anyway.
    NanoAssert(AvmCore::config.arch >= 5);

#if defined(__ARMCC__)
    // ARMCC can do this with an intrinsic.
    leading_zeroes = __clz(data);
#elif defined(__GNUC__)
    // GCC can use inline assembler to insert a CLZ instruction.
    __asm (
        "   clz     %0, %1  \n"
        :   "=r"    (leading_zeroes)
        :   "r"     (data)
    );
#elif defined(WINCE)
    // WinCE can do this with an intrinsic.
    leading_zeroes = _CountLeadingZeros(data);
#else
    // Other platforms must fall back to a C routine. This won't be as
    // efficient as the CLZ instruction, but it is functional.
    uint32_t    try_shift;

    leading_zeroes = 0;

    // This loop does a bisection search rather than the obvious rotation loop.
    // This should be faster, though it will still be no match for CLZ.
    for (try_shift = 16; try_shift != 0; try_shift /= 2) {
        uint32_t    shift = leading_zeroes + try_shift;
        if (((data << shift) >> shift) == data) {
            leading_zeroes = shift;
        }
    }
#endif

    // Assert that the operation worked!
    NanoAssert(((0xffffffff >> leading_zeroes) & data) == data);

    return leading_zeroes;
}

// The ARM instruction set allows some flexibility to the second operand of
// most arithmetic operations. When operand 2 is an immediate value, it takes
// the form of an 8-bit value rotated by an even value in the range 0-30.
//
// Some values that can be encoded this scheme — such as 0xf000000f — are
// probably fairly rare in practice and require extra code to detect, so this
// function implements a fast CLZ-based heuristic to detect any value that can
// be encoded using just a shift, and not a full rotation. For example,
// 0xff000000 and 0x000000ff are both detected, but 0xf000000f is not.
//
// This function will return true to indicate that the encoding was successful,
// or false to indicate that the literal could not be encoded as an operand 2
// immediate. If successful, the encoded value will be written to *enc.
inline bool
Assembler::encOp2Imm(uint32_t literal, uint32_t * enc)
{
    // The number of leading zeroes in the literal. This is used to calculate
    // the rotation component of the encoding.
    uint32_t    leading_zeroes;

    // Components of the operand 2 encoding.
    uint32_t    rot;
    uint32_t    imm8;

    // Check the literal to see if it is a simple 8-bit value. I suspect that
    // most literals are in fact small values, so doing this check early should
    // give a decent speed-up.
    if (literal < 256)
    {
        *enc = literal;
        return true;
    }

    // Determine the number of leading zeroes in the literal. This is used to
    // calculate the required rotation.
    leading_zeroes = CountLeadingZeroes(literal);

    // We've already done a check to see if the literal is an 8-bit value, so
    // leading_zeroes must be less than (and not equal to) (32-8)=24. However,
    // if it is greater than 24, this algorithm will break, so debug code
    // should use an assertion here to check that we have a value that we
    // expect.
    NanoAssert(leading_zeroes < 24);

    // Assuming that we have a field of no more than 8 bits for a valid
    // literal, we can calculate the required rotation by subtracting
    // leading_zeroes from (32-8):
    //
    // Example:
    //      0: Known to be zero.
    //      1: Known to be one.
    //      X: Either zero or one.
    //      .: Zero in a valid operand 2 literal.
    //
    //  Literal:     [ 1XXXXXXX ........ ........ ........ ]
    //  leading_zeroes = 0
    //  Therefore rot (left) = 24.
    //  Encoded 8-bit literal:                  [ 1XXXXXXX ]
    //
    //  Literal:     [ ........ ..1XXXXX XX...... ........ ]
    //  leading_zeroes = 10
    //  Therefore rot (left) = 14.
    //  Encoded 8-bit literal:                  [ 1XXXXXXX ]
    //
    // Note, however, that we can only encode even shifts, and so
    // "rot=24-leading_zeroes" is not sufficient by itself. By ignoring
    // zero-bits in odd bit positions, we can ensure that we get a valid
    // encoding.
    //
    // Example:
    //  Literal:     [ 01XXXXXX ........ ........ ........ ]
    //  leading_zeroes = 1
    //  Therefore rot (left) = round_up(23) = 24.
    //  Encoded 8-bit literal:                  [ 01XXXXXX ]
    rot = 24 - (leading_zeroes & ~1);

    // The imm8 component of the operand 2 encoding can be calculated from the
    // rot value.
    imm8 = literal >> rot;

    // The validity of the literal can be checked by reversing the
    // calculation. It is much easier to decode the immediate than it is to
    // encode it!
    if (literal != (imm8 << rot)) {
        // The encoding is not valid, so report the failure. Calling code
        // should use some other method of loading the value (such as LDR).
        return false;
    }

    // The operand is valid, so encode it.
    // Note that the ARM encoding is actually described by a rotate to the
    // _right_, so rot must be negated here. Calculating a left shift (rather
    // than calculating a right rotation) simplifies the above code.
    *enc = ((-rot << 7) & 0xf00) | imm8;

    // Assert that the operand was properly encoded.
    NanoAssert(decOp2Imm(*enc) == literal);

    return true;
}

// Encode "rd = rn + imm" using an appropriate instruction sequence.
// Set stat to 1 to update the status flags. Otherwise, set it to 0 or omit it.
// (The declaration in NativeARM.h defines the default value of stat as 0.)
//
// It is not valid to call this function if:
//   (rd == IP) AND (rn == IP) AND !encOp2Imm(imm) AND !encOp2Imm(-imm)
// Where: if (encOp2Imm(imm)), imm can be encoded as an ARM operand 2 using the
// encOp2Imm method.
void
Assembler::asm_add_imm(Register rd, Register rn, int32_t imm, int stat /* =0 */)
{
    // Operand 2 encoding of the immediate.
    uint32_t    op2imm;

    NanoAssert(IsGpReg(rd));
    NanoAssert(IsGpReg(rn));
    NanoAssert((stat & 1) == stat);

    // Try to encode the value directly as an operand 2 immediate value, then
    // fall back to loading the value into a register.
    if (encOp2Imm(imm, &op2imm)) {
        ADDis(rd, rn, op2imm, stat);
    } else if (encOp2Imm(-imm, &op2imm)) {
        // We could not encode the value for ADD, so try to encode it for SUB.
        // Note that this is valid even if stat is set, _unless_ imm is 0, but
        // that case is caught above.
        NanoAssert(imm != 0);
        SUBis(rd, rn, op2imm, stat);
    } else {
        // We couldn't encode the value directly, so use an intermediate
        // register to encode the value. We will use IP to do this unless rn is
        // IP; in that case we can reuse rd. This allows every case other than
        // "ADD IP, IP, =#imm".
        Register    rm = (rn == IP) ? (rd) : (IP);
        NanoAssert(rn != rm);

        ADDs(rd, rn, rm, stat);
        asm_ld_imm(rm, imm);
    }
}

// Encode "rd = rn - imm" using an appropriate instruction sequence.
// Set stat to 1 to update the status flags. Otherwise, set it to 0 or omit it.
// (The declaration in NativeARM.h defines the default value of stat as 0.)
//
// It is not valid to call this function if:
//   (rd == IP) AND (rn == IP) AND !encOp2Imm(imm) AND !encOp2Imm(-imm)
// Where: if (encOp2Imm(imm)), imm can be encoded as an ARM operand 2 using the
// encOp2Imm method.
void
Assembler::asm_sub_imm(Register rd, Register rn, int32_t imm, int stat /* =0 */)
{
    // Operand 2 encoding of the immediate.
    uint32_t    op2imm;

    NanoAssert(IsGpReg(rd));
    NanoAssert(IsGpReg(rn));
    NanoAssert((stat & 1) == stat);

    // Try to encode the value directly as an operand 2 immediate value, then
    // fall back to loading the value into a register.
    if (encOp2Imm(imm, &op2imm)) {
        SUBis(rd, rn, op2imm, stat);
    } else if (encOp2Imm(-imm, &op2imm)) {
        // We could not encode the value for SUB, so try to encode it for ADD.
        // Note that this is valid even if stat is set, _unless_ imm is 0, but
        // that case is caught above.
        NanoAssert(imm != 0);
        ADDis(rd, rn, op2imm, stat);
    } else {
        // We couldn't encode the value directly, so use an intermediate
        // register to encode the value. We will use IP to do this unless rn is
        // IP; in that case we can reuse rd. This allows every case other than
        // "SUB IP, IP, =#imm".
        Register    rm = (rn == IP) ? (rd) : (IP);
        NanoAssert(rn != rm);

        SUBs(rd, rn, rm, stat);
        asm_ld_imm(rm, imm);
    }
}

// Encode "rd = rn & imm" using an appropriate instruction sequence.
// Set stat to 1 to update the status flags. Otherwise, set it to 0 or omit it.
// (The declaration in NativeARM.h defines the default value of stat as 0.)
//
// It is not valid to call this function if:
//   (rd == IP) AND (rn == IP) AND !encOp2Imm(imm) AND !encOp2Imm(~imm)
// Where: if (encOp2Imm(imm)), imm can be encoded as an ARM operand 2 using the
// encOp2Imm method.
void
Assembler::asm_and_imm(Register rd, Register rn, int32_t imm, int stat /* =0 */)
{
    // Operand 2 encoding of the immediate.
    uint32_t    op2imm;

    NanoAssert(IsGpReg(rd));
    NanoAssert(IsGpReg(rn));
    NanoAssert((stat & 1) == stat);

    // Try to encode the value directly as an operand 2 immediate value, then
    // fall back to loading the value into a register.
    if (encOp2Imm(imm, &op2imm)) {
        ANDis(rd, rn, op2imm, stat);
    } else if (encOp2Imm(~imm, &op2imm)) {
        // Use BIC with the inverted immediate.
        BICis(rd, rn, op2imm, stat);
    } else {
        // We couldn't encode the value directly, so use an intermediate
        // register to encode the value. We will use IP to do this unless rn is
        // IP; in that case we can reuse rd. This allows every case other than
        // "AND IP, IP, =#imm".
        Register    rm = (rn == IP) ? (rd) : (IP);
        NanoAssert(rn != rm);

        ANDs(rd, rn, rm, stat);
        asm_ld_imm(rm, imm);
    }
}

// Encode "rd = rn | imm" using an appropriate instruction sequence.
// Set stat to 1 to update the status flags. Otherwise, set it to 0 or omit it.
// (The declaration in NativeARM.h defines the default value of stat as 0.)
//
// It is not valid to call this function if:
//   (rd == IP) AND (rn == IP) AND !encOp2Imm(imm)
// Where: if (encOp2Imm(imm)), imm can be encoded as an ARM operand 2 using the
// encOp2Imm method.
void
Assembler::asm_orr_imm(Register rd, Register rn, int32_t imm, int stat /* =0 */)
{
    // Operand 2 encoding of the immediate.
    uint32_t    op2imm;

    NanoAssert(IsGpReg(rd));
    NanoAssert(IsGpReg(rn));
    NanoAssert((stat & 1) == stat);

    // Try to encode the value directly as an operand 2 immediate value, then
    // fall back to loading the value into a register.
    if (encOp2Imm(imm, &op2imm)) {
        ORRis(rd, rn, op2imm, stat);
    } else {
        // We couldn't encode the value directly, so use an intermediate
        // register to encode the value. We will use IP to do this unless rn is
        // IP; in that case we can reuse rd. This allows every case other than
        // "ORR IP, IP, =#imm".
        Register    rm = (rn == IP) ? (rd) : (IP);
        NanoAssert(rn != rm);

        ORRs(rd, rn, rm, stat);
        asm_ld_imm(rm, imm);
    }
}

// Encode "rd = rn ^ imm" using an appropriate instruction sequence.
// Set stat to 1 to update the status flags. Otherwise, set it to 0 or omit it.
// (The declaration in NativeARM.h defines the default value of stat as 0.)
//
// It is not valid to call this function if:
//   (rd == IP) AND (rn == IP) AND !encOp2Imm(imm)
// Where: if (encOp2Imm(imm)), imm can be encoded as an ARM operand 2 using the
// encOp2Imm method.
void
Assembler::asm_eor_imm(Register rd, Register rn, int32_t imm, int stat /* =0 */)
{
    // Operand 2 encoding of the immediate.
    uint32_t    op2imm;

    NanoAssert(IsGpReg(rd));
    NanoAssert(IsGpReg(rn));
    NanoAssert((stat & 1) == stat);

    // Try to encode the value directly as an operand 2 immediate value, then
    // fall back to loading the value into a register.
    if (encOp2Imm(imm, &op2imm)) {
        EORis(rd, rn, op2imm, stat);
    } else {
        // We couldn't encoder the value directly, so use an intermediate
        // register to encode the value. We will use IP to do this unless rn is
        // IP; in that case we can reuse rd. This allows every case other than
        // "EOR IP, IP, =#imm".
        Register    rm = (rn == IP) ? (rd) : (IP);
        NanoAssert(rn != rm);

        EORs(rd, rn, rm, stat);
        asm_ld_imm(rm, imm);
    }
}

// --------------------------------
// Assembler functions.
// --------------------------------

void
Assembler::nInit(AvmCore*)
{
}

NIns*
Assembler::genPrologue()
{
    /**
     * Prologue
     */

    // NJ_RESV_OFFSET is space at the top of the stack for us
    // to use for parameter passing (8 bytes at the moment)
    uint32_t stackNeeded = STACK_GRANULARITY * _activation.highwatermark + NJ_STACK_OFFSET;
    uint32_t savingCount = 2;

    uint32_t savingMask = rmask(FP) | rmask(LR);

    if (!_thisfrag->lirbuf->explicitSavedRegs) {
        for (int i = 0; i < NumSavedRegs; ++i)
            savingMask |= rmask(savedRegs[i]);
        savingCount += NumSavedRegs;
    }

    // so for alignment purposes we've pushed return addr and fp
    uint32_t stackPushed = STACK_GRANULARITY * savingCount;
    uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
    int32_t amt = aligned - stackPushed;

    // Make room on stack for what we are doing
    if (amt)
        asm_sub_imm(SP, SP, amt);

    verbose_only( asm_output("## %p:",(void*)_nIns); )
    verbose_only( asm_output("## patch entry"); )
    NIns *patchEntry = _nIns;

    MOV(FP, SP);
    PUSH_mask(savingMask);
    return patchEntry;
}

void
Assembler::nFragExit(LInsp guard)
{
    SideExit *  exit = guard->record()->exit;
    Fragment *  frag = exit->target;

    bool        target_is_known = frag && frag->fragEntry;

    if (target_is_known) {
        // The target exists so we can simply emit a branch to its location.
        JMP_far(frag->fragEntry);
    } else {
        // The target doesn't exit yet, so emit a jump to the epilogue. If the
        // target is created later on, the jump will be patched.

        GuardRecord *   gr = guard->record();

        // Jump to the epilogue. This may get patched later, but JMP_far always
        // emits two instructions even when only one is required, so patching
        // will work correctly.
        JMP_far(_epilogue);

        // Load the guard record pointer into R2. We want it in R0 but we can't
        // do this at this stage because R0 is used for something else.
        // I don't understand why I can't load directly into R0. It works for
        // the JavaScript JIT but not for the Regular Expression compiler.
        // However, I haven't pushed this further as it only saves a single MOV
        // instruction in genEpilogue.
        asm_ld_imm(R2, int(gr));

        // Set the jmp pointer to the start of the sequence so that patched
        // branches can skip the LDi sequence.
        gr->jmp = _nIns;
    }

#ifdef NJ_VERBOSE
    if (config.show_stats) {
        // load R1 with Fragment *fromFrag, target fragment
        // will make use of this when calling fragenter().
        int fromfrag = int((Fragment*)_thisfrag);
        asm_ld_imm(argRegs[1], fromfrag);
    }
#endif

    // Pop the stack frame.
    MOV(SP, FP);
}

NIns*
Assembler::genEpilogue()
{
    // On ARMv5+, loading directly to PC correctly handles interworking.
    // Note that we don't support anything older than ARMv5.
    NanoAssert(AvmCore::config.arch >= 5);

    RegisterMask savingMask = rmask(FP) | rmask(PC);
    if (!_thisfrag->lirbuf->explicitSavedRegs)
        for (int i = 0; i < NumSavedRegs; ++i)
            savingMask |= rmask(savedRegs[i]);

    POP_mask(savingMask); // regs

    // Pop the stack frame.
    // As far as I can tell, the generated code doesn't use the stack between
    // popping the stack frame in nFragExit and getting here and so this MOV
    // should be redundant. However, removing this seems to break some regular
    // expression stuff.
    MOV(SP,FP);

    // nFragExit loads the guard record pointer into R2, but we need it in R0
    // so it must be moved here.
    MOV(R0,R2); // return GuardRecord*

    return _nIns;
}

/* gcc/linux use the ARM EABI; Windows CE uses the legacy abi.
 *
 * Under EABI:
 * - doubles are 64-bit aligned both in registers and on the stack.
 *   If the next available argument register is R1, it is skipped
 *   and the double is placed in R2:R3.  If R0:R1 or R2:R3 are not
 *   available, the double is placed on the stack, 64-bit aligned.
 * - 32-bit arguments are placed in registers and 32-bit aligned
 *   on the stack.
 *
 * Under legacy ABI:
 * - doubles are placed in subsequent arg registers; if the next
 *   available register is r3, the low order word goes into r3
 *   and the high order goes on the stack.
 * - 32-bit arguments are placed in the next available arg register,
 * - both doubles and 32-bit arguments are placed on stack with 32-bit
 *   alignment.
 */

void
Assembler::asm_arg(ArgSize sz, LInsp p, Register r)
{
    // should never be called -- the ARM-specific longer form of
    // asm_arg is used on ARM.
    NanoAssert(0);
}

/*
 * asm_arg will update r and stkd to indicate where the next
 * argument should go.  If r == UnknownReg, then the argument
 * is placed on the stack at stkd, and stkd is updated.
 *
 * Note that this currently doesn't actually use stkd on input,
 * except for figuring out alignment; it always pushes to SP.
 * See TODO in asm_call.
 */
void
Assembler::asm_arg(ArgSize sz, LInsp arg, Register& r, int& stkd)
{
    NanoAssert((sz == ARGSIZE_F) || (sz == ARGSIZE_LO));

    if (sz == ARGSIZE_F) {
#ifdef NJ_ARM_EABI
        NanoAssert(r == UnknownReg || r == R0 || r == R2);

        // if we're about to put this on the stack, make sure the
        // stack is 64-bit aligned
        if (r == UnknownReg && (stkd&7) != 0) {
            SUBis(SP, SP, 4, 0);
            stkd += 4;
        }
#endif

        Reservation* argRes = getresv(arg);

        // handle qjoin first; won't ever show up if VFP is available
        if (arg->isop(LIR_qjoin)) {
            NanoAssert(!AvmCore::config.vfp);
            asm_arg(ARGSIZE_LO, arg->oprnd1(), r, stkd);
            asm_arg(ARGSIZE_LO, arg->oprnd2(), r, stkd);
        } else if (!argRes || argRes->reg == UnknownReg || !AvmCore::config.vfp) {
            // if we don't have a register allocated,
            // or we're not vfp, just read from memory.
            if (arg->isop(LIR_quad)) {

                // XXX use some load-multiple action here from our const pool?
                int32_t v = arg->imm64_0();     // for the first iteration
                for (int k = 0; k < 2; k++) {
                    if (r != UnknownReg) {
                        asm_ld_imm(r, v);
                        r = nextreg(r);
                        if (r == R4)
                            r = UnknownReg;
                    } else {
                        STR_preindex(IP, SP, -4);
                        asm_ld_imm(IP, v);
                        stkd += 4;
                    }
                    v = arg->imm64_1();         // for the second iteration
                }
            } else {
                int d = findMemFor(arg);

                for (int k = 0; k < 2; k++) {
                    if (r != UnknownReg) {
                        LDR(r, FP, d + k*4);
                        r = nextreg(r);
                        if (r == R4)
                            r = UnknownReg;
                    } else {
                        STR_preindex(IP, SP, -4);
                        LDR(IP, FP, d + k*4);
                        stkd += 4;
                    }
                }
            }
        } else {
            // handle the VFP with-register case
            Register sr = argRes->reg;
            if (r != UnknownReg && r < R3) {
                FMRRD(r, nextreg(r), sr);

                // make sure the next register is correct on return
                if (r == R0)
                    r = R2;
                else
                    r = UnknownReg;
            } else if (r == R3) {
                // legacy ABI only
                STR_preindex(IP, SP, -4);
                FMRDL(IP, sr);
                FMRDH(r, sr);
                stkd += 4;

                r = UnknownReg;
            } else {
                FSTD(sr, SP, 0);
                SUBis(SP, SP, 8, 0);
                stkd += 8;
                r = UnknownReg;
            }
        }
    } else if (sz == ARGSIZE_LO) {
        if (r != UnknownReg) {
            NanoAssert(r <= R3);

            if (arg->isconst()) {
                asm_ld_imm(r, arg->imm32());
            } else {
                Reservation* argRes = getresv(arg);
                if (argRes) {
                    if (argRes->reg == UnknownReg) {
                        // load it into the arg reg
                        int d = findMemFor(arg);
                        if (arg->isop(LIR_ialloc)) {
                            asm_add_imm(r, FP, d);
                        } else {
                            LDR(r, FP, d);
                        }
                    } else {
                        MOV(r, argRes->reg);
                    }
                } else {
                    findSpecificRegFor(arg, r);
                }
            }

            if (r < R3) {
                r = nextreg(r);
            } else {
                r = UnknownReg;
            }
        } else {
            int d = findMemFor(arg);
            STR_preindex(IP, SP, -4);
            if (arg->isop(LIR_ialloc)) {
                asm_add_imm(IP, FP, d);
            } else {
                LDR(IP, FP, d);
            }
            stkd += 4;
        }
    } else {
        NanoAssert(0);
    }
}

void
Assembler::asm_call(LInsp ins)
{
    const CallInfo* call = ins->callInfo();
    Reservation *callRes = getresv(ins);

    uint32_t atypes = call->_argtypes;

    // skip return type
    ArgSize rsize = (ArgSize)(atypes & ARGSIZE_MASK_ANY);

    atypes >>= ARGSIZE_SHIFT;

    // if we're using VFP, and the return type is a double,
    // it'll come back in R0/R1.  We need to either place it
    // in the result fp reg, or store it.

    if (AvmCore::config.vfp && rsize == ARGSIZE_F) {
        NanoAssert(ins->opcode() == LIR_fcall);
        NanoAssert(callRes);

        Register rr = callRes->reg;
        int d = disp(callRes);
        freeRsrcOf(ins, rr != UnknownReg);

        if (rr != UnknownReg) {
            NanoAssert(IsFpReg(rr));
            FMDRR(rr,R0,R1);
        } else {
            NanoAssert(d);
            STR(R0, FP, d+0);
            STR(R1, FP, d+4);
        }
    }

    // Make the call using BLX (when necessary) so that we can interwork with
    // Thumb(-2) code.
    BranchWithLink((NIns*)(call->_address));

    ArgSize sizes[MAXARGS];
    uint32_t argc = call->get_sizes(sizes);

    Register r = R0;
    int stkd = 0;

    // XXX TODO we should go through the args and figure out how much
    // stack space we'll need, allocate it up front, and then do
    // SP-relative stores using stkd instead of doing STR_preindex for
    // every stack write like we currently do in asm_arg.

    for(uint32_t i = 0; i < argc; i++) {
        uint32_t j = argc - i - 1;
        ArgSize sz = sizes[j];
        LInsp arg = ins->arg(j);

        NanoAssert(r < R4 || r == UnknownReg);

#ifdef NJ_ARM_EABI
        if (sz == ARGSIZE_F) {
            if (r == R1)
                r = R2;
            else if (r == R3)
                r = UnknownReg;
        }
#endif

        asm_arg(sz, arg, r, stkd);
    }
}

Register
Assembler::nRegisterAllocFromSet(int set)
{
    NanoAssert(set != 0);

    // The CountLeadingZeroes function will use the CLZ instruction where
    // available. In other cases, it will fall back to a (slower) C
    // implementation.
    Register r = (Register)(31-CountLeadingZeroes(set));
    _allocator.free &= ~rmask(r);

    NanoAssert(IsGpReg(r) || IsFpReg(r));
    NanoAssert((rmask(r) & set) == rmask(r));

    return r;
}

void
Assembler::nRegisterResetAll(RegAlloc& a)
{
    // add scratch registers to our free list for the allocator
    a.clear();
    a.used = 0;
    a.free =
        rmask(R0) | rmask(R1) | rmask(R2) | rmask(R3) | rmask(R4) |
        rmask(R5) | rmask(R6) | rmask(R7) | rmask(R8) | rmask(R9) |
        rmask(R10);
    if (AvmCore::config.vfp)
        a.free |= FpRegs;

    debug_only(a.managed = a.free);
}

NIns*
Assembler::nPatchBranch(NIns* at, NIns* target)
{
    // Patch the jump in a loop, as emitted by JMP_far.
    // Figure out which, and do the right thing.

    NIns* was = 0;

    // Determine how the existing branch was emitted so we can report the
    // original destination. Note that this is only useful for debug purposes;
    // no real code uses this result.
    debug_only(
        if (at[0] == (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4) )) {
            // The existing branch looks like this:
            //  at[0]           LDR pc, [addr]
            //  at[1]   addr:   target
            was = (NIns*) at[1];
        } else if ((at[0] && 0xff000000) == (NIns)( COND_AL | (0xA<<24))) {
            // The existing branch looks like this:
            //  at[0]           B target
            //  at[1]           BKPT (dummy instruction).
            was = (NIns*) (((intptr_t)at + 8) + (intptr_t)((at[0] & 0xffffff) << 2));
        } else {
            // The existing code is not a branch. This can occur, for example,
            // when patching exit code generated by nFragExit. Exit branches to
            // an epilogue load a value into R2 (using LDi), but this is not
            // required for other exit branches so the new branch can be
            // emitted over the top of the LDi sequence. It would be nice to
            // assert that we're looking at an LDi sequence, but this is not
            // trivial because the output of LDi is both platform- and
            // context-dependent.
            was = (NIns*)-1;    // Return an obviously incorrect target address.
        }
    );

    // Assert that the existing placeholder is not conditional.
    NanoAssert((uint32_t)(at[0] & 0xf0000000) == COND_AL);

    // We only have to patch unconditional branches, but these may take one of
    // the following patterns:
    //
    //  --- Short branch.
    //          B       ±32MB
    //
    //  --- Long branch.
    //          LDR     PC, #lit
    //  lit:    #target

    intptr_t offs = PC_OFFSET_FROM(target, at);
    if (isS24(offs>>2)) {
        // Emit a simple branch (B) in the first of the two available
        // instruction addresses.
        at[0] = (NIns)( COND_AL | (0xA<<24) | ((offs >> 2) & 0xffffff) );
        // and reset at[1] for good measure
        at[1] = BKPT_insn;
    } else {
        // Emit a branch to a pc-relative address, which we'll store right
        // after this instruction
        at[0] = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4) );
        // the target address
        at[1] = (NIns)(target);
    }
    VALGRIND_DISCARD_TRANSLATIONS(at, 2*sizeof(NIns));

#if defined(UNDER_CE)
    // we changed the code, so we need to do this (sadly)
    FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
#elif defined(AVMPLUS_LINUX)
    __clear_cache((char*)at, (char*)(at+3));
#endif

#ifdef AVMPLUS_PORTING_API
    NanoJIT_PortAPI_FlushInstructionCache(at, at+3);
#endif

    return was;
}

RegisterMask
Assembler::hint(LIns* i, RegisterMask allow /* = ~0 */)
{
    uint32_t op = i->opcode();
    int prefer = ~0;

    if (op==LIR_call || op==LIR_fcall)
        prefer = rmask(R0);
    else if (op == LIR_callh)
        prefer = rmask(R1);
    else if (op == LIR_iparam)
        prefer = rmask(imm2register(i->paramArg()));

    if (_allocator.free & allow & prefer)
        allow &= prefer;
    return allow;
}

void
Assembler::asm_qjoin(LIns *ins)
{
    int d = findMemFor(ins);
    NanoAssert(d);
    LIns* lo = ins->oprnd1();
    LIns* hi = ins->oprnd2();

    Register r = findRegFor(hi, GpRegs);
    STR(r, FP, d+4);

    // okay if r gets recycled.
    r = findRegFor(lo, GpRegs);
    STR(r, FP, d);
    freeRsrcOf(ins, false); // if we had a reg in use, emit a ST to flush it to mem
}

void
Assembler::asm_store32(LIns *value, int dr, LIns *base)
{
    Reservation *rA, *rB;
    Register ra, rb;
    if (base->isop(LIR_ialloc)) {
        rb = FP;
        dr += findMemFor(base);
        ra = findRegFor(value, GpRegs);
    } else {
        findRegFor2(GpRegs, value, rA, base, rB);
        ra = rA->reg;
        rb = rB->reg;
    }

    if (!isS12(dr)) {
        STR(ra, IP, 0);
        asm_add_imm(IP, rb, dr);
    } else {
        STR(ra, rb, dr);
    }
}

void
Assembler::asm_restore(LInsp i, Reservation *resv, Register r)
{
    if (i->isop(LIR_ialloc)) {
        asm_add_imm(r, FP, disp(resv));
    } else if (IsFpReg(r)) {
        NanoAssert(AvmCore::config.vfp);

        // We can't easily load immediate values directly into FP registers, so
        // ensure that memory is allocated for the constant and load it from
        // memory.
        int d = findMemFor(i);
        if (isS8(d >> 2)) {
            FLDD(r, FP, d);
        } else {
            FLDD(r, IP, 0);
            asm_add_imm(IP, FP, d);
        }
#if 0
    // This code tries to use a small constant load to restore the value of r.
    // However, there was a comment explaining that using this regresses
    // crypto-aes by about 50%. I do not see that behaviour; however, enabling
    // this code does cause a JavaScript failure in the first of the
    // createMandelSet tests in trace-tests. I can't explain either the
    // original performance issue or the crash that I'm seeing.
    } else if (i->isconst()) {
        // asm_ld_imm will automatically select between LDR and MOV as
        // appropriate.
        if (!resv->arIndex)
            i->resv()->clear();
        asm_ld_imm(r, i->imm32());
#endif
    } else {
        int d = findMemFor(i);
        LDR(r, FP, d);
    }

    verbose_only(
        asm_output("        restore %s",_thisfrag->lirbuf->names->formatRef(i));
    )
}

void
Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
{
    (void) pop;
    (void) quad;
    if (d) {
        if (IsFpReg(rr)) {
            if (isS8(d >> 2)) {
                FSTD(rr, FP, d);
            } else {
                FSTD(rr, IP, 0);
                asm_add_imm(IP, FP, d);
            }
        } else {
            STR(rr, FP, d);
        }
    }
}

void
Assembler::asm_load64(LInsp ins)
{
    //output("<<< load64");

    NanoAssert(ins->isQuad());

    LIns* base = ins->oprnd1();
    int offset = ins->disp();

    Reservation *resv = getresv(ins);
    NanoAssert(resv);
    Register rr = resv->reg;
    int d = disp(resv);

    Register rb = findRegFor(base, GpRegs);
    NanoAssert(IsGpReg(rb));
    freeRsrcOf(ins, false);

    //output("--- load64: Finished register allocation.");

    if (AvmCore::config.vfp && rr != UnknownReg) {
        // VFP is enabled and the result will go into a register.
        NanoAssert(IsFpReg(rr));

        if (!isS8(offset >> 2) || (offset&3) != 0) {
            FLDD(rr,IP,0);
            asm_add_imm(IP, rb, offset);
        } else {
            FLDD(rr,rb,offset);
        }
    } else {
        // Either VFP is not available or the result needs to go into memory;
        // in either case, VFP instructions are not required. Note that the
        // result will never be loaded into registers if VFP is not available.
        NanoAssert(resv->reg == UnknownReg);
        NanoAssert(d != 0);

        // Check that the offset is 8-byte (64-bit) aligned.
        NanoAssert((d & 0x7) == 0);

        // *(uint64_t*)(FP+d) = *(uint64_t*)(rb+offset)
        asm_mmq(FP, d, rb, offset);
    }

    //output(">>> load64");
}

void
Assembler::asm_store64(LInsp value, int dr, LInsp base)
{
    //asm_output("<<< store64 (dr: %d)", dr);

    if (AvmCore::config.vfp) {
        //Reservation *valResv = getresv(value);
        Register rb = findRegFor(base, GpRegs);

        if (value->isconstq()) {
            underrunProtect(LD32_size*2 + 8);

            // XXX use another reg, get rid of dependency
            STR(IP, rb, dr);
            asm_ld_imm(IP, value->imm64_0(), false);
            STR(IP, rb, dr+4);
            asm_ld_imm(IP, value->imm64_1(), false);

            return;
        }

        Register rv = findRegFor(value, FpRegs);

        NanoAssert(rb != UnknownReg);
        NanoAssert(rv != UnknownReg);

        Register baseReg = rb;
        intptr_t baseOffset = dr;

        if (!isS8(dr)) {
            baseReg = IP;
            baseOffset = 0;
        }

        FSTD(rv, baseReg, baseOffset);

        if (!isS8(dr)) {
            asm_add_imm(IP, rb, dr);
        }

        // if it's a constant, make sure our baseReg/baseOffset location
        // has the right value
        if (value->isconstq()) {
            underrunProtect(4*4);
            asm_quad_nochk(rv, value->imm64_0(), value->imm64_1());
        }
    } else {
        int da = findMemFor(value);
        Register rb = findRegFor(base, GpRegs);
        // *(uint64_t*)(rb+dr) = *(uint64_t*)(FP+da)
        asm_mmq(rb, dr, FP, da);
    }

    //asm_output(">>> store64");
}

// stick a quad into register rr, where p points to the two
// 32-bit parts of the quad, optinally also storing at FP+d
void
Assembler::asm_quad_nochk(Register rr, int32_t imm64_0, int32_t imm64_1)
{
    // We're not going to use a slot, because it might be too far
    // away.  Instead, we're going to stick a branch in the stream to
    // jump over the constants, and then load from a short PC relative
    // offset.

    // stream should look like:
    //    branch A
    //    imm64_0
    //    imm64_1
    // A: FLDD PC-16

    FLDD(rr, PC, -16);

    *(--_nIns) = (NIns) imm64_1;
    *(--_nIns) = (NIns) imm64_0;

    JMP_nochk(_nIns+2);
}

void
Assembler::asm_quad(LInsp ins)
{
    Reservation *   res = getresv(ins);
    int             d = disp(res);
    Register        rr = res->reg;

    freeRsrcOf(ins, false);

    if (AvmCore::config.vfp &&
        rr != UnknownReg)
    {
        if (d)
            FSTD(rr, FP, d);

        underrunProtect(4*4);
        asm_quad_nochk(rr, ins->imm64_0(), ins->imm64_1());
    } else {
        NanoAssert(d);
        STR(IP, FP, d+4);
        asm_ld_imm(IP, ins->imm64_1());
        STR(IP, FP, d);
        asm_ld_imm(IP, ins->imm64_0());
    }
}

void
Assembler::asm_nongp_copy(Register r, Register s)
{
    if (IsFpReg(r) && IsFpReg(s)) {
        // fp->fp
        FCPYD(r, s);
    } else {
        // We can't move a double-precision FP register into a 32-bit GP
        // register, so assert that no calling code is trying to do that.
        NanoAssert(0);
    }
}

Register
Assembler::asm_binop_rhs_reg(LInsp)
{
    return UnknownReg;
}

/**
 * copy 64 bits: (rd+dd) <- (rs+ds)
 */
void
Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
{
    // The value is either a 64bit struct or maybe a float that isn't live in
    // an FPU reg.  Either way, don't put it in an FPU reg just to load & store
    // it.
    // This operation becomes a simple 64-bit memcpy.

    // In order to make the operation optimal, we will require two GP
    // registers. We can't allocate a register here because the caller may have
    // called freeRsrcOf, and allocating a register here may cause something
    // else to spill onto the stack which has just be conveniently freed by
    // freeRsrcOf (resulting in stack corruption).
    //
    // Falling back to a single-register implementation of asm_mmq is better
    // than adjusting the callers' behaviour (to allow us to allocate another
    // register here) because spilling a register will end up being slower than
    // just using the same register twice anyway.
    //
    // Thus, if there is a free register which we can borrow, we will emit the
    // following code:
    //  LDR rr, [rs, #ds]
    //  LDR ip, [rs, #(ds+4)]
    //  STR rr, [rd, #dd]
    //  STR ip, [rd, #(dd+4)]
    // (Where rr is the borrowed register.)
    //
    // If there is no free register, don't spill an existing allocation. Just
    // do the following:
    //  LDR ip, [rs, #ds]
    //  STR ip, [rd, #dd]
    //  LDR ip, [rs, #(ds+4)]
    //  STR ip, [rd, #(dd+4)]

    // Ensure that the PC is not used as either base register. The instruction
    // generation macros call underrunProtect, and a side effect of this is
    // that we may be pushed onto another page, so the PC is not a reliable
    // base register.
    NanoAssert(rs != PC);
    NanoAssert(rd != PC);

    // Find the list of free registers from the allocator's free list and the
    // GpRegs mask. This excludes any floating-point registers that may be on
    // the free list.
    RegisterMask    free = _allocator.free & GpRegs;

    if (free) {
        // There is at least one register on the free list, so grab one for
        // temporary use. There is no need to allocate it explicitly because
        // we won't need it after this function returns.

        // The CountLeadingZeroes can be used to quickly find a set bit in the
        // free mask.
        Register    rr = (Register)(31-CountLeadingZeroes(free));

        // Note: Not every register in GpRegs is usable here. However, these
        // registers will never appear on the free list.
        NanoAssert((free & rmask(PC)) == 0);
        NanoAssert((free & rmask(LR)) == 0);
        NanoAssert((free & rmask(SP)) == 0);
        NanoAssert((free & rmask(IP)) == 0);
        NanoAssert((free & rmask(FP)) == 0);

        // Emit the actual instruction sequence.

        STR(IP, rd, dd+4);
        STR(rr, rd, dd);
        LDR(IP, rs, ds+4);
        LDR(rr, rs, ds);
    } else {
        // There are no free registers, so fall back to using IP twice.
        STR(IP, rd, dd+4);
        LDR(IP, rs, ds+4);
        STR(IP, rd, dd);
        LDR(IP, rs, ds);
    }
}

void
Assembler::nativePageReset()
{
    _nSlot = 0;
    _startingSlot = 0;
    _nExitSlot = 0;
}

void
Assembler::nativePageSetup()
{
    if (!_nIns)
        codeAlloc(codeStart, codeEnd, _nIns);
    if (!_nExitIns)
        codeAlloc(exitStart, exitEnd, _nExitIns);

    // constpool starts at top of page and goes down,
    // code starts at bottom of page and moves up
    if (!_nSlot)
        _nSlot = codeStart;
    if (!_nExitSlot)
        _nExitSlot = exitStart;
}


void
Assembler::underrunProtect(int bytes)
{
    NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");
    NanoAssert(_nSlot != 0 && int(_nIns)-int(_nSlot) <= 4096);
    uintptr_t top = uintptr_t(_nSlot);
    uintptr_t pc = uintptr_t(_nIns);
    if (pc - bytes < top)
    {
        verbose_only(verbose_outputf("        %p:", _nIns);)
        NIns* target = _nIns;
        if (_inExit)
            codeAlloc(exitStart, exitEnd, _nIns);
        else
            codeAlloc(codeStart, codeEnd, _nIns);

        _nSlot = _inExit ? exitStart : codeStart;

        // _nSlot points to the first empty position in the new code block
        // _nIns points just past the last empty position.
        // Assume B_nochk won't ever try to write to _nSlot. See B_cond_chk macro.
        B_nochk(target);
    }
}

void
Assembler::JMP_far(NIns* addr)
{
    // Even if a simple branch is all that is required, this function must emit
    // two words so that the branch can be arbitrarily patched later on.
    underrunProtect(8);

    intptr_t offs = PC_OFFSET_FROM(addr,_nIns-2);

    if (isS24(offs>>2)) {
        // Emit a BKPT to ensure that we reserve enough space for a full 32-bit
        // branch patch later on. The BKPT should never be executed.
        BKPT_nochk();

        // B [PC+offs]
        *(--_nIns) = (NIns)( COND_AL | (0xA<<24) | ((offs>>2) & 0xFFFFFF) );

        asm_output("b %p", (void*)addr);
    } else {
        // Insert the target address as a constant in the instruction stream.
        *(--_nIns) = (NIns)((addr));
        // ldr pc, [pc, #-4] // load the address into pc, reading it from [pc-4] (e.g.,
        // the next instruction)
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4));

        asm_output("ldr pc, =%p", (void*)addr);
    }
}

// Perform a branch with link, and ARM/Thumb exchange if necessary. The actual
// BLX instruction is only available from ARMv5 onwards, but as we don't
// support anything older than that this function will not attempt to output
// pre-ARMv5 sequences.
//
// Note: This function is not designed to be used with branches which will be
// patched later, though it will work if the patcher knows how to patch the
// generated instruction sequence.
void
Assembler::BranchWithLink(NIns* addr)
{
    // Most branches emitted by TM are loaded through a register, so always
    // reserve enough space for the LDR sequence. This should give us a slight
    // net gain over reserving the exact amount required for shorter branches.
    // This _must_ be called before PC_OFFSET_FROM as it can move _nIns!
    underrunProtect(4+LD32_size);

    // We don't support ARMv4(T) and will emit ARMv5+ instruction, so assert
    // that we have a suitable processor.
    NanoAssert(AvmCore::config.arch >= 5);

    // Calculate the offset from the instruction that is about to be
    // written (at _nIns-1) to the target.
    intptr_t offs = PC_OFFSET_FROM(addr,_nIns-1);

    // ARMv5 and above can use BLX <imm> for branches within ±32MB of the
    // PC and BLX Rm for long branches.
    if (isS24(offs>>2)) {
        // the value we need to stick in the instruction; masked,
        // because it will be sign-extended back to 32 bits.
        intptr_t offs2 = (offs>>2) & 0xffffff;

        if (((intptr_t)addr & 1) == 0) {
            // The target is ARM, so just emit a BL.

            // BL addr
            *(--_nIns) = (NIns)( (COND_AL) | (0xB<<24) | (offs2) );
            asm_output("bl %p", (void*)addr);
        } else {
            // The target is Thumb, so emit a BLX.

            // The (pre-shifted) value of the "H" bit in the BLX encoding.
            uint32_t    H = (offs & 0x2) << 23;

            // BLX addr
            *(--_nIns) = (NIns)( (0xF << 28) | (0x5<<25) | (H) | (offs2) );
            asm_output("blx %p", (void*)addr);
        }
    } else {
        // BLX IP
        *(--_nIns) = (NIns)( (COND_AL) | (0x12<<20) | (0xFFF<<8) | (0x3<<4) | (IP) );
        asm_output("blx ip (=%p)", (void*)addr);

        // LDR IP, =addr
        asm_ld_imm(IP, (int32_t)addr, false);
    }
}

// Emit the code required to load a memory address into a register as follows:
// d = *(b+off)
// underrunProtect calls from this function can be disabled by setting chk to
// false. However, this function can use more than LD32_size bytes of space if
// the offset is out of the range of a LDR instruction; the maximum space this
// function requires for underrunProtect is 4+LD32_size.
void
Assembler::asm_ldr_chk(Register d, Register b, int32_t off, bool chk)
{
    if (IsFpReg(d)) {
        FLDD_chk(d,b,off,chk);
        return;
    }

    NanoAssert(IsGpReg(d));
    NanoAssert(IsGpReg(b));

    // We can't use underrunProtect if the base register is the PC because
    // underrunProtect might move the PC if there isn't enough space on the
    // current page.
    NanoAssert((b != PC) || (!chk));

    if (isU12(off)) {
        // LDR d, b, #+off
        if (chk) underrunProtect(4);
        *(--_nIns) = (NIns)( COND_AL | (0x59<<20) | (b<<16) | (d<<12) | off );
    } else if (isU12(-off)) {
        // LDR d, b, #-off
        if (chk) underrunProtect(4);
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (b<<16) | (d<<12) | -off );
    } else {
        // The offset is over 4096 (and outside the range of LDR), so we need
        // to add a level of indirection to get the address into IP.

        // Because of that, we can't do a PC-relative load unless it fits within
        // the single-instruction forms above.

        NanoAssert(b != IP);

        if (chk) underrunProtect(4+LD32_size);

        *(--_nIns) = (NIns)( COND_AL | (0x79<<20) | (b<<16) | (d<<12) | IP );
        asm_ld_imm(IP, off, false);
    }

    asm_output("ldr %s, [%s, #%d]",gpn(d),gpn(b),(off));
}

// Emit the code required to load an immediate value (imm) into general-purpose
// register d. Optimal (MOV-based) mechanisms are used if the immediate can be
// encoded using ARM's operand 2 encoding. Otherwise, a slot is used on the
// literal pool and LDR is used to load the value.
//
// chk can be explicitly set to false in order to disable underrunProtect calls
// from this function; this allows the caller to perform the check manually.
// This function guarantees not to use more than LD32_size bytes of space.
void
Assembler::asm_ld_imm(Register d, int32_t imm, bool chk /* = true */)
{
    uint32_t    op2imm;

    NanoAssert(IsGpReg(d));

    // Attempt to encode the immediate using the second operand of MOV or MVN.
    // This is the simplest solution and generates the shortest and fastest
    // code, but can only encode a limited set of values.

    if (encOp2Imm(imm, &op2imm)) {
        // Use MOV to encode the literal.
        MOVis(d, op2imm, 0);
        return;
    }

    if (encOp2Imm(~imm, &op2imm)) {
        // Use MVN to encode the inverted literal.
        MVNis(d, op2imm, 0);
        return;
    }

    // Try to use simple MOV, MVN or MOV(W|T) instructions to load the
    // immediate. If this isn't possible, load it from memory.
    //  - We cannot use MOV(W|T) on cores older than the introduction of
    //    Thumb-2 or if the target register is the PC.
    if (AvmCore::config.thumb2 && (d != PC)) {
        // ARMv6T2 and above have MOVW and MOVT.
        uint32_t    high_h = (uint32_t)imm >> 16;
        uint32_t    low_h = imm & 0xffff;

        if (high_h != 0) {
            // Load the high half-word (if necessary).
            MOVTi_chk(d, high_h, chk);
        }
        // Load the low half-word. This also zeroes the high half-word, and
        // thus must execute _before_ MOVT, and is necessary even if low_h is 0
        // because MOVT will not change the existing low half-word.
        MOVWi_chk(d, low_h, chk);

        return;
    }

    // We couldn't encode the literal in the instruction stream, so load it
    // from memory.

    // Because the literal pool is on the same page as the generated code, it
    // will almost always be within the ±4096 range of a LDR. However, this may
    // not be the case if _nSlot is at the start of the page and _nIns is at
    // the end because the PC is 8 bytes ahead of _nIns. This is unlikely to
    // happen, but if it does occur we can simply waste a word or two of
    // literal space.

    // We must do the underrunProtect before PC_OFFSET_FROM as underrunProtect
    // can move the PC if there isn't enough space on the current page!
    if (chk) {
        underrunProtect(LD32_size);
    }

    int offset = PC_OFFSET_FROM(_nSlot, _nIns-1);
    // If the offset is out of range, waste literal space until it is in range.
    while (offset <= -4096) {
        ++_nSlot;
        offset += sizeof(_nSlot);
    }
    NanoAssert(isS12(offset) && (offset < 0));

    // Write the literal.
    *(_nSlot++) = imm;
    asm_output("## imm= 0x%x", imm);

    // Load the literal.
    LDR_nochk(d,PC,offset);
    NanoAssert(uintptr_t(_nIns) + 8 + offset == uintptr_t(_nSlot-1));
    NanoAssert(*((int32_t*)_nSlot-1) == imm);
}

// Branch to target address _t with condition _c, doing underrun
// checks (_chk == 1) or skipping them (_chk == 0).
//
// Set the target address (_t) to 0 if the target is not yet known and the
// branch will be patched up later.
//
// If the jump is to a known address (with _t != 0) and it fits in a relative
// jump (±32MB), emit that.
// If the jump is unconditional, emit the dest address inline in
// the instruction stream and load it into pc.
// If the jump has a condition, but noone's mucked with _nIns and our _nSlot
// pointer is valid, stick the constant in the slot and emit a conditional
// load into pc.
// Otherwise, emit the conditional load into pc from a nearby constant,
// and emit a jump to jump over it it in case the condition fails.
//
// NB: JMP_nochk depends on this not calling samepage() when _c == AL
void
Assembler::B_cond_chk(ConditionCode _c, NIns* _t, bool _chk)
{
    int32_t offs = PC_OFFSET_FROM(_t,_nIns-1);
    //nj_dprintf("B_cond_chk target: 0x%08x offset: %d @0x%08x\n", _t, offs, _nIns-1);

    // We don't patch conditional branches, and nPatchBranch can't cope with
    // them. We should therefore check that they are not generated at this
    // stage.
    NanoAssert((_t != 0) || (_c == AL));

    // optimistically check if this will fit in 24 bits
    if (_chk && isS24(offs>>2) && (_t != 0)) {
        underrunProtect(4);
        // recalculate the offset, because underrunProtect may have
        // moved _nIns to a new page
        offs = PC_OFFSET_FROM(_t,_nIns-1);
    }

    // Emit one of the following patterns:
    //
    //  --- Short branch. This can never be emitted if the branch target is not
    //      known.
    //          B(cc)   ±32MB
    //
    //  --- Long unconditional branch.
    //          LDR     PC, #lit
    //  lit:    #target
    //
    //  --- Long conditional branch. Note that conditional branches will never
    //      be patched, so the nPatchBranch function doesn't need to know where
    //      the literal pool is located.
    //          LDRcc   PC, #lit
    //          ; #lit is in the literal pool at _nSlot
    //
    //  --- Long conditional branch (if the slot isn't on the same page as the instruction).
    //          LDRcc   PC, #lit
    //          B       skip        ; Jump over the literal data.
    //  lit:    #target
    //  skip:   [...]

    if (isS24(offs>>2) && (_t != 0)) {
        // The underrunProtect for this was done above (if required by _chk).
        *(--_nIns) = (NIns)( ((_c)<<28) | (0xA<<24) | (((offs)>>2) & 0xFFFFFF) );
    } else if (_c == AL) {
        if(_chk) underrunProtect(8);
        *(--_nIns) = (NIns)(_t);
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | 0x4 );
    } else if (PC_OFFSET_FROM(_nSlot, _nIns-1) > -0x1000 /*~(NJ_PAGE_SIZE-1)*/) {
        if(_chk) underrunProtect(8);
        *(_nSlot++) = (NIns)(_t);
        offs = PC_OFFSET_FROM(_nSlot-1,_nIns-1);
        NanoAssert(offs < 0);
        *(--_nIns) = (NIns)( ((_c)<<28) | (0x51<<20) | (PC<<16) | (PC<<12) | ((-offs) & 0xFFF) );
        asm_output("ldr%s %s, [%s, #-%d]", condNames[_c], gpn(PC), gpn(PC), -offs);
        NanoAssert(uintptr_t(_nIns)+8+offs == uintptr_t(_nSlot-1));
    } else {
        if(_chk) underrunProtect(12);
        // Emit a pointer to the target as a literal in the instruction stream.
        *(--_nIns) = (NIns)(_t);
        // Emit a branch to skip over the literal. The PC value is 8 bytes
        // ahead of the executing instruction, so to branch two instructions
        // forward this must branch 8-8=0 bytes.
        *(--_nIns) = (NIns)( COND_AL | (0xA<<24) | 0x0 );
        // Emit the conditional branch.
        *(--_nIns) = (NIns)( ((_c)<<28) | (0x51<<20) | (PC<<16) | (PC<<12) | 0x0 );
    }

    asm_output("b%s %p", condNames[_c], (void*)(_t));
}

/*
 * VFP
 */

void
Assembler::asm_i2f(LInsp ins)
{
    Register rr = prepResultReg(ins, FpRegs);
    Register srcr = findRegFor(ins->oprnd1(), GpRegs);

    // todo: support int value in memory, as per x86
    NanoAssert(srcr != UnknownReg);

    FSITOD(rr, FpSingleScratch);
    FMSR(FpSingleScratch, srcr);
}

void
Assembler::asm_u2f(LInsp ins)
{
    Register rr = prepResultReg(ins, FpRegs);
    Register sr = findRegFor(ins->oprnd1(), GpRegs);

    // todo: support int value in memory, as per x86
    NanoAssert(sr != UnknownReg);

    FUITOD(rr, FpSingleScratch);
    FMSR(FpSingleScratch, sr);
}

void
Assembler::asm_fneg(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    Register rr = prepResultReg(ins, FpRegs);

    Reservation* rA = getresv(lhs);
    Register sr;

    if (!rA || rA->reg == UnknownReg)
        sr = findRegFor(lhs, FpRegs);
    else
        sr = rA->reg;

    FNEGD(rr, sr);
}

void
Assembler::asm_fop(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    LInsp rhs = ins->oprnd2();
    LOpcode op = ins->opcode();

    NanoAssert(op >= LIR_fadd && op <= LIR_fdiv);

    // rr = ra OP rb

    Register rr = prepResultReg(ins, FpRegs);

    Register ra = findRegFor(lhs, FpRegs);
    Register rb = (rhs == lhs) ? ra : findRegFor(rhs, FpRegs & ~rmask(ra));

    // XXX special-case 1.0 and 0.0

    switch (op)
    {
        case LIR_fadd:      FADDD(rr,ra,rb);    break;
        case LIR_fsub:      FSUBD(rr,ra,rb);    break;
        case LIR_fmul:      FMULD(rr,ra,rb);    break;
        case LIR_fdiv:      FDIVD(rr,ra,rb);    break;
        default:            NanoAssert(0);      break;
    }
}

void
Assembler::asm_fcmp(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    LInsp rhs = ins->oprnd2();
    LOpcode op = ins->opcode();

    NanoAssert(op >= LIR_feq && op <= LIR_fge);

    Register ra = findRegFor(lhs, FpRegs);
    Register rb = findRegFor(rhs, FpRegs);

    FMSTAT();
    FCMPD(ra, rb);
}

Register
Assembler::asm_prep_fcall(Reservation*, LInsp)
{
    /* Because ARM actually returns the result in (R0,R1), and not in a
     * floating point register, the code to move the result into a correct
     * register is at the beginning of asm_call(). This function does
     * nothing.
     *
     * The reason being that if this function did something, the final code
     * sequence we'd get would be something like:
     *     MOV {R0-R3},params        [from asm_call()]
     *     BL function               [from asm_call()]
     *     MOV {R0-R3},spilled data  [from evictScratchRegs()]
     *     MOV Dx,{R0,R1}            [from this function]
     * which is clearly broken.
     *
     * This is not a problem for non-floating point calls, because the
     * restoring of spilled data into R0 is done via a call to prepResultReg(R0)
     * at the same point in the sequence as this function is called, meaning that
     * evictScratchRegs() will not modify R0. However, prepResultReg is not aware
     * of the concept of using a register pair (R0,R1) for the result of a single
     * operation, so it can only be used here with the ultimate VFP register, and
     * not R0/R1, which potentially allows for R0/R1 to get corrupted as described.
     */
    return UnknownReg;
}

NIns*
Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ)
{
    LOpcode condop = cond->opcode();
    NanoAssert(cond->isCond());

    // The old "never" condition code has special meaning on newer ARM cores,
    // so use "always" as a sensible default code.
    ConditionCode cc = AL;

    // Detect whether or not this is a floating-point comparison.
    bool    fp_cond;

    // Select the appropriate ARM condition code to match the LIR instruction.
    switch (condop)
    {
        // Floating-point conditions. Note that the VFP LT/LE conditions
        // require use of the unsigned condition codes, even though
        // float-point comparisons are always signed.
        case LIR_feq:   cc = EQ;    fp_cond = true;     break;
        case LIR_flt:   cc = LO;    fp_cond = true;     break;
        case LIR_fle:   cc = LS;    fp_cond = true;     break;
        case LIR_fge:   cc = GE;    fp_cond = true;     break;
        case LIR_fgt:   cc = GT;    fp_cond = true;     break;

        // Standard signed and unsigned integer comparisons.
        case LIR_eq:    cc = EQ;    fp_cond = false;    break;
        case LIR_ov:    cc = VS;    fp_cond = false;    break;
        case LIR_lt:    cc = LT;    fp_cond = false;    break;
        case LIR_le:    cc = LE;    fp_cond = false;    break;
        case LIR_gt:    cc = GT;    fp_cond = false;    break;
        case LIR_ge:    cc = GE;    fp_cond = false;    break;
        case LIR_ult:   cc = LO;    fp_cond = false;    break;
        case LIR_ule:   cc = LS;    fp_cond = false;    break;
        case LIR_ugt:   cc = HI;    fp_cond = false;    break;
        case LIR_uge:   cc = HS;    fp_cond = false;    break;

        // Default case for invalid or unexpected LIR instructions.
        default:        cc = AL;    fp_cond = false;    break;
    }

    // Invert the condition if required.
    if (branchOnFalse)
        cc = OppositeCond(cc);

    // Ensure that we got a sensible condition code.
    NanoAssert((cc != AL) && (cc != NV));

    // Ensure that we don't hit floating-point LIR codes if VFP is disabled.
    NanoAssert(AvmCore::config.vfp || !fp_cond);

    // Emit a suitable branch instruction.
    B_cond(cc, targ);

    // Store the address of the branch instruction so that we can return it.
    // asm_[f]cmp will move _nIns so we must do this now.
    NIns *at = _nIns;

    if (fp_cond)
        asm_fcmp(cond);
    else
        asm_cmp(cond);

    return at;
}

void
Assembler::asm_cmp(LIns *cond)
{
    LOpcode condop = cond->opcode();

    // LIR_ov recycles the flags set by arithmetic ops
    if ((condop == LIR_ov))
        return;

    LInsp lhs = cond->oprnd1();
    LInsp rhs = cond->oprnd2();
    Reservation *rA, *rB;

    // Not supported yet.
    NanoAssert(!lhs->isQuad() && !rhs->isQuad());

    // ready to issue the compare
    if (rhs->isconst()) {
        int c = rhs->imm32();
        if (c == 0 && cond->isop(LIR_eq)) {
            Register r = findRegFor(lhs, GpRegs);
            TST(r,r);
            // No 64-bit immediates so fall-back to below
        } else if (!rhs->isQuad()) {
            Register r = getBaseReg(lhs, c, GpRegs);
            asm_cmpi(r, c);
        } else {
            NanoAssert(0);
        }
    } else {
        findRegFor2(GpRegs, lhs, rA, rhs, rB);
        Register ra = rA->reg;
        Register rb = rB->reg;
        CMP(ra, rb);
    }
}

void
Assembler::asm_cmpi(Register r, int32_t imm)
{
    if (imm < 0) {
        if (imm > -256) {
            ALUi(AL, cmn, 1, 0, r, -imm);
        } else {
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    } else {
        if (imm < 256) {
            ALUi(AL, cmp, 1, 0, r, imm);
        } else {
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    }
}

void
Assembler::asm_loop(LInsp ins, NInsList& loopJumps)
{
    // XXX asm_loop should be in Assembler.cpp!

    JMP_far(0);
    loopJumps.add(_nIns);

    // If the target we are looping to is in a different fragment, we have to restore
    // SP since we will target fragEntry and not loopEntry.
    if (ins->record()->exit->target != _thisfrag)
        MOV(SP,FP);
}

void
Assembler::asm_fcond(LInsp ins)
{
    // only want certain regs
    Register r = prepResultReg(ins, AllowableFlagRegs);

    switch (ins->opcode()) {
        case LIR_feq: SET(r,EQ); break;
        case LIR_flt: SET(r,LO); break;
        case LIR_fle: SET(r,LS); break;
        case LIR_fge: SET(r,GE); break;
        case LIR_fgt: SET(r,GT); break;
        default: NanoAssert(0); break;
    }

    asm_fcmp(ins);
}

void
Assembler::asm_cond(LInsp ins)
{
    Register r = prepResultReg(ins, AllowableFlagRegs);
    switch(ins->opcode())
    {
        case LIR_eq:    SET(r,EQ);      break;
        case LIR_ov:    SET(r,VS);      break;
        case LIR_lt:    SET(r,LT);      break;
        case LIR_le:    SET(r,LE);      break;
        case LIR_gt:    SET(r,GT);      break;
        case LIR_ge:    SET(r,GE);      break;
        case LIR_ult:   SET(r,LO);      break;
        case LIR_ule:   SET(r,LS);      break;
        case LIR_ugt:   SET(r,HI);      break;
        case LIR_uge:   SET(r,HS);      break;
        default:        NanoAssert(0);  break;
    }
    asm_cmp(ins);
}

void
Assembler::asm_arith(LInsp ins)
{
    LOpcode op = ins->opcode();
    LInsp   lhs = ins->oprnd1();
    LInsp   rhs = ins->oprnd2();

    RegisterMask    allow = GpRegs;

    // We always need the result register and the first operand register.
    Register        rr = prepResultReg(ins, allow);
    Reservation *   rA = getresv(lhs);
    Register        ra = UnknownReg;
    Register        rb = UnknownReg;

    // If this is the last use of lhs in reg, we can re-use the result reg.
    if (!rA || (ra = rA->reg) == UnknownReg)
        ra = findSpecificRegFor(lhs, rr);

    // Don't re-use the registers we've already allocated.
    NanoAssert(rr != UnknownReg);
    NanoAssert(ra != UnknownReg);
    allow &= ~rmask(rr);
    allow &= ~rmask(ra);

    // If the rhs is constant, we can use the instruction-specific code to
    // determine if the value can be encoded in an ARM instruction. If the
    // value cannot be encoded, it will be loaded into a register.
    //
    // Note that the MUL instruction can never take an immediate argument so
    // even if the argument is constant, we must allocate a register for it.
    //
    // Note: It is possible to use a combination of the barrel shifter and the
    // basic arithmetic instructions to generate constant multiplications.
    // However, LIR_mul is never invoked with a constant during
    // trace-tests.js so it is very unlikely to be worthwhile implementing it.
    if (rhs->isconst() && op != LIR_mul)
    {
        if ((op == LIR_add || op == LIR_iaddp) && lhs->isop(LIR_ialloc)) {
            // Add alloc+const. The result should be the address of the
            // allocated space plus a constant.
            Register    rs = prepResultReg(ins, allow);
            int         d = findMemFor(lhs) + rhs->imm32();

            NanoAssert(rs != UnknownReg);
            asm_add_imm(rs, FP, d);
        }

        int32_t imm32 = rhs->imm32();

        switch (op)
        {
            case LIR_iaddp: asm_add_imm(rr, ra, imm32);     break;
            case LIR_add:   asm_add_imm(rr, ra, imm32, 1);  break;
            case LIR_sub:   asm_sub_imm(rr, ra, imm32, 1);  break;
            case LIR_and:   asm_and_imm(rr, ra, imm32);     break;
            case LIR_or:    asm_orr_imm(rr, ra, imm32);     break;
            case LIR_xor:   asm_eor_imm(rr, ra, imm32);     break;
            case LIR_lsh:   LSLi(rr, ra, imm32);            break;
            case LIR_rsh:   ASRi(rr, ra, imm32);            break;
            case LIR_ush:   LSRi(rr, ra, imm32);            break;

            default:
                NanoAssertMsg(0, "Unsupported");
                break;
        }

        // We've already emitted an instruction, so return now.
        return;
    }

    // The rhs is either a register or cannot be encoded as a constant.

    if (lhs == rhs) {
        rb = ra;
    } else {
        rb = asm_binop_rhs_reg(ins);
        if (rb == UnknownReg)
            rb = findRegFor(rhs, allow);
        allow &= ~rmask(rb);
    }
    NanoAssert(rb != UnknownReg);

    switch (op)
    {
        case LIR_iaddp: ADDs(rr, ra, rb, 0);    break;
        case LIR_add:   ADDs(rr, ra, rb, 1);    break;
        case LIR_sub:   SUBs(rr, ra, rb, 1);    break;
        case LIR_and:   ANDs(rr, ra, rb, 0);    break;
        case LIR_or:    ORRs(rr, ra, rb, 0);    break;
        case LIR_xor:   EORs(rr, ra, rb, 0);    break;

        case LIR_mul:
            // ARMv5 and earlier cores cannot do a MUL where the first operand
            // is also the result, so we need a special case to handle that.
            //
            // We try to use rb as the first operand by default because it is
            // common for (rr == ra) and is thus likely to be the most
            // efficient case; if ra is no longer used after this LIR
            // instruction, it is re-used for the result register (rr).
            if ((AvmCore::config.arch > 5) || (rr != rb)) {
                // Newer cores place no restrictions on the registers used in a
                // MUL instruction (compared to other arithmetic instructions).
                MUL(rr, rb, ra);
            } else {
                // config.arch is ARMv5 (or below) and rr == rb, so we must
                // find a different way to encode the instruction.

                // If possible, swap the arguments to avoid the restriction.
                if (rr != ra) {
                    // We know that rr == rb, so this will be something like
                    // rX = rY * rX.
                    MUL(rr, ra, rb);
                } else {
                    // We're trying to do rX = rX * rX, so we must use a
                    // temporary register to achieve this correctly on ARMv5.

                    // The register allocator will never allocate IP so it will
                    // be safe to use here.
                    NanoAssert(ra != IP);
                    NanoAssert(rr != IP);

                    // In this case, rr == ra == rb.
                    MUL(rr, IP, rb);
                    MOV(IP, ra);
                }
            }
            break;
        
        // The shift operations need a mask to match the JavaScript
        // specification because the ARM architecture allows a greater shift
        // range than JavaScript.
        case LIR_lsh:
            LSL(rr, ra, IP);
            ANDi(IP, rb, 0x1f);
            break;
        case LIR_rsh:
            ASR(rr, ra, IP);
            ANDi(IP, rb, 0x1f);
            break;
        case LIR_ush:
            LSR(rr, ra, IP);
            ANDi(IP, rb, 0x1f);
            break;
        default:
            NanoAssertMsg(0, "Unsupported");
            break;
    }
}

void
Assembler::asm_neg_not(LInsp ins)
{
    LOpcode op = ins->opcode();
    Register rr = prepResultReg(ins, GpRegs);

    LIns* lhs = ins->oprnd1();
    Reservation *rA = getresv(lhs);
    // if this is last use of lhs in reg, we can re-use result reg
    Register ra;
    if (rA == 0 || (ra=rA->reg) == UnknownReg)
        ra = findSpecificRegFor(lhs, rr);
    // else, rA already has a register assigned.
    NanoAssert(ra != UnknownReg);

    if (op == LIR_not)
        MVN(rr, ra);
    else
        RSBS(rr, ra);
}

void
Assembler::asm_ld(LInsp ins)
{
    LOpcode op = ins->opcode();
    LIns* base = ins->oprnd1();
    int d = ins->disp();

    Register rr = prepResultReg(ins, GpRegs);
    Register ra = getBaseReg(base, d, GpRegs);

    // these will always be 4-byte aligned
    if (op == LIR_ld || op == LIR_ldc) {
        LDR(rr, ra, d);
        return;
    }

    // these will be 2 or 4-byte aligned
    if (op == LIR_ldcs) {
        LDRH(rr, ra, d);
        return;
    }

    // aaand this is just any byte.
    if (op == LIR_ldcb) {
        LDRB(rr, ra, d);
        return;
    }

    NanoAssertMsg(0, "Unsupported instruction in asm_ld");
}

void
Assembler::asm_cmov(LInsp ins)
{
    NanoAssert(ins->opcode() == LIR_cmov);
    LIns* condval = ins->oprnd1();
    LIns* iftrue  = ins->oprnd2();
    LIns* iffalse = ins->oprnd3();

    NanoAssert(condval->isCmp());
    NanoAssert(!iftrue->isQuad() && !iffalse->isQuad());

    const Register rr = prepResultReg(ins, GpRegs);

    // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
    // (This is true on Intel, is it true on all architectures?)
    const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
    switch (condval->opcode()) {
        // note that these are all opposites...
        case LIR_eq:    MOVNE(rr, iffalsereg);  break;
        case LIR_ov:    MOVVC(rr, iffalsereg);  break;
        case LIR_lt:    MOVGE(rr, iffalsereg);  break;
        case LIR_le:    MOVGT(rr, iffalsereg);  break;
        case LIR_gt:    MOVLE(rr, iffalsereg);  break;
        case LIR_ge:    MOVLT(rr, iffalsereg);  break;
        case LIR_ult:   MOVCS(rr, iffalsereg);  break;
        case LIR_ule:   MOVHI(rr, iffalsereg);  break;
        case LIR_ugt:   MOVLS(rr, iffalsereg);  break;
        case LIR_uge:   MOVCC(rr, iffalsereg);  break;
        default: debug_only( NanoAssert(0) );   break;
    }
    /*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
    asm_cmp(condval);
}

void
Assembler::asm_qhi(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    LIns *q = ins->oprnd1();
    int d = findMemFor(q);
    LDR(rr, FP, d+4);
}

void
Assembler::asm_qlo(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    LIns *q = ins->oprnd1();
    int d = findMemFor(q);
    LDR(rr, FP, d);
}

void
Assembler::asm_param(LInsp ins)
{
    uint32_t a = ins->paramArg();
    uint32_t kind = ins->paramKind();
    if (kind == 0) {
        // ordinary param
        AbiKind abi = _thisfrag->lirbuf->abi;
        uint32_t abi_regcount = abi == ABI_FASTCALL ? 2 : abi == ABI_THISCALL ? 1 : 0;
        if (a < abi_regcount) {
            // incoming arg in register
            prepResultReg(ins, rmask(argRegs[a]));
        } else {
            // incoming arg is on stack, and EBP points nearby (see genPrologue)
            Register r = prepResultReg(ins, GpRegs);
            int d = (a - abi_regcount) * sizeof(intptr_t) + 8;
            LDR(r, FP, d);
        }
    } else {
        // saved param
        prepResultReg(ins, rmask(savedRegs[a]));
    }
}

void
Assembler::asm_int(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    asm_ld_imm(rr, ins->imm32());
}

}

#endif /* FEATURE_NANOJIT */
