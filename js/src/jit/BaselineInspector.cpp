/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineInspector.h"

#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"

#include "jit/BaselineIC.h"
#include "jit/CacheIRCompiler.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/ObjectGroup-inl.h"
#include "vm/ReceiverGuard-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

bool
SetElemICInspector::sawOOBDenseWrite() const
{
    if (!icEntry_) {
        return false;
    }

    // Check for a write hole bit on the SetElem_Fallback stub.
    ICStub* stub = icEntry_->fallbackStub();
    if (stub->isSetElem_Fallback()) {
        return stub->toSetElem_Fallback()->hasDenseAdd();
    }

    return false;
}

bool
SetElemICInspector::sawOOBTypedArrayWrite() const
{
    if (!icEntry_) {
        return false;
    }

    ICStub* stub = icEntry_->fallbackStub();
    if (stub->isSetElem_Fallback()) {
        return stub->toSetElem_Fallback()->hasTypedArrayOOB();
    }

    return false;
}

template <typename S, typename T>
static bool
VectorAppendNoDuplicate(S& list, T value)
{
    for (size_t i = 0; i < list.length(); i++) {
        if (list[i] == value) {
            return true;
        }
    }
    return list.append(value);
}

static bool
AddReceiver(const ReceiverGuard& receiver,
            BaselineInspector::ReceiverVector& receivers,
            BaselineInspector::ObjectGroupVector& convertUnboxedGroups)
{
    if (receiver.group) {
        AutoSweepObjectGroup sweep(receiver.group);
        if (auto* layout = receiver.group->maybeUnboxedLayout(sweep)) {
            if (layout->nativeGroup()) {
                return VectorAppendNoDuplicate(convertUnboxedGroups, receiver.group);
            }
        }
    }
    return VectorAppendNoDuplicate(receivers, receiver);
}

static bool
GetCacheIRReceiverForNativeReadSlot(ICCacheIR_Monitored* stub, ReceiverGuard* receiver)
{
    // We match either:
    //
    //   GuardIsObject 0
    //   GuardShape 0
    //   LoadFixedSlotResult 0 or LoadDynamicSlotResult 0
    //
    // or
    //
    //   GuardIsObject 0
    //   GuardGroup 0
    //   1: GuardAndLoadUnboxedExpando 0
    //   GuardShape 1
    //   LoadFixedSlotResult 1 or LoadDynamicSlotResult 1

    *receiver = ReceiverGuard();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    if (reader.matchOp(CacheOp::GuardGroup, objId)) {
        receiver->group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

        if (!reader.matchOp(CacheOp::GuardAndLoadUnboxedExpando, objId)) {
            return false;
        }
        objId = reader.objOperandId();
    }

    if (reader.matchOp(CacheOp::GuardShape, objId)) {
        receiver->shape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());
        return reader.matchOpEither(CacheOp::LoadFixedSlotResult, CacheOp::LoadDynamicSlotResult);
    }

    return false;
}

static bool
GetCacheIRReceiverForUnboxedProperty(ICCacheIR_Monitored* stub, ReceiverGuard* receiver)
{
    // We match:
    //
    //   GuardIsObject 0
    //   GuardGroup 0
    //   LoadUnboxedPropertyResult 0 ..

    *receiver = ReceiverGuard();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::GuardGroup, objId)) {
        return false;
    }
    receiver->group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    return reader.matchOp(CacheOp::LoadUnboxedPropertyResult, objId);
}

static bool
GetCacheIRReceiverForNativeSetSlot(ICCacheIR_Updated* stub, ReceiverGuard* receiver)
{
    // We match either:
    //
    //   GuardIsObject 0
    //   GuardGroup 0
    //   GuardShape 0
    //   StoreFixedSlot 0 or StoreDynamicSlot 0
    //
    // or
    //
    //   GuardIsObject 0
    //   GuardGroup 0
    //   1: GuardAndLoadUnboxedExpando 0
    //   GuardShape 1
    //   StoreFixedSlot 1 or StoreDynamicSlot 1

    *receiver = ReceiverGuard();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::GuardGroup, objId)) {
        return false;
    }
    ObjectGroup* group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    if (reader.matchOp(CacheOp::GuardAndLoadUnboxedExpando, objId)) {
        objId = reader.objOperandId();
    }

    if (!reader.matchOp(CacheOp::GuardShape, objId)) {
        return false;
    }
    Shape* shape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOpEither(CacheOp::StoreFixedSlot, CacheOp::StoreDynamicSlot)) {
        return false;
    }

    *receiver = ReceiverGuard(group, shape);
    return true;
}

static bool
GetCacheIRReceiverForUnboxedProperty(ICCacheIR_Updated* stub, ReceiverGuard* receiver)
{
    // We match:
    //
    //   GuardIsObject 0
    //   GuardGroup 0
    //   GuardType 1 type | GuardIsObjectOrNull 1
    //   StoreUnboxedProperty 0

    *receiver = ReceiverGuard();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    ValOperandId rhsId = ValOperandId(1);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::GuardGroup, objId)) {
        return false;
    }
    ObjectGroup* group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    if (reader.matchOp(CacheOp::GuardType, rhsId)) {
        reader.valueType(); // Skip.
    } else {
        if (!reader.matchOp(CacheOp::GuardIsObjectOrNull, rhsId)) {
            return false;
        }
    }

    if (!reader.matchOp(CacheOp::StoreUnboxedProperty)) {
        return false;
    }

    *receiver = ReceiverGuard(group, nullptr);
    return true;
}

