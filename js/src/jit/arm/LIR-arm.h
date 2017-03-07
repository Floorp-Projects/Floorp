/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_LIR_arm_h
#define jit_arm_LIR_arm_h

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

// Convert a 32-bit unsigned integer to a double.
class LWasmUint32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(WasmUint32ToDouble)

    LWasmUint32ToDouble(const LAllocation& input) {
        setOperand(0, input);
    }
};

// Convert a 32-bit unsigned integer to a float32.
class LWasmUint32ToFloat32 : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(WasmUint32ToFloat32)

    LWasmUint32ToFloat32(const LAllocation& input) {
        setOperand(0, input);
    }
};

class LDivI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(DivI);

    LDivI(const LAllocation& lhs, const LAllocation& rhs,
          const LDefinition& temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }

    MDiv* mir() const {
        return mir_->toDiv();
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

// LSoftDivI is a software divide for ARM cores that don't support a hardware
// divide instruction.
//
// It is implemented as a proper C function so it trashes r0, r1, r2 and r3.
// The call also trashes lr, and has the ability to trash ip. The function also
// takes two arguments (dividend in r0, divisor in r1). The LInstruction gets
// encoded such that the divisor and dividend are passed in their apropriate
// registers and end their life at the start of the instruction by the use of
// useFixedAtStart. The result is returned in r0 and the other three registers
// that can be trashed are marked as temps. For the time being, the link
// register is not marked as trashed because we never allocate to the link
// register. The FP registers are not trashed.
class LSoftDivI : public LBinaryMath<3>
{
  public:
    LIR_HEADER(SoftDivI);

    LSoftDivI(const LAllocation& lhs, const LAllocation& rhs,
              const LDefinition& temp1, const LDefinition& temp2, const LDefinition& temp3) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }

    MDiv* mir() const {
        return mir_->toDiv();
    }
};

class LDivPowTwoI : public LInstructionHelper<1, 1, 0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(DivPowTwoI)

    LDivPowTwoI(const LAllocation& lhs, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
    }

    const LAllocation* numerator() {
        return getOperand(0);
    }

    int32_t shift() {
        return shift_;
    }

    MDiv* mir() const {
        return mir_->toDiv();
    }
};

class LModI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(ModI);

    LModI(const LAllocation& lhs, const LAllocation& rhs,
          const LDefinition& callTemp)
    {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, callTemp);
    }

    const LDefinition* callTemp() {
        return getTemp(0);
    }

    MMod* mir() const {
        return mir_->toMod();
    }
};

class LSoftModI : public LBinaryMath<4>
{
  public:
    LIR_HEADER(SoftModI);

    LSoftModI(const LAllocation& lhs, const LAllocation& rhs,
              const LDefinition& temp1, const LDefinition& temp2, const LDefinition& temp3,
              const LDefinition& callTemp)
    {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
        setTemp(3, callTemp);
    }

    const LDefinition* callTemp() {
        return getTemp(3);
    }

    MMod* mir() const {
        return mir_->toMod();
    }
};

class LModPowTwoI : public LInstructionHelper<1, 1, 0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(ModPowTwoI);
    int32_t shift()
    {
        return shift_;
    }

    LModPowTwoI(const LAllocation& lhs, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
    }

    MMod* mir() const {
        return mir_->toMod();
    }
};

class LModMaskI : public LInstructionHelper<1, 1, 2>
{
    const int32_t shift_;

  public:
    LIR_HEADER(ModMaskI);

    LModMaskI(const LAllocation& lhs, const LDefinition& temp1, const LDefinition& temp2,
              int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }

    int32_t shift() const {
        return shift_;
    }

    MMod* mir() const {
        return mir_->toMod();
    }
};

// Takes a tableswitch with an integer to decide.
class LTableSwitch : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(TableSwitch);

    LTableSwitch(const LAllocation& in, const LDefinition& inputCopy, MTableSwitch* ins) {
        setOperand(0, in);
        setTemp(0, inputCopy);
        setMir(ins);
    }

    MTableSwitch* mir() const {
        return mir_->toTableSwitch();
    }

    const LAllocation* index() {
        return getOperand(0);
    }
    const LDefinition* tempInt() {
        return getTemp(0);
    }
    // This is added to share the same CodeGenerator prefixes.
    const LDefinition* tempPointer() {
        return nullptr;
    }
};

// Takes a tableswitch with an integer to decide.
class LTableSwitchV : public LInstructionHelper<0, BOX_PIECES, 2>
{
  public:
    LIR_HEADER(TableSwitchV);

