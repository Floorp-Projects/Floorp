/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_none_BaselineRegisters_none_h
#define jit_none_BaselineRegisters_none_h

#include "jit/IonMacroAssembler.h"

namespace js {
namespace jit {

static MOZ_CONSTEXPR_VAR Register BaselineFrameReg = { 0 };
static MOZ_CONSTEXPR_VAR Register BaselineStackReg = { 0 };

static MOZ_CONSTEXPR_VAR ValueOperand R0 = JSReturnOperand;
static MOZ_CONSTEXPR_VAR ValueOperand R1 = JSReturnOperand;
static MOZ_CONSTEXPR_VAR ValueOperand R2 = JSReturnOperand;

static MOZ_CONSTEXPR_VAR Register BaselineTailCallReg = { 0 };
static MOZ_CONSTEXPR_VAR Register BaselineStubReg = { 0 };

static MOZ_CONSTEXPR_VAR Register ExtractTemp0 = { 0 };
static MOZ_CONSTEXPR_VAR Register ExtractTemp1 = { 0 };

static MOZ_CONSTEXPR_VAR FloatRegister FloatReg0 = { 0 };
static MOZ_CONSTEXPR_VAR FloatRegister FloatReg1 = { 0 };

} // namespace jit
} // namespace js

#endif /* jit_none_BaselineRegisters_none_h */