bool
BaselineInspector::maybeInfoForPropertyOp(jsbytecode* pc, ReceiverVector& receivers,
                                          ObjectGroupVector& convertUnboxedGroups)
{
    // Return a list of the receivers seen by the baseline IC for the current
    // op. Empty lists indicate no receivers are known, or there was an
    // uncacheable access. convertUnboxedGroups is used for unboxed object
    // groups which have been seen, but have had instances converted to native
    // objects and should be eagerly converted by Ion.
    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());

    if (!hasBaselineScript()) {
        return true;
    }

    MOZ_ASSERT(isValidPC(pc));
    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    while (stub->next()) {
        ReceiverGuard receiver;
        if (stub->isCacheIR_Monitored()) {
            if (!GetCacheIRReceiverForNativeReadSlot(stub->toCacheIR_Monitored(), &receiver) &&
                !GetCacheIRReceiverForUnboxedProperty(stub->toCacheIR_Monitored(), &receiver))
            {
                receivers.clear();
                return true;
            }
        } else if (stub->isCacheIR_Updated()) {
            if (!GetCacheIRReceiverForNativeSetSlot(stub->toCacheIR_Updated(), &receiver) &&
                !GetCacheIRReceiverForUnboxedProperty(stub->toCacheIR_Updated(), &receiver))
            {
                receivers.clear();
                return true;
            }
        } else {
            receivers.clear();
            return true;
        }

        if (!AddReceiver(receiver, receivers, convertUnboxedGroups)) {
            return false;
        }

        stub = stub->next();
    }

    if (stub->toFallbackStub()->state().hasFailures()) {
        receivers.clear();
    }

    // Don't inline if there are more than 5 receivers.
    if (receivers.length() > 5) {
        receivers.clear();
    }

    return true;
}

ICStub*
BaselineInspector::monomorphicStub(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    // IonBuilder::analyzeNewLoopTypes may call this (via expectedResultType
    // below) on code that's unreachable, according to BytecodeAnalysis. Use
    // maybeICEntryFromPC to handle this.
    const ICEntry* entry = maybeICEntryFromPC(pc);
    if (!entry) {
        return nullptr;
    }

    ICStub* stub = entry->firstStub();
    ICStub* next = stub->next();

    if (!next || !next->isFallback()) {
        return nullptr;
    }

    return stub;
}

bool
BaselineInspector::dimorphicStub(jsbytecode* pc, ICStub** pfirst, ICStub** psecond)
{
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    ICStub* next = stub->next();
    ICStub* after = next ? next->next() : nullptr;

    if (!after || !after->isFallback()) {
        return false;
    }

    *pfirst = stub;
    *psecond = next;
    return true;
}

// Process the type guards in the stub in order to reveal the
// underlying operation.
static void
SkipBinaryGuards(CacheIRReader& reader)
{
    while (true) {
        // Two skip opcodes
        if (reader.matchOp(CacheOp::GuardIsInt32) ||
            reader.matchOp(CacheOp::GuardType) ||
            reader.matchOp(CacheOp::TruncateDoubleToUInt32) ||
            reader.matchOp(CacheOp::GuardIsBoolean))
        {
            reader.skip(); // Skip over operandId
            reader.skip(); // Skip over result/type.
            continue;
        }

        // One skip
        if (reader.matchOp(CacheOp::GuardIsNumber) ||
            reader.matchOp(CacheOp::GuardIsString) ||
            reader.matchOp(CacheOp::GuardIsObject))
        {
            reader.skip(); // Skip over operandId
            continue;
        }
        return;
    }
}

static MIRType
ParseCacheIRStub(ICStub* stub)
{
    ICCacheIR_Regular* cacheirStub = stub->toCacheIR_Regular();
    CacheIRReader reader(cacheirStub->stubInfo());
    SkipBinaryGuards(reader);
    switch (reader.readOp()) {
      case CacheOp::LoadUndefinedResult:
        return MIRType::Undefined;
      case CacheOp::LoadBooleanResult:
        return MIRType::Boolean;
      case CacheOp::LoadStringResult:
      case CacheOp::CallStringConcatResult:
      case CacheOp::CallStringObjectConcatResult:
      case CacheOp::CallInt32ToString:
      case CacheOp::CallNumberToString:
        return MIRType::String;
      case CacheOp::DoubleAddResult:
      case CacheOp::DoubleSubResult:
      case CacheOp::DoubleMulResult:
      case CacheOp::DoubleDivResult:
      case CacheOp::DoubleModResult:
      case CacheOp::DoubleNegationResult:
        return MIRType::Double;
      case CacheOp::Int32AddResult:
      case CacheOp::Int32SubResult:
      case CacheOp::Int32MulResult:
      case CacheOp::Int32DivResult:
      case CacheOp::Int32ModResult:
      case CacheOp::Int32BitOrResult:
      case CacheOp::Int32BitXorResult:
      case CacheOp::Int32BitAndResult:
      case CacheOp::Int32LeftShiftResult:
      case CacheOp::Int32RightShiftResult:
      case CacheOp::Int32NotResult:
      case CacheOp::Int32NegationResult:
        return MIRType::Int32;
      // Int32URightShiftResult may return a double under some
      // circumstances.
      case CacheOp::Int32URightShiftResult:
        reader.skip(); // Skip over lhs
        reader.skip(); // Skip over rhs
        return reader.readByte() == 0 ? MIRType::Int32 : MIRType::Double;
      case CacheOp::LoadValueResult:
        return MIRType::Value;
      default:
        MOZ_CRASH("Unknown op");
        return MIRType::None;
    }
}

