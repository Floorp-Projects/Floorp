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

#include "LIR.h"
#include "Lowering.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "IonSpewer.h"
#include "jsanalyze.h"
#include "jsbool.h"
#include "jsnum.h"
#include "jsobjinlines.h"
#include "shared/Lowering-shared-inl.h"

using namespace js;
using namespace ion;

bool
LIRGenerator::visitParameter(MParameter *param)
{
    ptrdiff_t offset;
    if (param->index() == MParameter::THIS_SLOT)
        offset = THIS_FRAME_SLOT;
    else
        offset = 1 + param->index();

    LParameter *ins = new LParameter;
    if (!defineBox(ins, param, LDefinition::PRESET))
        return false;

    offset *= sizeof(Value);
#if defined(JS_NUNBOX32)
# if defined(IS_BIG_ENDIAN)
    ins->getDef(0)->setOutput(LArgument(offset));
    ins->getDef(1)->setOutput(LArgument(offset + 4));
# else
    ins->getDef(0)->setOutput(LArgument(offset + 4));
    ins->getDef(1)->setOutput(LArgument(offset));
# endif
#elif defined(JS_PUNBOX64)
    ins->getDef(0)->setOutput(LArgument(offset));
#endif

    return true;
}

bool
LIRGenerator::visitCallee(MCallee *callee)
{
    LCallee *ins = new LCallee;
    if (!define(ins, callee, LDefinition::PRESET))
        return false;

    ins->getDef(0)->setOutput(LArgument(-sizeof(IonJSFrameLayout)
                                        + IonJSFrameLayout::offsetOfCalleeToken()));

    return true;
}

bool
LIRGenerator::visitGoto(MGoto *ins)
{
    return add(new LGoto(ins->target()));
}

bool
LIRGenerator::visitCheckOverRecursed(MCheckOverRecursed *ins)
{
    LCheckOverRecursed *lir = new LCheckOverRecursed(temp(LDefinition::GENERAL));

    if (!assignSafepoint(lir, ins))
        return false;
    return add(lir);
}

