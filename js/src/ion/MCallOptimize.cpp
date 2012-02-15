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

void
IonBuilder::discardCallArgs(uint32 argc, MDefinition **argv)
{
    // Bytecode order: Function, This, Arg0, Arg1, ..., ArgN, Call.
    // Copy PassArg arguments from ArgN to This.
    for (int32 i = argc; i >= 0; i--) {
        MPassArg *passArg = current->pop()->toPassArg();
        MBasicBlock *block = passArg->block();
        MDefinition *wrapped = passArg->getArgument();
        passArg->replaceAllUsesWith(wrapped);
        block->remove(passArg);
        argv[i] = wrapped;
    }

    // Discard function it would be removed by DCE if it is not captured by a
    // resume point.
    current->pop();
}

bool
IonBuilder::optimizeNativeCall(uint32 argc)
{
    /* Ensure that the function is a native function. */
    types::TypeSet *calleeTypes = oracle->getCallTarget(script, argc, pc);
    if (!calleeTypes)
        return false;

    JSObject *funObject = calleeTypes->getSingleton(cx);
    if (!funObject)
        return false;

    if (!funObject->isFunction())
        return false;
    JSFunction *fun = funObject->toFunction();

    JSNative native = fun->maybeNative();
    if (!native)
        return false;

    /* Check if there is a match for the current native function */

    types::TypeSet *returnTypes = oracle->getCallReturn(script, pc);
    MIRType returnType = MIRTypeFromValueType(returnTypes->getKnownTypeTag(cx));

    if (argc == 0)
        return false;

    types::TypeSet *arg1Types = oracle->getCallArg(script, argc, 1, pc);
    MIRType arg1Type = MIRTypeFromValueType(arg1Types->getKnownTypeTag(cx));
    if (argc == 1) {
        MDefinition *argv[2];
        if (native == js_math_abs) {
            // argThis == MPassArg(MConstant(Math))
            if ((arg1Type == MIRType_Double || arg1Type == MIRType_Int32) &&
                arg1Type == returnType) {
                discardCallArgs(argc, argv);
                MAbs *ins = MAbs::New(argv[1], returnType);
                current->add(ins);
                current->push(ins);
                return true;
            }
        }
        if (native == js_math_floor) {
            // argThis == MPassArg(MConstant(Math))
            if (arg1Type == MIRType_Double && returnType == MIRType_Int32) {
                discardCallArgs(argc, argv);
                MRound *ins = new MRound(argv[1], MRound::RoundingMode_Floor);
                current->add(ins);
                current->push(ins);
                return true;
            }
            if (arg1Type == MIRType_Int32 && returnType == MIRType_Int32) {
                // i == Math.floor(i)
                discardCallArgs(argc, argv);
                current->push(argv[1]);
                return true;
            }
        }
        if (native == js_math_round) {
            // argThis == MPassArg(MConstant(Math))
            if (arg1Type == MIRType_Double && returnType == MIRType_Int32) {
                discardCallArgs(argc, argv);
                MRound *ins = new MRound(argv[1], MRound::RoundingMode_Round);
                current->add(ins);
                current->push(ins);
                return true;
            }
            if (arg1Type == MIRType_Int32 && returnType == MIRType_Int32) {
                // i == Math.round(i)
                discardCallArgs(argc, argv);
                current->push(argv[1]);
                return true;
            }
        }
        // TODO: js_math_ceil
        return false;
    }

    return false;
}

} // namespace ion
} // namespace js