MIRType
BaselineInspector::expectedResultType(jsbytecode* pc)
{
    // Look at the IC entries for this op to guess what type it will produce,
    // returning MIRType::None otherwise. Note that IonBuilder may call this
    // for bytecode ops that are unreachable and don't have a Baseline IC, see
    // comment in monomorphicStub.

    ICStub* stub = monomorphicStub(pc);
    if (!stub) {
        return MIRType::None;
    }

    switch (stub->kind()) {
      case ICStub::CacheIR_Regular:
        return ParseCacheIRStub(stub);
      default:
        return MIRType::None;
    }
}

// Return the MIRtype corresponding to the guard the reader is pointing
// to, and ensure that afterwards the reader is pointing to the next op
// (consume operands).
//
// An expected parameter is provided to allow GuardType to check we read
// the guard from the expected operand in debug builds.
static bool
GuardType(CacheIRReader& reader, mozilla::Array<MIRType,2>& guardType)
{
    CacheOp op = reader.readOp();
    uint8_t guardOperand = reader.readByte();

    // We only have two entries for guard types.
    if (guardOperand > 1) {
        return false;
    }

    // Already assigned this guard a type, fail.
    if (guardType[guardOperand] != MIRType::None) {
        return false;
    }

    switch (op) {
        // 0 Skip cases
        case CacheOp::GuardIsString:
            guardType[guardOperand] = MIRType::String;
            break;
        case CacheOp::GuardIsSymbol:
            guardType[guardOperand] = MIRType::Symbol;
            break;
        case CacheOp::GuardIsNumber:
            guardType[guardOperand] = MIRType::Double;
            break;
        case CacheOp::GuardIsUndefined:
            guardType[guardOperand] = MIRType::Undefined;
            break;
        // 1 skip
        case CacheOp::GuardIsInt32:
            guardType[guardOperand] = MIRType::Int32;
            // Skip over result
            reader.skip();
            break;
        case CacheOp::GuardIsBoolean:
            guardType[guardOperand] = MIRType::Boolean;
            // Skip over result
            reader.skip();
            break;
        // Unknown op --
        default:
            return false;
    }
    return true;
}

// This code works for all Compare ICs where the pattern is
//
//  <Guard LHS/RHS>
//  <Guard RHS/LHS>
//  <CompareResult>
//
// in other cases (like StrictlyDifferentTypes) it will just
// return CompareUnknown
static MCompare::CompareType
ParseCacheIRStubForCompareType(ICCacheIR_Regular* stub)
{
    CacheIRReader reader(stub->stubInfo());

    // Two element array to allow parsing the guards
    // in whichever order they appear.
    mozilla::Array<MIRType, 2> guards = { MIRType::None, MIRType::None };

    // Parse out two guards
    if (!GuardType(reader, guards)) {
        return MCompare::Compare_Unknown;
    }
    if (!GuardType(reader, guards)) {
        return MCompare::Compare_Unknown;
    }

    // The lhs and rhs ids are asserted in
    // CompareIRGenerator::tryAttachStub.
    MIRType lhs_guard = guards[0];
    MIRType rhs_guard = guards[1];

    if (lhs_guard == rhs_guard) {
        if (lhs_guard == MIRType::Int32) {
            return MCompare::Compare_Int32;
        }
        if (lhs_guard == MIRType::Double) {
            return MCompare::Compare_Double;
        }
        return MCompare::Compare_Unknown;
    }

    if ((lhs_guard == MIRType::Int32 && rhs_guard == MIRType::Boolean) ||
        (lhs_guard == MIRType::Boolean && rhs_guard == MIRType::Int32))
    {
        // RHS is converting
        if (rhs_guard == MIRType::Boolean) {
            return MCompare::Compare_Int32MaybeCoerceRHS;
        }

        return MCompare::Compare_Int32MaybeCoerceLHS;
    }

    if ((lhs_guard == MIRType::Double && rhs_guard == MIRType::Undefined) ||
        (lhs_guard == MIRType::Undefined && rhs_guard == MIRType::Double))
    {
        // RHS is converting
        if (rhs_guard == MIRType::Undefined) {
            return MCompare::Compare_DoubleMaybeCoerceRHS;
        }

        return MCompare::Compare_DoubleMaybeCoerceLHS;
    }

    return MCompare::Compare_Unknown;
}

static bool
CoercingCompare(MCompare::CompareType type)
{
    // Prefer the coercing types if they exist, otherwise just use first's type.
    if (type == MCompare::Compare_DoubleMaybeCoerceLHS ||
        type == MCompare::Compare_DoubleMaybeCoerceRHS ||
        type == MCompare::Compare_Int32MaybeCoerceLHS  ||
        type == MCompare::Compare_Int32MaybeCoerceRHS)
    {
        return true;
    }
    return false;
}


static MCompare::CompareType
CompatibleType(MCompare::CompareType first, MCompare::CompareType second)
{
    // Caller should have dealt with this case.
    MOZ_ASSERT(first != second);

    // Prefer the coercing types if they exist, otherwise, return Double,
    // which is the general cover.
    if (CoercingCompare(first)) {
        return first;
    }

    if (CoercingCompare(second)) {
        return second;
    }

    // At this point we expect a Double/Int32 pair, so we return Double.
    MOZ_ASSERT(first == MCompare::Compare_Int32 || first == MCompare::Compare_Double);
    MOZ_ASSERT(second == MCompare::Compare_Int32 || second == MCompare::Compare_Double);
    MOZ_ASSERT(first != second);

    return MCompare::Compare_Double;
}