    LTableSwitchV(const LBoxAllocation& input, const LDefinition& inputCopy,
                  const LDefinition& floatCopy, MTableSwitch* ins)
    {
        setBoxOperand(InputValue, input);
        setTemp(0, inputCopy);
        setTemp(1, floatCopy);
        setMir(ins);
    }

    MTableSwitch* mir() const {
        return mir_->toTableSwitch();
    }

    static const size_t InputValue = 0;

    const LDefinition* tempInt() {
        return getTemp(0);
    }
    const LDefinition* tempFloat() {
        return getTemp(1);
    }
    const LDefinition* tempPointer() {
        return nullptr;
    }
};

class LGuardShape : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(GuardShape);

    LGuardShape(const LAllocation& in, const LDefinition& temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
    const MGuardShape* mir() const {
        return mir_->toGuardShape();
    }
    const LDefinition* tempInt() {
        return getTemp(0);
    }
};

class LGuardObjectGroup : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(GuardObjectGroup);

    LGuardObjectGroup(const LAllocation& in, const LDefinition& temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
    const MGuardObjectGroup* mir() const {
        return mir_->toGuardObjectGroup();
    }
    const LDefinition* tempInt() {
        return getTemp(0);
    }
};

class LMulI : public LBinaryMath<0>
{
  public:
    LIR_HEADER(MulI);

    MMul* mir() {
        return mir_->toMul();
    }
};

class LUDiv : public LBinaryMath<0>
{
  public:
    LIR_HEADER(UDiv);

    MDiv* mir() {
        return mir_->toDiv();
    }
};

class LUMod : public LBinaryMath<0>
{
  public:
    LIR_HEADER(UMod);

    MMod* mir() {
        return mir_->toMod();
    }
};

class LSoftUDivOrMod : public LBinaryMath<3>
{
  public:
    LIR_HEADER(SoftUDivOrMod);

    LSoftUDivOrMod(const LAllocation& lhs, const LAllocation& rhs, const LDefinition& temp1,
                   const LDefinition& temp2, const LDefinition& temp3) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, temp3);
    }

    MInstruction* mir() {
        return mir_->toInstruction();
    }
};

class LAsmJSCompareExchangeCallout : public LCallInstructionHelper<1, 4, 2>
{
  public:
    LIR_HEADER(AsmJSCompareExchangeCallout)
    LAsmJSCompareExchangeCallout(const LAllocation& ptr, const LAllocation& oldval,
                                 const LAllocation& newval, const LAllocation& tls,
                                 const LDefinition& temp1, const LDefinition& temp2)
    {
        setOperand(0, ptr);
        setOperand(1, oldval);
        setOperand(2, newval);
        setOperand(3, tls);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    const LAllocation* ptr() {
        return getOperand(0);
    }
    const LAllocation* oldval() {
        return getOperand(1);
    }
    const LAllocation* newval() {
        return getOperand(2);
    }
    const LAllocation* tls() {
        return getOperand(3);
    }

    const MAsmJSCompareExchangeHeap* mir() const {
        return mir_->toAsmJSCompareExchangeHeap();
    }
};

class LAsmJSAtomicExchangeCallout : public LCallInstructionHelper<1, 3, 2>
{
  public:
    LIR_HEADER(AsmJSAtomicExchangeCallout)

    LAsmJSAtomicExchangeCallout(const LAllocation& ptr, const LAllocation& value,
                                const LAllocation& tls, const LDefinition& temp1,
                                const LDefinition& temp2)
    {
        setOperand(0, ptr);
        setOperand(1, value);
        setOperand(2, tls);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    const LAllocation* ptr() {
        return getOperand(0);
    }
    const LAllocation* value() {
        return getOperand(1);
    }
    const LAllocation* tls() {
        return getOperand(2);
    }

    const MAsmJSAtomicExchangeHeap* mir() const {
        return mir_->toAsmJSAtomicExchangeHeap();
    }
};

class LAsmJSAtomicBinopCallout : public LCallInstructionHelper<1, 3, 2>
{
  public:
    LIR_HEADER(AsmJSAtomicBinopCallout)
    LAsmJSAtomicBinopCallout(const LAllocation& ptr, const LAllocation& value,
                             const LAllocation& tls, const LDefinition& temp1,
                             const LDefinition& temp2)
    {
        setOperand(0, ptr);
        setOperand(1, value);
        setOperand(2, tls);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    const LAllocation* ptr() {
        return getOperand(0);
    }
    const LAllocation* value() {
        return getOperand(1);
    }
    const LAllocation* tls() {
        return getOperand(2);
    }

    const MAsmJSAtomicBinopHeap* mir() const {
        return mir_->toAsmJSAtomicBinopHeap();
    }
};

class LWasmTruncateToInt64 : public LCallInstructionHelper<INT64_PIECES, 1, 0>
{
  public:
    LIR_HEADER(WasmTruncateToInt64);

