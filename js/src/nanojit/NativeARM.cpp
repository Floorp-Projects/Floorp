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
 *   Jacob Bramley <Jacob.Bramley@arm.com>
 *   Tero Koskinen <tero.koskinen@digia.com>
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

#if defined(FEATURE_NANOJIT) && defined(NANOJIT_ARM)

namespace nanojit
{

#ifdef NJ_VERBOSE
const char* regNames[] = {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","fp","ip","sp","lr","pc",
                          "d0","d1","d2","d3","d4","d5","d6","d7","s0"};
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
static inline uint32_t
CountLeadingZeroesSlow(uint32_t data)
{
    // Other platforms must fall back to a C routine. This won't be as
    // efficient as the CLZ instruction, but it is functional.
    uint32_t    try_shift;

    uint32_t    leading_zeroes = 0;

    // This loop does a bisection search rather than the obvious rotation loop.
    // This should be faster, though it will still be no match for CLZ.
    for (try_shift = 16; try_shift != 0; try_shift /= 2) {
        uint32_t    shift = leading_zeroes + try_shift;
        if (((data << shift) >> shift) == data) {
            leading_zeroes = shift;
        }
    }

    return leading_zeroes;
}

inline uint32_t
Assembler::CountLeadingZeroes(uint32_t data)
{
    uint32_t    leading_zeroes;

#if defined(__ARMCC__)
    // ARMCC can do this with an intrinsic.
    leading_zeroes = __clz(data);
#elif defined(__GNUC__)
    // GCC can use inline assembler to insert a CLZ instruction.
    if (ARM_ARCH_AT_LEAST(5)) {
        __asm (
#if defined(ANDROID) && (NJ_COMPILER_ARM_ARCH < 7)
        // On Android gcc compiler, the clz instruction is not supported with a
        // target smaller than armv7, despite it being legal for armv5+.
            "   .arch armv7-a\n"
        // Force the object to be tagged as armv4t to avoid it bumping the final
        // binary target.
            "   .object_arch armv4t\n"
#elif (NJ_COMPILER_ARM_ARCH < 5)
        // Targetting armv5t allows a toolchain with armv4t target to still build
        // with clz, and clz to be used when appropriate at runtime.
            "   .arch armv5t\n"
        // Force the object file to be tagged as armv4t to avoid it bumping the
        // final binary target.
            "   .object_arch armv4t\n"
#endif
            "   clz     %0, %1  \n"
            :   "=r"    (leading_zeroes)
            :   "r"     (data)
        );
    } else {
        leading_zeroes = CountLeadingZeroesSlow(data);
    }
#else
    leading_zeroes = CountLeadingZeroesSlow(data);
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
    int32_t    rot;
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

    // As a special case to simplify code elsewhere, emit nothing where we
    // don't want to update the flags (stat == 0), the second operand is 0 and
    // (rd == rn). Such instructions are effectively NOPs.
    if ((imm == 0) && (stat == 0) && (rd == rn)) {
        return;
    }

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

    // As a special case to simplify code elsewhere, emit nothing where we
    // don't want to update the flags (stat == 0), the second operand is 0 and
    // (rd == rn). Such instructions are effectively NOPs.
    if ((imm == 0) && (stat == 0) && (rd == rn)) {
        return;
    }

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
Assembler::nInit()
{
    nHints[LIR_calli]  = rmask(retRegs[0]);
    nHints[LIR_hcalli] = rmask(retRegs[1]);
    nHints[LIR_paramp] = PREFER_SPECIAL;
}

void Assembler::nBeginAssembly()
{
    max_out_args = 0;
}

NIns*
Assembler::genPrologue()
{
    /**
     * Prologue
     */

    // NJ_RESV_OFFSET is space at the top of the stack for us
    // to use for parameter passing (8 bytes at the moment)
    uint32_t stackNeeded = max_out_args + STACK_GRANULARITY * _activation.stackSlotsNeeded();
    uint32_t savingCount = 2;

    uint32_t savingMask = rmask(FP) | rmask(LR);

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
Assembler::nFragExit(LIns* guard)
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

        GuardRecord *gr = guard->record();

        if (!_epilogue)
            _epilogue = genEpilogue();

        // Jump to the epilogue. This may get patched later, but JMP_far always
        // emits two instructions even when only one is required, so patching
        // will work correctly.
        JMP_far(_epilogue);

        // In the future you may want to move this further down so that we can
        // overwrite the r0 guard record load during a patch to a different
        // fragment with some assumed input-register state. Not today though.
        gr->jmp = _nIns;

        // NB: this is a workaround for the fact that, by patching a
        // fragment-exit jump, we could be changing the *meaning* of the R0
        // register we're passing to the jump target. If we jump to the
        // epilogue, ideally R0 means "return value when exiting fragment".
        // If we patch this to jump to another fragment however, R0 means
        // "incoming 0th parameter". This is just a quirk of ARM ABI. So
        // we compromise by passing "return value" to the epilogue in IP,
        // not R0, and have the epilogue MOV(R0, IP) first thing.

        asm_ld_imm(IP, int(gr));
    }

#ifdef NJ_VERBOSE
    if (_config.arm_show_stats) {
        // load R1 with Fragment *fromFrag, target fragment
        // will make use of this when calling fragenter().
        int fromfrag = int((Fragment*)_thisfrag);
        asm_ld_imm(argRegs[1], fromfrag);
    }
#endif

    // profiling for the exit
    verbose_only(
       if (_logc->lcbits & LC_FragProfile) {
           asm_inc_m32( &guard->record()->profCount );
       }
    )

    // Pop the stack frame.
    MOV(SP, FP);
}

NIns*
Assembler::genEpilogue()
{
    RegisterMask savingMask;

    if (ARM_ARCH_AT_LEAST(5)) {
        // On ARMv5+, loading directly to PC correctly handles interworking.
        savingMask = rmask(FP) | rmask(PC);

    } else {
        // On ARMv4T, interworking is not handled properly, therefore, we pop
        // lr and use bx lr to avoid that.
        savingMask = rmask(FP) | rmask(LR);
        BX(LR);
    }
    POP_mask(savingMask); // regs

    // NB: this is the later half of the dual-nature patchable exit branch
    // workaround noted above in nFragExit. IP has the "return value"
    // incoming, we need to move it to R0.
    MOV(R0, IP);

    return _nIns;
}

/*
 * asm_arg will encode the specified argument according to the current ABI, and
 * will update r and stkd as appropriate so that the next argument can be
 * encoded.
 *
 * Linux has used ARM's EABI for some time; support for the legacy ABI
 * has now been removed.
 *
 * Under EABI:
 * - doubles are 64-bit aligned both in registers and on the stack.
 *   If the next available argument register is R1, it is skipped
 *   and the double is placed in R2:R3.  If R0:R1 or R2:R3 are not
 *   available, the double is placed on the stack, 64-bit aligned.
 * - 32-bit arguments are placed in registers and 32-bit aligned
 *   on the stack.
 *
 * Under EABI with hardware floating-point procedure-call variant:
 * - Same as EABI, but doubles are passed in D0..D7 registers.
 */
void
Assembler::asm_arg(ArgType ty, LIns* arg, ParameterRegisters& params)
{
    // The stack pointer must always be at least aligned to 4 bytes.
    NanoAssert((params.stkd & 3) == 0);

    if (ty == ARGTYPE_D) {
        // This task is fairly complex and so is delegated to asm_arg_64.
        asm_arg_64(arg, params);
    } else {
        NanoAssert(ty == ARGTYPE_I || ty == ARGTYPE_UI);
        // pre-assign registers R0-R3 for arguments (if they fit)
        if (params.r < R4) {
            asm_regarg(ty, arg, params.r);
            params.r = Register(params.r + 1);
        } else {
            asm_stkarg(arg, params.stkd);
            params.stkd += 4;
        }
    }
}

// Encode a 64-bit floating-point argument using the appropriate ABI.
// This function operates in the same way as asm_arg, except that it will only
// handle arguments where (ArgType)ty == ARGTYPE_D.

#ifdef NJ_ARM_EABI_HARD_FLOAT
void
Assembler::asm_arg_64(LIns* arg, ParameterRegisters& params)
{
    NanoAssert(IsFpReg(params.float_r));
    if (params.float_r <= D7) {
        findSpecificRegFor(arg, params.float_r);
        params.float_r = Register(params.float_r + 1);
    } else {
        NanoAssertMsg(0, "Only 8 floating point arguments supported");
    }
}

#else
void
Assembler::asm_arg_64(LIns* arg, ParameterRegisters& params)
{
    // The stack pointer must always be at least aligned to 4 bytes.
    NanoAssert((params.stkd & 3) == 0);
    // The only use for this function when we are using soft floating-point
    // is for LIR_ii2d.
    NanoAssert(ARM_VFP || arg->isop(LIR_ii2d));

    // EABI requires that 64-bit arguments are aligned on even-numbered
    // registers, as R0:R1 or R2:R3. If the register base is at an
    // odd-numbered register, advance it. Note that this will push r past
    // R3 if r is R3 to start with, and will force the argument to go on
    // the stack.
    if ((params.r == R1) || (params.r == R3)) {
        params.r = Register(params.r + 1);
    }

    if (params.r < R3) {
        Register    ra = params.r;
        Register    rb = Register(params.r + 1);
        params.r = Register(rb + 1);

        // EABI requires that 64-bit arguments are aligned on even-numbered
        // registers, as R0:R1 or R2:R3.
        NanoAssert( ((ra == R0) && (rb == R1)) || ((ra == R2) && (rb == R3)) );

        // Put the argument in ra and rb. If the argument is in a VFP register,
        // use FMRRD to move it to ra and rb. Otherwise, let asm_regarg deal
        // with the argument as if it were two 32-bit arguments.
        if (ARM_VFP) {
            Register dm = findRegFor(arg, FpRegs);
            FMRRD(ra, rb, dm);
        } else {
            asm_regarg(ARGTYPE_I, arg->oprnd1(), ra);
            asm_regarg(ARGTYPE_I, arg->oprnd2(), rb);
        }
    } else {
        // The argument won't fit in registers, so pass on to asm_stkarg.
        // EABI requires that 64-bit arguments are 64-bit aligned.
        if ((params.stkd & 7) != 0) {
            // stkd will always be aligned to at least 4 bytes; this was
            // asserted on entry to this function.
            params.stkd += 4;
        }
        if (ARM_VFP) {
            asm_stkarg(arg, params.stkd);
        } else {
            asm_stkarg(arg->oprnd1(), params.stkd);
            asm_stkarg(arg->oprnd2(), params.stkd+4);
        }
        params.stkd += 8;
    }
}
#endif // NJ_ARM_EABI_HARD_FLOAT

void
Assembler::asm_regarg(ArgType ty, LIns* p, Register rd)
{
    // Note that we don't have to prepareResultReg here because it is already
    // done by the caller, and the target register is passed as 'rd'.
    // Similarly, we don't have to freeResourcesOf(p).

    if (ty == ARGTYPE_I || ty == ARGTYPE_UI)
    {
        // Put the argument in register rd.
        if (p->isImmI()) {
            asm_ld_imm(rd, p->immI());
        } else {
            if (p->isInReg()) {
                MOV(rd, p->getReg());
            } else {
                // Re-use the target register if the source is no longer
                // required. This saves a MOV instruction.
                findSpecificRegForUnallocated(p, rd);
            }
        }
    } else {
        NanoAssert(ty == ARGTYPE_D);
        // Floating-point arguments are handled as two integer arguments.
        NanoAssert(false);
    }
}

void
Assembler::asm_stkarg(LIns* arg, int stkd)
{
    // The ABI doesn't allow accesses below the SP.
    NanoAssert(stkd >= 0);
    // The argument resides somewhere in registers, so we simply need to
    // push it onto the stack.
    if (arg->isI()) {
        Register rt = findRegFor(arg, GpRegs);
        asm_str(rt, SP, stkd);
    } else {
        // According to the comments in asm_arg_64, LIR_ii2d
        // can have a 64-bit argument even if VFP is disabled. However,
        // asm_arg_64 will split the argument and issue two 32-bit
        // arguments to asm_stkarg so we can ignore that case here.
        NanoAssert(arg->isD());
        NanoAssert(ARM_VFP);
        Register dt = findRegFor(arg, FpRegs);
        // EABI requires that 64-bit arguments are 64-bit aligned.
        NanoAssert((stkd % 8) == 0);
        FSTD(dt, SP, stkd);
    }
}

void
Assembler::asm_call(LIns* ins)
{
    if (ARM_VFP && ins->isop(LIR_calld)) {
        /* Because ARM actually returns the result in (R0,R1), and not in a
         * floating point register, the code to move the result into a correct
         * register is below.  We do nothing here.
         *
         * The reason being that if we did something here, the final code
         * sequence we'd get would be something like:
         *     MOV {R0-R3},params        [from below]
         *     BL function               [from below]
         *     MOV {R0-R3},spilled data  [from evictScratchRegsExcept()]
         *     MOV Dx,{R0,R1}            [from here]
         * which is clearly broken.
         *
         * This is not a problem for non-floating point calls, because the
         * restoring of spilled data into R0 is done via a call to
         * prepareResultReg(R0) in the other branch of this if-then-else,
         * meaning that evictScratchRegsExcept() will not modify R0. However,
         * prepareResultReg is not aware of the concept of using a register
         * pair (R0,R1) for the result of a single operation, so it can only be
         * used here with the ultimate VFP register, and not R0/R1, which
         * potentially allows for R0/R1 to get corrupted as described.
         */
#ifdef NJ_ARM_EABI_HARD_FLOAT
        /* With ARM hardware floating point ABI, D0 is used to return the double
         * from the function. We need to prepare it like we do for R0 in the else
         * branch.
         */
        prepareResultReg(ins, rmask(D0));
        freeResourcesOf(ins);
#endif
    } else if (!ins->isop(LIR_callv)) {
        prepareResultReg(ins, rmask(retRegs[0]));
        // Immediately free the resources as we need to re-use the register for
        // the arguments.
        freeResourcesOf(ins);
    }

    // Do this after we've handled the call result, so we don't
    // force the call result to be spilled unnecessarily.

    evictScratchRegsExcept(0);

    const CallInfo* ci = ins->callInfo();
    ArgType argTypes[MAXARGS];
    uint32_t argc = ci->getArgTypes(argTypes);
    bool indirect = ci->isIndirect();

    // If we aren't using VFP, assert that the LIR operation is an integer
    // function call.
    NanoAssert(ARM_VFP || ins->isop(LIR_callv) || ins->isop(LIR_calli));

    // If we're using VFP, but not hardware floating point ABI, and
    // the return type is a double, it'll come back in R0/R1.
    // We need to either place it in the result fp reg, or store it.
    // See comments above for more details as to why this is necessary here
    // for floating point calls, but not for integer calls.
    if (!ARM_EABI_HARD && ARM_VFP && ins->isExtant()) {
        // If the result size is a floating-point value, treat the result
        // specially, as described previously.
        if (ci->returnType() == ARGTYPE_D) {
            NanoAssert(ins->isop(LIR_calld));

            if (ins->isInReg()) {
                Register dd = ins->getReg();
                // Copy the result to the (VFP) result register.
                FMDRR(dd, R0, R1);
            } else {
                int d = findMemFor(ins);
                // Immediately free the resources so the arguments can re-use
                // the slot.
                freeResourcesOf(ins);

                // The result doesn't have a register allocated, so store the
                // result (in R0,R1) directly to its stack slot.
                asm_str(R0, FP, d+0);
                asm_str(R1, FP, d+4);
            }
        }
    }

    // Emit the branch.
    if (!indirect) {
        verbose_only(if (_logc->lcbits & LC_Native)
            outputf("        %p:", _nIns);
        )

        BranchWithLink((NIns*)ci->_address);
    } else {
        // Indirect call: we assign the address arg to LR
        if (ARM_ARCH_AT_LEAST(5)) {
            BLX(LR);
        } else {
            underrunProtect(12);
            BX(IP);
            MOV(LR, PC);
            MOV(IP, LR);
        }
        asm_regarg(ARGTYPE_I, ins->arg(--argc), LR);
    }

    // Encode the arguments, starting at R0 and with an empty argument stack (0).
    // With hardware fp ABI, floating point arguments start from D0.
    ParameterRegisters params = init_params(0, R0, D0);

    // Iterate through the argument list and encode each argument according to
    // the ABI.
    // Note that we loop through the arguments backwards as LIR specifies them
    // in reverse order.
    uint32_t    i = argc;
    while(i--) {
        asm_arg(argTypes[i], ins->arg(i), params);
    }

    if (params.stkd > max_out_args) {
        max_out_args = params.stkd;
    }
}

Register
Assembler::nRegisterAllocFromSet(RegisterMask set)
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
    a.free =
        rmask(R0) | rmask(R1) | rmask(R2) | rmask(R3) | rmask(R4) |
        rmask(R5) | rmask(R6) | rmask(R7) | rmask(R8) | rmask(R9) |
        rmask(R10) | rmask(LR);
    if (ARM_VFP) {
        a.free |=
            rmask(D0) | rmask(D1) | rmask(D2) | rmask(D3) |
            rmask(D4) | rmask(D5) | rmask(D6) | rmask(D7);
    }
}