MCompare::CompareType
BaselineInspector::expectedCompareType(jsbytecode* pc)
{
    ICStub* first = monomorphicStub(pc);
    ICStub* second = nullptr;
    if (!first && !dimorphicStub(pc, &first, &second)) {
        return MCompare::Compare_Unknown;
    }

    if (ICStub* fallback = second ? second->next() : first->next()) {
        MOZ_ASSERT(fallback->isFallback());
        if (fallback->toFallbackStub()->state().hasFailures()) {
            return MCompare::Compare_Unknown;
        }
    }

    MCompare::CompareType first_type = ParseCacheIRStubForCompareType(first->toCacheIR_Regular());
    if (!second) {
        return first_type;
    }

    MCompare::CompareType second_type = ParseCacheIRStubForCompareType(second->toCacheIR_Regular());

    if (first_type == MCompare::Compare_Unknown || second_type == MCompare::Compare_Unknown) {
        return MCompare::Compare_Unknown;
    }

    if (first_type == second_type) {
        return first_type;
    }

    return CompatibleType(first_type, second_type);
}

static bool
TryToSpecializeBinaryArithOp(ICStub** stubs,
                             uint32_t nstubs,
                             MIRType* result)
{
    DebugOnly<bool> sawInt32 = false;
    bool sawDouble = false;
    bool sawOther = false;

    for (uint32_t i = 0; i < nstubs; i++) {
        switch (stubs[i]->kind()) {
          case ICStub::CacheIR_Regular:
            switch (ParseCacheIRStub(stubs[i])) {
              case MIRType::Double:
                sawDouble = true;
                break;
              case MIRType::Int32:
                sawInt32 = true;
                break;
              default:
                sawOther = true;
                break;
            }
            break;
          default:
            sawOther = true;
            break;
        }
    }

    if (sawOther) {
        return false;
    }

    if (sawDouble) {
        *result = MIRType::Double;
        return true;
    }

    MOZ_ASSERT(sawInt32);
    *result = MIRType::Int32;
    return true;
}

MIRType
BaselineInspector::expectedBinaryArithSpecialization(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return MIRType::None;
    }

    MIRType result;
    ICStub* stubs[2];

    const ICEntry& entry = icEntryFromPC(pc);
    ICFallbackStub* stub = entry.fallbackStub();
    if (stub->state().hasFailures())
    {
        return MIRType::None;
    }

    stubs[0] = monomorphicStub(pc);
    if (stubs[0]) {
        if (TryToSpecializeBinaryArithOp(stubs, 1, &result)) {
            return result;
        }
    }

    if (dimorphicStub(pc, &stubs[0], &stubs[1])) {
        if (TryToSpecializeBinaryArithOp(stubs, 2, &result)) {
            return result;
        }
    }

    return MIRType::None;
}

bool
BaselineInspector::hasSeenNegativeIndexGetElement(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    if (stub->isGetElem_Fallback()) {
        return stub->toGetElem_Fallback()->hasNegativeIndex();
    }
    return false;
}

bool
BaselineInspector::hasSeenAccessedGetter(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback()) {
        return stub->toGetProp_Fallback()->hasAccessedGetter();
    }
    return false;
}

bool
BaselineInspector::hasSeenNonStringIterMore(jsbytecode* pc)
{
    MOZ_ASSERT(JSOp(*pc) == JSOP_MOREITER);

    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    return stub->toIteratorMore_Fallback()->hasNonStringResult();
}

bool
BaselineInspector::hasSeenDoubleResult(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    MOZ_ASSERT(stub->isUnaryArith_Fallback() || stub->isBinaryArith_Fallback());

    if (stub->isUnaryArith_Fallback()) {
        return stub->toUnaryArith_Fallback()->sawDoubleResult();
    }

    return stub->toBinaryArith_Fallback()->sawDoubleResult();
}

JSObject*
BaselineInspector::getTemplateObject(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        switch (stub->kind()) {
          case ICStub::NewArray_Fallback:
            return stub->toNewArray_Fallback()->templateObject();
          case ICStub::NewObject_Fallback:
            return stub->toNewObject_Fallback()->templateObject();
          case ICStub::Rest_Fallback:
            return stub->toRest_Fallback()->templateObject();
          case ICStub::Call_Scripted:
            if (JSObject* obj = stub->toCall_Scripted()->templateObject()) {
                return obj;
            }
            break;
          default:
            break;
        }
    }

    return nullptr;
}

ObjectGroup*
BaselineInspector::getTemplateObjectGroup(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        switch (stub->kind()) {
          case ICStub::NewArray_Fallback:
            return stub->toNewArray_Fallback()->templateGroup();
          default:
            break;
        }
    }

    return nullptr;
}

JSFunction*
BaselineInspector::getSingleCallee(jsbytecode* pc)
{
    MOZ_ASSERT(*pc == JSOP_NEW);

    if (!hasBaselineScript()) {
        return nullptr;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.firstStub();

    if (entry.fallbackStub()->state().hasFailures()) {
        return nullptr;
    }

    if (!stub->isCall_Scripted() || stub->next() != entry.fallbackStub()) {
        return nullptr;
    }

    return stub->toCall_Scripted()->callee();
}

JSObject*
BaselineInspector::getTemplateObjectForNative(jsbytecode* pc, Native native)
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCall_Native() && stub->toCall_Native()->callee()->native() == native) {
            return stub->toCall_Native()->templateObject();
        }
    }

    return nullptr;
}

bool
BaselineInspector::isOptimizableConstStringSplit(jsbytecode* pc, JSString** strOut,
                                                 JSString** sepOut, ArrayObject** objOut)
{
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);

    // If ConstStringSplit stub is attached, must have only one stub attached.
    if (entry.fallbackStub()->numOptimizedStubs() != 1) {
        return false;
    }

    ICStub* stub = entry.firstStub();
    if (stub->kind() != ICStub::Call_ConstStringSplit) {
        return false;
    }

    *strOut = stub->toCall_ConstStringSplit()->expectedStr();
    *sepOut = stub->toCall_ConstStringSplit()->expectedSep();
    *objOut = stub->toCall_ConstStringSplit()->templateObject();
    return true;
}

