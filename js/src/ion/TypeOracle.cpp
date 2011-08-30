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

#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::ion;
using namespace js::types;

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

    Binary res;
    res.lhs = getMIRType(script->analysis()->poppedTypes(pc, 1));
    res.rhs = getMIRType(script->analysis()->poppedTypes(pc, 0));
    res.rval = getMIRType(script->analysis()->pushedTypes(pc, 0));
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
    return TypeScript::ArgTypes(script, index);
}