static inline ConditionCode
get_cc(NIns *ins)
{
    return ConditionCode((*ins >> 28) & 0xF);
}

static inline bool
branch_is_B(NIns* branch)
{
    return (*branch & 0x0E000000) == 0x0A000000;
}

static inline bool
branch_is_LDR_PC(NIns* branch)
{
    return (*branch & 0x0F7FF000) == 0x051FF000;
}

// Is this an instruction of the form  ldr/str reg, [fp, #-imm] ?
static inline bool
is_ldstr_reg_fp_minus_imm(/*OUT*/uint32_t* isLoad, /*OUT*/uint32_t* rX,
                          /*OUT*/uint32_t* immX, NIns i1)
{
    if ((i1 & 0xFFEF0000) != 0xE50B0000)
        return false;
    *isLoad = (i1 >> 20) & 1;
    *rX     = (i1 >> 12) & 0xF;
    *immX   = i1 & 0xFFF;
    return true;
}

// Is this an instruction of the form  ldmdb/stmdb fp, regset ?
static inline bool
is_ldstmdb_fp(/*OUT*/uint32_t* isLoad, /*OUT*/uint32_t* regSet, NIns i1)
{
    if ((i1 & 0xFFEF0000) != 0xE90B0000)
        return false;
    *isLoad = (i1 >> 20) & 1;
    *regSet = i1 & 0xFFFF;
    return true;
}