JSObject*
BaselineInspector::getTemplateObjectForClassHook(jsbytecode* pc, const Class* clasp)
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCall_ClassHook() && stub->toCall_ClassHook()->clasp() == clasp) {
            return stub->toCall_ClassHook()->templateObject();
        }
    }

    return nullptr;
}

LexicalEnvironmentObject*
BaselineInspector::templateNamedLambdaObject()
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    JSObject* res = baselineScript()->templateEnvironment();
    if (script->bodyScope()->hasEnvironment()) {
        res = res->enclosingEnvironment();
    }
    MOZ_ASSERT(res);

    return &res->as<LexicalEnvironmentObject>();
}

CallObject*
BaselineInspector::templateCallObject()
{
    if (!hasBaselineScript()) {
        return nullptr;
    }

    JSObject* res = baselineScript()->templateEnvironment();
    MOZ_ASSERT(res);

    return &res->as<CallObject>();
}

static bool
MatchCacheIRReceiverGuard(CacheIRReader& reader, ICStub* stub, const CacheIRStubInfo* stubInfo,
                          ObjOperandId objId, ReceiverGuard* receiver)
{
    // This matches the CacheIR emitted in TestMatchingReceiver.
    //
    // Either:
    //
    //   GuardShape objId
    //
    // or:
    //
    //   GuardGroup objId
    //   [GuardNoUnboxedExpando objId]
    //
    // or:
    //
    //   GuardGroup objId
    //   expandoId: GuardAndLoadUnboxedExpando
    //   GuardShape expandoId

    *receiver = ReceiverGuard();

    if (reader.matchOp(CacheOp::GuardShape, objId)) {
        // The first case.
        receiver->shape = stubInfo->getStubField<Shape*>(stub, reader.stubOffset());
        return true;
    }

    if (!reader.matchOp(CacheOp::GuardGroup, objId)) {
        return false;
    }
    receiver->group = stubInfo->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::GuardAndLoadUnboxedExpando, objId)) {
        // Second case, just a group guard.
        reader.matchOp(CacheOp::GuardNoUnboxedExpando, objId);
        return true;
    }

    // Third case.
    ObjOperandId expandoId = reader.objOperandId();
    if (!reader.matchOp(CacheOp::GuardShape, expandoId)) {
        return false;
    }

    receiver->shape = stubInfo->getStubField<Shape*>(stub, reader.stubOffset());
    return true;
}

static bool
AddCacheIRGlobalGetter(ICCacheIR_Monitored* stub, bool innerized,
                       JSObject** holder_, Shape** holderShape_,
                       JSFunction** commonGetter, Shape** globalShape_, bool* isOwnProperty,
                       BaselineInspector::ReceiverVector& receivers,
                       BaselineInspector::ObjectGroupVector& convertUnboxedGroups,
                       JSScript* script)
{
    // We are matching on the IR generated by tryAttachGlobalNameGetter:
    //
    //   GuardShape objId
    //   globalId = LoadEnclosingEnvironment objId
    //   GuardShape globalId
    //   <holderId = LoadObject <holder>>
    //   <GuardShape holderId>
    //   CallNativeGetterResult globalId

    if (innerized) {
        return false;
    }

    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardShape, objId)) {
        return false;
    }
    Shape* globalLexicalShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::LoadEnclosingEnvironment, objId)) {
        return false;
    }
    ObjOperandId globalId = reader.objOperandId();

    if (!reader.matchOp(CacheOp::GuardShape, globalId)) {
        return false;
    }
    Shape* globalShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());
    MOZ_ASSERT(globalShape->getObjectClass()->flags & JSCLASS_IS_GLOBAL);

    JSObject* holder = &script->global();
    Shape* holderShape = globalShape;
    if (reader.matchOp(CacheOp::LoadObject)) {
        ObjOperandId holderId = reader.objOperandId();
        holder = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset()).get();

        if (!reader.matchOp(CacheOp::GuardShape, holderId)) {
            return false;
        }
        holderShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());
    }

    // This guard will always fail, try the next stub.
    if (holder->as<NativeObject>().lastProperty() != holderShape) {
        return true;
    }

    if (!reader.matchOp(CacheOp::CallNativeGetterResult, globalId)) {
        return false;
    }
    size_t offset = reader.stubOffset();
    JSFunction* getter =
        &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

    ReceiverGuard receiver;
    receiver.shape = globalLexicalShape;
    if (!AddReceiver(receiver, receivers, convertUnboxedGroups)) {
        return false;
    }

    if (!*commonGetter) {
        *holder_ = holder;
        *holderShape_ = holderShape;
        *commonGetter = getter;
        *globalShape_ = globalShape;

        // This is always false, because the getters never live on the globalLexical.
        *isOwnProperty = false;
    } else if (*isOwnProperty || holderShape != *holderShape_ || globalShape != *globalShape_) {
        return false;
    } else {
        MOZ_ASSERT(*commonGetter == getter);
    }

    return true;
}

