/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineInspector.h"

#include "mozilla/DebugOnly.h"

#include "jit/BaselineIC.h"
#include "jit/CacheIRCompiler.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/ObjectGroup-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

bool
SetElemICInspector::sawOOBDenseWrite() const
{
    if (!icEntry_)
        return false;

    // Check for an element adding stub.
    for (ICStub* stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (stub->isSetElem_DenseOrUnboxedArrayAdd())
            return true;
    }

    // Check for a write hole bit on the SetElem_Fallback stub.
    ICStub* stub = icEntry_->fallbackStub();
    if (stub->isSetElem_Fallback())
        return stub->toSetElem_Fallback()->hasArrayWriteHole();

    return false;
}

bool
SetElemICInspector::sawOOBTypedArrayWrite() const
{
    if (!icEntry_)
        return false;

    // Check for SetElem_TypedArray stubs with expectOutOfBounds set.
    for (ICStub* stub = icEntry_->firstStub(); stub; stub = stub->next()) {
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
    for (ICStub* stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (stub->isSetElem_DenseOrUnboxedArrayAdd() || stub->isSetElem_DenseOrUnboxedArray())
            return true;
    }
    return false;
}

bool
SetElemICInspector::sawTypedArrayWrite() const
{
    if (!icEntry_)
        return false;

    // Check for a SetElem_TypedArray stub.
    for (ICStub* stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (stub->isSetElem_TypedArray())
            return true;
    }
    return false;
}

template <typename S, typename T>
static bool
VectorAppendNoDuplicate(S& list, T value)
{
    for (size_t i = 0; i < list.length(); i++) {
        if (list[i] == value)
            return true;
    }
    return list.append(value);
}

static bool
AddReceiver(const ReceiverGuard& receiver,
            BaselineInspector::ReceiverVector& receivers,
            BaselineInspector::ObjectGroupVector& convertUnboxedGroups)
{
    if (receiver.group && receiver.group->maybeUnboxedLayout()) {
        if (receiver.group->unboxedLayout().nativeGroup())
            return VectorAppendNoDuplicate(convertUnboxedGroups, receiver.group);
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
    if (!reader.matchOp(CacheOp::GuardIsObject, objId))
        return false;

    if (reader.matchOp(CacheOp::GuardGroup, objId)) {
        receiver->group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

        if (!reader.matchOp(CacheOp::GuardAndLoadUnboxedExpando, objId))
            return false;
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
    if (!reader.matchOp(CacheOp::GuardIsObject, objId))
        return false;

    if (!reader.matchOp(CacheOp::GuardGroup, objId))
        return false;
    receiver->group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    return reader.matchOp(CacheOp::LoadUnboxedPropertyResult, objId);
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

    if (!hasBaselineScript())
        return true;

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
        } else if (stub->isSetProp_Native()) {
            receiver = ReceiverGuard(stub->toSetProp_Native()->group(),
                                     stub->toSetProp_Native()->shape());
        } else if (stub->isSetProp_Unboxed()) {
            receiver = ReceiverGuard(stub->toSetProp_Unboxed()->group(), nullptr);
        } else {
            receivers.clear();
            return true;
        }

        if (!AddReceiver(receiver, receivers, convertUnboxedGroups))
            return false;

        stub = stub->next();
    }

    if (stub->isGetProp_Fallback()) {
        if (stub->toGetProp_Fallback()->hadUnoptimizableAccess())
            receivers.clear();
    } else {
        if (stub->toSetProp_Fallback()->hadUnoptimizableAccess())
            receivers.clear();
    }

    // Don't inline if there are more than 5 receivers.
    if (receivers.length() > 5)
        receivers.clear();

    return true;
}

ICStub*
BaselineInspector::monomorphicStub(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return nullptr;

    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    ICStub* next = stub->next();

    if (!next || !next->isFallback())
        return nullptr;

    return stub;
}

bool
BaselineInspector::dimorphicStub(jsbytecode* pc, ICStub** pfirst, ICStub** psecond)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    ICStub* next = stub->next();
    ICStub* after = next ? next->next() : nullptr;

    if (!after || !after->isFallback())
        return false;

    *pfirst = stub;
    *psecond = next;
    return true;
}