// Make an instruction of the form ldmdb/stmdb fp, regset
static inline NIns
mk_ldstmdb_fp(uint32_t isLoad, uint32_t regSet)
{
    return 0xE90B0000 | (regSet & 0xFFFF) | ((isLoad & 1) << 20);
}

// Compute the number of 1 bits in the lowest 16 bits of regSet
static inline uint32_t
size_of_regSet(uint32_t regSet)
{
   uint32_t x = regSet;
   x = (x & 0x5555) + ((x >> 1) & 0x5555);
   x = (x & 0x3333) + ((x >> 2) & 0x3333);
   x = (x & 0x0F0F) + ((x >> 4) & 0x0F0F);
   x = (x & 0x00FF) + ((x >> 8) & 0x00FF);
   return x;
}

// See if two ARM instructions, i1 and i2, can be combined into one
static bool
do_peep_2_1(/*OUT*/NIns* merged, NIns i1, NIns i2)
{
    uint32_t rX, rY, immX, immY, isLoadX, isLoadY, regSet;
    /*   ld/str rX, [fp, #-8]
         ld/str rY, [fp, #-4]
         ==>
         ld/stmdb fp, {rX, rY}
         when
         X < Y and X != fp and Y != fp and X != 15 and Y != 15
    */
    if (is_ldstr_reg_fp_minus_imm(&isLoadX, &rX, &immX, i1) &&
        is_ldstr_reg_fp_minus_imm(&isLoadY, &rY, &immY, i2) &&
        immX == 8 && immY == 4 && rX < rY &&
        isLoadX == isLoadY &&
        rX != FP && rY != FP &&
         rX != 15 && rY != 15) {
        *merged = mk_ldstmdb_fp(isLoadX, (1 << rX) | (1<<rY));
        return true;
    }
    /*   ld/str   rX, [fp, #-N]
         ld/stmdb fp, regset
         ==>
         ld/stmdb fp, union(regset,{rX})
         when
         regset is nonempty
         X < all elements of regset
         N == 4 * (1 + card(regset))
         X != fp and X != 15
    */
    if (is_ldstr_reg_fp_minus_imm(&isLoadX, &rX, &immX, i1) &&
        is_ldstmdb_fp(&isLoadY, &regSet, i2) &&
        regSet != 0 &&
        (regSet & ((1 << (rX + 1)) - 1)) == 0 &&
        immX == 4 * (1 + size_of_regSet(regSet)) &&
        isLoadX == isLoadY &&
        rX != FP && rX != 15) {
        *merged = mk_ldstmdb_fp(isLoadX, regSet | (1 << rX));
        return true;
    }
    return false;
}

// Determine whether or not it's safe to look at _nIns[1].
// Necessary condition for safe peepholing with do_peep_2_1.
static inline bool
does_next_instruction_exist(NIns* _nIns, NIns* codeStart, NIns* codeEnd,
                            NIns* exitStart, NIns* exitEnd)
{
    return (exitStart <= _nIns && _nIns+1 < exitEnd) ||
           (codeStart <= _nIns && _nIns+1 < codeEnd);
}

void
Assembler::nPatchBranch(NIns* branch, NIns* target)
{
    // Patch the jump in a loop

    //
    // There are two feasible cases here, the first of which has 2 sub-cases:
    //
    //   (1) We are patching a patchable unconditional jump emitted by
    //       JMP_far.  All possible encodings we may be looking at with
    //       involve 2 words, though we *may* have to change from 1 word to
    //       2 or vice verse.
    //
    //          1a:  B ±32MB ; BKPT
    //          1b:  LDR PC [PC, #-4] ; $imm
    //
    //   (2) We are patching a patchable conditional jump emitted by
    //       B_cond_chk.  Short conditional jumps are non-patchable, so we
    //       won't have one here; will only ever have an instruction of the
    //       following form:
    //
    //          LDRcc PC [PC, #lit] ...
    //
    //       We don't actually know whether the lit-address is in the
    //       constant pool or in-line of the instruction stream, following
    //       the insn (with a jump over it) and we don't need to. For our
    //       purposes here, cases 2, 3 and 4 all look the same.
    //
    // For purposes of handling our patching task, we group cases 1b and 2
    // together, and handle case 1a on its own as it might require expanding
    // from a short-jump to a long-jump.
    //
    // We do not handle contracting from a long-jump to a short-jump, though
    // this is a possible future optimisation for case 1b. For now it seems
    // not worth the trouble.
    //

    if (branch_is_B(branch)) {
        // Case 1a
        // A short B branch, must be unconditional.
        NanoAssert(get_cc(branch) == AL);

        int32_t offset = PC_OFFSET_FROM(target, branch);
        if (isS24(offset>>2)) {
            // We can preserve the existing form, just rewrite its offset.
            NIns cond = *branch & 0xF0000000;
            *branch = (NIns)( cond | (0xA<<24) | ((offset>>2) & 0xFFFFFF) );
        } else {
            // We need to expand the existing branch to a long jump.
            // make sure the next instruction is a dummy BKPT
            NanoAssert(*(branch+1) == BKPT_insn);

            // Set the branch instruction to   LDRcc pc, [pc, #-4]
            NIns cond = *branch & 0xF0000000;
            *branch++ = (NIns)( cond | (0x51<<20) | (PC<<16) | (PC<<12) | (4));
            *branch++ = (NIns)target;
        }
    } else {
        // Case 1b & 2
        // Not a B branch, must be LDR, might be any kind of condition.
        NanoAssert(branch_is_LDR_PC(branch));

        NIns *addr = branch+2;
        int offset = (*branch & 0xFFF) / sizeof(NIns);
        if (*branch & (1<<23)) {
            addr += offset;
        } else {
            addr -= offset;
        }

        // Just redirect the jump target, leave the insn alone.
        *addr = (NIns) target;
    }
}

RegisterMask
Assembler::nHint(LIns* ins)
{
    NanoAssert(ins->isop(LIR_paramp));
    RegisterMask prefer = 0;
    if (ins->paramKind() == 0)
        if (ins->paramArg() < 4)
            prefer = rmask(argRegs[ins->paramArg()]);
    return prefer;
}

void
Assembler::asm_qjoin(LIns *ins)
{
    int d = findMemFor(ins);
    NanoAssert(d);
    LIns* lo = ins->oprnd1();
    LIns* hi = ins->oprnd2();

    Register rlo;
    Register rhi;

    findRegFor2(GpRegs, lo, rlo, GpRegs, hi, rhi);

    asm_str(rhi, FP, d+4);
    asm_str(rlo, FP, d);

    freeResourcesOf(ins);
}

void
Assembler::asm_store32(LOpcode op, LIns *value, int dr, LIns *base)
{
    Register ra, rb;
    getBaseReg2(GpRegs, value, ra, GpRegs, base, rb, dr);

    switch (op) {
        case LIR_sti:
            if (isU12(-dr) || isU12(dr)) {
                STR(ra, rb, dr);
            } else {
                STR(ra, IP, 0);
                asm_add_imm(IP, rb, dr);
            }
            return;
        case LIR_sti2c:
            if (isU12(-dr) || isU12(dr)) {
                STRB(ra, rb, dr);
            } else {
                STRB(ra, IP, 0);
                asm_add_imm(IP, rb, dr);
            }
            return;
        case LIR_sti2s:
            // Similar to the sti/stb case, but the max offset is smaller.
            if (isU8(-dr) || isU8(dr)) {
                STRH(ra, rb, dr);
            } else {
                STRH(ra, IP, 0);
                asm_add_imm(IP, rb, dr);
            }
            return;
        default:
            NanoAssertMsg(0, "asm_store32 should never receive this LIR opcode");
            return;
    }
}

bool
canRematALU(LIns *ins)
{
    // Return true if we can generate code for this instruction that neither
    // sets CCs, clobbers an input register, nor requires allocating a register.
    switch (ins->opcode()) {
    case LIR_addi:
    case LIR_subi:
    case LIR_andi:
    case LIR_ori:
    case LIR_xori:
        return ins->oprnd1()->isInReg() && ins->oprnd2()->isImmI();
    default:
        ;
    }
    return false;
}

bool
Assembler::canRemat(LIns* ins)
{
    return ins->isImmI() || ins->isop(LIR_allocp) || canRematALU(ins);
}