static bool
AddCacheIRGetPropFunction(ICCacheIR_Monitored* stub, bool innerized,
                          JSObject** holder, Shape** holderShape,
                          JSFunction** commonGetter, Shape** globalShape, bool* isOwnProperty,
                          BaselineInspector::ReceiverVector& receivers,
                          BaselineInspector::ObjectGroupVector& convertUnboxedGroups,
                          JSScript* script)
{
    // We match either an own getter:
    //
    //   GuardIsObject objId
    //   [..WindowProxy innerization..]
    //   <GuardReceiver objId>
    //   Call(Scripted|Native)GetterResult objId
    //
    // Or a getter on the prototype:
    //
    //   GuardIsObject objId
    //   [..WindowProxy innerization..]
    //   <GuardReceiver objId>
    //   LoadObject holderId
    //   GuardShape holderId
    //   Call(Scripted|Native)GetterResult objId
    //
    // If |innerized| is true, we replaced a WindowProxy with the Window
    // object and we're only interested in Baseline getter stubs that performed
    // the same optimization. This means we expect the following ops for the
    // [..WindowProxy innerization..] above:
    //
    //   GuardClass objId WindowProxy
    //   objId = LoadObject <global>

    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return AddCacheIRGlobalGetter(stub, innerized, holder, holderShape, commonGetter,
                                      globalShape, isOwnProperty, receivers, convertUnboxedGroups,
                                      script);
    }

    if (innerized) {
        if (!reader.matchOp(CacheOp::GuardClass, objId) ||
            reader.guardClassKind() != GuardClassKind::WindowProxy)
        {
            return false;
        }
        if (!reader.matchOp(CacheOp::LoadObject)) {
            return false;
        }
        objId = reader.objOperandId();
        DebugOnly<JSObject*> obj =
            stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset()).get();
        MOZ_ASSERT(obj->is<GlobalObject>());
    }

    ReceiverGuard receiver;
    if (!MatchCacheIRReceiverGuard(reader, stub, stub->stubInfo(), objId, &receiver)) {
        return false;
    }

    if (reader.matchOp(CacheOp::CallScriptedGetterResult, objId) ||
        reader.matchOp(CacheOp::CallNativeGetterResult, objId))
    {
        // This is an own property getter, the first case.
        MOZ_ASSERT(receiver.shape);
        MOZ_ASSERT(!receiver.group);

        size_t offset = reader.stubOffset();
        JSFunction* getter =
            &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

        if (*commonGetter && (!*isOwnProperty || *globalShape || *holderShape != receiver.shape)) {
            return false;
        }

        MOZ_ASSERT_IF(*commonGetter, *commonGetter == getter);
        *holder = nullptr;
        *holderShape = receiver.shape;
        *commonGetter = getter;
        *isOwnProperty = true;
        return true;
    }

    if (!reader.matchOp(CacheOp::LoadObject)) {
        return false;
    }
    ObjOperandId holderId = reader.objOperandId();
    JSObject* obj = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::GuardShape, holderId)) {
        return false;
    }
    Shape* objShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::CallScriptedGetterResult, objId) &&
        !reader.matchOp(CacheOp::CallNativeGetterResult, objId))
    {
        return false;
    }

    // A getter on the prototype.
    size_t offset = reader.stubOffset();
    JSFunction* getter =
        &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

    Shape* thisGlobalShape = nullptr;
    if (getter->isNative() && receiver.shape &&
        (receiver.shape->getObjectClass()->flags & JSCLASS_IS_GLOBAL))
    {
        thisGlobalShape = receiver.shape;
    }

    if (*commonGetter &&
        (*isOwnProperty || *globalShape != thisGlobalShape || *holderShape != objShape))
    {
        return false;
    }

    MOZ_ASSERT_IF(*commonGetter, *commonGetter == getter);

    if (obj->as<NativeObject>().lastProperty() != objShape) {
        // Skip this stub as the shape is no longer correct.
        return true;
    }

    if (!AddReceiver(receiver, receivers, convertUnboxedGroups)) {
        return false;
    }

    *holder = obj;
    *holderShape = objShape;
    *commonGetter = getter;
    *isOwnProperty = false;
    return true;
}

bool
BaselineInspector::commonGetPropFunction(jsbytecode* pc, bool innerized,
                                         JSObject** holder, Shape** holderShape,
                                         JSFunction** commonGetter, Shape** globalShape,
                                         bool* isOwnProperty,
                                         ReceiverVector& receivers,
                                         ObjectGroupVector& convertUnboxedGroups)
{
    if (!hasBaselineScript()) {
        return false;
    }

    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());

    *globalShape = nullptr;
    *commonGetter = nullptr;
    const ICEntry& entry = icEntryFromPC(pc);

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCacheIR_Monitored()) {
            if (!AddCacheIRGetPropFunction(stub->toCacheIR_Monitored(), innerized,
                                           holder, holderShape,
                                           commonGetter, globalShape, isOwnProperty, receivers,
                                           convertUnboxedGroups, script))
            {
                return false;
            }
        } else if (stub->isFallback()) {
            // If we have an unoptimizable access, don't try to optimize.
            if (stub->toFallbackStub()->state().hasFailures()) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (!*commonGetter) {
        return false;
    }

    MOZ_ASSERT(*isOwnProperty == !*holder);
    MOZ_ASSERT(*isOwnProperty == (receivers.empty() && convertUnboxedGroups.empty()));
    return true;
}

static JSFunction*
GetMegamorphicGetterSetterFunction(ICStub* stub, const CacheIRStubInfo* stubInfo, bool isGetter)
{
    // We match:
    //
    //   GuardIsObject objId
    //   GuardHasGetterSetter objId propShape
    //
    // propShape has the getter/setter we're interested in.

    CacheIRReader reader(stubInfo);

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return nullptr;
    }

    if (!reader.matchOp(CacheOp::GuardHasGetterSetter, objId)) {
        return nullptr;
    }
    Shape* propShape = stubInfo->getStubField<Shape*>(stub, reader.stubOffset());

    JSObject* obj = isGetter ? propShape->getterObject() : propShape->setterObject();
    return &obj->as<JSFunction>();
}

