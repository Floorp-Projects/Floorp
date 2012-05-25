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
#include "CodeGenerator-x64.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "jsnum.h"
#include "jsscope.h"
#include "jsscopeinlines.h"

using namespace js;
using namespace js::ion;

CodeGeneratorX64::CodeGeneratorX64(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorX86Shared(gen, graph)
{
}

ValueOperand
CodeGeneratorX64::ToValue(LInstruction *ins, size_t pos)
{
    return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand
CodeGeneratorX64::ToOutValue(LInstruction *ins)
{
    return ValueOperand(ToRegister(ins->getDef(0)));
}

bool
CodeGeneratorX64::visitDouble(LDouble *ins)
{
    const LDefinition *out = ins->output();
    masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
    return true;
}

FrameSizeClass
FrameSizeClass::FromDepth(uint32 frameDepth)
{
    return FrameSizeClass::None();
}

uint32
FrameSizeClass::frameSize() const
{
    JS_NOT_REACHED("x64 does not use frame size classes");
    return 0;
}

bool
CodeGeneratorX64::visitValue(LValue *value)
{
    LDefinition *reg = value->getDef(0);
    masm.moveValue(value->value(), ToRegister(reg));
    return true;
}

bool
CodeGeneratorX64::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame  = value->getOperand(0);
    const LDefinition *target = value->getDef(0);

    const ptrdiff_t valueOffset = value->mir()->frameOffset();

    masm.movq(Operand(ToRegister(frame), valueOffset), ToRegister(target));

    return true;
}

static inline JSValueShiftedTag
MIRTypeToShiftedTag(MIRType type)
{
    JS_ASSERT(type != MIRType_Double && type >= MIRType_Boolean);
    return (JSValueShiftedTag) JSVAL_TYPE_TO_SHIFTED_TAG(ValueTypeFromMIRType(type));
}

bool
CodeGeneratorX64::visitBox(LBox *box)
{
    const LAllocation *in = box->getOperand(0);
    const LDefinition *result = box->getDef(0);

    if (box->type() != MIRType_Double) {
        JSValueShiftedTag tag = MIRTypeToShiftedTag(box->type());
        masm.boxValue(tag, ToOperand(in), ToRegister(result));
    } else {
        masm.movqsd(ToFloatRegister(in), ToRegister(result));
    }
    return true;
}

bool
CodeGeneratorX64::visitUnbox(LUnbox *unbox)
{
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    const LDefinition *result = unbox->output();
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
        Assembler::Condition cond;
        switch (mir->type()) {
          case MIRType_Int32:
            cond = masm.testInt32(Assembler::NotEqual, value);
            break;
          case MIRType_Boolean:
            cond = masm.testBoolean(Assembler::NotEqual, value);
            break;
          case MIRType_Object:
            cond = masm.testObject(Assembler::NotEqual, value);
            break;
          case MIRType_String:
            cond = masm.testString(Assembler::NotEqual, value);
            break;
          default:
            JS_NOT_REACHED("Given MIRType cannot be unboxed.");
            return false;
        }
        if (!bailoutIf(cond, unbox->snapshot()))
            return false;
    }

    switch (mir->type()) {
      case MIRType_Int32:
        masm.unboxInt32(value, ToRegister(result));
        break;
      case MIRType_Boolean:
        masm.unboxBoolean(value, ToRegister(result));
        break;
      case MIRType_Object:
        masm.unboxObject(value, ToRegister(result));
        break;
      case MIRType_String:
        masm.unboxString(value, ToRegister(result));
        break;
      default:
        JS_NOT_REACHED("Given MIRType cannot be unboxed.");
        break;
    }
    
    return true;
}

bool
CodeGeneratorX64::visitLoadSlotV(LLoadSlotV *load)
{
    Register dest = ToRegister(load->outputValue());
    Register base = ToRegister(load->input());
    int32 offset = load->mir()->slot() * sizeof(js::Value);

    masm.movq(Operand(base, offset), dest);
    return true;
}

void
CodeGeneratorX64::loadUnboxedValue(Operand source, MIRType type, const LDefinition *dest)
{
    switch (type) {
      case MIRType_Double:
        masm.loadInt32OrDouble(source, ToFloatRegister(dest));
        break;

      case MIRType_Object:
      case MIRType_String:
      {
        Register out = ToRegister(dest);
        masm.movq(source, out);
        masm.unboxObject(ValueOperand(out), out);
        break;
      }

      case MIRType_Int32:
      case MIRType_Boolean:
        masm.movl(source, ToRegister(dest));
        break;

      default:
        JS_NOT_REACHED("unexpected type");
    }
}

