/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TypeOracle.h"

#include "IonSpewer.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsanalyze.h"

using namespace js;
using namespace js::ion;
using namespace js::types;
using namespace js::analyze;

bool
TypeInferenceOracle::init(JSContext *cx, HandleScript script)
{
    this->cx = cx;
    this->script_.init(script);
    return JSScript::ensureRanInference(cx, script);
}

MIRType
GetMIRType(JSValueType type)
{
    /* Get the suggested representation to use for values in a given type set. */
    switch (type) {
      case JSVAL_TYPE_UNDEFINED:
        return MIRType_Undefined;
      case JSVAL_TYPE_NULL:
        return MIRType_Null;
      case JSVAL_TYPE_BOOLEAN:
        return MIRType_Boolean;
      case JSVAL_TYPE_INT32:
        return MIRType_Int32;
      case JSVAL_TYPE_DOUBLE:
        return MIRType_Double;
      case JSVAL_TYPE_STRING:
        return MIRType_String;
      case JSVAL_TYPE_OBJECT:
        return MIRType_Object;
      case JSVAL_TYPE_MAGIC:
        return MIRType_Magic;
      default:
        return MIRType_Value;
    }
}

MIRType
TypeInferenceOracle::getMIRType(StackTypeSet *types)
{
    return GetMIRType(types->getKnownTypeTag());
}

MIRType
TypeInferenceOracle::getMIRType(HeapTypeSet *types)
{
    return GetMIRType(types->getKnownTypeTag(cx));
}

TypeOracle::UnaryTypes
TypeInferenceOracle::unaryTypes(UnrootedScript script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script());

    UnaryTypes res;
    res.inTypes = script->analysis()->poppedTypes(pc, 0);
    res.outTypes = script->analysis()->pushedTypes(pc, 0);
    return res;
}

TypeOracle::BinaryTypes
TypeInferenceOracle::binaryTypes(UnrootedScript script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script());

    JSOp op = (JSOp)*pc;

    BinaryTypes res;
    if (op == JSOP_NEG || op == JSOP_POS) {
        res.lhsTypes = script->analysis()->poppedTypes(pc, 0);
        res.rhsTypes = NULL;
        res.outTypes = script->analysis()->pushedTypes(pc, 0);
    } else {
        res.lhsTypes = script->analysis()->poppedTypes(pc, 1);
        res.rhsTypes = script->analysis()->poppedTypes(pc, 0);
        res.outTypes = script->analysis()->pushedTypes(pc, 0);
    }
    return res;
}

TypeOracle::Unary
TypeInferenceOracle::unaryOp(UnrootedScript script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script());

    Unary res;
    res.ival = getMIRType(script->analysis()->poppedTypes(pc, 0));
    res.rval = getMIRType(script->analysis()->pushedTypes(pc, 0));
    return res;
}

TypeOracle::Binary
TypeInferenceOracle::binaryOp(UnrootedScript script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script());

    JSOp op = (JSOp)*pc;

    Binary res;
    if (op == JSOP_NEG || op == JSOP_POS) {
        res.lhs = getMIRType(script->analysis()->poppedTypes(pc, 0));
        res.rhs = MIRType_Int32;
        res.rval = getMIRType(script->analysis()->pushedTypes(pc, 0));
    } else {
        res.lhs = getMIRType(script->analysis()->poppedTypes(pc, 1));
        res.rhs = getMIRType(script->analysis()->poppedTypes(pc, 0));
        res.rval = getMIRType(script->analysis()->pushedTypes(pc, 0));
    }
    return res;
}

StackTypeSet *
TypeInferenceOracle::thisTypeSet(UnrootedScript script)
{
    JS_ASSERT(script == this->script());
    return TypeScript::ThisTypes(script);
}