bool
LIRGenerator::visitNewArray(MNewArray *ins)
{
    LNewArray *lir = new LNewArray();

    if (!defineVMReturn(lir, ins))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitPrepareCall(MPrepareCall *ins)
{
    allocateArguments(ins->argc());
    return true;
}

bool
LIRGenerator::visitPassArg(MPassArg *arg)
{
    MDefinition *opd = arg->getArgument();
    JS_ASSERT(opd->type() == MIRType_Value);

    uint32 argslot = getArgumentSlot(arg->getArgnum());

    LStackArg *stack = new LStackArg(argslot);
    if (!useBox(stack, 0, opd))
        return false;

    // Pass through the virtual register of the operand.
    // This causes snapshots to correctly copy the operand on the stack.
    // 
    // This keeps the backing store around longer than strictly required.
    // We could do better by informing snapshots about the argument vector.
    arg->setVirtualRegister(opd->virtualRegister());

    return add(stack);
}

bool
LIRGenerator::visitCall(MCall *call)
{
    uint32 argc = call->argc();
    JS_ASSERT(call->getFunction()->type() == MIRType_Object);

    JS_ASSERT(CallTempReg1 != CallTempReg2);
    JS_ASSERT(CallTempReg1 != ArgumentsRectifierReg);
    JS_ASSERT(CallTempReg2 != ArgumentsRectifierReg);

    // Height of the current argument vector.
    uint32 argslot = getArgumentSlotForCall();

    // A call is entirely stateful, depending upon arguments already being
    // stored in an argument vector. Therefore visitCall() may be generic.
    LCallGeneric *ins = new LCallGeneric(useFixed(call->getFunction(), CallTempReg1),
                                         argslot,
                                         tempFixed(ArgumentsRectifierReg),
                                         tempFixed(CallTempReg2));
    if (!defineReturn(ins, call))
        return false;
    if (!assignSnapshot(ins))
        return false;
    if (!assignSafepoint(ins, call))
        return false;

    freeArguments(argc);
    return true;
}

static JSOp
ReorderComparison(JSOp op, MDefinition **lhsp, MDefinition **rhsp)
{
    MDefinition *lhs = *lhsp;
    MDefinition *rhs = *rhsp;

    if (lhs->isConstant()) {
        *rhsp = lhs;
        *lhsp = rhs;
        return js::analyze::ReverseCompareOp(op);
    }
    return op;
}

bool
LIRGenerator::visitTest(MTest *test)
{
    MDefinition *opd = test->getOperand(0);
    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    if (opd->type() == MIRType_Value) {
        LTestVAndBranch *lir = new LTestVAndBranch(ifTrue, ifFalse, tempFloat());
        if (!useBox(lir, LTestVAndBranch::Input, opd))
            return false;
        return add(lir);
    }

    // These must be explicitly sniffed out since they are constants and have
    // no payload.
    if (opd->type() == MIRType_Undefined || opd->type() == MIRType_Null)
        return add(new LGoto(ifFalse));

    // Check if the operand for this test is a compare operation. If it is, we want
    // to emit an LCompare*AndBranch rather than an LTest*AndBranch, to fuse the
    // compare and jump instructions.
    if (opd->isCompare()) {
        MCompare *comp = opd->toCompare();
        MDefinition *left = comp->getOperand(0);
        MDefinition *right = comp->getOperand(1);

        if (comp->specialization() == MIRType_Int32) {
            JSOp op = ReorderComparison(comp->jsop(), &left, &right);
            return add(new LCompareIAndBranch(op, useRegister(left), useAnyOrConstant(right),
                                              ifTrue, ifFalse));
        }
        if (comp->specialization() == MIRType_Double) {
            return add(new LCompareDAndBranch(comp->jsop(), useRegister(left),
                                              useRegister(right), ifTrue, ifFalse));
        }
        // :TODO: implment LCompareVAndBranch. Bug: 679804
    }

    if (opd->type() == MIRType_Double)
        return add(new LTestDAndBranch(useRegister(opd), ifTrue, ifFalse));

    return add(new LTestIAndBranch(useRegister(opd), ifTrue, ifFalse));
}

bool
LIRGenerator::visitCompare(MCompare *comp)
{
    MDefinition *left = comp->getOperand(0);
    MDefinition *right = comp->getOperand(1);

    if (comp->specialization() != MIRType_None) {
        // Sniff out if the output of this compare is used only for a branching.
        // If it is, then we willl emit an LCompare*AndBranch instruction in place
        // of this compare and any test that uses this compare. Thus, we can
        // ignore this Compare.
        bool willOptimize = true;
        for (MUseIterator iter(comp->usesBegin()); iter!= comp->usesEnd(); iter++) {
            MNode *node = iter->node();
            if (node->isResumePoint() ||
                (node->isDefinition() && !node->toDefinition()->isControlInstruction()))
            {
                willOptimize = false;
                break;
            }
        }

        if (willOptimize && comp->canEmitAtUses())
            return emitAtUses(comp);

        if (comp->specialization() == MIRType_Int32) {
            JSOp op = ReorderComparison(comp->jsop(), &left, &right);
            return define(new LCompareI(op, useRegister(left), useAnyOrConstant(right)), comp);
        }

        if (comp->specialization() == MIRType_Double)
            return define(new LCompareD(comp->jsop(), useRegister(left), useRegister(right)), comp);
    }

    // :TODO: implement LCompareV. Bug: 679804
    JS_NOT_REACHED("LCompareV NYI");
    return true;
}

static void
ReorderCommutative(MDefinition **lhsp, MDefinition **rhsp)
{
    MDefinition *lhs = *lhsp;
    MDefinition *rhs = *rhsp;

    // Put the constant in the left-hand side, if there is one.
    if (lhs->isConstant()) {
        *rhsp = lhs;
        *lhsp = rhs;
    }
}

bool
LIRGenerator::lowerBitOp(JSOp op, MInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        ReorderCommutative(&lhs, &rhs);
        return lowerForALU(new LBitOp(op), ins, lhs, rhs);
    }

    JS_NOT_REACHED("NYI");
    return false;
}

bool
LIRGenerator::visitBitNot(MBitNot *ins)
{
    MDefinition *input = ins->getOperand(0);

    JS_ASSERT(input->type() == MIRType_Int32);

    return lowerForALU(new LBitNot(), ins, input);
}

bool
LIRGenerator::visitBitAnd(MBitAnd *ins)
{
    return lowerBitOp(JSOP_BITAND, ins);
}