bool
CodeGeneratorX64::visitLoadSlotT(LLoadSlotT *load)
{
    Register base = ToRegister(load->input());
    int32 offset = load->mir()->slot() * sizeof(js::Value);

    loadUnboxedValue(Operand(base, offset), load->mir()->type(), load->output());

    return true;
}

void
CodeGeneratorX64::storeUnboxedValue(const LAllocation *value, MIRType valueType,
                                    Operand dest, MIRType slotType)
{
    if (valueType == MIRType_Double) {
        masm.movsd(ToFloatRegister(value), dest);
        return;
    }

    // For known integers and booleans, we can just store the unboxed value if
    // the slot has the same type.
    if ((valueType == MIRType_Int32 || valueType == MIRType_Boolean) &&
        slotType == valueType) {
        if (value->isConstant()) {
            Value val = *value->toConstant();
            if (valueType == MIRType_Int32)
                masm.movl(Imm32(val.toInt32()), dest);
            else
                masm.movl(Imm32(val.toBoolean() ? 1 : 0), dest);
        } else {
            masm.movl(ToRegister(value), dest);
        }
        return;
    }

    if (value->isConstant()) {
        masm.moveValue(*value->toConstant(), ScratchReg);
        masm.movq(ScratchReg, dest);
    } else {
        JSValueShiftedTag tag = MIRTypeToShiftedTag(valueType);
        masm.boxValue(tag, ToOperand(value), ScratchReg);
        masm.movq(ScratchReg, dest);
    }
}

bool
CodeGeneratorX64::visitStoreSlotT(LStoreSlotT *store)
{
    Register base = ToRegister(store->slots());
    int32 offset = store->mir()->slot() * sizeof(js::Value);

    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();
    MIRType slotType = store->mir()->slotType();

    if (store->mir()->needsBarrier())
        masm.emitPreBarrier(Address(base, offset), slotType);

    storeUnboxedValue(value, valueType, Operand(base, offset), slotType);
    return true;
}

bool
CodeGeneratorX64::visitLoadElementT(LLoadElementT *load)
{
    Operand source = createArrayElementOperand(ToRegister(load->elements()), load->index());
    loadUnboxedValue(source, load->mir()->type(), load->output());

    JS_ASSERT(!load->mir()->needsHoleCheck());
    return true;
}


void
CodeGeneratorX64::storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                                    const Register &elements, const LAllocation *index)
{
    Operand dest = createArrayElementOperand(elements, index);
    storeUnboxedValue(value, valueType, dest, elementType);
}

bool
CodeGeneratorX64::visitImplicitThis(LImplicitThis *lir)
{
    Register callee = ToRegister(lir->callee());

    // The implicit |this| is always |undefined| if the function's environment
    // is the current global.
    GlobalObject *global = gen->info().script()->global();
    masm.cmpPtr(Operand(callee, JSFunction::offsetOfEnvironment()), ImmGCPtr(global));

    // TODO: OOL stub path.
    if (!bailoutIf(Assembler::NotEqual, lir->snapshot()))
        return false;

    masm.moveValue(UndefinedValue(), ToOutValue(lir));
    return true;
}

bool
CodeGeneratorX64::visitRecompileCheck(LRecompileCheck *lir)
{
    // Bump the script's use count and bailout if the script is hot. Note that
    // it's safe to bake in this pointer since scripts are never nursery
    // allocated and jitcode will be purged before doing a compacting GC.
    const uint32_t *useCount = gen->info().script()->addressOfUseCount();
    masm.movq(ImmWord(useCount), ScratchReg);

    Operand addr(ScratchReg, 0);
    masm.addl(Imm32(1), addr);
    masm.cmpl(addr, Imm32(js_IonOptions.usesBeforeInlining));
    if (!bailoutIf(Assembler::AboveOrEqual, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorX64::visitInterruptCheck(LInterruptCheck *lir)
{
    typedef bool (*pf)(JSContext *);
    static const VMFunction interruptCheckInfo = FunctionInfo<pf>(InterruptCheck);

    OutOfLineCode *ool = oolCallVM(interruptCheckInfo, lir, (ArgList()), StoreNothing());
    if (!ool)
        return false;

    void *interrupt = (void*)&gen->cx->runtime->interrupt;
    masm.movq(ImmWord(interrupt), ScratchReg);
    masm.cmpl(Operand(ScratchReg, 0), Imm32(0));
    masm.j(Assembler::NonZero, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

