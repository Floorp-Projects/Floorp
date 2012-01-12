/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef js_ion_type_oracle_h__
#define js_ion_type_oracle_h__

#include "jsscript.h"
#include "IonTypes.h"

namespace js {
namespace ion {

// The ordering of this enumeration is important: Anything < Value is a
// specialized type. Furthermore, anything < String has trivial conversion to
// a number.
enum MIRType
{
    MIRType_Undefined,
    MIRType_Null,
    MIRType_Boolean,
    MIRType_Int32,
    MIRType_Double,
    MIRType_String,
    MIRType_Object,
    MIRType_Value,
    MIRType_Any,        // Any type.
    MIRType_None,       // Invalid, used as a placeholder.
    MIRType_Slots,      // A slots vector
    MIRType_Elements,   // An elements vector
    MIRType_StackFrame  // StackFrame pointer for OSR.
};

class TypeOracle
{
  public:
    struct UnaryTypes {
        types::TypeSet *inTypes;
        types::TypeSet *outTypes;
    };

    struct BinaryTypes {
        types::TypeSet *lhsTypes;
        types::TypeSet *rhsTypes;
        types::TypeSet *outTypes;
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
    virtual UnaryTypes unaryTypes(JSScript *script, jsbytecode *pc) = 0;
    virtual BinaryTypes binaryTypes(JSScript *script, jsbytecode *pc) = 0;
    virtual Unary unaryOp(JSScript *script, jsbytecode *pc) = 0;
    virtual Binary binaryOp(JSScript *script, jsbytecode *pc) = 0;
    virtual types::TypeSet *thisTypeSet(JSScript *script) { return NULL; }
    virtual types::TypeSet *parameterTypeSet(JSScript *script, size_t index) { return NULL; }
    virtual types::TypeSet *globalPropertyTypeSet(JSScript *script, jsbytecode *pc, jsid id) {
        return NULL;
    }
    virtual types::TypeSet *propertyRead(JSScript *script, jsbytecode *pc) {
        return NULL;
    }
    virtual types::TypeSet *propertyReadBarrier(JSScript *script, jsbytecode *pc) {
        return NULL;
    }
    virtual types::TypeSet *globalPropertyWrite(JSScript *script, jsbytecode *pc,
                                                jsid id, bool *canSpecialize) {
        *canSpecialize = true;
        return NULL;
    }
    virtual types::TypeSet *returnTypeSet(JSScript *script, jsbytecode *pc, types::TypeSet **barrier) {
        *barrier = NULL;
        return NULL;
    }
    virtual bool elementReadIsDense(JSScript *script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementReadIsPacked(JSScript *script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementWriteIsDense(JSScript *script, jsbytecode *pc) {
        return false;
    }
    virtual bool elementWriteIsPacked(JSScript *script, jsbytecode *pc) {
        return false;
    }
    virtual bool propertyWriteCanSpecialize(JSScript *script, jsbytecode *pc) {
        return true;
    }
    virtual bool propertyWriteNeedsBarrier(JSScript *script, jsbytecode *pc, jsid id) {
        return true;
    }
    virtual MIRType elementWrite(JSScript *script, jsbytecode *pc) {
        return MIRType_None;
    }
    virtual bool arrayPrototypeHasIndexedProperty() {
        return true;
    }
    virtual bool canInlineCalls() {
        return false;
    }

    /* |pc| must be a |JSOP_CALL|. */
    virtual types::TypeSet *getCallTarget(JSScript *caller, uint32 argc, jsbytecode *pc) {
        JS_ASSERT(JSOp(*pc) == JSOP_CALL);
        return NULL;
    }

    virtual bool canEnterInlinedScript(JSScript *callee) {
        return false;
    }
};

class DummyOracle : public TypeOracle
{
  public:
    UnaryTypes unaryTypes(JSScript *script, jsbytecode *pc) {
        UnaryTypes u;
        u.inTypes = NULL;
        u.outTypes = NULL;
        return u;
    }
    BinaryTypes binaryTypes(JSScript *script, jsbytecode *pc) {
        BinaryTypes b;
        b.lhsTypes = NULL;
        b.rhsTypes = NULL;
        b.outTypes = NULL;
        return b;
    }
    Unary unaryOp(JSScript *script, jsbytecode *pc) {
        Unary u;
        u.ival = MIRType_Int32;
        u.rval = MIRType_Int32;
        return u;
    }
    Binary binaryOp(JSScript *script, jsbytecode *pc) {
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
    JSScript *script;

    MIRType getMIRType(types::TypeSet *types);

  public:
    TypeInferenceOracle() : cx(NULL), script(NULL) {}

    bool init(JSContext *cx, JSScript *script);

    UnaryTypes unaryTypes(JSScript *script, jsbytecode *pc);
    BinaryTypes binaryTypes(JSScript *script, jsbytecode *pc);
    Unary unaryOp(JSScript *script, jsbytecode *pc);
    Binary binaryOp(JSScript *script, jsbytecode *pc);
    types::TypeSet *thisTypeSet(JSScript *script);
    types::TypeSet *parameterTypeSet(JSScript *script, size_t index);
    types::TypeSet *globalPropertyTypeSet(JSScript *script, jsbytecode *pc, jsid id);
    types::TypeSet *propertyRead(JSScript *script, jsbytecode *pc);
    types::TypeSet *propertyReadBarrier(JSScript *script, jsbytecode *pc);
    types::TypeSet *globalPropertyWrite(JSScript *script, jsbytecode *pc, jsid id, bool *canSpecialize);
    types::TypeSet *returnTypeSet(JSScript *script, jsbytecode *pc, types::TypeSet **barrier);
    types::TypeSet *getCallTarget(JSScript *caller, uint32 argc, jsbytecode *pc);
    bool elementReadIsDense(JSScript *script, jsbytecode *pc);
    bool elementReadIsPacked(JSScript *script, jsbytecode *pc);
    bool elementWriteIsDense(JSScript *script, jsbytecode *pc);
    bool elementWriteIsPacked(JSScript *script, jsbytecode *pc);
    bool propertyWriteCanSpecialize(JSScript *script, jsbytecode *pc);
    bool propertyWriteNeedsBarrier(JSScript *script, jsbytecode *pc, jsid id);
    MIRType elementWrite(JSScript *script, jsbytecode *pc);
    bool arrayPrototypeHasIndexedProperty();
    bool canInlineCalls();
    bool canEnterInlinedScript(JSScript *inlineScript);
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
    default:
      JS_ASSERT(type == MIRType_Object);
      return JSVAL_TYPE_OBJECT;
  }
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
    case MIRType_Value:
      return "Value";
    case MIRType_Any:
      return "Any";
    case MIRType_None:
      return "None";
    case MIRType_Slots:
      return "Slots";
    case MIRType_Elements:
      return "Elements";
    case MIRType_StackFrame:
      return "StackFrame";
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

} /* ion */
} /* js */

#endif // js_ion_type_oracle_h__

