/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineInspector.h"

#include "jit/BaselineIC.h"

using namespace js;
using namespace js::ion;

bool
SetElemICInspector::sawOOBDenseWrite() const
{
    if (!icEntry_)
        return false;

    // Check for a SetElem_DenseAdd stub.
    for (ICStub *stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (stub->isSetElem_DenseAdd())
            return true;
    }
    return false;
}

bool
SetElemICInspector::sawOOBTypedArrayWrite() const
{
    if (!icEntry_)
        return false;

    // Check for SetElem_TypedArray stubs with expectOutOfBounds set.
    for (ICStub *stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (!stub->isSetElem_TypedArray())
            continue;
        if (stub->toSetElem_TypedArray()->expectOutOfBounds())
            return true;
    }
    return false;
}

bool
SetElemICInspector::sawDenseWrite() const
{
    if (!icEntry_)
        return false;

    // Check for a SetElem_DenseAdd or SetElem_Dense stub.
    for (ICStub *stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (stub->isSetElem_DenseAdd() || stub->isSetElem_Dense())
            return true;
    }
    return false;
}

bool
BaselineInspector::maybeShapesForPropertyOp(jsbytecode *pc, Vector<Shape *> &shapes)
{
    // Return a list of shapes seen by the baseline IC for the current op.
    // An empty list indicates no shapes are known, or there was an uncacheable
    // access.
    JS_ASSERT(shapes.empty());

    if (!hasBaselineScript())
        return true;

    JS_ASSERT(isValidPC(pc));
    const ICEntry &entry = icEntryFromPC(pc);

    ICStub *stub = entry.firstStub();
    while (stub->next()) {
        Shape *shape;
        if (stub->isGetProp_Native()) {
            shape = stub->toGetProp_Native()->shape();
        } else if (stub->isSetProp_Native()) {
            shape = stub->toSetProp_Native()->shape();
        } else {
            shapes.clear();
            return true;
        }

        // Don't add the same shape twice (this can happen if there are multiple
        // SetProp_Native stubs with different TypeObject's).
        bool found = false;
        for (size_t i = 0; i < shapes.length(); i++) {
            if (shapes[i] == shape) {
                found = true;
                break;
            }
        }

        if (!found && !shapes.append(shape))
            return false;

        stub = stub->next();
    }

    if (stub->isGetProp_Fallback()) {
        if (stub->toGetProp_Fallback()->hadUnoptimizableAccess())
            shapes.clear();
    } else {
        if (stub->toSetProp_Fallback()->hadUnoptimizableAccess())
            shapes.clear();
    }

    // Don't inline if there are more than 5 shapes.
    if (shapes.length() > 5)
        shapes.clear();

    return true;
}

ICStub *
BaselineInspector::monomorphicStub(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return NULL;

    const ICEntry &entry = icEntryFromPC(pc);

    ICStub *stub = entry.firstStub();
    ICStub *next = stub->next();

    if (!next || !next->isFallback())
        return NULL;

    return stub;
}

bool
BaselineInspector::dimorphicStub(jsbytecode *pc, ICStub **pfirst, ICStub **psecond)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry &entry = icEntryFromPC(pc);

    ICStub *stub = entry.firstStub();
    ICStub *next = stub->next();
    ICStub *after = next ? next->next() : NULL;

    if (!after || !after->isFallback())
        return false;

    *pfirst = stub;
    *psecond = next;
    return true;
}

MIRType
BaselineInspector::expectedResultType(jsbytecode *pc)
{
    // Look at the IC entries for this op to guess what type it will produce,
    // returning MIRType_None otherwise.

    ICStub *stub = monomorphicStub(pc);
    if (!stub)
        return MIRType_None;

    switch (stub->kind()) {
      case ICStub::BinaryArith_Int32:
        if (stub->toBinaryArith_Int32()->allowDouble())
            return MIRType_Double;
        return MIRType_Int32;
      case ICStub::BinaryArith_BooleanWithInt32:
      case ICStub::UnaryArith_Int32:
      case ICStub::BinaryArith_DoubleWithInt32:
        return MIRType_Int32;
      case ICStub::BinaryArith_Double:
      case ICStub::UnaryArith_Double:
        return MIRType_Double;
      case ICStub::BinaryArith_StringConcat:
      case ICStub::BinaryArith_StringObjectConcat:
        return MIRType_String;
      default:
        return MIRType_None;
    }
}

