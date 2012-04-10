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

#ifndef jsion_codegen_h__
#define jsion_codegen_h__

#if defined(JS_CPU_X86)
# include "x86/CodeGenerator-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/CodeGenerator-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/CodeGenerator-arm.h"
#else
#error "CPU Not Supported"
#endif

namespace js {
namespace ion {

class OutOfLineNewArray;
class OutOfLineNewObject;
class OutOfLineCreateThis;
class CheckOverRecursedFailure;
class OutOfLineUnboxDouble;
class OutOfLineCache;
class OutOfLineStoreElementHole;
class OutOfLineTypeOfV;
class OutOfLineLoadTypedArray;

class CodeGenerator : public CodeGeneratorSpecific
{
    bool generateArgumentsChecks();
    bool generateBody();

  public:
    CodeGenerator(MIRGenerator *gen, LIRGraph &graph);

  public:
    bool generate();

    bool visitLabel(LLabel *lir);
    bool visitNop(LNop *lir);
    bool visitOsiPoint(LOsiPoint *lir);
    bool visitGoto(LGoto *lir);
    bool visitParameter(LParameter *lir);
    bool visitCallee(LCallee *lir);
    bool visitStart(LStart *lir);
    bool visitReturn(LReturn *ret);
    bool visitDefVar(LDefVar *lir);
    bool visitOsrEntry(LOsrEntry *lir);
    bool visitOsrScopeChain(LOsrScopeChain *lir);
    bool visitStackArg(LStackArg *lir);
    bool visitValueToInt32(LValueToInt32 *lir);
    bool visitValueToDouble(LValueToDouble *lir);
    bool visitInt32ToDouble(LInt32ToDouble *lir);
    bool visitTestVAndBranch(LTestVAndBranch *lir);
    bool visitIntToString(LIntToString *lir);
    bool visitInteger(LInteger *lir);
    bool visitRegExp(LRegExp *lir);
    bool visitLambda(LLambda *lir);
    bool visitPointer(LPointer *lir);
    bool visitSlots(LSlots *lir);
    bool visitStoreSlotV(LStoreSlotV *store);
    bool visitElements(LElements *lir);
    bool visitTypeBarrier(LTypeBarrier *lir);
    bool visitMonitorTypes(LMonitorTypes *lir);
    bool visitCallNative(LCallNative *call);
    bool visitCallGeneric(LCallGeneric *call);
    bool visitCallConstructor(LCallConstructor *call);
    bool visitDoubleToInt32(LDoubleToInt32 *lir);
    bool visitNewArrayCallVM(LNewArray *lir);
    bool visitNewArray(LNewArray *lir);
    bool visitOutOfLineNewArray(OutOfLineNewArray *ool);
    bool visitNewObjectVMCall(LNewObject *lir);
    bool visitNewObject(LNewObject *lir);
    bool visitOutOfLineNewObject(OutOfLineNewObject *ool);
    bool visitInitProp(LInitProp *lir);
    bool visitCreateThisVMCall(LCreateThis *lir);
    bool visitCreateThis(LCreateThis *lir);
    bool visitOutOfLineCreateThis(OutOfLineCreateThis *ool);
    bool visitArrayLength(LArrayLength *lir);
    bool visitTypedArrayLength(LTypedArrayLength *lir);
    bool visitTypedArrayElements(LTypedArrayElements *lir);
    bool visitStringLength(LStringLength *lir);
    bool visitInitializedLength(LInitializedLength *lir);
    bool visitSetInitializedLength(LSetInitializedLength *lir);
    bool visitNotV(LNotV *ins);
    bool visitBoundsCheck(LBoundsCheck *lir);
    bool visitBoundsCheckRange(LBoundsCheckRange *lir);
    bool visitBoundsCheckLower(LBoundsCheckLower *lir);
    bool visitLoadFixedSlotV(LLoadFixedSlotV *ins);
    bool visitLoadFixedSlotT(LLoadFixedSlotT *ins);
    bool visitStoreFixedSlotV(LStoreFixedSlotV *ins);
    bool visitStoreFixedSlotT(LStoreFixedSlotT *ins);
    bool visitAbsI(LAbsI *lir);
    bool visitBinaryV(LBinaryV *lir);
    bool visitCompareV(LCompareV *lir);
    bool visitIsNullOrUndefined(LIsNullOrUndefined *lir);
    bool visitIsNullOrUndefinedAndBranch(LIsNullOrUndefinedAndBranch *lir);
    bool visitConcat(LConcat *lir);
    bool visitCharCodeAt(LCharCodeAt *lir);
    bool visitFromCharCode(LFromCharCode *lir);
    bool visitFunctionEnvironment(LFunctionEnvironment *lir);
    bool visitCallGetProperty(LCallGetProperty *lir);
    bool visitCallGetName(LCallGetName *lir);
    bool visitCallGetNameTypeOf(LCallGetNameTypeOf *lir);
    bool visitCallGetElement(LCallGetElement *lir);
    bool visitCallSetElement(LCallSetElement *lir);
    bool visitThrow(LThrow *lir);
    bool visitTypeOfV(LTypeOfV *lir);
    bool visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool);
    bool visitToIdV(LToIdV *lir);
    bool visitLoadElementV(LLoadElementV *load);
    bool visitLoadElementHole(LLoadElementHole *lir);
    bool visitStoreElementT(LStoreElementT *lir);
    bool visitStoreElementV(LStoreElementV *lir);
    bool visitStoreElementHoleT(LStoreElementHoleT *lir);
    bool visitStoreElementHoleV(LStoreElementHoleV *lir);
    bool emitArrayPopShift(LInstruction *lir, const MArrayPopShift *mir, Register obj,
                           Register elementsTemp, Register lengthTemp, TypedOrValueRegister out);
    bool visitArrayPopShiftV(LArrayPopShiftV *lir);
    bool visitArrayPopShiftT(LArrayPopShiftT *lir);
    bool emitArrayPush(LInstruction *lir, const MArrayPush *mir, Register obj,
                       ConstantOrRegister value, Register elementsTemp, Register length);
    bool visitArrayPushV(LArrayPushV *lir);
    bool visitArrayPushT(LArrayPushT *lir);
    bool visitLoadTypedArrayElement(LLoadTypedArrayElement *lir);
    bool visitLoadTypedArrayElementHole(LLoadTypedArrayElementHole *lir);
    bool visitStoreTypedArrayElement(LStoreTypedArrayElement *lir);
    bool visitClampIToUint8(LClampIToUint8 *lir);
    bool visitClampDToUint8(LClampDToUint8 *lir);
    bool visitClampVToUint8(LClampVToUint8 *lir);
    bool visitOutOfLineLoadTypedArray(OutOfLineLoadTypedArray *ool);
    bool visitCallIteratorStart(LCallIteratorStart *lir);
    bool visitIteratorStart(LIteratorStart *lir);
    bool visitIteratorNext(LIteratorNext *lir);
    bool visitIteratorMore(LIteratorMore *lir);
    bool visitIteratorEnd(LIteratorEnd *lir);
    bool visitCallSetProperty(LCallSetProperty *ins);
    bool visitCallDeleteProperty(LCallDeleteProperty *lir);
    bool visitBitNotV(LBitNotV *lir);
    bool visitBitOpV(LBitOpV *lir);