bool
LIRGenerator::visitBitOr(MBitOr *ins)
{
    return lowerBitOp(JSOP_BITOR, ins);
}

bool
LIRGenerator::visitBitXor(MBitXor *ins)
{
    return lowerBitOp(JSOP_BITXOR, ins);
}

bool
LIRGenerator::lowerShiftOp(JSOp op, MInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        LShiftOp *lir = new LShiftOp(op);
        if (op == JSOP_URSH) {
            MUrsh *ursh = ins->toUrsh();
            if (ursh->fallible() && !assignSnapshot(lir))
                return false;
        }
        return lowerForShift(lir, ins, lhs, rhs);
    }
    JS_NOT_REACHED("NYI");
    return false;
}

bool
LIRGenerator::visitLsh(MLsh *ins)
{
    return lowerShiftOp(JSOP_LSH, ins);
}

bool
LIRGenerator::visitRsh(MRsh *ins)
{
    return lowerShiftOp(JSOP_RSH, ins);
}

bool
LIRGenerator::visitUrsh(MUrsh *ins)
{
    return lowerShiftOp(JSOP_URSH, ins);
}

bool
LIRGenerator::visitAdd(MAdd *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        ReorderCommutative(&lhs, &rhs);
        LAddI *lir = new LAddI;
        if (!assignSnapshot(lir))
            return false;
        return lowerForALU(lir, ins, lhs, rhs);
    }

    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_ADD), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_ADD, ins);
}

bool
LIRGenerator::visitSub(MSub *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        LSubI *lir = new LSubI;
        if (!assignSnapshot(lir))
            return false;
        return lowerForALU(lir, ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_SUB), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_SUB, ins);
}

bool
LIRGenerator::visitMul(MMul *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();
    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        ReorderCommutative(&lhs, &rhs);
        return lowerMulI(ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_MUL), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_MUL, ins);
}

bool
LIRGenerator::visitDiv(MDiv *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();
    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        return lowerDivI(ins);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_DIV), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_DIV, ins);
}

bool
LIRGenerator::visitMod(MMod *ins)
{
    MDefinition *lhs = ins->lhs();

    if (ins->type() == MIRType_Int32 &&
        ins->specialization() == MIRType_Int32)
    {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        return lowerModI(ins);
    }
    // TODO: Implement for ins->specialization() == MIRType_Double

    return lowerBinaryV(JSOP_MOD, ins);
}

bool
LIRGenerator::lowerBinaryV(JSOp op, MBinaryInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == MIRType_Value);
    JS_ASSERT(rhs->type() == MIRType_Value);

    LBinaryV *lir = new LBinaryV(op);
    if (!useBox(lir, LBinaryV::LhsInput, lhs))
        return false;
    if (!useBox(lir, LBinaryV::RhsInput, rhs))
        return false;
    if (!defineVMReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitConcat(MConcat *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == MIRType_String);
    JS_ASSERT(rhs->type() == MIRType_String);

    LConcat *lir = new LConcat(useRegister(lhs), useRegister(rhs));
    if (!defineVMReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStart(MStart *start)
{
    // Create a snapshot that captures the initial state of the function.
    LStart *lir = new LStart;
    if (!assignSnapshot(lir))
        return false;

    if (start->startType() == MStart::StartType_Default)
        lirGraph_.setEntrySnapshot(lir->snapshot());
    return add(lir);
}

bool
LIRGenerator::visitOsrEntry(MOsrEntry *entry)
{
    LOsrEntry *lir = new LOsrEntry;
    return defineFixed(lir, entry, LAllocation(AnyRegister(OsrFrameReg)));
}

bool
LIRGenerator::visitOsrValue(MOsrValue *value)
{
    LOsrValue *lir = new LOsrValue(useRegister(value->entry()));
    return defineBox(lir, value);
}

bool
LIRGenerator::visitOsrScopeChain(MOsrScopeChain *object)
{
    LOsrScopeChain *lir = new LOsrScopeChain(useRegister(object->entry()));
    return define(lir, object);
}

bool
LIRGenerator::visitToDouble(MToDouble *convert)
{
    MDefinition *opd = convert->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToDouble *lir = new LValueToDouble();
        if (!useBox(lir, LValueToDouble::Input, opd))
            return false;
        return define(lir, convert) && assignSnapshot(lir);
      }

      case MIRType_Null:
        return lowerConstantDouble(0, convert);

      case MIRType_Undefined:
        return lowerConstantDouble(js_NaN, convert);

      case MIRType_Int32:
      case MIRType_Boolean:
      {
        LInt32ToDouble *lir = new LInt32ToDouble(useRegister(opd));
        return define(lir, convert);
      }

      case MIRType_Double:
        return redefine(convert, opd);

      default:
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        JS_NOT_REACHED("unexpected type");
    }
    return false;
}