void
Assembler::asm_restore(LIns* i, Register r)
{
    // The following registers should never be restored:
    NanoAssert(r != PC);
    NanoAssert(r != IP);
    NanoAssert(r != SP);

    if (i->isop(LIR_allocp)) {
        int d = findMemFor(i);
        asm_add_imm(r, FP, d);
    } else if (i->isImmI()) {
        asm_ld_imm(r, i->immI());
    } else if (canRematALU(i)) {
        Register rn = i->oprnd1()->getReg();
        int32_t imm = i->oprnd2()->immI();
        switch (i->opcode()) {
        case LIR_addi: asm_add_imm(r, rn, imm, /*stat=*/ 0); break;
        case LIR_subi: asm_sub_imm(r, rn, imm, /*stat=*/ 0); break;
        case LIR_andi: asm_and_imm(r, rn, imm, /*stat=*/ 0); break;
        case LIR_ori:  asm_orr_imm(r, rn, imm, /*stat=*/ 0); break;
        case LIR_xori: asm_eor_imm(r, rn, imm, /*stat=*/ 0); break;
        default:       NanoAssert(0);                        break;
        }
    } else {
        // We can't easily load immediate values directly into FP registers, so
        // ensure that memory is allocated for the constant and load it from
        // memory.
        int d = findMemFor(i);
        if (ARM_VFP && IsFpReg(r)) {
            if (isU8(d/4) || isU8(-d/4)) {
                FLDD(r, FP, d);
            } else {
                FLDD(r, IP, d%1024);
                asm_add_imm(IP, FP, d-(d%1024));
            }
        } else {
            NIns merged;
            LDR(r, FP, d);
            // See if we can merge this load into an immediately following
            // one, by creating or extending an LDM instruction.
            if (/* is it safe to poke _nIns[1] ? */
                does_next_instruction_exist(_nIns, codeStart, codeEnd,
                                                   exitStart, exitEnd)
                && /* can we merge _nIns[0] into _nIns[1] ? */
                   do_peep_2_1(&merged, _nIns[0], _nIns[1])) {
                _nIns[1] = merged;
                _nIns++;
                verbose_only(
                    _nInsAfter++;
                    asm_output("merge next into LDMDB");
                )
            }
        }
    }
}

void
Assembler::asm_spill(Register rr, int d, bool quad)
{
    (void) quad;
    NanoAssert(d);
    // The following registers should never be spilled:
    NanoAssert(rr != PC);
    NanoAssert(rr != IP);
    NanoAssert(rr != SP);
    if (ARM_VFP && IsFpReg(rr)) {
        if (isU8(d/4) || isU8(-d/4)) {
            FSTD(rr, FP, d);
        } else {
            FSTD(rr, IP, d%1024);
            asm_add_imm(IP, FP, d-(d%1024));
        }
    } else {
        NIns merged;
        // asm_str always succeeds, but returns '1' to indicate that it emitted
        // a simple, easy-to-merge STR.
        if (asm_str(rr, FP, d)) {
            // See if we can merge this store into an immediately following one,
            // one, by creating or extending a STM instruction.
            if (/* is it safe to poke _nIns[1] ? */
                    does_next_instruction_exist(_nIns, codeStart, codeEnd,
                        exitStart, exitEnd)
                    && /* can we merge _nIns[0] into _nIns[1] ? */
                    do_peep_2_1(&merged, _nIns[0], _nIns[1])) {
                _nIns[1] = merged;
                _nIns++;
                verbose_only(
                    _nInsAfter++;
                    asm_output("merge next into STMDB");
                )
            }
        }
    }
}

void
Assembler::asm_load64(LIns* ins)
{
    NanoAssert(ins->isD());

    if (ARM_VFP) {
        Register    dd;
        LIns*       base = ins->oprnd1();
        Register    rn = findRegFor(base, GpRegs);
        int         offset = ins->disp();

        if (ins->isInReg()) {
            dd = prepareResultReg(ins, FpRegs & ~rmask(D0));
        } else {
            // If the result isn't already in a register, use the VFP scratch
            // register for the result and store it directly into memory.
            NanoAssert(ins->isInAr());
            int d = arDisp(ins);
            evictIfActive(D0);
            dd = D0;
            // VFP can only do loads and stores with a range of ±1020, so we
            // might need to do some arithmetic to extend its range.
            if (isU8(d/4) || isU8(-d/4)) {
                FSTD(dd, FP, d);
            } else {
                FSTD(dd, IP, d%1024);
                asm_add_imm(IP, FP, d-(d%1024));
            }
        }

        switch (ins->opcode()) {
            case LIR_ldd:
                if (isU8(offset/4) || isU8(-offset/4)) {
                    FLDD(dd, rn, offset);
                } else {
                    FLDD(dd, IP, offset%1024);
                    asm_add_imm(IP, rn, offset-(offset%1024));
                }
                break;
            case LIR_ldf2d:
                evictIfActive(D0);
                FCVTDS(dd, S0);
                if (isU8(offset/4) || isU8(-offset/4)) {
                    FLDS(S0, rn, offset);
                } else {
                    FLDS(S0, IP, offset%1024);
                    asm_add_imm(IP, rn, offset-(offset%1024));
                }
                break;
            default:
                NanoAssertMsg(0, "LIR opcode unsupported by asm_load64.");
                break;
        }
    } else {
        NanoAssert(ins->isInAr());
        int         d = arDisp(ins);

        LIns*       base = ins->oprnd1();
        Register    rn = findRegFor(base, GpRegs);
        int         offset = ins->disp();

        switch (ins->opcode()) {
            case LIR_ldd:
                asm_mmq(FP, d, rn, offset);
                break;
            case LIR_ldf2d:
                NanoAssertMsg(0, "LIR_ldf2d is not yet implemented for soft-float.");
                break;
            default:
                NanoAssertMsg(0, "LIR opcode unsupported by asm_load64.");
                break;
        }
    }

    freeResourcesOf(ins);
}

void
Assembler::asm_store64(LOpcode op, LIns* value, int dr, LIns* base)
{
    NanoAssert(value->isD());

    if (ARM_VFP) {
        Register dd = findRegFor(value, FpRegs & ~rmask(D0));
        Register rn = findRegFor(base, GpRegs);

        switch (op) {
            case LIR_std:
                // VFP can only do stores with a range of ±1020, so we might
                // need to do some arithmetic to extend its range.
                if (isU8(dr/4) || isU8(-dr/4)) {
                    FSTD(dd, rn, dr);
                } else {
                    FSTD(dd, IP, dr%1024);
                    asm_add_imm(IP, rn, dr-(dr%1024));
                }

                break;
            case LIR_std2f:
                // VFP can only do stores with a range of ±1020, so we might
                // need to do some arithmetic to extend its range.
                evictIfActive(D0);
                if (isU8(dr/4) || isU8(-dr/4)) {
                    FSTS(S0, rn, dr);
                } else {
                    FSTS(S0, IP, dr%1024);
                    asm_add_imm(IP, rn, dr-(dr%1024));
                }

                FCVTSD(S0, dd);

                break;
            default:
                NanoAssertMsg(0, "LIR opcode unsupported by asm_store64.");
                break;
        }
    } else {
        int         d = findMemFor(value);
        Register    rn = findRegFor(base, GpRegs);

        switch (op) {
            case LIR_std:
                // Doubles in soft-float never get registers allocated, so this
                // is always a simple two-word memcpy.
                // *(uint64_t*)(rb+dr) = *(uint64_t*)(FP+da)
                asm_mmq(rn, dr, FP, d);
                break;
            case LIR_std2f:
                NanoAssertMsg(0, "TODO: Soft-float implementation of LIR_std2f.");
                break;
            default:
                NanoAssertMsg(0, "LIR opcode unsupported by asm_store64.");
                break;
        }
    }
}

// Load the float64 specified by immDhi:immDlo into VFP register dd.
void
Assembler::asm_immd_nochk(Register dd, int32_t immDlo, int32_t immDhi)
{
    // We're not going to use a slot, because it might be too far
    // away.  Instead, we're going to stick a branch in the stream to
    // jump over the constants, and then load from a short PC relative
    // offset.

    // stream should look like:
    //    branch A
    //    immDlo
    //    immDhi
    // A: FLDD PC-16

    FLDD(dd, PC, -16);

    *(--_nIns) = (NIns) immDhi;
    *(--_nIns) = (NIns) immDlo;

    B_nochk(_nIns+2);
}

void
Assembler::asm_immd(LIns* ins)
{
    // If the value isn't in a register, it's simplest to use integer
    // instructions to put the value in its stack slot. Otherwise, use a VFP
    // load to get the value from a literal pool.
    if (ARM_VFP && ins->isInReg()) {
        Register dd = prepareResultReg(ins, FpRegs);
        underrunProtect(4*4);
        asm_immd_nochk(dd, ins->immDlo(), ins->immDhi());
    } else {
        NanoAssert(ins->isInAr());
        int d = arDisp(ins);
        asm_str(IP, FP, d+4);
        asm_ld_imm(IP, ins->immDhi());
        asm_str(IP, FP, d);
        asm_ld_imm(IP, ins->immDlo());
    }

    freeResourcesOf(ins);
}