// Whether a baseline stub kind is suitable for a double comparison that
// converts its operands to doubles.
static bool
CanUseDoubleCompare(ICStub::Kind kind)
{
    return kind == ICStub::Compare_Double || kind == ICStub::Compare_NumberWithUndefined;
}

// Whether a baseline stub kind is suitable for an int32 comparison that
// converts its operands to int32.
static bool
CanUseInt32Compare(ICStub::Kind kind)
{
    return kind == ICStub::Compare_Int32 || kind == ICStub::Compare_Int32WithBoolean;
}

MCompare::CompareType
BaselineInspector::expectedCompareType(jsbytecode *pc)
{
    ICStub *first = monomorphicStub(pc), *second = NULL;
    if (!first && !dimorphicStub(pc, &first, &second))
        return MCompare::Compare_Unknown;

    if (CanUseInt32Compare(first->kind()) && (!second || CanUseInt32Compare(second->kind())))
        return MCompare::Compare_Int32;

    if (CanUseDoubleCompare(first->kind()) && (!second || CanUseDoubleCompare(second->kind()))) {
        ICCompare_NumberWithUndefined *coerce =
            first->isCompare_NumberWithUndefined()
            ? first->toCompare_NumberWithUndefined()
            : (second && second->isCompare_NumberWithUndefined())
              ? second->toCompare_NumberWithUndefined()
              : NULL;
        if (coerce) {
            return coerce->lhsIsUndefined()
                   ? MCompare::Compare_DoubleMaybeCoerceLHS
                   : MCompare::Compare_DoubleMaybeCoerceRHS;
        }
        return MCompare::Compare_Double;
    }

    return MCompare::Compare_Unknown;
}

static bool
TryToSpecializeBinaryArithOp(ICStub **stubs,
                             uint32_t nstubs,
                             MIRType *result)
{
    bool sawInt32 = false;
    bool sawDouble = false;
    bool sawOther = false;

    for (uint32_t i = 0; i < nstubs; i++) {
        switch (stubs[i]->kind()) {
          case ICStub::BinaryArith_Int32:
            sawInt32 = true;
            break;
          case ICStub::BinaryArith_BooleanWithInt32:
            sawInt32 = true;
            break;
          case ICStub::BinaryArith_Double:
            sawDouble = true;
            break;
          case ICStub::BinaryArith_DoubleWithInt32:
            sawDouble = true;
            break;
          default:
            sawOther = true;
            break;
        }
    }

    if (sawOther)
        return false;

    if (sawDouble) {
        *result = MIRType_Double;
        return true;
    }

    JS_ASSERT(sawInt32);
    *result = MIRType_Int32;
    return true;
}

MIRType
BaselineInspector::expectedBinaryArithSpecialization(jsbytecode *pc)
{
    MIRType result;
    ICStub *stubs[2];

    stubs[0] = monomorphicStub(pc);
    if (stubs[0]) {
        if (TryToSpecializeBinaryArithOp(stubs, 1, &result))
            return result;
    }

    if (dimorphicStub(pc, &stubs[0], &stubs[1])) {
        if (TryToSpecializeBinaryArithOp(stubs, 2, &result))
            return result;
    }

    return MIRType_None;
}

bool
BaselineInspector::hasSeenNonNativeGetElement(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry &entry = icEntryFromPC(pc);
    ICStub *stub = entry.fallbackStub();

    if (stub->isGetElem_Fallback())
        return stub->toGetElem_Fallback()->hasNonNativeAccess();
    return false;
}

bool
BaselineInspector::hasSeenNegativeIndexGetElement(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry &entry = icEntryFromPC(pc);
    ICStub *stub = entry.fallbackStub();

    if (stub->isGetElem_Fallback())
        return stub->toGetElem_Fallback()->hasNegativeIndex();
    return false;
}

bool
BaselineInspector::hasSeenAccessedGetter(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry &entry = icEntryFromPC(pc);
    ICStub *stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback())
        return stub->toGetProp_Fallback()->hasAccessedGetter();
    return false;
}

bool
BaselineInspector::hasSeenDoubleResult(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry &entry = icEntryFromPC(pc);
    ICStub *stub = entry.fallbackStub();

    JS_ASSERT(stub->isUnaryArith_Fallback() || stub->isBinaryArith_Fallback());

    if (stub->isUnaryArith_Fallback())
        return stub->toUnaryArith_Fallback()->sawDoubleResult();
    else
        return stub->toBinaryArith_Fallback()->sawDoubleResult();

    return false;
}