bool
TypeInferenceOracle::getOsrTypes(jsbytecode *osrPc, Vector<MIRType> &slotTypes)
{
    JS_ASSERT(JSOp(*osrPc) == JSOP_LOOPENTRY);
    JS_ASSERT(script()->code < osrPc);
    JS_ASSERT(osrPc < script()->code + script()->length);

    Vector<types::StackTypeSet *> slotTypeSets(cx);
    if (!slotTypeSets.resize(TotalSlots(script())))
        return false;

    for (uint32_t slot = ThisSlot(); slot < TotalSlots(script()); slot++)
        slotTypeSets[slot] = TypeScript::SlotTypes(script(), slot);

    jsbytecode *pc = script()->code;
    ScriptAnalysis *analysis = script()->analysis();

    // To determine the slot types at the OSR pc, we have to do a forward walk
    // over the bytecode to reconstruct the types.
    for (;;) {
        Bytecode *opinfo = analysis->maybeCode(pc);
        if (opinfo) {
            if (opinfo->jumpTarget) {
                // Update variable types for all new values at this bytecode.
                if (const SlotValue *newv = analysis->newValues(pc)) {
                    while (newv->slot) {
                        if (newv->slot < TotalSlots(script()))
                            slotTypeSets[newv->slot] = analysis->getValueTypes(newv->value);
                        newv++;
                    }
                }
            }

            if (BytecodeUpdatesSlot(JSOp(*pc))) {
                uint32_t slot = GetBytecodeSlot(script(), pc);
                if (analysis->trackSlot(slot))
                    slotTypeSets[slot] = analysis->pushedTypes(pc, 0);
            }
        }

        if (pc == osrPc)
            break;

        pc += GetBytecodeLength(pc);
    }

    JS_ASSERT(pc == osrPc);

    // TI always includes the |this| slot, but Ion only does so for function
    // scripts. This means we have to subtract 1 for global/eval scripts.
    JS_ASSERT(ThisSlot() == 1);
    JS_ASSERT(ArgSlot(0) == 2);

#ifdef DEBUG
    uint32_t stackDepth = analysis->getCode(osrPc).stackDepth;
#endif

    if (script()->function()) {
        JS_ASSERT(slotTypes.length() == TotalSlots(script()) + stackDepth);

        for (size_t i = ThisSlot(); i < TotalSlots(script()); i++)
            slotTypes[i] = getMIRType(slotTypeSets[i]);
    } else {
        JS_ASSERT(slotTypes.length() == TotalSlots(script()) + stackDepth - 1);

        for (size_t i = ArgSlot(0); i < TotalSlots(script()); i++)
            slotTypes[i - 1] = getMIRType(slotTypeSets[i]);
    }

    return true;
}

StackTypeSet *
TypeInferenceOracle::parameterTypeSet(UnrootedScript script, size_t index)
{
    JS_ASSERT(script == this->script());
    return TypeScript::ArgTypes(script, index);
}

StackTypeSet *
TypeInferenceOracle::propertyRead(UnrootedScript script, jsbytecode *pc)
{
    return script->analysis()->pushedTypes(pc, 0);
}

StackTypeSet *
TypeInferenceOracle::propertyReadBarrier(HandleScript script, jsbytecode *pc)
{
    if (script->analysis()->typeBarriers(cx, pc))
        return script->analysis()->bytecodeTypes(pc);
    return NULL;
}

bool
TypeInferenceOracle::propertyReadIdempotent(HandleScript script, jsbytecode *pc, HandleId id)
{
    if (script->analysis()->getCode(pc).notIdempotent)
        return false;

    if (id != MakeTypeId(cx, id))
        return false;

    StackTypeSet *types = script->analysis()->poppedTypes(pc, 0);
    if (!types || types->unknownObject())
        return false;

    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        if (types->getSingleObject(i))
            return false;

        if (TypeObject *obj = types->getTypeObject(i)) {
            if (obj->unknownProperties())
                return false;

            // Check if the property has been reconfigured or is a getter.
            HeapTypeSet *propertyTypes = obj->getProperty(cx, id, false);
            if (!propertyTypes || propertyTypes->isOwnProperty(cx, obj, true))
                return false;
        }
    }

    return true;
}

bool
TypeInferenceOracle::propertyReadAccessGetter(UnrootedScript script, jsbytecode *pc)
{
    return script->analysis()->getCode(pc).accessGetter;
}

bool
TypeInferenceOracle::inObjectIsDenseArray(HandleScript script, jsbytecode *pc)
{
    // Check whether the object is a dense array and index is int32 or double.
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 1);
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 0);

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    JSValueType objType = obj->getKnownTypeTag();
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    return !obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY);
}

bool
TypeInferenceOracle::inArrayIsPacked(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *types = script->analysis()->poppedTypes(pc, 0);
    return !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
}

bool
TypeInferenceOracle::elementReadIsDenseArray(UnrootedScript script, jsbytecode *pc)
{
    // Check whether the object is a dense array and index is int32 or double.
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 1);
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 0);

    JSValueType objType = obj->getKnownTypeTag();
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    return !obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY);
}

bool
TypeInferenceOracle::elementReadIsTypedArray(UnrootedScript script, jsbytecode *pc, int *arrayType)
{
    // Check whether the object is a typed array and index is int32 or double.
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 1);
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 0);

    JSValueType objType = obj->getKnownTypeTag();
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    if (obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY))
        return false;

    *arrayType = obj->getTypedArrayType();
    if (*arrayType == TypedArray::TYPE_MAX)
        return false;

    JS_ASSERT(*arrayType >= 0 && *arrayType < TypedArray::TYPE_MAX);

    // Unlike dense arrays, the types of elements in typed arrays are not
    // guaranteed to be present in the object's type, and we need to use
    // knowledge about the possible contents of the array vs. the types
    // that have been read out of it to figure out how to do the load.
    types::TypeSet *result = propertyRead(script, pc);
    if (*arrayType == TypedArray::TYPE_FLOAT32 || *arrayType == TypedArray::TYPE_FLOAT64) {
        if (!result->hasType(types::Type::DoubleType()))
            return false;
    } else {
        if (!result->hasType(types::Type::Int32Type()))
            return false;
    }

    return true;
}

