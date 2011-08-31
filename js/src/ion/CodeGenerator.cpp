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
 *   David Anderson <dvander@alliedmods.net>
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

#include "CodeGenerator.h"
#include "IonLinker.h"
#include "MIRGenerator.h"
#include "shared/CodeGenerator-shared-inl.h"
#include "jsnum.h"

using namespace js;
using namespace js::ion;

CodeGenerator::CodeGenerator(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorSpecific(gen, graph)
{
}

bool
CodeGenerator::visitValueToInt32(LValueToInt32 *lir)
{
    ValueOperand operand = ToValue(lir, LValueToInt32::Input);
    Register output = ToRegister(lir->output());

    Assembler::Condition cond;
    Label done, simple, isInt32, isBool, notDouble;

    // Type-check switch.
    cond = masm.testInt32(Assembler::Equal, operand);
    masm.j(cond, &isInt32);
    cond = masm.testBoolean(Assembler::Equal, operand);
    masm.j(cond, &isBool);
    cond = masm.testDouble(Assembler::NotEqual, operand);
    masm.j(cond, &notDouble);

    // If the value is a double, see if it fits in a 32-bit int. We need to ask
    // the platform-specific codegenerator to do this.
    FloatRegister temp = ToFloatRegister(lir->tempFloat());
    masm.unboxDouble(operand, temp);

    Label fails;
    switch (lir->mode()) {
      case LValueToInt32::TRUNCATE:
        emitTruncateDouble(temp, output, &fails);
        break;
      default:
        JS_ASSERT(lir->mode() == LValueToInt32::NORMAL);
        emitDoubleToInt32(temp, output, &fails);
        break;
    }
    if (!bailoutFrom(&fails, lir->snapshot()))
        return false;
    masm.jump(&done);

    masm.bind(&notDouble);

    if (lir->mode() == LValueToInt32::NORMAL) {
        // If the value is not null, it's a string, object, or undefined,
        // which we can't handle here.
        cond = masm.testNull(Assembler::NotEqual, operand);
        if (!bailoutIf(cond, lir->snapshot()))
            return false;
    } else {
        // Test for string or object - then fallthrough to null, which will
        // also handle undefined.
        cond = masm.testObject(Assembler::Equal, operand);
        if (!bailoutIf(cond, lir->snapshot()))
            return false;
        cond = masm.testString(Assembler::Equal, operand);
        if (!bailoutIf(cond, lir->snapshot()))
            return false;
    }
    
    // The value is null - just emit 0.
    masm.mov(Imm32(0), output);
    masm.jump(&done);

    // Just unbox a bool, the result is 0 or 1.
    masm.bind(&isBool);
    masm.unboxBoolean(operand, output);
    masm.jump(&done);

    // Integers can be unboxed.
    masm.bind(&isInt32);
    masm.unboxInt32(operand, output);

    masm.bind(&done);

    return true;
}

static const double DoubleZero = 0.0;

bool
CodeGenerator::visitValueToDouble(LValueToDouble *lir)
{
    ValueOperand operand = ToValue(lir, LValueToDouble::Input);
    FloatRegister output = ToFloatRegister(lir->output());

    Assembler::Condition cond;
    Label isDouble, isInt32, isBool, isNull, done;

    // Type-check switch.
    cond = masm.testDouble(Assembler::Equal, operand);
    masm.j(cond, &isDouble);
    cond = masm.testInt32(Assembler::Equal, operand);
    masm.j(cond, &isInt32);
    cond = masm.testBoolean(Assembler::Equal, operand);
    masm.j(cond, &isBool);
    cond = masm.testNull(Assembler::Equal, operand);
    masm.j(cond, &isNull);

    cond = masm.testUndefined(Assembler::NotEqual, operand);
    if (!bailoutIf(cond, lir->snapshot()))
        return false;
    masm.loadStaticDouble(&js_NaN, output);
    masm.jump(&done);

    masm.bind(&isNull);
    masm.loadStaticDouble(&DoubleZero, output);
    masm.jump(&done);

    masm.bind(&isBool);
    masm.boolValueToDouble(operand, output);
    masm.jump(&done);

    masm.bind(&isInt32);
    masm.int32ValueToDouble(operand, output);
    masm.jump(&done);

    masm.bind(&isDouble);
    masm.unboxDouble(operand, output);
    masm.bind(&done);

    return true;
}

bool
CodeGenerator::visitInt32ToDouble(LInt32ToDouble *lir)
{
    masm.convertInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitTestVAndBranch(LTestVAndBranch *lir)
{
    const ValueOperand value = ToValue(lir, LTestVAndBranch::Input);

    Register tag = splitTagForTest(value);

    Assembler::Condition cond;

    // Eventually we will want some sort of type filter here. For now, just
    // emit all easy cases. For speed we use the cached tag for all comparison,
    // except for doubles, which we test last (as the operation can clobber the
    // tag, which may be in ScratchReg).
    cond = masm.testUndefined(Assembler::Equal, tag);
    masm.j(cond, lir->ifFalse());

    cond = masm.testNull(Assembler::Equal, tag);
    masm.j(cond, lir->ifFalse());

    cond = masm.testObject(Assembler::Equal, tag);
    masm.j(cond, lir->ifTrue());

    Label notBoolean;
    cond = masm.testBoolean(Assembler::NotEqual, tag);
    masm.j(cond, &notBoolean);
    cond = masm.testBooleanTruthy(false, value);
    masm.j(cond, lir->ifFalse());
    masm.jump(lir->ifTrue());
    masm.bind(&notBoolean);

    Label notInt32;
    cond = masm.testInt32(Assembler::NotEqual, tag);
    masm.j(cond, &notInt32);
    cond = masm.testInt32Truthy(false, value);
    masm.j(cond, lir->ifFalse());
    masm.jump(lir->ifTrue());
    masm.bind(&notInt32);

    // Test if a string is non-empty.
    Label notString;
    cond = masm.testString(Assembler::NotEqual, tag);
    masm.j(cond, &notString);
    cond = testStringTruthy(false, value);
    masm.j(cond, lir->ifFalse());
    masm.jump(lir->ifTrue());
    masm.bind(&notString);

    // If we reach here the value is a double.
    masm.unboxDouble(value, ToFloatRegister(lir->tempFloat()));
    cond = masm.testDoubleTruthy(false, ToFloatRegister(lir->tempFloat()));
    masm.j(cond, lir->ifFalse());
    masm.jump(lir->ifTrue());

    return true;
}

bool
CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32 *lir)
{
    Label fails;

    emitTruncateDouble(ToFloatRegister(lir->input()), ToRegister(lir->output()), &fails);
    if (!bailoutFrom(&fails, lir->snapshot()))
        return false;

    return true;
}

bool
CodeGenerator::generateBody()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        current = graph.getBlock(i);
        masm.bind(current->label());
        for (LInstructionIterator iter = current->begin(); iter != current->end(); iter++) {
            if (!iter->accept(this))
                return false;
        }
        if (masm.oom())
            return false;
    }
    return true;
}

bool
CodeGenerator::generate()
{
    JSContext *cx = gen->cx;

    if (frameClass_ != FrameSizeClass::None()) {
        deoptTable_ = cx->compartment->ionCompartment()->getBailoutTable(cx, frameClass_);
        if (!deoptTable_)
            return false;
    }

    if (!generatePrologue())
        return false;
    if (!generateBody())
        return false;
    if (!generateEpilogue())
        return false;
    if (!generateOutOfLineCode())
        return false;

    if (masm.oom())
        return false;

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    JS_ASSERT(!gen->script->ion);

    gen->script->ion = IonScript::New(cx, snapshots_.length(), bailouts_.length(),
                                      graph.numConstants());
    if (!gen->script->ion)
        return false;

    gen->script->ion->setMethod(code);
    gen->script->ion->setDeoptTable(deoptTable_);
    if (snapshots_.length())
        gen->script->ion->copySnapshots(&snapshots_);
    if (bailouts_.length())
        gen->script->ion->copyBailoutTable(&bailouts_[0]);
    if (graph.numConstants())
        gen->script->ion->copyConstants(graph.constantPool());

    linkAbsoluteLabels();

    return true;
}