MIRType
BaselineInspector::expectedResultType(jsbytecode* pc)
{
    // Look at the IC entries for this op to guess what type it will produce,
    // returning MIRType::None otherwise.

    ICStub* stub = monomorphicStub(pc);
    if (!stub)
        return MIRType::None;

    switch (stub->kind()) {
      case ICStub::BinaryArith_Int32:
        if (stub->toBinaryArith_Int32()->allowDouble())
            return MIRType::Double;
        return MIRType::Int32;
      case ICStub::BinaryArith_BooleanWithInt32:
      case ICStub::UnaryArith_Int32:
      case ICStub::BinaryArith_DoubleWithInt32:
        return MIRType::Int32;
      case ICStub::BinaryArith_Double:
      case ICStub::UnaryArith_Double:
        return MIRType::Double;
      case ICStub::BinaryArith_StringConcat:
      case ICStub::BinaryArith_StringObjectConcat:
        return MIRType::String;
      default:
        return MIRType::None;
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
BaselineInspector::expectedCompareType(jsbytecode* pc)
{
    ICStub* first = monomorphicStub(pc);
    ICStub* second = nullptr;
    if (!first && !dimorphicStub(pc, &first, &second))
        return MCompare::Compare_Unknown;

    if (ICStub* fallback = second ? second->next() : first->next()) {
        MOZ_ASSERT(fallback->isFallback());
        if (fallback->toCompare_Fallback()->hadUnoptimizableAccess())
            return MCompare::Compare_Unknown;
    }

    if (CanUseInt32Compare(first->kind()) && (!second || CanUseInt32Compare(second->kind()))) {
        ICCompare_Int32WithBoolean* coerce =
            first->isCompare_Int32WithBoolean()
            ? first->toCompare_Int32WithBoolean()
            : ((second && second->isCompare_Int32WithBoolean())
               ? second->toCompare_Int32WithBoolean()
               : nullptr);
        if (coerce) {
            return coerce->lhsIsInt32()
                   ? MCompare::Compare_Int32MaybeCoerceRHS
                   : MCompare::Compare_Int32MaybeCoerceLHS;
        }
        return MCompare::Compare_Int32;
    }

    if (CanUseDoubleCompare(first->kind()) && (!second || CanUseDoubleCompare(second->kind()))) {
        ICCompare_NumberWithUndefined* coerce =
            first->isCompare_NumberWithUndefined()
            ? first->toCompare_NumberWithUndefined()
            : (second && second->isCompare_NumberWithUndefined())
              ? second->toCompare_NumberWithUndefined()
              : nullptr;
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
TryToSpecializeBinaryArithOp(ICStub** stubs,
                             uint32_t nstubs,
                             MIRType* result)
{
    DebugOnly<bool> sawInt32 = false;
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
    if (!hasBaselineScript())
        return MIRType::None;

    MIRType result;
    ICStub* stubs[2];

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();
    if (stub->isBinaryArith_Fallback() &&
        stub->toBinaryArith_Fallback()->hadUnoptimizableOperands())
    {
        return MIRType::None;
    }

    stubs[0] = monomorphicStub(pc);
    if (stubs[0]) {
        if (TryToSpecializeBinaryArithOp(stubs, 1, &result))
            return result;
    }

    if (dimorphicStub(pc, &stubs[0], &stubs[1])) {
        if (TryToSpecializeBinaryArithOp(stubs, 2, &result))
            return result;
    }

    return MIRType::None;
}

bool
BaselineInspector::hasSeenNonNativeGetElement(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    if (stub->isGetElem_Fallback())
        return stub->toGetElem_Fallback()->hasNonNativeAccess();
    return false;
}

bool
BaselineInspector::hasSeenNegativeIndexGetElement(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    if (stub->isGetElem_Fallback())
        return stub->toGetElem_Fallback()->hasNegativeIndex();
    return false;
}

bool
BaselineInspector::hasSeenAccessedGetter(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback())
        return stub->toGetProp_Fallback()->hasAccessedGetter();
    return false;
}

bool
BaselineInspector::hasSeenNonStringIterMore(jsbytecode* pc)
{
    MOZ_ASSERT(JSOp(*pc) == JSOP_MOREITER);

    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    return stub->toIteratorMore_Fallback()->hasNonStringResult();
}

bool
BaselineInspector::hasSeenDoubleResult(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.fallbackStub();

    MOZ_ASSERT(stub->isUnaryArith_Fallback() || stub->isBinaryArith_Fallback());

    if (stub->isUnaryArith_Fallback())
        return stub->toUnaryArith_Fallback()->sawDoubleResult();
    else
        return stub->toBinaryArith_Fallback()->sawDoubleResult();

    return false;
}

JSObject*
BaselineInspector::getTemplateObject(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return nullptr;

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
            if (JSObject* obj = stub->toCall_Scripted()->templateObject())
                return obj;
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
    if (!hasBaselineScript())
        return nullptr;

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

    if (!hasBaselineScript())
        return nullptr;

    const ICEntry& entry = icEntryFromPC(pc);
    ICStub* stub = entry.firstStub();

    if (entry.fallbackStub()->toCall_Fallback()->hadUnoptimizableCall())
        return nullptr;

    if (!stub->isCall_Scripted() || stub->next() != entry.fallbackStub())
        return nullptr;

    return stub->toCall_Scripted()->callee();
}

JSObject*
BaselineInspector::getTemplateObjectForNative(jsbytecode* pc, Native native)
{
    if (!hasBaselineScript())
        return nullptr;

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCall_Native() && stub->toCall_Native()->callee()->native() == native)
            return stub->toCall_Native()->templateObject();
    }

    return nullptr;
}

bool
BaselineInspector::isOptimizableCallStringSplit(jsbytecode* pc, JSString** strOut, JSString** sepOut,
                                                JSObject** objOut)
{
    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);

    // If StringSplit stub is attached, must have only one stub attached.
    if (entry.fallbackStub()->numOptimizedStubs() != 1)
        return false;

    ICStub* stub = entry.firstStub();
    if (stub->kind() != ICStub::Call_StringSplit)
        return false;

    *strOut = stub->toCall_StringSplit()->expectedStr();
    *sepOut = stub->toCall_StringSplit()->expectedSep();
    *objOut = stub->toCall_StringSplit()->templateObject();
    return true;
}