    bool visitCheckOverRecursed(LCheckOverRecursed *lir);
    bool visitCheckOverRecursedFailure(CheckOverRecursedFailure *ool);

    bool visitUnboxDouble(LUnboxDouble *lir);
    bool visitOutOfLineUnboxDouble(OutOfLineUnboxDouble *ool);
    bool visitOutOfLineStoreElementHole(OutOfLineStoreElementHole *ool);

    bool visitOutOfLineCacheGetProperty(OutOfLineCache *ool);
    bool visitOutOfLineGetElementCache(OutOfLineCache *ool);
    bool visitOutOfLineSetPropertyCache(OutOfLineCache *ool);
    bool visitOutOfLineBindNameCache(OutOfLineCache *ool);

    bool visitGetPropertyCacheV(LGetPropertyCacheV *ins) {
        return visitCache(ins);
    }
    bool visitGetPropertyCacheT(LGetPropertyCacheT *ins) {
        return visitCache(ins);
    }
    bool visitGetElementCacheV(LGetElementCacheV *ins) {
        return visitCache(ins);
    }
    bool visitBindNameCache(LBindNameCache *ins) {
        return visitCache(ins);
    }
    bool visitSetPropertyCacheV(LSetPropertyCacheV *ins) {
        return visitCache(ins);
    }
    bool visitSetPropertyCacheT(LSetPropertyCacheT *ins) {
        return visitCache(ins);
    }

  private:
    bool visitCache(LInstruction *load);
    bool visitCallSetProperty(LInstruction *ins);

    ConstantOrRegister getSetPropertyValue(LInstruction *ins);
    bool generateBranchV(const ValueOperand &value, Label *ifTrue, Label *ifFalse, FloatRegister fr);
};

} // namespace ion
} // namespace js

#endif // jsion_codegen_h__