void
Assembler::asm_nongp_copy(Register r, Register s)
{
    if (ARM_VFP && IsFpReg(r) && IsFpReg(s)) {
        // fp->fp
        FCPYD(r, s);
    } else {
        // We can't move a double-precision FP register into a 32-bit GP
        // register, so assert that no calling code is trying to do that.
        NanoAssert(0);
    }
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
    // called deprecated_freeRsrcOf, and allocating a register here may cause something
    // else to spill onto the stack which has just be conveniently freed by
    // deprecated_freeRsrcOf (resulting in stack corruption).
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
    //
    // Note that if rs+4 or rd+4 is outside the LDR or STR range, extra
    // instructions will be emitted as required to make the code work.

    // Ensure that the PC is not used as either base register. The instruction
    // generation macros call underrunProtect, and a side effect of this is
    // that we may be pushed onto another page, so the PC is not a reliable
    // base register.
    NanoAssert(rs != PC);
    NanoAssert(rd != PC);

    // We use IP as a swap register, so check that it isn't used for something
    // else by the caller.
    NanoAssert(rs != IP);
    NanoAssert(rd != IP);

    // Find the list of free registers from the allocator's free list and the
    // GpRegs mask. This excludes any floating-point registers that may be on
    // the free list.
    RegisterMask    free = _allocator.free & AllowableFlagRegs;

    // Ensure that ds and dd are within the +/-4095 offset range of STR and
    // LDR. If either is out of range, adjust and modify rd or rs so that the
    // load works correctly.
    // The modification here is performed after the LDR/STR block (because code
    // is emitted backwards), so this one is the reverse operation.

    int32_t dd_adj = 0;
    int32_t ds_adj = 0;

    if ((dd+4) >= 0x1000) {
        dd_adj = ((dd+4) & ~0xfff);
    } else if (dd <= -0x1000) {
        dd_adj = -((-dd) & ~0xfff);
    }
    if ((ds+4) >= 0x1000) {
        ds_adj = ((ds+4) & ~0xfff);
    } else if (ds <= -0x1000) {
        ds_adj = -((-ds) & ~0xfff);
    }

    // These will emit no code if d*_adj is 0.
    asm_sub_imm(rd, rd, dd_adj);
    asm_sub_imm(rs, rs, ds_adj);

    ds -= ds_adj;
    dd -= dd_adj;

    if (free) {
        // There is at least one register on the free list, so grab one for
        // temporary use. There is no need to allocate it explicitly because
        // we won't need it after this function returns.

        // The CountLeadingZeroes utility can be used to quickly find a set bit
        // in the free mask.
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

    // Re-adjust the base registers. (These will emit no code if d*_adj is 0.
    asm_add_imm(rd, rd, dd_adj);
    asm_add_imm(rs, rs, ds_adj);
}

// Increment the 32-bit profiling counter at pCtr, without
// changing any registers.
verbose_only(
void Assembler::asm_inc_m32(uint32_t* pCtr)
{
    // We need to temporarily free up two registers to do this, so
    // just push r0 and r1 on the stack.  This assumes that the area
    // at r13 - 8 .. r13 - 1 isn't being used for anything else at
    // this point (this is guaranteed by the EABI).
    //
    // Plan: emit the following bit of code.  It's not efficient, but
    // this is for profiling debug builds only, and is self contained,
    // except for above comment re stack use.
    //
    // E92D0003                 push    {r0,r1}
    // E59F0000                 ldr     r0, [r15]   ; pCtr
    // EA000000                 b       .+8         ; jump over imm
    // 12345678                 .word   0x12345678  ; pCtr
    // E5901000                 ldr     r1, [r0]
    // E2811001                 add     r1, r1, #1
    // E5801000                 str     r1, [r0]
    // E8BD0003                 pop     {r0,r1}

    // We need keep the 4 words beginning at "ldr r0, [r15]"
    // together.  Simplest to underrunProtect the whole thing.
    underrunProtect(8*4);
    IMM32(0xE8BD0003);       //  pop     {r0,r1}
    IMM32(0xE5801000);       //  str     r1, [r0]
    IMM32(0xE2811001);       //  add     r1, r1, #1
    IMM32(0xE5901000);       //  ldr     r1, [r0]
    IMM32((uint32_t)pCtr);   //  .word   pCtr
    IMM32(0xEA000000);       //  b       .+8
    IMM32(0xE59F0000);       //  ldr     r0, [r15]
    IMM32(0xE92D0003);       //  push    {r0,r1}
}
)

void
Assembler::nativePageReset()
{
    _nSlot = 0;
    _nExitSlot = 0;
}

void
Assembler::nativePageSetup()
{
    NanoAssert(!_inExit);
    if (!_nIns)
        codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes), NJ_MAX_CPOOL_OFFSET);

    // constpool starts at top of page and goes down,
    // code starts at bottom of page and moves up
    if (!_nSlot)
        _nSlot = codeStart;
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
        // This may be in a normal code chunk or an exit code chunk.
        codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes), NJ_MAX_CPOOL_OFFSET);

        _nSlot = codeStart;

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

        asm_output("bkpt");

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
    underrunProtect(8+LD32_size);

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

            // BL target
            *(--_nIns) = (NIns)( (COND_AL) | (0xB<<24) | (offs2) );
            asm_output("bl %p", (void*)addr);
            return;
        } else if (ARM_ARCH_AT_LEAST(5)) {
            // The target is Thumb, so emit a BLX (ARMv5+)
            // The (pre-shifted) value of the "H" bit in the BLX encoding.
            uint32_t    H = (offs & 0x2) << 23;

            // BLX addr
            *(--_nIns) = (NIns)( (0xF << 28) | (0x5<<25) | (H) | (offs2) );
            asm_output("blx %p", (void*)addr);
            return;
        }
        /* If we get here, it means we are on ARMv4T, and the target is Thumb,
           in which case we want to emit a branch with a register */
    }
    if (ARM_ARCH_AT_LEAST(5)) {
        // Load the target address into IP and branch to that. We've already
        // done underrunProtect, so we can skip that here.
        BLX(IP, false);
    } else {
        BX(IP);
        MOV(LR, PC);
    }
    // LDR IP, =addr
    asm_ld_imm(IP, (int32_t)addr, false);
}

// This is identical to BranchWithLink(NIns*) but emits a branch to an address
// held in a register rather than a literal address.
inline void
Assembler::BLX(Register addr, bool chk /* = true */)
{
    // We need to emit an ARMv5+ instruction, so assert that we have a suitable
    // processor. Note that we don't support ARMv4(T), but this serves as a
    // useful sanity check.
    NanoAssert(ARM_ARCH_AT_LEAST(5));

    NanoAssert(IsGpReg(addr));

    if (chk) {
        underrunProtect(4);
    }

    // BLX reg
    *(--_nIns) = (NIns)( (COND_AL) | (0x12<<20) | (0xFFF<<8) | (0x3<<4) | (addr) );
    asm_output("blx %s", gpn(addr));
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
    if (ARM_VFP && IsFpReg(d)) {
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

        NanoAssert(b != PC);
        NanoAssert(b != IP);

        if (chk) underrunProtect(4+LD32_size);

        *(--_nIns) = (NIns)( COND_AL | (0x79<<20) | (b<<16) | (d<<12) | IP );
        asm_ld_imm(IP, off, false);
    }

    asm_output("ldr %s, [%s, #%d]",gpn(d),gpn(b),(off));
}

// Emit a store, using a register base and an arbitrary immediate offset. This
// behaves like a STR instruction, but doesn't care about the offset range, and
// emits one of the following instruction sequences:
//
// ----
// STR  rt, [rr, #offset]
// ----
// asm_add_imm  ip, rr, #(offset & ~0xfff)
// STR  rt, [ip, #(offset & 0xfff)]
// ----
// # This one's fairly horrible, but should be rare.
// asm_add_imm  rr, rr, #(offset & ~0xfff)
// STR  rt, [ip, #(offset & 0xfff)]
// asm_sub_imm  rr, rr, #(offset & ~0xfff)
// ----
// SUB-based variants (for negative offsets) are also supported.
// ----
//
// The return value is 1 if a simple STR could be emitted, or 0 if the required
// sequence was more complex.
int32_t
Assembler::asm_str(Register rt, Register rr, int32_t offset)
{
    // We can't do PC-relative stores, and we can't store the PC value, because
    // we use macros (such as STR) which call underrunProtect, and this can
    // push _nIns to a new page, thus making any PC value impractical to
    // predict.
    NanoAssert(rr != PC);
    NanoAssert(rt != PC);
    if (offset >= 0) {
        // The offset is positive, so use ADD (and variants).
        if (isU12(offset)) {
            STR(rt, rr, offset);
            return 1;
        }

        if (rt != IP) {
            STR(rt, IP, offset & 0xfff);
            asm_add_imm(IP, rr, offset & ~0xfff);
        } else {
            int32_t adj = offset & ~0xfff;
            asm_sub_imm(rr, rr, adj);
            STR(rt, rr, offset-adj);
            asm_add_imm(rr, rr, adj);
        }
    } else {
        // The offset is negative, so use SUB (and variants).
        if (isU12(-offset)) {
            STR(rt, rr, offset);
            return 1;
        }

        if (rt != IP) {
            STR(rt, IP, -((-offset) & 0xfff));
            asm_sub_imm(IP, rr, (-offset) & ~0xfff);
        } else {
            int32_t adj = ((-offset) & ~0xfff);
            asm_add_imm(rr, rr, adj);
            STR(rt, rr, offset+adj);
            asm_sub_imm(rr, rr, adj);
        }
    }

    return 0;
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
    //
    // (Note that we use Thumb-2 if arm_arch is ARMv7 or later; the only earlier
    // ARM core that provided Thumb-2 is ARMv6T2/ARM1156, which is a real-time
    // core that nanojit is unlikely to ever target.)
    if (ARM_ARCH_AT_LEAST(7) && (d != PC)) {
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
    NanoAssert((isU12(-offset) || isU12(offset)) && (offset <= -8));

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
// NB: B_nochk depends on this not calling samepage() when _c == AL
void
Assembler::B_cond_chk(ConditionCode _c, NIns* _t, bool _chk)
{
    int32_t offs = PC_OFFSET_FROM(_t,_nIns-1);
    //nj_dprintf("B_cond_chk target: 0x%08x offset: %d @0x%08x\n", _t, offs, _nIns-1);

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
        asm_output("b%s %p", _c == AL ? "" : condNames[_c], (void*)(_t));
    } else if (_c == AL) {
        if(_chk) underrunProtect(8);
        *(--_nIns) = (NIns)(_t);
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | 0x4 );
        asm_output("b%s %p", _c == AL ? "" : condNames[_c], (void*)(_t));
    } else if (PC_OFFSET_FROM(_nSlot, _nIns-1) > -0x1000) {
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
        asm_output("b%s %p", _c == AL ? "" : condNames[_c], (void*)(_t));
    }
}