bool
BaselineInspector::megamorphicGetterSetterFunction(jsbytecode* pc, bool isGetter,
                                                   JSFunction** getterOrSetter)
{
    if (!hasBaselineScript()) {
        return false;
    }

    *getterOrSetter = nullptr;
    const ICEntry& entry = icEntryFromPC(pc);

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCacheIR_Monitored()) {
            MOZ_ASSERT(isGetter);
            JSFunction* getter =
                GetMegamorphicGetterSetterFunction(stub,
                                                   stub->toCacheIR_Monitored()->stubInfo(),
                                                   isGetter);
            if (!getter || (*getterOrSetter && *getterOrSetter != getter)) {
                return false;
            }
            *getterOrSetter = getter;
            continue;
        }
        if (stub->isCacheIR_Updated()) {
            MOZ_ASSERT(!isGetter);
            JSFunction* setter =
                GetMegamorphicGetterSetterFunction(stub,
                                                   stub->toCacheIR_Updated()->stubInfo(),
                                                   isGetter);
            if (!setter || (*getterOrSetter && *getterOrSetter != setter)) {
                return false;
            }
            *getterOrSetter = setter;
            continue;
        }
        if (stub->isFallback()) {
            if (stub->toFallbackStub()->state().hasFailures()) {
                return false;
            }
            if (stub->toFallbackStub()->state().mode() != ICState::Mode::Megamorphic) {
                return false;
            }
            continue;
        }

        return false;
    }

    if (!*getterOrSetter) {
        return false;
    }

    return true;
}

static bool
AddCacheIRSetPropFunction(ICCacheIR_Updated* stub, JSObject** holder, Shape** holderShape,
                          JSFunction** commonSetter, bool* isOwnProperty,
                          BaselineInspector::ReceiverVector& receivers,
                          BaselineInspector::ObjectGroupVector& convertUnboxedGroups)
{
    // We match either an own setter:
    //
    //   GuardIsObject objId
    //   <GuardReceiver objId>
    //   Call(Scripted|Native)Setter objId
    //
    // Or a setter on the prototype:
    //
    //   GuardIsObject objId
    //   <GuardReceiver objId>
    //   LoadObject holderId
    //   GuardShape holderId
    //   Call(Scripted|Native)Setter objId

    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    ReceiverGuard receiver;
    if (!MatchCacheIRReceiverGuard(reader, stub, stub->stubInfo(), objId, &receiver)) {
        return false;
    }

    if (reader.matchOp(CacheOp::CallScriptedSetter, objId) ||
        reader.matchOp(CacheOp::CallNativeSetter, objId))
    {
        // This is an own property setter, the first case.
        MOZ_ASSERT(receiver.shape);
        MOZ_ASSERT(!receiver.group);

        size_t offset = reader.stubOffset();
        JSFunction* setter =
            &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

        if (*commonSetter && (!*isOwnProperty || *holderShape != receiver.shape)) {
            return false;
        }

        MOZ_ASSERT_IF(*commonSetter, *commonSetter == setter);
        *holder = nullptr;
        *holderShape = receiver.shape;
        *commonSetter = setter;
        *isOwnProperty = true;
        return true;
    }

    if (!reader.matchOp(CacheOp::LoadObject)) {
        return false;
    }
    ObjOperandId holderId = reader.objOperandId();
    JSObject* obj = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::GuardShape, holderId)) {
        return false;
    }
    Shape* objShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::CallScriptedSetter, objId) &&
        !reader.matchOp(CacheOp::CallNativeSetter, objId))
    {
        return false;
    }

    // A setter on the prototype.
    size_t offset = reader.stubOffset();
    JSFunction* setter =
        &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

    if (*commonSetter && (*isOwnProperty || *holderShape != objShape)) {
        return false;
    }

    MOZ_ASSERT_IF(*commonSetter, *commonSetter == setter);

    if (obj->as<NativeObject>().lastProperty() != objShape) {
        // Skip this stub as the shape is no longer correct.
        return true;
    }

    if (!AddReceiver(receiver, receivers, convertUnboxedGroups)) {
        return false;
    }

    *holder = obj;
    *holderShape = objShape;
    *commonSetter = setter;
    *isOwnProperty = false;
    return true;
}

bool
BaselineInspector::commonSetPropFunction(jsbytecode* pc, JSObject** holder, Shape** holderShape,
                                         JSFunction** commonSetter, bool* isOwnProperty,
                                         ReceiverVector& receivers,
                                         ObjectGroupVector& convertUnboxedGroups)
{
    if (!hasBaselineScript()) {
        return false;
    }

    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());

    *commonSetter = nullptr;
    const ICEntry& entry = icEntryFromPC(pc);

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCacheIR_Updated()) {
            if (!AddCacheIRSetPropFunction(stub->toCacheIR_Updated(),
                                           holder, holderShape,
                                           commonSetter, isOwnProperty, receivers,
                                           convertUnboxedGroups))
            {
                return false;
            }
        } else if (!stub->isFallback() ||
                   stub->toFallbackStub()->state().hasFailures())
        {
            // We have an unoptimizable access, so don't try to optimize.
            return false;
        }
    }

    if (!*commonSetter) {
        return false;
    }

    MOZ_ASSERT(*isOwnProperty == !*holder);
    return true;
}

