/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x64_LIR_x64_h
#define jit_x64_LIR_x64_h

namespace js {
namespace jit {

// Given an untyped input, guards on whether it's a specific type and returns
// the unboxed payload.
class LUnboxBase : public LInstructionHelper<1, 1, 0>
{
  public:
    explicit LUnboxBase(const LAllocation& input) {
        setOperand(0, input);
    }

    static const size_t Input = 0;

    MUnbox* mir() const {
        return mir_->toUnbox();
    }
};

class LUnbox : public LUnboxBase {
  public:
    LIR_HEADER(Unbox)

    explicit LUnbox(const LAllocation& input)
      : LUnboxBase(input)
    { }

    const char* extraName() const {
        return StringFromMIRType(mir()->type());
    }
};

class LUnboxFloatingPoint : public LUnboxBase {
    MIRType type_;

  public:
    LIR_HEADER(UnboxFloatingPoint)

    LUnboxFloatingPoint(const LAllocation& input, MIRType type)
      : LUnboxBase(input),
        type_(type)
    { }

    MIRType type() const {
        return type_;
    }
    const char* extraName() const {
        return StringFromMIRType(type_);
    }
};

// Convert a 32-bit unsigned integer to a double.
class LAsmJSUInt32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(AsmJSUInt32ToDouble)

    explicit LAsmJSUInt32ToDouble(const LAllocation& input) {
        setOperand(0, input);
    }
};

// Convert a 32-bit unsigned integer to a float32.
class LAsmJSUInt32ToFloat32 : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(AsmJSUInt32ToFloat32)

    explicit LAsmJSUInt32ToFloat32(const LAllocation& input) {
        setOperand(0, input);
    }
};

class LAsmJSLoadFuncPtr : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(AsmJSLoadFuncPtr);
    LAsmJSLoadFuncPtr(const LAllocation& index, const LDefinition& temp) {
        setOperand(0, index);
        setTemp(0, temp);
    }
    MAsmJSLoadFuncPtr* mir() const {
        return mir_->toAsmJSLoadFuncPtr();
    }
    const LAllocation* index() {
        return getOperand(0);
    }
    const LDefinition* temp() {
        return getTemp(0);
    }
};

// Math.random().
class LRandom : public LInstructionHelper<1, 0, 5>
{
  public:
    LIR_HEADER(Random)
    LRandom(const LDefinition &temp, const LDefinition &temp2, const LDefinition &temp3,
            const LDefinition &temp4, const LDefinition &temp5)
    {
        setTemp(0, temp);
        setTemp(1, temp2);
        setTemp(2, temp3);
        setTemp(3, temp4);
        setTemp(4, temp5);
    }
    const LDefinition* temp() {
        return getTemp(0);
    }
    const LDefinition* temp2() {
        return getTemp(1);
    }
    const LDefinition *temp3() {
        return getTemp(2);
    }
    const LDefinition *temp4() {
        return getTemp(3);
    }
    const LDefinition *temp5() {
        return getTemp(4);
    }

    MRandom* mir() const {
        return mir_->toRandom();
    }
};

} // namespace jit
} // namespace js

#endif /* jit_x64_LIR_x64_h */
