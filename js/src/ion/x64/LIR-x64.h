/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_lir_x64_h__
#define jsion_lir_x64_h__

#include "ion/TypeOracle.h"

namespace js {
namespace ion {

// Given a typed input, returns an untyped box.
class LBox : public LInstructionHelper<1, 1, 0>
{
    MIRType type_;

  public:
    LIR_HEADER(Box)

    LBox(MIRType type, const LAllocation &payload)
      : type_(type)
    {
        setOperand(0, payload);
    }

    MIRType type() const {
        return type_;
    }
};

// Given an untyped input, guards on whether it's a specific type and returns
// the unboxed payload.
class LUnboxBase : public LInstructionHelper<1, 1, 0>
{
  public:
    LUnboxBase(const LAllocation &input) {
        setOperand(0, input);
    }

    static const size_t Input = 0;

    MUnbox *mir() const {
        return mir_->toUnbox();
    }
};

class LUnbox : public LUnboxBase {
  public:
    LIR_HEADER(Unbox)

    LUnbox(const LAllocation &input)
      : LUnboxBase(input)
    { }
};

class LUnboxDouble : public LUnboxBase {
  public:
    LIR_HEADER(UnboxDouble)

    LUnboxDouble(const LAllocation &input)
      : LUnboxBase(input)
    { }
};

// Convert a 32-bit unsigned integer to a double.
class LUInt32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(UInt32ToDouble)

    LUInt32ToDouble(const LAllocation &input) {
        setOperand(0, input);
    }
};

class LAsmJSLoadFuncPtr : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(AsmJSLoadFuncPtr);
    LAsmJSLoadFuncPtr(const LAllocation &index, const LDefinition &temp) {
        setOperand(0, index);
        setTemp(0, temp);
    }
    MAsmJSLoadFuncPtr *mir() const {
        return mir_->toAsmJSLoadFuncPtr();
    }
    const LAllocation *index() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_lir_x64_h__