bool
TypeInferenceOracle::elementReadIsString(UnrootedScript script, jsbytecode *pc)
{
    // Check for string[index].
    StackTypeSet *value = script->analysis()->poppedTypes(pc, 1);
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 0);

    if (value->getKnownTypeTag() != JSVAL_TYPE_STRING)
        return false;

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    // This function is used for jsop_getelem_string which should return
    // undefined if this is out-side the string bounds. Currently we just
    // fallback to a CallGetElement.
    StackTypeSet *pushed = script->analysis()->pushedTypes(pc, 0);
    if (pushed->getKnownTypeTag() != JSVAL_TYPE_STRING)
        return false;

    return true;
}

bool
TypeInferenceOracle::elementReadIsPacked(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *types = script->analysis()->poppedTypes(pc, 1);
    return !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
}

void
TypeInferenceOracle::elementReadGeneric(UnrootedScript script, jsbytecode *pc, bool *cacheable, bool *monitorResult)
{
    MIRType obj = getMIRType(script->analysis()->poppedTypes(pc, 1));
    MIRType id = getMIRType(script->analysis()->poppedTypes(pc, 0));

    *cacheable = (obj == MIRType_Object &&
                  (id == MIRType_Value || id == MIRType_Int32 || id == MIRType_String));

    // Turn off cacheing if the element is int32 and we've seen non-native objects as the target
    // of this getelem.
    if (*cacheable && id == MIRType_Int32 && script->analysis()->getCode(pc).nonNativeGetElement)
        *cacheable = false;

    if (*cacheable)
        *monitorResult = (id == MIRType_String || script->analysis()->getCode(pc).getStringElement);
    else
        *monitorResult = true;
}

bool
TypeInferenceOracle::elementWriteIsDenseArray(HandleScript script, jsbytecode *pc)
{
    // Check whether the object is a dense array and index is int32 or double.
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 2);
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 1);

    JSValueType objType = obj->getKnownTypeTag();
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    return !obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY);
}

bool
TypeInferenceOracle::elementWriteIsTypedArray(UnrootedScript script, jsbytecode *pc, int *arrayType)
{
    // Check whether the object is a dense array and index is int32 or double.
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 2);
    StackTypeSet *id = script->analysis()->poppedTypes(pc, 1);

    JSValueType objType = obj->getKnownTypeTag();
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag();
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    if (obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY))
        return false;

    *arrayType = obj->getTypedArrayType();
    if (*arrayType == TypedArray::TYPE_MAX)
        return false;

    return true;
}

bool
TypeInferenceOracle::elementWriteIsPacked(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *types = script->analysis()->poppedTypes(pc, 2);
    return !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
}

bool
TypeInferenceOracle::setElementHasWrittenHoles(UnrootedScript script, jsbytecode *pc)
{
    return script->analysis()->getCode(pc).arrayWriteHole;
}

MIRType
TypeInferenceOracle::elementWrite(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *objTypes = script->analysis()->poppedTypes(pc, 2);
    MIRType elementType = MIRType_None;
    unsigned count = objTypes->getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        if (objTypes->getSingleObject(i))
            return MIRType_None;

        if (TypeObject *object = objTypes->getTypeObject(i)) {
            types::HeapTypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
            if (!elementTypes)
                return MIRType_None;

            MIRType type = getMIRType(elementTypes);
            if (type == MIRType_None)
                return MIRType_None;

            if (elementType == MIRType_None)
                elementType = type;
            else if (elementType != type)
                return MIRType_None;
        }
    }

    return elementType;
}

bool
TypeInferenceOracle::arrayPrototypeHasIndexedProperty()
{
    RootedScript scriptRoot(cx, script());
    return ArrayPrototypeHasIndexedProperty(cx, scriptRoot);
}

bool
TypeInferenceOracle::canInlineCalls()
{
    return script()->analysis()->hasFunctionCalls();
}

bool
TypeInferenceOracle::propertyWriteCanSpecialize(UnrootedScript script, jsbytecode *pc)
{
    return !script->analysis()->getCode(pc).monitoredTypes;
}

bool
TypeInferenceOracle::propertyWriteNeedsBarrier(UnrootedScript script, jsbytecode *pc, jsid id)
{
    StackTypeSet *types = script->analysis()->poppedTypes(pc, 1);
    return types->propertyNeedsBarrier(cx, id);
}