    LWasmTruncateToInt64(const LAllocation& in)
    {
        setOperand(0, in);
    }

    MWasmTruncateToInt64* mir() const {
        return mir_->toWasmTruncateToInt64();
    }
};

class LInt64ToFloatingPointCall: public LCallInstructionHelper<1, INT64_PIECES, 0>
{
  public:
    LIR_HEADER(Int64ToFloatingPointCall);

    MInt64ToFloatingPoint* mir() const {
        return mir_->toInt64ToFloatingPoint();
    }
};

namespace details {

// Base class for the int64 and non-int64 variants.
template<size_t NumDefs>
class LWasmUnalignedLoadBase : public details::LWasmLoadBase<NumDefs, 4>
{
  public:
    typedef LWasmLoadBase<NumDefs, 4> Base;
    explicit LWasmUnalignedLoadBase(const LAllocation& ptr, const LDefinition& ptrCopy,
                                    const LDefinition& temp1, const LDefinition& temp2,
                                    const LDefinition& temp3)
      : Base(ptr)
    {
        Base::setTemp(0, ptrCopy);
        Base::setTemp(1, temp1);
        Base::setTemp(2, temp2);
        Base::setTemp(3, temp3);
    }

    const LDefinition* ptrCopy() {
        return Base::getTemp(0);
    }
};

} // namespace details

class LWasmUnalignedLoad : public details::LWasmUnalignedLoadBase<1>
{
  public:
    explicit LWasmUnalignedLoad(const LAllocation& ptr, const LDefinition& ptrCopy,
                                const LDefinition& temp1, const LDefinition& temp2,
                                const LDefinition& temp3)
      : LWasmUnalignedLoadBase(ptr, ptrCopy, temp1, temp2, temp3)
    {}
    LIR_HEADER(WasmUnalignedLoad);
};

class LWasmUnalignedLoadI64 : public details::LWasmUnalignedLoadBase<INT64_PIECES>
{
  public:
    explicit LWasmUnalignedLoadI64(const LAllocation& ptr, const LDefinition& ptrCopy,
                                   const LDefinition& temp1, const LDefinition& temp2,
                                   const LDefinition& temp3)
      : LWasmUnalignedLoadBase(ptr, ptrCopy, temp1, temp2, temp3)
    {}
    LIR_HEADER(WasmUnalignedLoadI64);
};

namespace details {

// Base class for the int64 and non-int64 variants.
template<size_t NumOps>
class LWasmUnalignedStoreBase : public LInstructionHelper<0, NumOps, 2>
{
  public:
    typedef LInstructionHelper<0, NumOps, 2> Base;

    static const uint32_t ValueIndex = 1;

    LWasmUnalignedStoreBase(const LAllocation& ptr, const LDefinition& ptrCopy,
                            const LDefinition& valueHelper)
    {
        Base::setOperand(0, ptr);
        Base::setTemp(0, ptrCopy);
        Base::setTemp(1, valueHelper);
    }
    MWasmStore* mir() const {
        return Base::mir_->toWasmStore();
    }
    const LDefinition* ptrCopy() {
        return Base::getTemp(0);
    }
    const LDefinition* valueHelper() {
        return Base::getTemp(1);
    }
};

} // namespace details

class LWasmUnalignedStore : public details::LWasmUnalignedStoreBase<2>
{
  public:
    LIR_HEADER(WasmUnalignedStore);
    LWasmUnalignedStore(const LAllocation& ptr, const LAllocation& value,
                        const LDefinition& ptrCopy, const LDefinition& valueHelper)
      : LWasmUnalignedStoreBase(ptr, ptrCopy, valueHelper)
    {
        setOperand(1, value);
    }
};

class LWasmUnalignedStoreI64 : public details::LWasmUnalignedStoreBase<1 + INT64_PIECES>
{
  public:
    LIR_HEADER(WasmUnalignedStoreI64);
    LWasmUnalignedStoreI64(const LAllocation& ptr, const LInt64Allocation& value,
                           const LDefinition& ptrCopy, const LDefinition& valueHelper)
      : LWasmUnalignedStoreBase(ptr, ptrCopy, valueHelper)
    {
        setInt64Operand(1, value);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_arm_LIR_arm_h */
