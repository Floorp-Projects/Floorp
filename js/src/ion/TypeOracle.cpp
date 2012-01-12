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
TypeInferenceOracle::init(JSContext *cx, JSScript *script)
{
    this->cx = cx;
    this->script = script;
    return script->ensureRanInference(cx);
}

MIRType
TypeInferenceOracle::getMIRType(TypeSet *types)
{
    /* Get the suggested representation to use for values in a given type set. */
    JSValueType type = types->getKnownTypeTag(cx);
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
      default:
        return MIRType_Value;
    }
}

TypeOracle::UnaryTypes
TypeInferenceOracle::unaryTypes(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script);

    UnaryTypes res;
    res.inTypes = script->analysis()->poppedTypes(pc, 0);
    res.outTypes = script->analysis()->pushedTypes(pc, 0);
    return res;
}

TypeOracle::BinaryTypes
TypeInferenceOracle::binaryTypes(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script);

    BinaryTypes res;
    res.lhsTypes = script->analysis()->poppedTypes(pc, 1);
    res.rhsTypes = script->analysis()->poppedTypes(pc, 0);
    res.outTypes = script->analysis()->pushedTypes(pc, 0);
    return res;
}

TypeOracle::Unary
TypeInferenceOracle::unaryOp(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script);

    Unary res;
    res.ival = getMIRType(script->analysis()->poppedTypes(pc, 0));
    res.rval = getMIRType(script->analysis()->pushedTypes(pc, 0));
    return res;
}

TypeOracle::Binary
TypeInferenceOracle::binaryOp(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(script == this->script);
    
    JSOp op = (JSOp)*pc;

    Binary res;
    if ((js_CodeSpec[op].format & JOF_INCDEC)) {
        res.lhs = getMIRType(script->analysis()->pushedTypes(pc, 0));
        res.rhs = MIRType_Int32;
        res.rval = res.lhs;
    } else if (op == JSOP_NEG) {
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

TypeSet *
TypeInferenceOracle::thisTypeSet(JSScript *script)
{
    JS_ASSERT(script == this->script);
    return TypeScript::ThisTypes(script);
}

TypeSet *
TypeInferenceOracle::parameterTypeSet(JSScript *script, size_t index)
{
    JS_ASSERT(script == this->script);
    TypeSet *argTypes = TypeScript::ArgTypes(script, index);
    if (argTypes)
        argTypes->addFreeze(cx);

    return argTypes;
}

TypeSet *
TypeInferenceOracle::propertyRead(JSScript *script, jsbytecode *pc)
{
    return script->analysis()->pushedTypes(pc, 0);
}

TypeSet *
TypeInferenceOracle::propertyReadBarrier(JSScript *script, jsbytecode *pc)
{
    if (script->analysis()->typeBarriers(cx, pc))
        return script->analysis()->bytecodeTypes(pc);
    return NULL;
}

bool
TypeInferenceOracle::elementReadIsDense(JSScript *script, jsbytecode *pc)
{
    // Check whether the object is a dense array and index is int32 or double.
    types::TypeSet *obj = script->analysis()->poppedTypes(pc, 1);
    types::TypeSet *id = script->analysis()->poppedTypes(pc, 0);

    JSValueType objType = obj->getKnownTypeTag(cx);
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag(cx);
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    return !obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY);
}

bool
TypeInferenceOracle::elementReadIsPacked(JSScript *script, jsbytecode *pc)
{
    types::TypeSet *types = script->analysis()->poppedTypes(pc, 1);
    return !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
}

bool
TypeInferenceOracle::elementWriteIsDense(JSScript *script, jsbytecode *pc)
{
    // Check whether the object is a dense array and index is int32 or double.
    types::TypeSet *obj = script->analysis()->poppedTypes(pc, 2);
    types::TypeSet *id = script->analysis()->poppedTypes(pc, 1);

    JSValueType objType = obj->getKnownTypeTag(cx);
    if (objType != JSVAL_TYPE_OBJECT)
        return false;

    JSValueType idType = id->getKnownTypeTag(cx);
    if (idType != JSVAL_TYPE_INT32 && idType != JSVAL_TYPE_DOUBLE)
        return false;

    return !obj->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY);
}

bool
TypeInferenceOracle::elementWriteIsPacked(JSScript *script, jsbytecode *pc)
{
    types::TypeSet *types = script->analysis()->poppedTypes(pc, 2);
    return !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
}

MIRType
TypeInferenceOracle::elementWrite(JSScript *script, jsbytecode *pc)
{
    types::TypeSet *objTypes = script->analysis()->poppedTypes(pc, 2);
    MIRType elementType = MIRType_None;
    unsigned count = objTypes->getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        if (objTypes->getSingleObject(i))
            return MIRType_None;

        if (TypeObject *object = objTypes->getTypeObject(i)) {
            types::TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
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

    // Recompile when other array types are added.
    objTypes->addFreeze(cx);
    return elementType;
}

bool
TypeInferenceOracle::arrayPrototypeHasIndexedProperty()
{
    return ArrayPrototypeHasIndexedProperty(cx, script);
}

bool
TypeInferenceOracle::canInlineCalls()
{
    return script->analysis()->hasFunctionCalls();
}

bool
TypeInferenceOracle::propertyWriteCanSpecialize(JSScript *script, jsbytecode *pc)
{
    return !script->analysis()->getCode(pc).monitoredTypes;
}

bool
TypeInferenceOracle::propertyWriteNeedsBarrier(JSScript *script, jsbytecode *pc, jsid id)
{
    // Return true if SETELEM-like instructions need a write barrier before modifying
    // a property. The object is the third value popped by SETELEM.
    types::TypeSet *types = script->analysis()->poppedTypes(pc, 2);
    return types->propertyNeedsBarrier(cx, id);
}

TypeSet *
TypeInferenceOracle::getCallTarget(JSScript *caller, uint32 argc, jsbytecode *pc)
{
    JS_ASSERT(caller == this->script);
    JS_ASSERT(JSOp(*pc) == JSOP_CALL);

    ScriptAnalysis *analysis = script->analysis();
    return analysis->poppedTypes(pc, argc + 1);
}

bool
TypeInferenceOracle::canEnterInlinedScript(JSScript *inlineScript)
{
    return inlineScript->hasAnalysis() && inlineScript->analysis()->ranInference() &&
        !inlineScript->analysis()->usesScopeChain();
}

TypeSet *
TypeInferenceOracle::globalPropertyWrite(JSScript *script, jsbytecode *pc, jsid id,
                                         bool *canSpecialize)
{
    *canSpecialize = !script->analysis()->getCode(pc).monitoredTypes;
    if (!*canSpecialize)
        return NULL;

    return globalPropertyTypeSet(script, pc, id);
}

TypeSet *
TypeInferenceOracle::returnTypeSet(JSScript *script, jsbytecode *pc, types::TypeSet **barrier)
{
    if (script->analysis()->getCode(pc).monitoredTypesReturn)
        *barrier = script->analysis()->bytecodeTypes(pc);
    else
        *barrier = NULL;
    return script->analysis()->pushedTypes(pc, 0);
}

TypeSet *
TypeInferenceOracle::globalPropertyTypeSet(JSScript *script, jsbytecode *pc, jsid id)
{
    return script->global()->getType(cx)->getProperty(cx, id, false);
}
