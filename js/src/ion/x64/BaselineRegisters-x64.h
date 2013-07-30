/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_x64_BaselineRegisters_x64_h
#define ion_x64_BaselineRegisters_x64_h

#ifdef JS_ION

#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

static MOZ_CONSTEXPR_VAR Register BaselineFrameReg    = rbp;
static MOZ_CONSTEXPR_VAR Register BaselineStackReg    = rsp;

static MOZ_CONSTEXPR_VAR ValueOperand R0(rcx);
static MOZ_CONSTEXPR_VAR ValueOperand R1(rbx);
static MOZ_CONSTEXPR_VAR ValueOperand R2(rax);

// BaselineTailCallReg and BaselineStubReg reuse
// registers from R2.
static MOZ_CONSTEXPR_VAR Register BaselineTailCallReg = rsi;
static MOZ_CONSTEXPR_VAR Register BaselineStubReg     = rdi;

static MOZ_CONSTEXPR_VAR Register ExtractTemp0        = r14;
static MOZ_CONSTEXPR_VAR Register ExtractTemp1        = r15;

// FloatReg0 must be equal to ReturnFloatReg.
static MOZ_CONSTEXPR_VAR FloatRegister FloatReg0      = xmm0;
static MOZ_CONSTEXPR_VAR FloatRegister FloatReg1      = xmm1;

} // namespace ion
} // namespace js

#endif // JS_ION

#endif /* ion_x64_BaselineRegisters_x64_h */
