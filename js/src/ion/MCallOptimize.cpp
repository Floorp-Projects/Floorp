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
 * The Original Code is Mozilla SpiderMonkey JavaScript (IonMonkey).
 *
 * The Initial Developer of the Original Code is
 *   Nicolas Pierron <nicolas.b.pierron@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nicolas B. Pierron <nicolas.b.pierron@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jslibmath.h"
#include "jsmath.h"

#include "MIR.h"
#include "MIRGraph.h"
#include "IonBuilder.h"

namespace js {
namespace ion {

IonBuilder::InliningStatus
IonBuilder::inlineNativeCall(JSNative native, uint32 argc, bool constructing)
{
    // Array natives.
    if (native == js_Array)
        return inlineArray(argc, constructing);
    if (native == js::array_pop)
        return inlineArrayPopShift(MArrayPopShift::Pop, argc, constructing);
    if (native == js::array_shift)
        return inlineArrayPopShift(MArrayPopShift::Shift, argc, constructing);
    if (native == js::array_push)
        return inlineArrayPush(argc, constructing);

    // Math natives.
    if (native == js_math_abs)
        return inlineMathAbs(argc, constructing);
    if (native == js_math_floor)
        return inlineMathFloor(argc, constructing);
    if (native == js_math_round)
        return inlineMathRound(argc, constructing);
    if (native == js_math_sqrt)
        return inlineMathSqrt(argc, constructing);
    if (native == js::math_sin)
        return inlineMathFunction(MMathFunction::Sin, argc, constructing);
    if (native == js::math_cos)
        return inlineMathFunction(MMathFunction::Cos, argc, constructing);
    if (native == js::math_tan)
        return inlineMathFunction(MMathFunction::Tan, argc, constructing);
    if (native == js::math_log)
        return inlineMathFunction(MMathFunction::Log, argc, constructing);

    // String natives.
    if (native == js_str_charCodeAt)
        return inlineStrCharCodeAt(argc, constructing);
    if (native == js::str_fromCharCode)
        return inlineStrFromCharCode(argc, constructing);
    if (native == js_str_charAt)
        return inlineStrCharAt(argc, constructing);

    return InliningStatus_NotInlined;
}

bool
IonBuilder::discardCallArgs(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!argv.resizeUninitialized(argc + 1))
        return false;

    for (int32 i = argc; i >= 0; i--) {
        // Unwrap each MPassArg, replacing it with its contents.
        MPassArg *passArg = bb->pop()->toPassArg();
        MBasicBlock *block = passArg->block();
        MDefinition *wrapped = passArg->getArgument();
        passArg->replaceAllUsesWith(wrapped);
        block->discard(passArg);

        // Remember contents in vector.
        argv[i] = wrapped;
    }

    return true;
}

bool
IonBuilder::discardCall(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!discardCallArgs(argc, argv, bb))
        return false;

    // Function MDefinition implicitly consumed by inlining.
    bb->pop();
    return true;
}

types::TypeSet*
IonBuilder::getInlineReturnTypeSet()
{
    types::TypeSet *barrier;
    types::TypeSet *returnTypes = oracle->returnTypeSet(script, pc, &barrier);

    JS_ASSERT(returnTypes);
    return returnTypes;
}

MIRType
IonBuilder::getInlineReturnType()
{
    types::TypeSet *returnTypes = getInlineReturnTypeSet();
    return MIRTypeFromValueType(returnTypes->getKnownTypeTag(cx));
}

types::TypeSet*
IonBuilder::getInlineArgTypeSet(uint32 argc, uint32 arg)
{
    types::TypeSet *argTypes = oracle->getCallArg(script, argc, arg, pc);
    JS_ASSERT(argTypes);
    return argTypes;
}