static bool
GetCacheIRReceiverForProtoReadSlot(ICCacheIR_Monitored* stub, ReceiverGuard* receiver,
                                   JSObject** holderResult)
{
    // We match:
    //
    //   GuardIsObject 0
    //   <ReceiverGuard>
    //   1: LoadObject holder
    //   GuardShape 1
    //   LoadFixedSlotResult 1 or LoadDynamicSlotResult 1

    *receiver = ReceiverGuard();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId objId = ObjOperandId(0);
    if (!reader.matchOp(CacheOp::GuardIsObject, objId)) {
        return false;
    }

    if (!MatchCacheIRReceiverGuard(reader, stub, stub->stubInfo(), objId, receiver)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::LoadObject)) {
        return false;
    }
    ObjOperandId holderId = reader.objOperandId();
    JSObject* holder = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset()).get();

    if (!reader.matchOp(CacheOp::GuardShape, holderId)) {
        return false;
    }
    Shape* holderShape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOpEither(CacheOp::LoadFixedSlotResult, CacheOp::LoadDynamicSlotResult)) {
        return false;
    }
    if (reader.objOperandId() != holderId) {
        return false;
    }

    if (holder->maybeShape() != holderShape) {
        return false;
    }
    if (*holderResult && *holderResult != holder) {
        return false;
    }

    *holderResult = holder;
    return true;
}

bool
BaselineInspector::maybeInfoForProtoReadSlot(jsbytecode* pc, ReceiverVector& receivers,
                                             ObjectGroupVector& convertUnboxedGroups,
                                             JSObject** holder)
{
    // This is like maybeInfoForPropertyOp, but for when the property exists on
    // the prototype.

    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());
    MOZ_ASSERT(!*holder);

    if (!hasBaselineScript()) {
        return true;
    }

    MOZ_ASSERT(isValidPC(pc));
    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    while (stub->next()) {
        ReceiverGuard receiver;
        if (stub->isCacheIR_Monitored()) {
            if (!GetCacheIRReceiverForProtoReadSlot(stub->toCacheIR_Monitored(), &receiver,
                                                    holder))
            {
                receivers.clear();
                return true;
            }
        } else {
            receivers.clear();
            return true;
        }

        if (!AddReceiver(receiver, receivers, convertUnboxedGroups)) {
            return false;
        }

        stub = stub->next();
    }

    if (stub->toFallbackStub()->state().hasFailures()) {
        receivers.clear();
    }

    // Don't inline if there are more than 5 receivers.
    if (receivers.length() > 5) {
        receivers.clear();
    }

    MOZ_ASSERT_IF(!receivers.empty(), *holder);
    return true;
}

static MIRType
GetCacheIRExpectedInputType(ICCacheIR_Monitored* stub)
{
    CacheIRReader reader(stub->stubInfo());

    if (reader.matchOp(CacheOp::GuardIsObject, ValOperandId(0))) {
        return MIRType::Object;
    }
    if (reader.matchOp(CacheOp::GuardIsString, ValOperandId(0))) {
        return MIRType::String;
    }
    if (reader.matchOp(CacheOp::GuardType, ValOperandId(0))) {
        JSValueType type = reader.valueType();
        return MIRTypeFromValueType(type);
    }

    MOZ_ASSERT_UNREACHABLE("Unexpected instruction");
    return MIRType::Value;
}

MIRType
BaselineInspector::expectedPropertyAccessInputType(jsbytecode* pc)
{
    if (!hasBaselineScript()) {
        return MIRType::Value;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    MIRType type = MIRType::None;

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        MIRType stubType = MIRType::None;
        if (stub->isCacheIR_Monitored()) {
            stubType = GetCacheIRExpectedInputType(stub->toCacheIR_Monitored());
            if (stubType == MIRType::Value) {
                return MIRType::Value;
            }
        } else if (stub->isGetElem_Fallback() || stub->isGetProp_Fallback()) {
            // If we have an unoptimizable access, don't try to optimize.
            if (stub->toFallbackStub()->state().hasFailures()) {
                return MIRType::Value;
            }
        } else {
            MOZ_CRASH("Unexpected stub");
        }

        if (type != MIRType::None) {
            if (type != stubType) {
                return MIRType::Value;
            }
        } else {
            type = stubType;
        }
    }

    return (type == MIRType::None) ? MIRType::Value : type;
}

bool
BaselineInspector::instanceOfData(jsbytecode* pc, Shape** shape, uint32_t* slot,
                                  JSObject** prototypeObject)
{
    MOZ_ASSERT(*pc == JSOP_INSTANCEOF);
    if (!hasBaselineScript()) {
        return false;
    }

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* firstStub = entry.firstStub();

    // Ensure singleton instanceof stub
    if (!firstStub->next() ||
        !firstStub->isCacheIR_Regular() ||
        !firstStub->next()->isInstanceOf_Fallback() ||
         firstStub->next()->toInstanceOf_Fallback()->state().hasFailures())
         {
             return false;
         }

    ICCacheIR_Regular* stub = entry.firstStub()->toCacheIR_Regular();
    CacheIRReader reader(stub->stubInfo());

    ObjOperandId rhsId = ObjOperandId(1);
    ObjOperandId resId = ObjOperandId(2);

    if (!reader.matchOp(CacheOp::GuardIsObject, rhsId)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::GuardShape, rhsId)) {
        return false;
    }

    *shape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::LoadObject, resId)) {
        return false;
    }

    *prototypeObject = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset()).get();

    if (IsInsideNursery(*prototypeObject)) {
        return false;
    }

    if (!reader.matchOp(CacheOp::GuardFunctionPrototype, rhsId)) {
        return false;
    }

    reader.skip(); // Skip over the protoID;

    *slot = stub->stubInfo()->getStubRawWord(stub, reader.stubOffset());

    return true;
}