JSObject*
BaselineInspector::getTemplateObjectForClassHook(jsbytecode* pc, const Class* clasp)
{
    if (!hasBaselineScript())
        return nullptr;

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCall_ClassHook() && stub->toCall_ClassHook()->clasp() == clasp)
            return stub->toCall_ClassHook()->templateObject();
    }

    return nullptr;
}

JSObject*
BaselineInspector::getTemplateObjectForSimdCtor(jsbytecode* pc, SimdType simdType)
{
    if (!hasBaselineScript())
        return nullptr;

    const ICEntry& entry = icEntryFromPC(pc);
    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isCall_ClassHook() && stub->toCall_ClassHook()->clasp() == &SimdTypeDescr::class_) {
            JSObject* templateObj = stub->toCall_ClassHook()->templateObject();
            InlineTypedObject& typedObj = templateObj->as<InlineTypedObject>();
            if (typedObj.typeDescr().as<SimdTypeDescr>().type() == simdType)
                return templateObj;
        }
    }

    return nullptr;
}

LexicalEnvironmentObject*
BaselineInspector::templateNamedLambdaObject()
{
    if (!hasBaselineScript())
        return nullptr;

    JSObject* res = baselineScript()->templateEnvironment();
    if (script->bodyScope()->hasEnvironment())
        res = res->enclosingEnvironment();
    MOZ_ASSERT(res);

    return &res->as<LexicalEnvironmentObject>();
}

CallObject*
BaselineInspector::templateCallObject()
{
    if (!hasBaselineScript())
        return nullptr;

    JSObject* res = baselineScript()->templateEnvironment();
    MOZ_ASSERT(res);

    return &res->as<CallObject>();
}

static Shape*
GlobalShapeForGetPropFunction(ICStub* stub)
{
    if (stub->isGetProp_CallNativeGlobal()) {
        ICGetProp_CallNativeGlobal* nstub = stub->toGetProp_CallNativeGlobal();
        if (nstub->isOwnGetter())
            return nullptr;

        Shape* shape = nstub->globalShape();
        MOZ_ASSERT(shape->getObjectClass()->flags & JSCLASS_IS_GLOBAL);
        return shape;
    }

    return nullptr;
}

static bool
MatchCacheIRReceiverGuard(CacheIRReader& reader, ICCacheIR_Monitored* stub, ObjOperandId objId,
                          ReceiverGuard* receiver)
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
        receiver->shape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());
        return true;
    }

    if (!reader.matchOp(CacheOp::GuardGroup, objId))
        return false;
    receiver->group = stub->stubInfo()->getStubField<ObjectGroup*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::GuardAndLoadUnboxedExpando, objId)) {
        // Second case, just a group guard.
        reader.matchOp(CacheOp::GuardNoUnboxedExpando, objId);
        return true;
    }

    // Third case.
    ObjOperandId expandoId = reader.objOperandId();
    if (!reader.matchOp(CacheOp::GuardShape, expandoId))
        return false;

    receiver->shape = stub->stubInfo()->getStubField<Shape*>(stub, reader.stubOffset());
    return true;
}