/*
 * VFP
 */

void
Assembler::asm_i2d(LIns* ins)
{
    Register dd = prepareResultReg(ins, FpRegs & ~rmask(D0));
    Register rt = findRegFor(ins->oprnd1(), GpRegs);

    evictIfActive(D0);
    FSITOD(dd, S0);
    FMSR(S0, rt);

    freeResourcesOf(ins);
}

void
Assembler::asm_ui2d(LIns* ins)
{
    Register dd = prepareResultReg(ins, FpRegs & ~rmask(D0));
    Register rt = findRegFor(ins->oprnd1(), GpRegs);

    evictIfActive(D0);
    FUITOD(dd, S0);
    FMSR(S0, rt);

    freeResourcesOf(ins);
}

void Assembler::asm_d2i(LIns* ins)
{
    evictIfActive(D0);
    if (ins->isInReg()) {
        Register rt = ins->getReg();
        FMRS(rt, S0);
    } else {
        // There's no active result register, so store the result directly into
        // memory to avoid the FP->GP transfer cost on Cortex-A8.
        int32_t d = arDisp(ins);
        // VFP can only do stores with a range of ±1020, so we might need to do
        // some arithmetic to extend its range.
        if (isU8(d/4) || isU8(-d/4)) {
            FSTS(S0, FP, d);
        } else {
            FSTS(S0, IP, d%1024);
            asm_add_imm(IP, FP, d-(d%1024));
        }
    }

    Register dm = findRegFor(ins->oprnd1(), FpRegs & ~rmask(D0));

    FTOSID(S0, dm);

    freeResourcesOf(ins);
}

void
Assembler::asm_fneg(LIns* ins)
{
    LIns* lhs = ins->oprnd1();

    Register dd = prepareResultReg(ins, FpRegs);
    // If the argument doesn't have a register assigned, re-use dd.
    Register dm = lhs->isInReg() ? lhs->getReg() : dd;

    FNEGD(dd, dm);

    freeResourcesOf(ins);
    if (dd == dm) {
        NanoAssert(!lhs->isInReg());
        findSpecificRegForUnallocated(lhs, dd);
    }
}

void
Assembler::asm_fop(LIns* ins)
{
    LIns*   lhs = ins->oprnd1();
    LIns*   rhs = ins->oprnd2();

    Register    dd = prepareResultReg(ins, FpRegs);
    // Try to re-use the result register for one of the arguments.
    Register    dn = lhs->isInReg() ? lhs->getReg() : dd;
    Register    dm = rhs->isInReg() ? rhs->getReg() : dd;
    if ((dn == dm) && (lhs != rhs)) {
        // We can't re-use the result register for both arguments, so force one
        // into its own register.
        dm = findRegFor(rhs, FpRegs & ~rmask(dd));
        NanoAssert(rhs->isInReg());
    }

    // TODO: Special cases for simple constants.

    switch(ins->opcode()) {
        case LIR_addd:      FADDD(dd,dn,dm);        break;
        case LIR_subd:      FSUBD(dd,dn,dm);        break;
        case LIR_muld:      FMULD(dd,dn,dm);        break;
        case LIR_divd:      FDIVD(dd,dn,dm);        break;
        default:            NanoAssert(0);          break;
    }

    freeResourcesOf(ins);

    // If we re-used the result register, mark it as active.
    if (dn == dd) {
        NanoAssert(!lhs->isInReg());
        findSpecificRegForUnallocated(lhs, dd);
    } else if (dm == dd) {
        NanoAssert(!rhs->isInReg());
        findSpecificRegForUnallocated(rhs, dd);
    } else {
        NanoAssert(lhs->isInReg());
        NanoAssert(rhs->isInReg());
    }
}

void
Assembler::asm_cmpd(LIns* ins)
{
    LIns* lhs = ins->oprnd1();
    LIns* rhs = ins->oprnd2();
    LOpcode op = ins->opcode();

    NanoAssert(ARM_VFP);
    NanoAssert(isCmpDOpcode(op));
    NanoAssert(lhs->isD() && rhs->isD());

    Register ra, rb;
    findRegFor2(FpRegs, lhs, ra, FpRegs, rhs, rb);

    int e_bit = (op != LIR_eqd);

    // Do the comparison and get results loaded in ARM status register.
    // TODO: For asm_condd, we should put the results directly into an ARM
    // machine register, then use bit operations to get the result.
    FMSTAT();
    FCMPD(ra, rb, e_bit);
}

/* Call this with targ set to 0 if the target is not yet known and the branch
 * will be patched up later.
 */
Branches
Assembler::asm_branch(bool branchOnFalse, LIns* cond, NIns* targ)
{
    LOpcode condop = cond->opcode();
    NanoAssert(cond->isCmp());
    NanoAssert(ARM_VFP || !isCmpDOpcode(condop));

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
        case LIR_eqd:   cc = EQ;    fp_cond = true;     break;
        case LIR_ltd:   cc = LO;    fp_cond = true;     break;
        case LIR_led:   cc = LS;    fp_cond = true;     break;
        case LIR_ged:   cc = GE;    fp_cond = true;     break;
        case LIR_gtd:   cc = GT;    fp_cond = true;     break;

        // Standard signed and unsigned integer comparisons.
        case LIR_eqi:   cc = EQ;    fp_cond = false;    break;
        case LIR_lti:   cc = LT;    fp_cond = false;    break;
        case LIR_lei:   cc = LE;    fp_cond = false;    break;
        case LIR_gti:   cc = GT;    fp_cond = false;    break;
        case LIR_gei:   cc = GE;    fp_cond = false;    break;
        case LIR_ltui:  cc = LO;    fp_cond = false;    break;
        case LIR_leui:  cc = LS;    fp_cond = false;    break;
        case LIR_gtui:  cc = HI;    fp_cond = false;    break;
        case LIR_geui:  cc = HS;    fp_cond = false;    break;

        // Default case for invalid or unexpected LIR instructions.
        default:        cc = AL;    fp_cond = false;    break;
    }

    // Invert the condition if required.
    if (branchOnFalse)
        cc = OppositeCond(cc);

    // Ensure that we got a sensible condition code.
    NanoAssert((cc != AL) && (cc != NV));

    // Ensure that we don't hit floating-point LIR codes if VFP is disabled.
    NanoAssert(ARM_VFP || !fp_cond);

    // Emit a suitable branch instruction.
    B_cond(cc, targ);

    // Store the address of the branch instruction so that we can return it.
    // asm_[f]cmp will move _nIns so we must do this now.
    NIns *at = _nIns;

    asm_cmp(cond);

    return Branches(at);
}

NIns* Assembler::asm_branch_ov(LOpcode op, NIns* target)
{
    // Because MUL can't set the V flag, we use SMULL and CMP to set the Z flag
    // to detect overflow on multiply. Thus, if we have a LIR_mulxovi, we must
    // be conditional on !Z, not V.
    ConditionCode cc = ( (op == LIR_mulxovi) || (op == LIR_muljovi) ? NE : VS );

    // Emit a suitable branch instruction.
    B_cond(cc, target);
    return _nIns;
}

