/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips32_LIR_mips32_h
#define jit_mips32_LIR_mips32_h

namespace js {
namespace jit {

class LBoxFloatingPoint : public LInstructionHelper<2, 1, 1>
{
    MIRType type_;

  public:
    LIR_HEADER(BoxFloatingPoint);

    LBoxFloatingPoint(const LAllocation& in, const LDefinition& temp, MIRType type)
      : type_(type)
    {
        setOperand(0, in);
        setTemp(0, temp);
    }

    MIRType type() const {
        return type_;
    }
    const char* extraName() const {
        return StringFromMIRType(type_);
    }
};

class LUnbox : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(Unbox);

    MUnbox* mir() const {
        return mir_->toUnbox();
    }
    const LAllocation* payload() {
        return getOperand(0);
    }
    const LAllocation* type() {
        return getOperand(1);
    }
    const char* extraName() const {
        return StringFromMIRType(mir()->type());
    }
};

class LUnboxFloatingPoint : public LInstructionHelper<1, 2, 0>
{
    MIRType type_;

  public:
    LIR_HEADER(UnboxFloatingPoint);

    static const size_t Input = 0;

    LUnboxFloatingPoint(const LBoxAllocation& input, MIRType type)
      : type_(type)
    {
        setBoxOperand(Input, input);
    }

    MUnbox* mir() const {
        return mir_->toUnbox();
    }

    MIRType type() const {
        return type_;
    }
    const char* extraName() const {
        return StringFromMIRType(type_);
    }
};

class LDivOrModI64 : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES*2, 0>
{
  public:
    LIR_HEADER(DivOrModI64)

    static const size_t Lhs = 0;
    static const size_t Rhs = INT64_PIECES;

    LDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs)
    {
        setInt64Operand(Lhs, lhs);
        setInt64Operand(Rhs, rhs);
    }
    MBinaryArithInstruction* mir() const {
        MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
        return static_cast<MBinaryArithInstruction*>(mir_);
    }
    bool canBeDivideByZero() const {
        if (mir_->isMod())
            return mir_->toMod()->canBeDivideByZero();
        return mir_->toDiv()->canBeDivideByZero();
    }
    bool canBeNegativeOverflow() const {
        if (mir_->isMod())
            return mir_->toMod()->canBeNegativeDividend();
        return mir_->toDiv()->canBeNegativeOverflow();
    }
    wasm::TrapOffset trapOffset() const {
        MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
        if (mir_->isMod())
            return mir_->toMod()->trapOffset();
        return mir_->toDiv()->trapOffset();
    }
};

class LUDivOrModI64 : public LCallInstructionHelper<INT64_PIECES, INT64_PIECES*2, 0>
{
  public:
    LIR_HEADER(UDivOrModI64)

    static const size_t Lhs = 0;
    static const size_t Rhs = INT64_PIECES;

    LUDivOrModI64(const LInt64Allocation& lhs, const LInt64Allocation& rhs)
    {
        setInt64Operand(Lhs, lhs);
        setInt64Operand(Rhs, rhs);
    }
    MBinaryArithInstruction* mir() const {
        MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
        return static_cast<MBinaryArithInstruction*>(mir_);
    }
    bool canBeDivideByZero() const {
        if (mir_->isMod())
            return mir_->toMod()->canBeDivideByZero();
        return mir_->toDiv()->canBeDivideByZero();
    }
    bool canBeNegativeOverflow() const {
        if (mir_->isMod())
            return mir_->toMod()->canBeNegativeDividend();
        return mir_->toDiv()->canBeNegativeOverflow();
    }
    wasm::TrapOffset trapOffset() const {
        MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
        if (mir_->isMod())
            return mir_->toMod()->trapOffset();
        return mir_->toDiv()->trapOffset();
    }
};

class LWasmTruncateToInt64 : public LCallInstructionHelper<INT64_PIECES, 1, 0>
{
  public:
    LIR_HEADER(WasmTruncateToInt64);

    explicit LWasmTruncateToInt64(const LAllocation& in)
    {
        setOperand(0, in);
    }

    MWasmTruncateToInt64* mir() const {
        return mir_->toWasmTruncateToInt64();
    }
};

} // namespace jit
} // namespace js

#endif /* jit_mips32_LIR_mips32_h */