bool
LIRGenerator::visitToInt32(MToInt32 *convert)
{
    MDefinition *opd = convert->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToInt32 *lir = new LValueToInt32(tempFloat(), LValueToInt32::NORMAL);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return define(lir, convert) && assignSnapshot(lir);
      }

      case MIRType_Null:
        return define(new LInteger(0), convert);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(convert, opd);

      case MIRType_Double:
      {
        LDoubleToInt32 *lir = new LDoubleToInt32(use(opd));
        return define(lir, convert) && assignSnapshot(lir);
      }

      default:
        // Undefined coerces to NaN, not int32.
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        JS_NOT_REACHED("unexpected type");
    }

    return false;
}

bool
LIRGenerator::visitTruncateToInt32(MTruncateToInt32 *truncate)
{
    MDefinition *opd = truncate->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToInt32 *lir = new LValueToInt32(tempFloat(), LValueToInt32::TRUNCATE);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return define(lir, truncate) && assignSnapshot(lir);
      }

      case MIRType_Null:
      case MIRType_Undefined:
        return define(new LInteger(0), truncate);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(truncate, opd);

      case MIRType_Double:
      {
        LTruncateDToInt32 *lir = new LTruncateDToInt32(useRegister(opd));
        return define(lir, truncate) && assignSnapshot(lir);
      }

      default:
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        JS_NOT_REACHED("unexpected type");
    }

    return false;
}