void
Assembler::asm_cmp(LIns *cond)
{
    LIns* lhs = cond->oprnd1();
    LIns* rhs = cond->oprnd2();

    // Forward floating-point comparisons directly to asm_cmpd to simplify
    // logic in other methods which need to issue an implicit comparison, but
    // don't care about the details of comparison itself.
    if (lhs->isD()) {
        NanoAssert(rhs->isD());
        asm_cmpd(cond);
        return;
    }

    NanoAssert(lhs->isI() && rhs->isI());

    // ready to issue the compare
    if (rhs->isImmI()) {
        int c = rhs->immI();
        Register r = findRegFor(lhs, GpRegs);
        asm_cmpi(r, c);
    } else {
        Register ra, rb;
        findRegFor2(GpRegs, lhs, ra, GpRegs, rhs, rb);
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
            underrunProtect(4 + LD32_size);
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    } else {
        if (imm < 256) {
            ALUi(AL, cmp, 1, 0, r, imm);
        } else {
            underrunProtect(4 + LD32_size);
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    }
}

void
Assembler::asm_condd(LIns* ins)
{
    Register rd = prepareResultReg(ins, GpRegs);

    // TODO: Modify cmpd to allow the FP flags to move directly to an ARM
    // machine register, then use simple bit operations here rather than
    // conditional moves.

    switch (ins->opcode()) {
        case LIR_eqd:   SETEQ(rd);      break;
        case LIR_ltd:   SETLO(rd);      break; // Note: VFP LT/LE operations require
        case LIR_led:   SETLS(rd);      break; // unsigned LO/LS condition codes!
        case LIR_ged:   SETGE(rd);      break;
        case LIR_gtd:   SETGT(rd);      break;
        default:        NanoAssert(0);  break;
    }

    freeResourcesOf(ins);

    asm_cmpd(ins);
}

void
Assembler::asm_cond(LIns* ins)
{
    Register rd = prepareResultReg(ins, GpRegs);
    LOpcode op = ins->opcode();

    switch(op)
    {
        case LIR_eqi:   SETEQ(rd);      break;
        case LIR_lti:   SETLT(rd);      break;
        case LIR_lei:   SETLE(rd);      break;
        case LIR_gti:   SETGT(rd);      break;
        case LIR_gei:   SETGE(rd);      break;
        case LIR_ltui:  SETLO(rd);      break;
        case LIR_leui:  SETLS(rd);      break;
        case LIR_gtui:  SETHI(rd);      break;
        case LIR_geui:  SETHS(rd);      break;
        default:        NanoAssert(0);  break;
    }

    freeResourcesOf(ins);

    asm_cmp(ins);
}

void
Assembler::asm_arith(LIns* ins)
{
    LOpcode     op = ins->opcode();
    LIns*       lhs = ins->oprnd1();
    LIns*       rhs = ins->oprnd2();

    // We always need the result register and the first operand register, so
    // find them up-front. (If the second operand is constant it is encoded
    // differently.)
    Register    rd = prepareResultReg(ins, GpRegs);

    // Try to re-use the result register for operand 1.
    Register    rn = lhs->isInReg() ? lhs->getReg() : rd;

    // If the rhs is constant, we can use the instruction-specific code to
    // determine if the value can be encoded in an ARM instruction. If the
    // value cannot be encoded, it will be loaded into a register.
    //
    // Note that the MUL instruction can never take an immediate argument so
    // even if the argument is constant, we must allocate a register for it.
    if (rhs->isImmI() && (op != LIR_muli) && (op != LIR_mulxovi) && (op != LIR_muljovi))
    {
        int32_t immI = rhs->immI();

        switch (op)
        {
            case LIR_addi:       asm_add_imm(rd, rn, immI);     break;
            case LIR_addjovi:
            case LIR_addxovi:    asm_add_imm(rd, rn, immI, 1);  break;
            case LIR_subi:       asm_sub_imm(rd, rn, immI);     break;
            case LIR_subjovi:
            case LIR_subxovi:    asm_sub_imm(rd, rn, immI, 1);  break;
            case LIR_andi:       asm_and_imm(rd, rn, immI);     break;
            case LIR_ori:        asm_orr_imm(rd, rn, immI);     break;
            case LIR_xori:       asm_eor_imm(rd, rn, immI);     break;
            case LIR_lshi:       LSLi(rd, rn, immI);            break;
            case LIR_rshi:       ASRi(rd, rn, immI);            break;
            case LIR_rshui:      LSRi(rd, rn, immI);            break;

            default:
                NanoAssertMsg(0, "Unsupported");
                break;
        }

        freeResourcesOf(ins);
        if (rd == rn) {
            // Mark the re-used register as active.
            NanoAssert(!lhs->isInReg());
            findSpecificRegForUnallocated(lhs, rd);
        }
        return;
    }

    // The rhs is either already in a register or cannot be encoded as an
    // Operand 2 constant for this operation.

    Register    rm = rhs->isInReg() ? rhs->getReg() : rd;
    if ((rm == rn) && (lhs != rhs)) {
        // We can't re-use the result register for both arguments, so force one
        // into its own register. We favour re-use for operand 2 (rm) here as
        // it is more likely to take a fast path for LIR_mul on ARMv5.
        rn = findRegFor(lhs, GpRegs & ~rmask(rd));
        NanoAssert(lhs->isInReg());
    }

    switch (op)
    {
        case LIR_addi:       ADDs(rd, rn, rm, 0);    break;
        case LIR_addjovi:
        case LIR_addxovi:    ADDs(rd, rn, rm, 1);    break;
        case LIR_subi:       SUBs(rd, rn, rm, 0);    break;
        case LIR_subjovi:
        case LIR_subxovi:    SUBs(rd, rn, rm, 1);    break;
        case LIR_andi:       ANDs(rd, rn, rm, 0);    break;
        case LIR_ori:        ORRs(rd, rn, rm, 0);    break;
        case LIR_xori:       EORs(rd, rn, rm, 0);    break;

        case LIR_muli:
            if (!ARM_ARCH_AT_LEAST(6) && (rd == rn)) {
                // ARMv4 and ARMv5 cannot handle a MUL where rd == rn, so
                // explicitly assign a new register to rn.
                NanoAssert(!lhs->isInReg());
                rn = findRegFor(lhs, GpRegs & ~rmask(rd) & ~rmask(rm));
                if (lhs == rhs) {
                    rm = rn;
                }
            }
            MUL(rd, rn, rm);
            break;
        case LIR_muljovi:
        case LIR_mulxovi:
            if (!ARM_ARCH_AT_LEAST(6) && (rd == rn)) {
                // ARMv5 (and earlier) cannot handle a MUL where rd == rn, so
                // if that is the case, explicitly assign a new register to rn.
                NanoAssert(!lhs->isInReg());
                rn = findRegFor(lhs, GpRegs & ~rmask(rd) & ~rmask(rm));
                if (lhs == rhs) {
                    rm = rn;
                }
            }
            // ARM cannot automatically detect overflow from a MUL operation,
            // so we have to perform some other arithmetic:
            //   SMULL  rr, ip, ra, rb
            //   CMP    ip, rr, ASR #31
            // An explanation can be found in bug 521161. This sets Z if we did
            // _not_ overflow, and clears it if we did.
            ALUr_shi(AL, cmp, 1, SBZ, IP, rd, ASR_imm, 31);
            SMULL(rd, IP, rn, rm);
            break;

        // The shift operations need a mask to match the JavaScript
        // specification because the ARM architecture allows a greater shift
        // range than JavaScript.
        case LIR_lshi:
            LSL(rd, rn, IP);
            ANDi(IP, rm, 0x1f);
            break;
        case LIR_rshi:
            ASR(rd, rn, IP);
            ANDi(IP, rm, 0x1f);
            break;
        case LIR_rshui:
            LSR(rd, rn, IP);
            ANDi(IP, rm, 0x1f);
            break;
        default:
            NanoAssertMsg(0, "Unsupported");
            break;
    }

    freeResourcesOf(ins);
    // If we re-used the result register, mark it as active.
    if (rn == rd) {
        NanoAssert(!lhs->isInReg());
        findSpecificRegForUnallocated(lhs, rd);
    } else if (rm == rd) {
        NanoAssert(!rhs->isInReg());
        findSpecificRegForUnallocated(rhs, rd);
    } else {
        NanoAssert(lhs->isInReg());
        NanoAssert(rhs->isInReg());
    }
}

void
Assembler::asm_neg_not(LIns* ins)
{
    LIns* lhs = ins->oprnd1();
    Register rr = prepareResultReg(ins, GpRegs);

    // If 'lhs' isn't in a register, we can give it the result register.
    Register ra = lhs->isInReg() ? lhs->getReg() : rr;

    if (ins->isop(LIR_noti)) {
        MVN(rr, ra);
    } else {
        NanoAssert(ins->isop(LIR_negi));
        RSBS(rr, ra);
    }

    freeResourcesOf(ins);
    if (!lhs->isInReg()) {
        NanoAssert(ra == rr);
        // Update the register state to indicate that we've claimed ra for lhs.
        findSpecificRegForUnallocated(lhs, ra);
    }
}

void
Assembler::asm_load32(LIns* ins)
{
    LOpcode op = ins->opcode();
    LIns*   base = ins->oprnd1();
    int     d = ins->disp();

    Register rt = prepareResultReg(ins, GpRegs);
    // Try to re-use the result register for the base pointer.
    Register rn = base->isInReg() ? base->getReg() : rt;

    // TODO: The x86 back-end has a special case where the base address is
    // given by LIR_addp. The same technique may be useful here to take
    // advantage of ARM's register+register addressing mode.

    switch (op) {
        case LIR_lduc2ui:
            if (isU12(-d) || isU12(d)) {
                LDRB(rt, rn, d);
            } else {
                LDRB(rt, IP, d%4096);
                asm_add_imm(IP, rn, d-(d%4096));
            }
            break;
        case LIR_ldus2ui:
            // Some ARM machines require 2-byte alignment here.
            // Similar to the lduc2ui case, but the max offset is smaller.
            if (isU8(-d) || isU8(d)) {
                LDRH(rt, rn, d);
            } else {
                LDRH(rt, IP, d%256);
                asm_add_imm(IP, rn, d-(d%256));
            }
            break;
        case LIR_ldi:
            // Some ARM machines require 4-byte alignment here.
            if (isU12(-d) || isU12(d)) {
                LDR(rt, rn, d);
            } else {
                LDR(rt, IP, d%4096);
                asm_add_imm(IP, rn, d-(d%4096));
            }
            break;
        case LIR_ldc2i:
            // Like LIR_lduc2ui, but sign-extend.
            // Some ARM machines require 2-byte alignment here.
            if (isU8(-d) || isU8(d)) {
                LDRSB(rt, rn, d);
            } else {
                LDRSB(rn, IP, d%256);
                asm_add_imm(IP, rn, d-(d%256));
            }
            break;
        case LIR_lds2i:
            // Like LIR_ldus2ui, but sign-extend.
            if (isU8(-d) || isU8(d)) {
                LDRSH(rt, rn, d);
            } else {
                LDRSH(rt, IP, d%256);
                asm_add_imm(IP, rn, d-(d%256));
            }
            break;
        default:
            NanoAssertMsg(0, "asm_load32 should never receive this LIR opcode");
            break;
    }

    freeResourcesOf(ins);

    if (rn == rt) {
        NanoAssert(!base->isInReg());
        findSpecificRegForUnallocated(base, rn);
    }
}

void
Assembler::asm_cmov(LIns* ins)
{
    LIns*           condval = ins->oprnd1();
    LIns*           iftrue  = ins->oprnd2();
    LIns*           iffalse = ins->oprnd3();
    RegisterMask    allow = ins->isD() ? FpRegs : GpRegs;
    ConditionCode   cc;

    NanoAssert(condval->isCmp());
    NanoAssert((ins->isop(LIR_cmovi) && iftrue->isI() && iffalse->isI()) ||
               (ins->isop(LIR_cmovd) && iftrue->isD() && iffalse->isD()));

    Register rd = prepareResultReg(ins, allow);

    // Try to re-use the result register for one of the arguments.
    Register rt = iftrue->isInReg() ? iftrue->getReg() : rd;
    Register rf = iffalse->isInReg() ? iffalse->getReg() : rd;
    // Note that iftrue and iffalse may actually be the same, though it
    // shouldn't happen with the LIR optimizers turned on.
    if ((rt == rf) && (iftrue != iffalse)) {
        // We can't re-use the result register for both arguments, so force one
        // into its own register.
        rf = findRegFor(iffalse, allow & ~rmask(rd));
        NanoAssert(iffalse->isInReg());
    }

    switch(condval->opcode()) {
        default:        NanoAssert(0);
        // Integer comparisons.
        case LIR_eqi:   cc = EQ;        break;
        case LIR_lti:   cc = LT;        break;
        case LIR_lei:   cc = LE;        break;
        case LIR_gti:   cc = GT;        break;
        case LIR_gei:   cc = GE;        break;
        case LIR_ltui:  cc = LO;        break;
        case LIR_leui:  cc = LS;        break;
        case LIR_gtui:  cc = HI;        break;
        case LIR_geui:  cc = HS;        break;
        // VFP comparisons.
        case LIR_eqd:   cc = EQ;        break;
        case LIR_ltd:   cc = LO;        break;
        case LIR_led:   cc = LS;        break;
        case LIR_ged:   cc = GE;        break;
        case LIR_gtd:   cc = GT;        break;
    }

    // Emit something like this:
    //      CMP         [...]
    //      MOV(CC)     rd, rf
    //      MOV(!CC)    rd, rt
    // If the destination was re-used for an input, the corresponding MOV will
    // be omitted as it will be redundant.
    if (ins->isI()) {
        if (rd != rf) {
            MOV_cond(OppositeCond(cc), rd, rf);
        }
        if (rd != rt) {
            MOV_cond(cc, rd, rt);
        }
    } else if (ins->isD()) {
        // The VFP sequence is similar to the integer sequence, but uses a
        // VFP instruction in place of MOV.
        NanoAssert(ARM_VFP);
        if (rd != rf) {
            FCPYD_cond(OppositeCond(cc), rd, rf);
        }
        if (rd != rt) {
            FCPYD_cond(cc, rd, rt);
        }
    } else {
        NanoAssert(0);
    }

    freeResourcesOf(ins);

    // If we re-used the result register, mark it as active for either iftrue
    // or iffalse (or both in the corner-case where they're the same).
    if (rt == rd) {
        NanoAssert(!iftrue->isInReg());
        findSpecificRegForUnallocated(iftrue, rd);
    } else if (rf == rd) {
        NanoAssert(!iffalse->isInReg());
        findSpecificRegForUnallocated(iffalse, rd);
    } else {
        NanoAssert(iffalse->isInReg());
        NanoAssert(iftrue->isInReg());
    }

    asm_cmp(condval);
}

void
Assembler::asm_qhi(LIns* ins)
{
    Register rd = prepareResultReg(ins, GpRegs);
    LIns *lhs = ins->oprnd1();
    int d = findMemFor(lhs);

    LDR(rd, FP, d+4);

    freeResourcesOf(ins);
}

void
Assembler::asm_qlo(LIns* ins)
{
    Register rd = prepareResultReg(ins, GpRegs);
    LIns *lhs = ins->oprnd1();
    int d = findMemFor(lhs);

    LDR(rd, FP, d);

    freeResourcesOf(ins);
}

void
Assembler::asm_param(LIns* ins)
{
    uint32_t a = ins->paramArg();
    uint32_t kind = ins->paramKind();
    if (kind == 0) {
        // Ordinary parameter. These are always (32-bit-)word-sized, and will
        // be in the first four registers (argRegs) and then on the stack.
        if (a < 4) {
            // Register argument.
            prepareResultReg(ins, rmask(argRegs[a]));
        } else {
            // Stack argument.
            Register r = prepareResultReg(ins, GpRegs);
            int d = (a - 4) * sizeof(intptr_t) + 8;
            LDR(r, FP, d);
        }
    } else {
        // Saved parameter.
        NanoAssert(a < (sizeof(savedRegs)/sizeof(savedRegs[0])));
        prepareResultReg(ins, rmask(savedRegs[a]));
    }
    freeResourcesOf(ins);
}

void
Assembler::asm_immi(LIns* ins)
{
    Register rd = prepareResultReg(ins, GpRegs);
    asm_ld_imm(rd, ins->immI());
    freeResourcesOf(ins);
}

void
Assembler::asm_ret(LIns *ins)
{
    genEpilogue();

    // NB: our contract with genEpilogue is actually that the return value
    // we are intending for R0 is currently IP, not R0. This has to do with
    // the strange dual-nature of the patchable jump in a side-exit. See
    // nPatchBranch.
    //
    // With hardware floating point ABI we can skip this for retd.
    if (!(ARM_EABI_HARD && ins->isop(LIR_retd))) {
        MOV(IP, R0);
    }

    // Pop the stack frame.
    MOV(SP,FP);

    releaseRegisters();
    assignSavedRegs();
    LIns *value = ins->oprnd1();
    if (ins->isop(LIR_reti)) {
        findSpecificRegFor(value, R0);
    }
    else {
        NanoAssert(ins->isop(LIR_retd));
        if (ARM_VFP) {
#ifdef NJ_ARM_EABI_HARD_FLOAT
            findSpecificRegFor(value, D0);
#else
            Register reg = findRegFor(value, FpRegs);
            FMRRD(R0, R1, reg);
#endif
        } else {
            NanoAssert(value->isop(LIR_ii2d));
            findSpecificRegFor(value->oprnd1(), R0); // lo
            findSpecificRegFor(value->oprnd2(), R1); // hi
        }
    }
}

void
Assembler::asm_jtbl(LIns* ins, NIns** table)
{
    Register indexreg = findRegFor(ins->oprnd1(), GpRegs);
    Register tmp = registerAllocTmp(GpRegs & ~rmask(indexreg));
    LDR_scaled(PC, tmp, indexreg, 2);      // LDR PC, [tmp + index*4]
    asm_ld_imm(tmp, (int32_t)table);       // tmp = #table
}

void Assembler::swapCodeChunks() {
    if (!_nExitIns)
        codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes), NJ_MAX_CPOOL_OFFSET);
    if (!_nExitSlot)
        _nExitSlot = exitStart;
    SWAP(NIns*, _nIns, _nExitIns);
    SWAP(NIns*, _nSlot, _nExitSlot);        // this one is ARM-specific
    SWAP(NIns*, codeStart, exitStart);
    SWAP(NIns*, codeEnd, exitEnd);
    verbose_only( SWAP(size_t, codeBytes, exitBytes); )
}

void Assembler::asm_insert_random_nop() {
    NanoAssert(0); // not supported
}

}
#endif /* FEATURE_NANOJIT */