static bool
AddCacheIRGetPropFunction(ICCacheIR_Monitored* stub, bool innerized,
                          JSObject** holder, Shape** holderShape,
                          JSFunction** commonGetter, Shape** globalShape, bool* isOwnProperty,
                          BaselineInspector::ReceiverVector& receivers,
                          BaselineInspector::ObjectGroupVector& convertUnboxedGroups)
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
    if (!reader.matchOp(CacheOp::GuardIsObject, objId))
        return false;

    if (innerized) {
        if (!reader.matchOp(CacheOp::GuardClass, objId) ||
            reader.guardClassKind() != GuardClassKind::WindowProxy)
        {
            return false;
        }
        if (!reader.matchOp(CacheOp::LoadObject))
            return false;
        objId = reader.objOperandId();
        DebugOnly<JSObject*> obj =
            stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset()).get();
        MOZ_ASSERT(obj->is<GlobalObject>());
    }

    ReceiverGuard receiver;
    if (!MatchCacheIRReceiverGuard(reader, stub, objId, &receiver))
        return false;

    if (reader.matchOp(CacheOp::CallScriptedGetterResult, objId) ||
        reader.matchOp(CacheOp::CallNativeGetterResult, objId))
    {
        // This is an own property getter, the first case.
        MOZ_ASSERT(receiver.shape);
        MOZ_ASSERT(!receiver.group);

        size_t offset = reader.stubOffset();
        JSFunction* getter =
            &stub->stubInfo()->getStubField<JSObject*>(stub, offset)->as<JSFunction>();

        if (*commonGetter && (!*isOwnProperty || *globalShape || *holderShape != receiver.shape))
            return false;

        MOZ_ASSERT_IF(*commonGetter, *commonGetter == getter);
        *holder = nullptr;
        *holderShape = receiver.shape;
        *commonGetter = getter;
        *isOwnProperty = true;
        return true;
    }

    if (!reader.matchOp(CacheOp::LoadObject))
        return false;
    ObjOperandId holderId = reader.objOperandId();
    JSObject* obj = stub->stubInfo()->getStubField<JSObject*>(stub, reader.stubOffset());

    if (!reader.matchOp(CacheOp::GuardShape, holderId))
        return false;
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

    if (!AddReceiver(receiver, receivers, convertUnboxedGroups))
        return false;

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
    if (!hasBaselineScript())
        return false;

    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());

    *globalShape = nullptr;
    *commonGetter = nullptr;
    const ICEntry& entry = icEntryFromPC(pc);

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isGetProp_CallNativeGlobal()) {
            ICGetPropCallGetter* nstub = static_cast<ICGetPropCallGetter*>(stub);
            bool isOwn = nstub->isOwnGetter();
            if (!isOwn && !AddReceiver(nstub->receiverGuard(), receivers, convertUnboxedGroups))
                return false;

            if (!*commonGetter) {
                *holder = isOwn ? nullptr : nstub->holder().get();
                *holderShape = nstub->holderShape();
                *commonGetter = nstub->getter();
                *globalShape = GlobalShapeForGetPropFunction(nstub);
                *isOwnProperty = isOwn;
            } else if (nstub->holderShape() != *holderShape ||
                       GlobalShapeForGetPropFunction(nstub) != *globalShape ||
                       isOwn != *isOwnProperty)
            {
                return false;
            } else {
                MOZ_ASSERT(*commonGetter == nstub->getter());
            }
        } else if (stub->isCacheIR_Monitored()) {
            if (!AddCacheIRGetPropFunction(stub->toCacheIR_Monitored(), innerized,
                                           holder, holderShape,
                                           commonGetter, globalShape, isOwnProperty, receivers,
                                           convertUnboxedGroups))
            {
                return false;
            }
        } else if (stub->isGetProp_Fallback()) {
            // If we have an unoptimizable access, don't try to optimize.
            if (stub->toGetProp_Fallback()->hadUnoptimizableAccess())
                return false;
        } else if (stub->isGetName_Fallback()) {
            if (stub->toGetName_Fallback()->hadUnoptimizableAccess())
                return false;
        } else {
            return false;
        }
    }

    if (!*commonGetter)
        return false;

    MOZ_ASSERT(*isOwnProperty == !*holder);
    MOZ_ASSERT(*isOwnProperty == (receivers.empty() && convertUnboxedGroups.empty()));
    return true;
}

