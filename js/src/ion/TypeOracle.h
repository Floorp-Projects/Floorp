/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_type_oracle_h__
#define js_ion_type_oracle_h__

#include "jsscript.h"
#include "IonTypes.h"

namespace js {
namespace ion {

enum LazyArgumentsType {
    MaybeArguments = 0,
    DefinitelyArguments,
    NotArguments
};

class TypeOracle
{
  public:
    struct UnaryTypes {
        types::StackTypeSet *inTypes;
        types::StackTypeSet *outTypes;
    };

    struct BinaryTypes {
        types::StackTypeSet *lhsTypes;
        types::StackTypeSet *rhsTypes;
        types::StackTypeSet *outTypes;
    };

    struct Unary {
        MIRType ival;
        MIRType rval;
    };
    struct Binary {
        MIRType lhs;
        MIRType rhs;
        MIRType rval;
    };

  public:
    virtual UnaryTypes unaryTypes(RawScript script, jsbytecode *pc) = 0;
    virtual BinaryTypes binaryTypes(RawScript script, jsbytecode *pc) = 0;
    virtual Unary unaryOp(RawScript script, jsbytecode *pc) = 0;
    virtual Binary binaryOp(RawScript script, jsbytecode *pc) = 0;
    virtual types::StackTypeSet *thisTypeSet(RawScript script) { return NULL; }
    virtual bool getOsrTypes(jsbytecode *osrPc, Vector<MIRType> &slotTypes) { return true; }
    virtual types::StackTypeSet *parameterTypeSet(RawScript script, size_t index) { return NULL; }
    virtual types::HeapTypeSet *globalPropertyTypeSet(RawScript script, jsbytecode *pc, jsid id) {
        return NULL;
    }
    virtual types::StackTypeSet *propertyRead(RawScript script, jsbytecode *pc) {
        return NULL;
    }
    virtual types::StackTypeSet *propertyReadBarrier(HandleScript script, jsbytecode *pc) {
        return NULL;
    }
    virtual bool propertyReadIdempotent(HandleScript script, jsbytecode *pc, HandleId id) {
        return false;
    }
    virtual bool propertyReadAccessGetter(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual types::HeapTypeSet *globalPropertyWrite(RawScript script, jsbytecode *pc,
                                                jsid id, bool *canSpecialize) {
        *canSpecialize = true;
        return NULL;
    }
    virtual types::StackTypeSet *returnTypeSet(RawScript script, jsbytecode *pc, types::StackTypeSet **barrier) {
        *barrier = NULL;
        return NULL;
    }
    virtual bool inObjectIsDenseNativeWithoutExtraIndexedProperties(HandleScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool inArrayIsPacked(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadIsDenseNative(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadIsTypedArray(RawScript script, jsbytecode *pc, int *arrayType) {
        return false;
    }
    virtual bool elementReadIsString(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadShouldAlwaysLoadDoubles(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadHasExtraIndexedProperty(RawScript, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadIsPacked(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual void elementReadGeneric(RawScript script, jsbytecode *pc, bool *cacheable, bool *monitorResult, bool *intIndex) {
        *cacheable = false;
        *monitorResult = true;
        *intIndex = false;
    }
    virtual bool setElementHasWrittenHoles(RawScript script, jsbytecode *pc) {
        return true;
    }
    virtual bool elementWriteIsDenseNative(HandleScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementWriteIsDenseNative(types::StackTypeSet *obj, types::StackTypeSet *id) {
        return false;
    }
    virtual bool elementWriteIsTypedArray(RawScript script, jsbytecode *pc, int *arrayType) {
        return false;
    }
    virtual bool elementWriteIsTypedArray(types::StackTypeSet *obj, types::StackTypeSet *id, int *arrayType) {
        return false;
    }
    virtual bool elementWriteNeedsDoubleConversion(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementWriteHasExtraIndexedProperty(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementWriteIsPacked(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool arrayResultShouldHaveDoubleConversion(RawScript script, jsbytecode *pc) {
        return false;
    }
    virtual bool propertyWriteCanSpecialize(RawScript script, jsbytecode *pc) {
        return true;
    }
    virtual bool propertyWriteNeedsBarrier(RawScript script, jsbytecode *pc, RawId id) {
        return true;
    }
    virtual bool elementWriteNeedsBarrier(RawScript script, jsbytecode *pc) {
        return true;
    }
    virtual bool elementWriteNeedsBarrier(types::StackTypeSet *obj) {
        return true;
    }
    virtual MIRType elementWrite(RawScript script, jsbytecode *pc) {
        return MIRType_None;
    }

    /* |pc| must be a |JSOP_CALL|. */
    virtual types::StackTypeSet *getCallTarget(RawScript caller, uint32_t argc, jsbytecode *pc) {
        // Same assertion as TypeInferenceOracle::getCallTarget.
        JS_ASSERT(js_CodeSpec[*pc].format & JOF_INVOKE && JSOp(*pc) != JSOP_EVAL);
        return NULL;
    }
    virtual types::StackTypeSet *getCallArg(RawScript script, uint32_t argc, uint32_t arg, jsbytecode *pc) {
        return NULL;
    }
    virtual types::StackTypeSet *getCallReturn(RawScript script, jsbytecode *pc) {
        return NULL;
    }
    virtual bool canInlineCall(HandleScript caller, jsbytecode *pc) {
        return false;
    }
    virtual types::TypeBarrier *callArgsBarrier(HandleScript caller, jsbytecode *pc) {
        return NULL;
    }
    virtual bool canEnterInlinedFunction(RawFunction callee) {
        return false;
    }
    virtual bool callReturnTypeSetMatches(RawScript callerScript, jsbytecode *callerPc,
                                     RawFunction callee)
    {
        return false;
    }
    virtual bool callArgsTypeSetIntersects(types::StackTypeSet *thisType, Vector<types::StackTypeSet *> &argvType, RawFunction callee)
    {
        return false;
    }
    virtual bool callArgsTypeSetMatches(types::StackTypeSet *thisType, Vector<types::StackTypeSet *> &argvType, RawFunction callee)
    {
        return false;
    }
    virtual types::StackTypeSet *aliasedVarBarrier(RawScript script, jsbytecode *pc,
                                                   types::StackTypeSet **barrier)
    {
        return NULL;
    }

    virtual LazyArgumentsType isArgumentObject(types::StackTypeSet *obj) {
        return MaybeArguments;
    }
    virtual LazyArgumentsType propertyReadMagicArguments(RawScript script, jsbytecode *pc) {
        return MaybeArguments;
    }
    virtual LazyArgumentsType elementReadMagicArguments(RawScript script, jsbytecode *pc) {
        return MaybeArguments;
    }
    virtual LazyArgumentsType elementWriteMagicArguments(RawScript script, jsbytecode *pc) {
        return MaybeArguments;
    }
};

class DummyOracle : public TypeOracle
{
  public:
    UnaryTypes unaryTypes(RawScript script, jsbytecode *pc) {
        UnaryTypes u;
        u.inTypes = NULL;
        u.outTypes = NULL;
        return u;
    }
    BinaryTypes binaryTypes(RawScript script, jsbytecode *pc) {
        BinaryTypes b;
        b.lhsTypes = NULL;
        b.rhsTypes = NULL;
        b.outTypes = NULL;
        return b;
    }
    Unary unaryOp(RawScript script, jsbytecode *pc) {
        Unary u;
        u.ival = MIRType_Int32;
        u.rval = MIRType_Int32;
        return u;
    }
    Binary binaryOp(RawScript script, jsbytecode *pc) {
        Binary b;
        b.lhs = MIRType_Int32;
        b.rhs = MIRType_Int32;
        b.rval = MIRType_Int32;
        return b;
    }
};

class TypeInferenceOracle : public TypeOracle
{
    JSContext *cx;
    HeapPtrScript script_;

    MIRType getMIRType(types::StackTypeSet *types);
    MIRType getMIRType(types::HeapTypeSet *types);

    bool analyzeTypesForInlinableCallees(JSContext *cx, JSScript *script,
                                         Vector<JSScript*> &seen);
    bool analyzeTypesForInlinableCallees(JSContext *cx, types::StackTypeSet *calleeTypes,
                                         Vector<JSScript*> &seen);

  public:
    TypeInferenceOracle() : cx(NULL), script_(NULL) {}

    bool init(JSContext *cx, JSScript *script, bool inlinedCall);

    RawScript script() { return script_.get(); }

    UnaryTypes unaryTypes(RawScript script, jsbytecode *pc);
    BinaryTypes binaryTypes(RawScript script, jsbytecode *pc);
    Unary unaryOp(RawScript script, jsbytecode *pc);
    Binary binaryOp(RawScript script, jsbytecode *pc);
    types::StackTypeSet *thisTypeSet(RawScript script);
    bool getOsrTypes(jsbytecode *osrPc, Vector<MIRType> &slotTypes);
    types::StackTypeSet *parameterTypeSet(RawScript script, size_t index);
    types::HeapTypeSet *globalPropertyTypeSet(RawScript script, jsbytecode *pc, jsid id);
    types::StackTypeSet *propertyRead(RawScript script, jsbytecode *pc);
    types::StackTypeSet *propertyReadBarrier(HandleScript script, jsbytecode *pc);
    bool propertyReadIdempotent(HandleScript script, jsbytecode *pc, HandleId id);
    bool propertyReadAccessGetter(RawScript script, jsbytecode *pc);
    types::HeapTypeSet *globalPropertyWrite(RawScript script, jsbytecode *pc, jsid id, bool *canSpecialize);
    types::StackTypeSet *returnTypeSet(RawScript script, jsbytecode *pc, types::StackTypeSet **barrier);
    types::StackTypeSet *getCallTarget(RawScript caller, uint32_t argc, jsbytecode *pc);
    types::StackTypeSet *getCallArg(RawScript caller, uint32_t argc, uint32_t arg, jsbytecode *pc);
    types::StackTypeSet *getCallReturn(RawScript caller, jsbytecode *pc);
    bool inObjectIsDenseNativeWithoutExtraIndexedProperties(HandleScript script, jsbytecode *pc);
    bool inArrayIsPacked(RawScript script, jsbytecode *pc);
    bool elementReadIsDenseNative(RawScript script, jsbytecode *pc);
    bool elementReadIsTypedArray(RawScript script, jsbytecode *pc, int *atype);
    bool elementReadIsString(RawScript script, jsbytecode *pc);
    bool elementReadShouldAlwaysLoadDoubles(RawScript script, jsbytecode *pc);
    bool elementReadHasExtraIndexedProperty(RawScript, jsbytecode *pc);
    bool elementReadIsPacked(RawScript script, jsbytecode *pc);
    void elementReadGeneric(RawScript script, jsbytecode *pc, bool *cacheable, bool *monitorResult, bool *intIndex);
    bool elementWriteIsDenseNative(HandleScript script, jsbytecode *pc);
    bool elementWriteIsDenseNative(types::StackTypeSet *obj, types::StackTypeSet *id);
    bool elementWriteIsTypedArray(RawScript script, jsbytecode *pc, int *arrayType);
    bool elementWriteIsTypedArray(types::StackTypeSet *obj, types::StackTypeSet *id, int *arrayType);
    bool elementWriteNeedsDoubleConversion(RawScript script, jsbytecode *pc);
    bool elementWriteHasExtraIndexedProperty(RawScript script, jsbytecode *pc);
    bool elementWriteIsPacked(RawScript script, jsbytecode *pc);
    bool arrayResultShouldHaveDoubleConversion(RawScript script, jsbytecode *pc);
    bool setElementHasWrittenHoles(RawScript script, jsbytecode *pc);
    bool propertyWriteCanSpecialize(RawScript script, jsbytecode *pc);
    bool propertyWriteNeedsBarrier(RawScript script, jsbytecode *pc, RawId id);
    bool elementWriteNeedsBarrier(RawScript script, jsbytecode *pc);
    bool elementWriteNeedsBarrier(types::StackTypeSet *obj);
    MIRType elementWrite(RawScript script, jsbytecode *pc);
    bool canInlineCall(HandleScript caller, jsbytecode *pc);
    types::TypeBarrier *callArgsBarrier(HandleScript caller, jsbytecode *pc);
    bool canEnterInlinedFunction(RawFunction callee);
    bool callReturnTypeSetMatches(RawScript callerScript, jsbytecode *callerPc, RawFunction callee);
    bool callArgsTypeSetIntersects(types::StackTypeSet *thisType, Vector<types::StackTypeSet *> &argvType, RawFunction callee);
    bool callArgsTypeSetMatches(types::StackTypeSet *thisType, Vector<types::StackTypeSet *> &argvType, RawFunction callee);
    types::StackTypeSet *aliasedVarBarrier(RawScript script, jsbytecode *pc, types::StackTypeSet **barrier);

    LazyArgumentsType isArgumentObject(types::StackTypeSet *obj);
    LazyArgumentsType propertyReadMagicArguments(RawScript script, jsbytecode *pc);
    LazyArgumentsType elementReadMagicArguments(RawScript script, jsbytecode *pc);
    LazyArgumentsType elementWriteMagicArguments(RawScript script, jsbytecode *pc);

};

static inline MIRType
MIRTypeFromValueType(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        return MIRType_Double;
      case JSVAL_TYPE_INT32:
        return MIRType_Int32;
      case JSVAL_TYPE_UNDEFINED:
        return MIRType_Undefined;
      case JSVAL_TYPE_STRING:
        return MIRType_String;
      case JSVAL_TYPE_BOOLEAN:
        return MIRType_Boolean;
      case JSVAL_TYPE_NULL:
        return MIRType_Null;
      case JSVAL_TYPE_OBJECT:
        return MIRType_Object;
      case JSVAL_TYPE_MAGIC:
        return MIRType_Magic;
      case JSVAL_TYPE_UNKNOWN:
        return MIRType_Value;
      default:
        JS_NOT_REACHED("unexpected jsval type");
        return MIRType_None;
    }
}

static inline JSValueType
ValueTypeFromMIRType(MIRType type)
{
  switch (type) {
    case MIRType_Undefined:
      return JSVAL_TYPE_UNDEFINED;
    case MIRType_Null:
      return JSVAL_TYPE_NULL;
    case MIRType_Boolean:
      return JSVAL_TYPE_BOOLEAN;
    case MIRType_Int32:
      return JSVAL_TYPE_INT32;
    case MIRType_Double:
      return JSVAL_TYPE_DOUBLE;
    case MIRType_String:
      return JSVAL_TYPE_STRING;
    case MIRType_Magic:
      return JSVAL_TYPE_MAGIC;
    default:
      JS_ASSERT(type == MIRType_Object);
      return JSVAL_TYPE_OBJECT;
  }
}

static inline JSValueTag
MIRTypeToTag(MIRType type)
{
    return JSVAL_TYPE_TO_TAG(ValueTypeFromMIRType(type));
}

static inline const char *
StringFromMIRType(MIRType type)
{
  switch (type) {
    case MIRType_Undefined:
      return "Undefined";
    case MIRType_Null:
      return "Null";
    case MIRType_Boolean:
      return "Bool";
    case MIRType_Int32:
      return "Int32";
    case MIRType_Double:
      return "Double";
    case MIRType_String:
      return "String";
    case MIRType_Object:
      return "Object";
    case MIRType_Magic:
      return "Magic";
    case MIRType_Value:
      return "Value";
    case MIRType_None:
      return "None";
    case MIRType_Slots:
      return "Slots";
    case MIRType_Elements:
      return "Elements";
    case MIRType_Pointer:
      return "Pointer";
    case MIRType_ForkJoinSlice:
      return "ForkJoinSlice";
    default:
      JS_NOT_REACHED("Unknown MIRType.");
      return "";
  }
}

static inline bool
IsNumberType(MIRType type)
{
    return type == MIRType_Int32 || type == MIRType_Double;
}

static inline bool
IsNullOrUndefined(MIRType type)
{
    return type == MIRType_Null || type == MIRType_Undefined;
}

} /* ion */
} /* js */

#endif // js_ion_type_oracle_h__
