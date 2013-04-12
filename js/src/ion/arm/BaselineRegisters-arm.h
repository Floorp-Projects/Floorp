/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_registers_arm_h__) && defined(JS_ION)
#define jsion_baseline_registers_arm_h__

#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

// r15 = program-counter
// r14 = link-register

// r13 = stack-pointer
// r11 = frame-pointer
static const Register BaselineFrameReg = r11;
static const Register BaselineStackReg = sp;

// ValueOperands R0, R1, and R2.
// R0 == JSReturnReg, and R2 uses registers not
// preserved across calls.  R1 value should be
// preserved across calls.
static const ValueOperand R0(r3, r2);
static const ValueOperand R1(r5, r4);
static const ValueOperand R2(r1, r0);

// BaselineTailCallReg and BaselineStubReg
// These use registers that are not preserved across
// calls.
static const Register BaselineTailCallReg = r14;
static const Register BaselineStubReg     = r9;

static const Register ExtractTemp0        = InvalidReg;
static const Register ExtractTemp1        = InvalidReg;

// Register used internally by MacroAssemblerARM.
static const Register BaselineSecondScratchReg = r6;

// R7 - R9 are generally available for use within stubcode.

// Note that BaselineTailCallReg is actually just the link
// register.  In ARM code emission, we do not clobber BaselineTailCallReg
// since we keep the return address for calls there.

// FloatReg0 must be equal to ReturnFloatReg. d1 is ScratchFloatReg.
static const FloatRegister FloatReg0      = d0;
static const FloatRegister FloatReg1      = d2;

} // namespace ion
} // namespace js

#endif