bool
LIRGenerator::visitToString(MToString *ins)
{
    MDefinition *opd = ins->input();

    switch (opd->type()) {
      case MIRType_Double:
      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Boolean:
        JS_NOT_REACHED("NYI: Lower MToString");
        break;

      case MIRType_Int32: {
        LIntToString *lir = new LIntToString(useRegister(opd));

        if (!defineVMReturn(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      default:
        // Objects might be effectful. (see ToPrimitive)
        JS_NOT_REACHED("unexpected type");
        break;
    }
    return false;
}

bool
LIRGenerator::visitCopy(MCopy *ins)
{
    JS_NOT_REACHED("unexpected copy");
    return false;
}

bool
LIRGenerator::visitImplicitThis(MImplicitThis *ins)
{
    JS_ASSERT(ins->callee()->type() == MIRType_Object);

    LImplicitThis *lir = new LImplicitThis(useRegister(ins->callee()));
    return assignSnapshot(lir) && defineBox(lir, ins);
}

bool
LIRGenerator::visitSlots(MSlots *ins)
{
    return define(new LSlots(useRegister(ins->object())), ins);
}

bool
LIRGenerator::visitElements(MElements *ins)
{
    return define(new LElements(useRegister(ins->object())), ins);
}

bool
LIRGenerator::visitLoadSlot(MLoadSlot *ins)
{
    switch (ins->type()) {
      case MIRType_Value:
        return defineBox(new LLoadSlotV(useRegister(ins->slots())), ins);

      case MIRType_Undefined:
      case MIRType_Null:
        JS_NOT_REACHED("typed load must have a payload");
        return false;

      default:
        return define(new LLoadSlotT(useRegister(ins->slots())), ins);
    }

    return true;
}

bool
LIRGenerator::visitFunctionEnvironment(MFunctionEnvironment *ins)
{
    return define(new LFunctionEnvironment(useRegister(ins->function())), ins);
}

bool
LIRGenerator::emitWriteBarrier(MInstruction *ins, MDefinition *input)
{
#ifdef JSGC_INCREMENTAL
    JS_ASSERT(GetIonContext()->cx->compartment->needsBarrier());

    switch (input->type()) {
      // Possible GCThings.
      case MIRType_Value: {
        LInstruction *barrier = new LWriteBarrierV;
        if (!useBox(barrier, LWriteBarrierV::Input, input))
            return false;
        add(barrier, ins);
        break;
      }

      // Known GCThings.
      case MIRType_String:
      case MIRType_Object:
        add(new LWriteBarrierT(useRegisterOrConstant(input)), ins);
        break;

      // Known non-GCThings.
      case MIRType_Undefined:
      case MIRType_Null:
      case MIRType_Boolean:
      case MIRType_Int32:
      case MIRType_Double:
        break;

      // Nonsensical input.
      default:
        JS_NOT_REACHED("Unexpected MIRType.");
    }
#endif
    return true;
}

bool
LIRGenerator::visitStoreSlot(MStoreSlot *ins)
{
    LInstruction *lir;

#ifdef JSGC_INCREMENTAL
    if (ins->needsBarrier() && !emitWriteBarrier(ins, ins->value()))
        return false;
#endif

    switch (ins->value()->type()) {
      case MIRType_Value:
        lir = new LStoreSlotV(useRegister(ins->slots()));
        if (!useBox(lir, LStoreSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);

      case MIRType_Double:
        return add(new LStoreSlotT(useRegister(ins->slots()), useRegister(ins->value())), ins);

      default:
        return add(new LStoreSlotT(useRegister(ins->slots()), useRegisterOrConstant(ins->value())),
                   ins);
    }

    return true;
}

bool
LIRGenerator::visitTypeBarrier(MTypeBarrier *ins)
{
    // Requesting a non-GC pointer is safe here since we never re-enter C++
    // from inside a type barrier test.
    LTypeBarrier *barrier = new LTypeBarrier(temp(LDefinition::GENERAL));
    if (!useBox(barrier, LTypeBarrier::Input, ins->input()))
        return false;
    if (!assignSnapshot(barrier, ins->bailoutKind()))
        return false;
    return defineAs(barrier, ins, ins->input()) && add(barrier);
}

bool
LIRGenerator::visitArrayLength(MArrayLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    return define(new LArrayLength(useRegister(ins->elements())), ins);
}

bool
LIRGenerator::visitInitializedLength(MInitializedLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    return define(new LInitializedLength(useRegister(ins->elements())), ins);
}

bool
LIRGenerator::visitBoundsCheck(MBoundsCheck *ins)
{
    LBoundsCheck *check = new LBoundsCheck(useRegisterOrConstant(ins->index()),
                                           useRegister(ins->length()));
    return assignSnapshot(check) && add(check, ins);
}

bool
LIRGenerator::visitLoadElement(MLoadElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    switch (ins->type()) {
      case MIRType_Value:
      {
        LLoadElementV *lir = new LLoadElementV(useRegister(ins->elements()),
                                               useRegisterOrConstant(ins->index()));
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        return defineBox(lir, ins);
      }
      case MIRType_Undefined:
      case MIRType_Null:
        JS_NOT_REACHED("typed load must have a payload");
        return false;

      default:
        JS_ASSERT(!ins->fallible());
        return define(new LLoadElementT(useRegister(ins->elements()),
                                        useRegisterOrConstant(ins->index())), ins);
    }
}

bool
LIRGenerator::visitStoreElement(MStoreElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

#ifdef JSGC_INCREMENTAL
    if (ins->needsBarrier() && !emitWriteBarrier(ins, ins->value()))
        return false;
#endif

    switch (ins->value()->type()) {
      case MIRType_Value:
      {
        LInstruction *lir = new LStoreElementV(useRegister(ins->elements()),
                                               useRegisterOrConstant(ins->index()));
        if (!useBox(lir, LStoreElementV::Value, ins->value()))
            return false;
        return add(lir, ins);
      }
      case MIRType_Double:
        return add(new LStoreElementT(useRegister(ins->elements()),
                                      useRegisterOrConstant(ins->index()),
                                      useRegister(ins->value())), ins);

      default:
        return add(new LStoreElementT(useRegister(ins->elements()),
                                      useRegisterOrConstant(ins->index()),
                                      useRegisterOrConstant(ins->value())), ins);
    }
}

bool
LIRGenerator::visitLoadFixedSlot(MLoadFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LLoadFixedSlotV *lir = new LLoadFixedSlotV(useRegister(ins->object()));
        return defineBox(lir, ins);
    }

    LLoadFixedSlotT *lir = new LLoadFixedSlotT(useRegister(ins->object()));
    return define(lir, ins);
}

bool
LIRGenerator::visitStoreFixedSlot(MStoreFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->value()->type() == MIRType_Value) {
        LStoreFixedSlotV *lir = new LStoreFixedSlotV(useRegister(ins->object()));

        if (!useBox(lir, LStoreFixedSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);
    }

    LStoreFixedSlotT *lir = new LStoreFixedSlotT(useRegister(ins->object()),
                                                 useRegisterOrConstant(ins->value()));

    return add(lir, ins);
}

bool
LIRGenerator::visitGetPropertyCache(MGetPropertyCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LGetPropertyCacheV *lir = new LGetPropertyCacheV(useRegister(ins->object()));
        if (!defineBox(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
    }

    LGetPropertyCacheT *lir = new LGetPropertyCacheT(useRegister(ins->object()));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGuardClass(MGuardClass *ins)
{
    LDefinition t = temp(LDefinition::GENERAL);
    LGuardClass *guard = new LGuardClass(useRegister(ins->obj()), t);
    return assignSnapshot(guard) && add(guard, ins);
}

bool
LIRGenerator::visitCallGetProperty(MCallGetProperty *ins)
{
    LCallGetProperty *lir = new LCallGetProperty();
    lir->setOperand(0, useRegister(ins->getOperand(0)));
    return defineVMReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetName(MCallGetName *ins)
{
    LCallGetName *lir = new LCallGetName();
    lir->setOperand(0, useRegister(ins->getOperand(0)));
    return defineVMReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetNameTypeOf(MCallGetNameTypeOf *ins)
{
    LCallGetNameTypeOf *lir = new LCallGetNameTypeOf();
    lir->setOperand(0, useRegister(ins->getOperand(0)));
    return defineVMReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGenericSetProperty(MGenericSetProperty *ins)
{
    LUse obj = useRegister(ins->obj());

    LInstruction *lir;
    if (ins->value()->type() == MIRType_Value) {
        if (ins->monitored())
            lir = new LCallSetPropertyV(obj);
        else
            lir = new LCacheSetPropertyV(obj);
        JS_STATIC_ASSERT(LCallSetPropertyV::Value == LCacheSetPropertyV::Value);
        if (!useBox(lir, LCallSetPropertyV::Value, ins->value()))
            return false;
    } else {
        LAllocation value = useRegisterOrConstant(ins->value());
        if (ins->monitored())
            lir = new LCallSetPropertyT(obj, value, ins->value()->type());
        else
            lir = new LCacheSetPropertyT(obj, value, ins->value()->type());
    }

    if (!add(lir, ins))
        return false;

    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStringLength(MStringLength *ins)
{
    JS_ASSERT(ins->string()->type() == MIRType_String);
    return define(new LStringLength(useRegister(ins->string())), ins);
}

static void
SpewResumePoint(MBasicBlock *block, MInstruction *ins, MResumePoint *resumePoint)
{
    fprintf(IonSpewFile, "Current resume point %p details:\n", (void *)resumePoint);
    fprintf(IonSpewFile, "    frame count: %u\n", resumePoint->frameCount());

    if (ins) {
        fprintf(IonSpewFile, "    taken after: ");
        ins->printName(IonSpewFile);
    } else {
        fprintf(IonSpewFile, "    taken at block %d entry", block->id());
    }
    fprintf(IonSpewFile, "\n");

    fprintf(IonSpewFile, "    pc: %p\n", resumePoint->pc());

    for (size_t i = 0; i < resumePoint->numOperands(); i++) {
        MDefinition *in = resumePoint->getOperand(i);
        fprintf(IonSpewFile, "    slot%u: ", (unsigned)i);
        in->printName(IonSpewFile);
        fprintf(IonSpewFile, "\n");
    }
}

bool
LIRGenerator::visitInstruction(MInstruction *ins)
{
    if (!gen->ensureBallast())
        return false;
    if (!ins->accept(this))
        return false;

    if (ins->resumePoint())
        updateResumeState(ins);

    if (gen->errored())
        return false;
#ifdef DEBUG
    ins->setInWorklistUnchecked();
#endif

    // If no post snapshot was present, then exit with success.
    if (!postSnapshot_)
        return true;

    // Post snapshots are copied to a new LInstruction which is added to the
    // stream of instruction.  This intruction avoid adding more instrumentation
    // into the register allocator.
    LSnapshot *post = postSnapshot_;
    postSnapshot_ = NULL;

    return add(new LCaptureAllocations(post));
}

bool
LIRGenerator::definePhis()
{
    size_t lirIndex = 0;
    MBasicBlock *block = current->mir();
    for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
        if (phi->type() == MIRType_Value) {
            if (!defineUntypedPhi(*phi, lirIndex))
                return false;
            lirIndex += BOX_PIECES;
        } else {
            if (!defineTypedPhi(*phi, lirIndex))
                return false;
            lirIndex += 1;
        }
    }
    return true;
}

void
LIRGenerator::updateResumeState(MInstruction *ins)
{
    lastResumePoint_ = ins->resumePoint();
    if (IonSpewEnabled(IonSpew_Snapshots))
        SpewResumePoint(NULL, ins, lastResumePoint_);
}

void
LIRGenerator::updateResumeState(MBasicBlock *block)
{
    lastResumePoint_ = block->entryResumePoint();
    if (IonSpewEnabled(IonSpew_Snapshots))
        SpewResumePoint(block, NULL, lastResumePoint_);
}

void
LIRGenerator::allocateArguments(uint32 argc)
{
    argslots_ += argc;
    if (argslots_ > maxargslots_)
        maxargslots_ = argslots_;
}

uint32
LIRGenerator::getArgumentSlot(uint32 argnum)
{
    // First slot has index 1.
    JS_ASSERT(argnum < argslots_);
    return argslots_ - argnum ;
}

void
LIRGenerator::freeArguments(uint32 argc)
{
    JS_ASSERT(argc <= argslots_);
    argslots_ -= argc;
}

bool
LIRGenerator::visitBlock(MBasicBlock *block)
{
    current = block->lir();
    updateResumeState(block);

    if (!definePhis())
        return false;

    if (!add(new LLabel()))
        return false;

    for (MInstructionIterator iter = block->begin(); *iter != block->lastIns(); iter++) {
        if (!visitInstruction(*iter))
            return false;
    }

    if (block->successorWithPhis()) {
        // If we have a successor with phis, lower the phi input now that we
        // are approaching the join point.
        MBasicBlock *successor = block->successorWithPhis();
        uint32 position = block->positionInPhiSuccessor();
        size_t lirIndex = 0;
        for (MPhiIterator phi(successor->phisBegin()); phi != successor->phisEnd(); phi++) {
            MDefinition *opd = phi->getOperand(position);
            if (!ensureDefined(opd))
                return false;

            JS_ASSERT(opd->type() == phi->type());

            if (phi->type() == MIRType_Value) {
                lowerUntypedPhiInput(*phi, position, successor->lir(), lirIndex);
                lirIndex += BOX_PIECES;
            } else {
                lowerTypedPhiInput(*phi, position, successor->lir(), lirIndex);
                lirIndex += 1;
            }
        }
    }

    // Now emit the last instruction, which is some form of branch.
    if (!visitInstruction(block->lastIns()))
        return false;

    return true;
}

bool
LIRGenerator::precreatePhi(LBlock *block, MPhi *phi)
{
    LPhi *lir = LPhi::New(gen, phi);
    if (!lir)
        return false;
    if (!block->addPhi(lir))
        return false;
    return true;
}

bool
LIRGenerator::generate()
{
    // Create all blocks and prep all phis beforehand.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        current = LBlock::New(*block);
        if (!current)
            return false;
        if (!lirGraph_.addBlock(current))
            return false;
        block->assignLir(current);

        // For each MIR phi, add LIR phis as appropriate. We'll fill in their
        // operands on each incoming edge, and set their definitions at the
        // start of their defining block.
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            int numPhis = (phi->type() == MIRType_Value) ? BOX_PIECES : 1;
            for (int i = 0; i < numPhis; i++) {
                if (!precreatePhi(block->lir(), *phi))
                    return false;
            }
        }
    }

    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (!visitBlock(*block))
            return false;
    }

    if (graph.osrBlock())
        lirGraph_.setOsrBlock(graph.osrBlock()->lir());

    lirGraph_.setArgumentSlotCount(maxargslots_);

    return true;
}