bool
BaselineInspector::commonSetPropFunction(jsbytecode* pc, JSObject** holder, Shape** holderShape,
                                         JSFunction** commonSetter, bool* isOwnProperty,
                                         ReceiverVector& receivers,
                                         ObjectGroupVector& convertUnboxedGroups)
{
    if (!hasBaselineScript())
        return false;

    MOZ_ASSERT(receivers.empty());
    MOZ_ASSERT(convertUnboxedGroups.empty());

    *commonSetter = nullptr;
    const ICEntry& entry = icEntryFromPC(pc);

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        if (stub->isSetProp_CallScripted() || stub->isSetProp_CallNative()) {
            ICSetPropCallSetter* nstub = static_cast<ICSetPropCallSetter*>(stub);
            bool isOwn = nstub->isOwnSetter();
            if (!isOwn && !AddReceiver(nstub->receiverGuard(), receivers, convertUnboxedGroups))
                return false;

            if (!*commonSetter) {
                *holder = isOwn ? nullptr : nstub->holder().get();
                *holderShape = nstub->holderShape();
                *commonSetter = nstub->setter();
                *isOwnProperty = isOwn;
            } else if (nstub->holderShape() != *holderShape || isOwn != *isOwnProperty) {
                return false;
            } else {
                MOZ_ASSERT(*commonSetter == nstub->setter());
            }
        } else if (!stub->isSetProp_Fallback() ||
                   stub->toSetProp_Fallback()->hadUnoptimizableAccess())
        {
            // We have an unoptimizable access, so don't try to optimize.
            return false;
        }
    }

    if (!*commonSetter)
        return false;

    MOZ_ASSERT(*isOwnProperty == !*holder);
    return true;
}

static MIRType
GetCacheIRExpectedInputType(ICCacheIR_Monitored* stub)
{
    CacheIRReader reader(stub->stubInfo());

    if (reader.matchOp(CacheOp::GuardIsObject, ValOperandId(0)))
        return MIRType::Object;
    if (reader.matchOp(CacheOp::GuardIsString, ValOperandId(0)))
        return MIRType::String;
    if (reader.matchOp(CacheOp::GuardType, ValOperandId(0))) {
        JSValueType type = reader.valueType();
        return MIRTypeFromValueType(type);
    }
    MOZ_CRASH("Unexpected instruction");
}

MIRType
BaselineInspector::expectedPropertyAccessInputType(jsbytecode* pc)
{
    if (!hasBaselineScript())
        return MIRType::Value;

    const ICEntry& entry = icEntryFromPC(pc);
    MIRType type = MIRType::None;

    for (ICStub* stub = entry.firstStub(); stub; stub = stub->next()) {
        MIRType stubType;
        switch (stub->kind()) {
          case ICStub::GetProp_Fallback:
            if (stub->toGetProp_Fallback()->hadUnoptimizableAccess())
                return MIRType::Value;
            continue;

          case ICStub::GetElem_Fallback:
            if (stub->toGetElem_Fallback()->hadUnoptimizableAccess())
                return MIRType::Value;
            continue;

          case ICStub::GetProp_Generic:
            return MIRType::Value;

          case ICStub::CacheIR_Monitored:
            stubType = GetCacheIRExpectedInputType(stub->toCacheIR_Monitored());
            if (stubType == MIRType::Value)
                return MIRType::Value;
            break;

          default:
            MOZ_CRASH("Unexpected stub");
        }

        if (type != MIRType::None) {
            if (type != stubType)
                return MIRType::Value;
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

    if (!hasBaselineScript())
        return false;

    const ICEntry& entry = icEntryFromPC(pc);

    ICStub* stub = entry.firstStub();
    if (!stub->isInstanceOf_Function() ||
        !stub->next()->isInstanceOf_Fallback() ||
        stub->next()->toInstanceOf_Fallback()->hadUnoptimizableAccess())
    {
        return false;
    }

    ICInstanceOf_Function* optStub = stub->toInstanceOf_Function();
    *shape = optStub->shape();
    *prototypeObject = optStub->prototypeObject();
    *slot = optStub->slot();

    if (IsInsideNursery(*prototypeObject))
        return false;

    return true;
}