bool
TypeInferenceOracle::elementWriteNeedsBarrier(UnrootedScript script, jsbytecode *pc)
{
    // Return true if SETELEM-like instructions need a write barrier before modifying
    // a property. The object is the third value popped by SETELEM.
    StackTypeSet *types = script->analysis()->poppedTypes(pc, 2);
    return types->propertyNeedsBarrier(cx, JSID_VOID);
}

StackTypeSet *
TypeInferenceOracle::getCallTarget(UnrootedScript caller, uint32_t argc, jsbytecode *pc)
{
    JS_ASSERT(caller == this->script());
    JS_ASSERT(js_CodeSpec[*pc].format & JOF_INVOKE && JSOp(*pc) != JSOP_EVAL);

    ScriptAnalysis *analysis = script()->analysis();
    return analysis->poppedTypes(pc, argc + 1);
}

StackTypeSet *
TypeInferenceOracle::getCallArg(UnrootedScript script, uint32_t argc, uint32_t arg, jsbytecode *pc)
{
    JS_ASSERT(argc >= arg);
    // Bytecode order: Function, This, Arg0, Arg1, ..., ArgN, Call.
    // |argc| does not include |this|.
    return script->analysis()->poppedTypes(pc, argc - arg);
}

StackTypeSet *
TypeInferenceOracle::getCallReturn(UnrootedScript script, jsbytecode *pc)
{
    return script->analysis()->pushedTypes(pc, 0);
}

bool
TypeInferenceOracle::canInlineCall(HandleScript caller, jsbytecode *pc)
{
    JS_ASSERT(types::IsInlinableCall(pc));

    Bytecode *code = caller->analysis()->maybeCode(pc);
    if (code->monitoredTypes || code->monitoredTypesReturn || caller->analysis()->typeBarriers(cx, pc))
        return false;
    return true;
}

bool
TypeInferenceOracle::canEnterInlinedFunction(JSFunction *target)
{
    AssertCanGC();
    RootedScript script(cx, target->nonLazyScript());
    if (!script->hasAnalysis() || !script->analysis()->ranInference())
        return false;

    if (!script->analysis()->ionInlineable())
        return false;

    if (script->analysis()->usesScopeChain())
        return false;

    if (target->getType(cx)->unknownProperties())
        return false;

    // TI calls ObjectStateChange to trigger invalidation of the caller.
    HeapTypeSet::WatchObjectStateChange(cx, target->getType(cx));
    return true;
}

HeapTypeSet *
TypeInferenceOracle::globalPropertyWrite(UnrootedScript script, jsbytecode *pc, jsid id,
                                         bool *canSpecialize)
{
    *canSpecialize = !script->analysis()->getCode(pc).monitoredTypes;
    if (!*canSpecialize)
        return NULL;

    return globalPropertyTypeSet(script, pc, id);
}

StackTypeSet *
TypeInferenceOracle::returnTypeSet(UnrootedScript script, jsbytecode *pc, types::StackTypeSet **barrier)
{
    if (script->analysis()->getCode(pc).monitoredTypesReturn)
        *barrier = script->analysis()->bytecodeTypes(pc);
    else
        *barrier = NULL;
    return script->analysis()->pushedTypes(pc, 0);
}

StackTypeSet *
TypeInferenceOracle::aliasedVarBarrier(UnrootedScript script, jsbytecode *pc, types::StackTypeSet **barrier)
{
    *barrier = script->analysis()->bytecodeTypes(pc);
    return script->analysis()->pushedTypes(pc, 0);
}

HeapTypeSet *
TypeInferenceOracle::globalPropertyTypeSet(UnrootedScript script, jsbytecode *pc, jsid id)
{
    TypeObject *type = script->global().getType(cx);
    if (type->unknownProperties())
        return NULL;

    return type->getProperty(cx, id, false);
}

LazyArgumentsType
TypeInferenceOracle::isArgumentObject(types::StackTypeSet *obj)
{
    if (obj->isMagicArguments())
        return DefinitelyArguments;
    if (obj->hasAnyFlag(TYPE_FLAG_LAZYARGS))
        return MaybeArguments;
    return NotArguments;
}

LazyArgumentsType
TypeInferenceOracle::propertyReadMagicArguments(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 0);
    return isArgumentObject(obj);
}

LazyArgumentsType
TypeInferenceOracle::elementReadMagicArguments(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 1);
    return isArgumentObject(obj);
}

LazyArgumentsType
TypeInferenceOracle::elementWriteMagicArguments(UnrootedScript script, jsbytecode *pc)
{
    StackTypeSet *obj = script->analysis()->poppedTypes(pc, 2);
    return isArgumentObject(obj);
}