MIRType
IonBuilder::getInlineArgType(uint32 argc, uint32 arg)
{
    types::TypeSet *argTypes = getInlineArgTypeSet(argc, arg);
    return MIRTypeFromValueType(argTypes->getKnownTypeTag(cx));
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFunction(MMathFunction::Function function, uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (!IsNumberType(getInlineArgType(argc, 1)))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MMathFunction *ins = MMathFunction::New(argv[1], function);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArray(uint32 argc, bool constructing)
{
    // Multiple arguments imply array initialization, not just construction.
    if (argc >= 2)
        return InliningStatus_NotInlined;

    uint32_t initLength = 0;

    // A single integer argument denotes initial length.
    if (argc == 1) {
        if (getInlineArgType(argc, 1) != MIRType_Int32)
            return InliningStatus_NotInlined;
        MDefinition *arg = current->peek(-1)->toPassArg()->getArgument();
        if (!arg->isConstant())
            return InliningStatus_NotInlined;

        // Negative lengths generate a RangeError, unhandled by the inline path.
        initLength = arg->toConstant()->value().toInt32();
        if (initLength >= JSObject::NELEMENTS_LIMIT)
            return InliningStatus_NotInlined;
    }

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    types::TypeObject *type = types::TypeScript::InitObject(cx, script, pc, JSProto_Array);
    MNewArray *ins = new MNewArray(initLength, type, MNewArray::NewArray_Unallocating);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayPopShift(MArrayPopShift::Mode mode, uint32 argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (returnType == MIRType_Undefined || returnType == MIRType_Null)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;

    // Pop and shift are only handled for dense arrays that have never been
    // used in an iterator: popping elements does not account for suppressing
    // deleted properties in active iterators.
    //
    // Inference's TypeConstraintCall generates the constraints that propagate
    // properties directly into the result type set.
    types::TypeObjectFlags unhandledFlags =
        types::OBJECT_FLAG_NON_DENSE_ARRAY | types::OBJECT_FLAG_ITERATED;

    types::TypeSet *thisTypes = getInlineArgTypeSet(argc, 0);
    if (thisTypes->hasObjectFlags(cx, unhandledFlags))
        return InliningStatus_NotInlined;
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    types::TypeSet *returnTypes = getInlineReturnTypeSet();
    bool needsHoleCheck = thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
    bool maybeUndefined = returnTypes->hasType(types::Type::UndefinedType());

    MArrayPopShift *ins = MArrayPopShift::New(argv[0], mode, needsHoleCheck, maybeUndefined);
    current->add(ins);
    current->push(ins);
    ins->setResultType(returnType);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayPush(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;

    // Inference's TypeConstraintCall generates the constraints that propagate
    // properties directly into the result type set.
    types::TypeSet *thisTypes = getInlineArgTypeSet(argc, 0);
    if (thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY))
        return InliningStatus_NotInlined;
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MArrayPush *ins = MArrayPush::New(argv[0], argv[1]);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathAbs(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = getInlineArgType(argc, 1);
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;
    if (argType != returnType)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MAbs *ins = MAbs::New(argv[1], returnType);
    current->add(ins);
    current->push(ins);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFloor(uint32 argc, bool constructing)
{  
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    MIRType argType = getInlineArgType(argc, 1);
    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;

    // Math.floor(int(x)) == int(x)
    if (argType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        current->push(argv[1]);
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        MFloor *ins = new MFloor(argv[1]);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathRound(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = getInlineArgType(argc, 1);

    // Math.round(int(x)) == int(x)
    if (argType == MIRType_Int32 && returnType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        current->push(argv[1]);
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double && returnType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        MRound *ins = new MRound(argv[1]);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathSqrt(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    MIRType argType = getInlineArgType(argc, 1);
    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (argType != MIRType_Double && argType != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MSqrt *sqrt = MSqrt::New(argv[1]);
    current->add(sqrt);
    current->push(sqrt);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharCodeAt(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MInstruction *index = MToInt32::New(argv[1]);
    current->add(index);

    MStringLength *length = MStringLength::New(argv[0]);
    current->add(length);

    index = addBoundsCheck(index, length);

    MCharCodeAt *charCode = MCharCodeAt::New(argv[0], index);
    current->add(charCode);
    current->push(charCode);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrFromCharCode(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MToInt32 *charCode = MToInt32::New(argv[1]);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharAt(uint32 argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MInstruction *index = MToInt32::New(argv[1]);
    current->add(index);

    MStringLength *length = MStringLength::New(argv[0]);
    current->add(length);

    index = addBoundsCheck(index, length);

    // String.charAt(x) = String.fromCharCode(String.charCodeAt(x))
    MCharCodeAt *charCode = MCharCodeAt::New(argv[0], index);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

} // namespace ion
} // namespace js
