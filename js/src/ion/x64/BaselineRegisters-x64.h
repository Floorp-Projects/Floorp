/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_registers_x64_h__) && defined(JS_ION)
#define jsion_baseline_registers_x64_h__

#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

static const Register frameReg = rbp;
static const Register spReg = rsp;

static const ValueOperand R0(r12);
static const ValueOperand R1(r13);
static const ValueOperand R2(r14);

} // namespace ion
} // namespace js

#endif

