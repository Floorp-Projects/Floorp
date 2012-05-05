/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
#include "IonSpewer.h"
#include "MIRGenerator.h"
#include "shared/CodeGenerator-shared-inl.h"
#include "jsnum.h"
#include "jsinterpinlines.h"

using namespace js;
using namespace js::ion;

namespace js {
namespace ion {

CodeGenerator::CodeGenerator(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorSpecific(gen, graph)
{
}

bool
CodeGenerator::visitValueToInt32(LValueToInt32 *lir)
{
    ValueOperand operand = ToValue(lir, LValueToInt32::Input);
    Register output = ToRegister(lir->output());

    Label done, simple, isInt32, isBool, notDouble;

    // Type-check switch.
    masm.branchTestInt32(Assembler::Equal, operand, &isInt32);
    masm.branchTestBoolean(Assembler::Equal, operand, &isBool);
    masm.branchTestDouble(Assembler::NotEqual, operand, &notDouble);

    // If the value is a double, see if it fits in a 32-bit int. We need to ask
    // the platform-specific codegenerator to do this.
    FloatRegister temp = ToFloatRegister(lir->tempFloat());
    masm.unboxDouble(operand, temp);

    Label fails;
    switch (lir->mode()) {
      case LValueToInt32::TRUNCATE:
        if (!emitTruncateDouble(temp, output))
            return false;
        break;
      default:
        JS_ASSERT(lir->mode() == LValueToInt32::NORMAL);
        emitDoubleToInt32(temp, output, &fails, lir->mir()->canBeNegativeZero());
        break;
    }
    masm.jump(&done);

    masm.bind(&notDouble);

    if (lir->mode() == LValueToInt32::NORMAL) {
        // If the value is not null, it's a string, object, or undefined,
        // which we can't handle here.
        masm.branchTestNull(Assembler::NotEqual, operand, &fails);
    } else {
        // Test for string or object - then fallthrough to null, which will
        // also handle undefined.
        masm.branchTestObject(Assembler::Equal, operand, &fails);
        masm.branchTestString(Assembler::Equal, operand, &fails);
    }

    if (fails.used() && !bailoutFrom(&fails, lir->snapshot()))
        return false;
    
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

    Label isDouble, isInt32, isBool, isNull, done;

    // Type-check switch.
    masm.branchTestDouble(Assembler::Equal, operand, &isDouble);
    masm.branchTestInt32(Assembler::Equal, operand, &isInt32);
    masm.branchTestBoolean(Assembler::Equal, operand, &isBool);
    masm.branchTestNull(Assembler::Equal, operand, &isNull);

    Assembler::Condition cond = masm.testUndefined(Assembler::NotEqual, operand);
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
CodeGenerator::visitDoubleToInt32(LDoubleToInt32 *lir)
{
    Label fail;
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    emitDoubleToInt32(input, output, &fail, lir->mir()->canBeNegativeZero());
    if (!bailoutFrom(&fail, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitTestVAndBranch(LTestVAndBranch *lir)
{
    const ValueOperand value = ToValue(lir, LTestVAndBranch::Input);
    masm.branchTestValueTruthy(value, lir->ifTrue(), ToFloatRegister(lir->tempFloat()));
    masm.jump(lir->ifFalse());
    return true;
}


bool
CodeGenerator::visitIntToString(LIntToString *lir)
{
    typedef JSString *(*pf)(JSContext *, int);
    static const VMFunction js_IntToStringInfo = FunctionInfo<pf>(js_IntToString);

    pushArg(ToRegister(lir->input()));
    return callVM(js_IntToStringInfo, lir);
}

bool
CodeGenerator::visitRegExp(LRegExp *lir)
{
    GlobalObject *global = gen->info().script()->global();
    JSObject *proto = global->getOrCreateRegExpPrototype(gen->cx);

    typedef JSObject *(*pf)(JSContext *, JSObject *, JSObject *);
    static const VMFunction CloneRegExpObjectInfo = FunctionInfo<pf>(CloneRegExpObject);

    pushArg(ImmGCPtr(proto));
    pushArg(ImmGCPtr(lir->mir()->source()));
    return callVM(CloneRegExpObjectInfo, lir);
}

bool
CodeGenerator::visitLambda(LLambda *lir)
{
    typedef JSObject *(*pf)(JSContext *, HandleFunction, HandleObject);
    static const VMFunction Info = FunctionInfo<pf>(js::Lambda);

    pushArg(ToRegister(lir->scopeChain()));
    pushArg(ImmGCPtr(lir->mir()->fun()));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitLabel(LLabel *lir)
{
    masm.bind(lir->label());
    return true;
}

bool
CodeGenerator::visitNop(LNop *lir)
{
    return true;
}

bool
CodeGenerator::visitOsiPoint(LOsiPoint *lir)
{
    // Note: markOsiPoint ensures enough space exists between the last
    // LOsiPoint and this one to patch adjacent call instructions.

    JS_ASSERT(masm.framePushed() == frameSize());

    uint32 osiCallPointOffset;
    if (!markOsiPoint(lir, &osiCallPointOffset))
        return false;

    LSafepoint *safepoint = lir->associatedSafepoint();
    JS_ASSERT(!safepoint->osiCallPointOffset());
    safepoint->setOsiCallPointOffset(osiCallPointOffset);
    return true;
}

bool
CodeGenerator::visitGoto(LGoto *lir)
{
    LBlock *target = lir->target()->lir();

    // No jump necessary if we can fall through to the next block.
    if (isNextBlock(target))
        return true;

    masm.jump(target->label());
    return true;
}

bool
CodeGenerator::visitParameter(LParameter *lir)
{
    return true;
}

bool
CodeGenerator::visitCallee(LCallee *lir)
{
    return true;
}

bool
CodeGenerator::visitStart(LStart *lir)
{
    return true;
}

bool
CodeGenerator::visitReturn(LReturn *lir)
{
#if defined(JS_NUNBOX32)
    DebugOnly<LAllocation *> type    = lir->getOperand(TYPE_INDEX);
    DebugOnly<LAllocation *> payload = lir->getOperand(PAYLOAD_INDEX);
    JS_ASSERT(ToRegister(type)    == JSReturnReg_Type);
    JS_ASSERT(ToRegister(payload) == JSReturnReg_Data);
#elif defined(JS_PUNBOX64)
    DebugOnly<LAllocation *> result = lir->getOperand(0);
    JS_ASSERT(ToRegister(result) == JSReturnReg);
#endif
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir() != *gen->graph().poBegin())
        masm.jump(returnLabel_);
    return true;
}

bool
CodeGenerator::visitOsrEntry(LOsrEntry *lir)
{
    // Remember the OSR entry offset into the code buffer.
    masm.flushBuffer();
    setOsrEntryOffset(masm.size());

    // Allocate the full frame for this function.
    masm.subPtr(Imm32(frameSize()), StackPointer);
    return true;
}

bool
CodeGenerator::visitOsrScopeChain(LOsrScopeChain *lir)
{
    const LAllocation *frame   = lir->getOperand(0);
    const LDefinition *object  = lir->getDef(0);

    const ptrdiff_t frameOffset = StackFrame::offsetOfScopeChain();

    masm.loadPtr(Address(ToRegister(frame), frameOffset), ToRegister(object));
    return true;
}

bool
CodeGenerator::visitStackArg(LStackArg *lir)
{
    ValueOperand val = ToValue(lir, 0);
    uint32 argslot = lir->argslot();
    int32 stack_offset = StackOffsetOfPassedArg(argslot);

    masm.storeValue(val, Address(StackPointer, stack_offset));
    return true;
}

bool
CodeGenerator::visitInteger(LInteger *lir)
{
    masm.move32(Imm32(lir->getValue()), ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitPointer(LPointer *lir)
{
    masm.movePtr(ImmGCPtr(lir->ptr()), ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitSlots(LSlots *lir)
{
    Address slots(ToRegister(lir->object()), JSObject::offsetOfSlots());
    masm.loadPtr(slots, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitStoreSlotV(LStoreSlotV *store)
{
    Register base = ToRegister(store->slots());
    int32 offset  = store->mir()->slot() * sizeof(Value);

    const ValueOperand value = ToValue(store, LStoreSlotV::Value);

    if (store->mir()->needsBarrier())
       masm.emitPreBarrier(Address(base, offset), MIRType_Value);

    masm.storeValue(value, Address(base, offset));
    return true;
}

bool
CodeGenerator::visitElements(LElements *lir)
{
    Address elements(ToRegister(lir->object()), JSObject::offsetOfElements());
    masm.loadPtr(elements, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitFunctionEnvironment(LFunctionEnvironment *lir)
{
    Address environment(ToRegister(lir->function()), JSFunction::offsetOfEnvironment());
    masm.loadPtr(environment, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitTypeBarrier(LTypeBarrier *lir)
{
    ValueOperand operand = ToValue(lir, LTypeBarrier::Input);
    Register scratch = ToRegister(lir->temp());

    Label mismatched;
    masm.guardTypeSet(operand, lir->mir()->typeSet(), scratch, &mismatched);
    if (!bailoutFrom(&mismatched, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitMonitorTypes(LMonitorTypes *lir)
{
    ValueOperand operand = ToValue(lir, LMonitorTypes::Input);
    Register scratch = ToRegister(lir->temp());

    Label mismatched;
    masm.guardTypeSet(operand, lir->mir()->typeSet(), scratch, &mismatched);
    if (!bailoutFrom(&mismatched, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitCallNative(LCallNative *call)
{
    JSFunction *target = call->function();
    JS_ASSERT(target);
    JS_ASSERT(target->isNative());

    int callargslot = call->argslot();
    int unusedStack = StackOffsetOfPassedArg(callargslot);

    // Registers used for callWithABI() argument-passing.
    const Register argJSContextReg = ToRegister(call->getArgJSContextReg());
    const Register argUintNReg     = ToRegister(call->getArgUintNReg());
    const Register argVpReg        = ToRegister(call->getArgVpReg());

    // Misc. temporary registers.
    const Register tempReg = ToRegister(call->getTempReg());

    DebugOnly<uint32> initialStack = masm.framePushed();

    masm.checkStackAlignment();

    // Native functions have the signature:
    //  bool (*)(JSContext *, unsigned, Value *vp)
    // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Allocate space for the outparam, moving the StackPointer to what will be &vp[1].
    masm.adjustStack(unusedStack);

    // Push a Value containing the callee object: natives are allowed to access their callee before
    // setitng the return value. The StackPointer is moved to &vp[0].
    masm.Push(ObjectValue(*target));

    // Preload arguments into registers.
    masm.loadJSContext(argJSContextReg);
    masm.move32(Imm32(call->nargs()), argUintNReg);
    masm.movePtr(StackPointer, argVpReg);

    // Construct exit frame.
    uint32 safepointOffset = masm.buildFakeExitFrame(tempReg);
    masm.linkExitFrame();
    if (!markSafepointAt(safepointOffset, call))
        return false;

    // Construct and execute call.
    masm.setupUnalignedABICall(3, tempReg);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argUintNReg);
    masm.passABIArg(argVpReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->native()));

    // Test for failure.
    Label success, exception;
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &exception);

    // Load the outparam vp[0] into output register(s).
    masm.loadValue(Address(StackPointer, IonExitFrameLayout::Size()), JSReturnOperand);
    masm.jump(&success);

    // Handle exception case.
    {
        masm.bind(&exception);
        masm.handleException();
    }
    masm.bind(&success);

    // Move the StackPointer back to its original location, unwinding the exit frame.
    masm.adjustStack(IonExitFrameLayout::Size() - unusedStack + sizeof(Value));
    JS_ASSERT(masm.framePushed() == initialStack);

    return true;
}

bool
CodeGenerator::visitCallGeneric(LCallGeneric *call)
{
    // Holds the function object.
    const LAllocation *callee = call->getFunction();
    Register calleereg = ToRegister(callee);

    // Temporary register for modifying the function object.
    const LAllocation *obj = call->getTempObject();
    Register objreg = ToRegister(obj);

    // Holds the function nargs. Initially undefined.
    const LAllocation *nargs = call->getNargsReg();
    Register nargsreg = ToRegister(nargs);

    uint32 callargslot = call->argslot();
    uint32 unusedStack = StackOffsetOfPassedArg(callargslot);

    masm.checkStackAlignment();

    // Unless already known, guard that calleereg is actually a function object.
    if (!call->hasSingleTarget()) {
        masm.loadObjClass(calleereg, nargsreg);
        masm.cmpPtr(nargsreg, ImmWord(&js::FunctionClass));
        if (!bailoutIf(Assembler::NotEqual, call->snapshot()))
            return false;
    }

    Label end, invoke;

    // Guard that calleereg is a non-native function:
    // Non-native iff (callee->flags & JSFUN_KINDMASK >= JSFUN_INTERPRETED).
    // This is equivalent to testing if any of the bits in JSFUN_KINDMASK are set.
    if (!call->hasSingleTarget()) {
        Address flags(calleereg, offsetof(JSFunction, flags));
        masm.load16ZeroExtend_mask(flags, Imm32(JSFUN_KINDMASK), nargsreg);
        masm.branch32(Assembler::LessThan, nargsreg, Imm32(JSFUN_INTERPRETED), &invoke);
    } else {
        // Native single targets are handled by LCallNative.
        JS_ASSERT(!call->getSingleTarget()->isNative());
    }


    // Knowing that calleereg is a non-native function, load the JSScript.
    masm.movePtr(Address(calleereg, offsetof(JSFunction, u.i.script_)), objreg);
    masm.movePtr(Address(objreg, offsetof(JSScript, ion)), objreg);

    // Guard that the IonScript has been compiled.
    masm.branchPtr(Assembler::BelowOrEqual, objreg, ImmWord(ION_DISABLED_SCRIPT), &invoke);

    // Nestle %esp up to the argument vector.
    masm.freeStack(unusedStack);

    // Construct the IonFramePrefix.
    uint32 descriptor = MakeFrameDescriptor(masm.framePushed(), IonFrame_JS);
    masm.Push(calleereg);
    masm.Push(Imm32(descriptor));

    Label thunk, rejoin;

    if (call->hasSingleTarget()) {
        // Missing arguments must have been explicitly appended by the IonBuilder.
        JS_ASSERT(call->getSingleTarget()->nargs <= call->nargs());
    } else {
        // Check whether the provided arguments satisfy target argc.
        masm.load16ZeroExtend(Address(calleereg, offsetof(JSFunction, nargs)), nargsreg);
        masm.cmp32(nargsreg, Imm32(call->nargs()));
        masm.j(Assembler::Above, &thunk);
    }

    // No argument fixup needed. Load the start of the target IonCode.
    {
        masm.movePtr(Address(objreg, offsetof(IonScript, method_)), objreg);
        masm.movePtr(Address(objreg, IonCode::OffsetOfCode()), objreg);
    }

    // Argument fixup needed. Get ready to call the argumentsRectifier.
    if (!call->hasSingleTarget()) {
        // Skip this thunk unless an explicit jump target.
        masm.jump(&rejoin);
        masm.bind(&thunk);

        // Hardcode the address of the argumentsRectifier code.
        IonCompartment *ion = gen->ionCompartment();
        IonCode *argumentsRectifier = ion->getArgumentsRectifier(gen->cx);
        if (!argumentsRectifier)
            return false;

        JS_ASSERT(ArgumentsRectifierReg != objreg);
        masm.move32(Imm32(call->nargs()), ArgumentsRectifierReg);
        masm.movePtr(ImmWord(argumentsRectifier->raw()), objreg);
    }

    masm.bind(&rejoin);

    masm.checkStackAlignment();

    // Finally call the function in objreg, as assigned by one of the paths above.
    masm.callIon(objreg);
    if (!markSafepoint(call))
        return false;

    // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
    // The return address has already been removed from the Ion frame.
    int prefixGarbage = sizeof(IonJSFrameLayout) - sizeof(void *);
    masm.adjustStack(prefixGarbage - unusedStack);

    masm.jump(&end);

    // Handle uncompiled or native functions.
    {
        masm.bind(&invoke);

        typedef bool (*pf)(JSContext *, JSFunction *, uint32, Value *, Value *);
        static const VMFunction InvokeFunctionInfo = FunctionInfo<pf>(InvokeFunction);

        // Nestle %esp up to the argument vector.
        // Each path must account for framePushed_ separately, for callVM to be valid.
        masm.freeStack(unusedStack);

        pushArg(StackPointer);                 // argv.
        pushArg(Imm32(call->bytecodeArgc()));  // argc.
        pushArg(calleereg);                    // JSFunction *.

        if (!callVM(InvokeFunctionInfo, call))
            return false;

        // Un-nestle %esp from the argument vector. No prefix was pushed.
        masm.reserveStack(unusedStack);
    }

    masm.bind(&end);

    // If the return value of the constructing function is Primitive,
    // replace the return value with the Object from CreateThis.
    if (call->mir()->isConstructing()) {
        Label notPrimitive;

        masm.branchTestPrimitive(Assembler::NotEqual, JSReturnOperand, &notPrimitive);
        masm.loadValue(Address(StackPointer, unusedStack), JSReturnOperand);

        masm.bind(&notPrimitive);
    }

    return true;
}

bool
CodeGenerator::visitCallConstructor(LCallConstructor *call)
{
    JS_ASSERT(call->mir()->isConstructing());

    // Holds the function object.
    const LAllocation *callee = call->getFunction();
    Register calleereg = ToRegister(callee);

    uint32 callargslot = call->argslot();
    uint32 unusedStack = StackOffsetOfPassedArg(callargslot);

    typedef bool (*pf)(JSContext *, JSFunction *, uint32, Value *, Value *);
    static const VMFunction InvokeConstructorFunctionInfo =
        FunctionInfo<pf>(InvokeConstructorFunction);

    // Nestle %esp up to the argument vector.
    masm.freeStack(unusedStack);

    pushArg(StackPointer);          // argv.
    pushArg(Imm32(call->nargs()));  // argc.
    pushArg(calleereg);             // JSFunction *.

    if (!callVM(InvokeConstructorFunctionInfo, call))
        return false;

    // Un-nestle %esp from the argument vector. No prefix was pushed.
    masm.reserveStack(unusedStack);

    return true;
}

// Registers safe for use before generatePrologue().
static const uint32 EntryTempMask = Registers::TempMask & ~(1 << OsrFrameReg.code());

bool
CodeGenerator::generateArgumentsChecks()
{
    MIRGraph &mir = gen->graph();
    MResumePoint *rp = mir.entryResumePoint();

    // Reserve the amount of stack the actual frame will use. We have to undo
    // this before falling through to the method proper though, because the
    // monomorphic call case will bypass this entire path.
    masm.reserveStack(frameSize());

    // No registers are allocated yet, so it's safe to grab anything.
    Register temp = GeneralRegisterSet(EntryTempMask).getAny();

    CompileInfo &info = gen->info();

    // Indexes need to be shifted by one, to skip the scope chain slot.
    JS_ASSERT(info.scopeChainSlot() == 0);
    static const uint32 START_SLOT = 1;

    Label mismatched;
    for (uint32 i = START_SLOT; i < CountArgSlots(info.fun()); i++) {
        // All initial parameters are guaranteed to be MParameters.
        MParameter *param = rp->getOperand(i)->toParameter();
        types::TypeSet *types = param->typeSet();
        if (!types || types->unknown())
            continue;

        // Use ReturnReg as a scratch register here, since not all platforms
        // have an actual ScratchReg.
        int32 offset = ArgToStackOffset((i - START_SLOT) * sizeof(Value));
        masm.guardTypeSet(Address(StackPointer, offset), types, temp, &mismatched);
    }

    if (mismatched.used() && !bailoutFrom(&mismatched, graph.entrySnapshot()))
        return false;

    masm.freeStack(frameSize());

    return true;
}

// Out-of-line path to report over-recursed error and fail.
class CheckOverRecursedFailure : public OutOfLineCodeBase<CodeGenerator>
{
    LCheckOverRecursed *lir_;

  public:
    CheckOverRecursedFailure(LCheckOverRecursed *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitCheckOverRecursedFailure(this);
    }

    LCheckOverRecursed *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitCheckOverRecursed(LCheckOverRecursed *lir)
{
    // Ensure that this frame will not cross the stack limit.
    // This is a weak check, justified by Ion using the C stack: we must always
    // be some distance away from the actual limit, since if the limit is
    // crossed, an error must be thrown, which requires more frames.
    //
    // It must always be possible to trespass past the stack limit.
    // Ion may legally place frames very close to the limit. Calling additional
    // C functions may then violate the limit without any checking.

    JSRuntime *rt = gen->cx->runtime;

    // No registers are allocated yet, so it's safe to grab anything.
    const LAllocation *limit = lir->limitTemp();
    Register limitReg = ToRegister(limit);

    // Since Ion frames exist on the C stack, the stack limit may be
    // dynamically set by JS_SetThreadStackLimit() and JS_SetNativeStackQuota().
    uintptr_t *limitAddr = &rt->ionStackLimit;
    masm.loadPtr(AbsoluteAddress(limitAddr), limitReg);

    CheckOverRecursedFailure *ool = new CheckOverRecursedFailure(lir);
    if (!addOutOfLineCode(ool))
        return false;

    // Conditional forward (unlikely) branch to failure.
    masm.branchPtr(Assembler::BelowOrEqual, StackPointer, limitReg, ool->entry());

    return true;
}

bool
CodeGenerator::visitDefVar(LDefVar *lir)
{
    Register scopeChain = ToRegister(lir->getScopeChain());
    Register nameTemp   = ToRegister(lir->nameTemp());

    typedef bool (*pf)(JSContext *, HandlePropertyName, unsigned, HandleObject);
    static const VMFunction DefVarOrConstInfo = FunctionInfo<pf>(DefVarOrConst);

    masm.movePtr(ImmWord(lir->mir()->name()), nameTemp);

    pushArg(scopeChain); // JSObject *
    pushArg(Imm32(lir->mir()->attrs())); // unsigned
    pushArg(nameTemp); // PropertyName *

    if (!callVM(DefVarOrConstInfo, lir))
        return false;

    return true;
}

bool
CodeGenerator::visitCheckOverRecursedFailure(CheckOverRecursedFailure *ool)
{
    // The OOL path is hit if the recursion depth has been exceeded.
    // Throw an InternalError for over-recursion.

    typedef bool (*pf)(JSContext *);
    static const VMFunction ReportOverRecursedInfo =
        FunctionInfo<pf>(ReportOverRecursed);

    if (!callVM(ReportOverRecursedInfo, ool->lir()))
        return false;

#ifdef DEBUG
    // Do not rejoin: the above call causes failure.
    masm.breakpoint();
#endif
    return true;
}

bool
CodeGenerator::generateBody()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        current = graph.getBlock(i);
        for (LInstructionIterator iter = current->begin(); iter != current->end(); iter++) {
            IonSpew(IonSpew_Codegen, "instruction %s", iter->opName());
            if (!iter->accept(this))
                return false;
        }
        if (masm.oom())
            return false;
    }
    return true;
}

// Out-of-line object allocation for LNewArray.
class OutOfLineNewArray : public OutOfLineCodeBase<CodeGenerator>
{
    LNewArray *lir_;

  public:
    OutOfLineNewArray(LNewArray *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewArray(this);
    }

    LNewArray *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitNewArrayCallVM(LNewArray *lir)
{
    Register objReg = ToRegister(lir->output());

    typedef JSObject *(*pf)(JSContext *, uint32, types::TypeObject *);
    static const VMFunction NewInitArrayInfo = FunctionInfo<pf>(NewInitArray);

    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    pushArg(ImmGCPtr(lir->mir()->type()));
    pushArg(Imm32(lir->mir()->count()));

    if (!callVM(NewInitArrayInfo, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);

    return true;
}

bool
CodeGenerator::visitNewArray(LNewArray *lir)
{
    Register objReg = ToRegister(lir->output());
    types::TypeObject *typeObj = lir->mir()->type();
    uint32 count = lir->mir()->count();

    size_t maxArraySlots =
        gc::GetGCKindSlots(gc::FINALIZE_OBJECT_LAST) - ObjectElements::VALUES_PER_HEADER;

    // Allocate space using the VMCall
    // when mir hints it needs to get allocated immediatly,
    // but only when data doesn't fit the available array slots.
    bool allocating = lir->mir()->isAllocating() && count > maxArraySlots;

    if (!gen->cx->typeInferenceEnabled() || !typeObj || allocating)
        return visitNewArrayCallVM(lir);

    OutOfLineNewArray *ool = new OutOfLineNewArray(lir);
    if (!addOutOfLineCode(ool))
        return false;

    JSObject *templateObject = NewDenseUnallocatedArray(gen->cx, count);
    if (!templateObject)
        return false;
    templateObject->setType(typeObj);

    masm.getNewObject(gen->cx, objReg, templateObject, ool->entry());
    masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitOutOfLineNewArray(OutOfLineNewArray *ool)
{
    if (!visitNewArrayCallVM(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

// Out-of-line object allocation for JSOP_NEWOBJECT.
class OutOfLineNewObject : public OutOfLineCodeBase<CodeGenerator>
{
    LNewObject *lir_;

  public:
    OutOfLineNewObject(LNewObject *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewObject(this);
    }

    LNewObject *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitNewObjectVMCall(LNewObject *lir)
{
    Register objReg = ToRegister(lir->output());

    typedef JSObject *(*pf)(JSContext *, HandleObject, types::TypeObject *);
    static const VMFunction Info = FunctionInfo<pf>(NewInitObject);

    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    pushArg(ImmGCPtr(lir->mir()->type()));
    pushArg(ImmGCPtr(lir->mir()->baseObj()));
    if (!callVM(Info, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);
    return true;
}

bool
CodeGenerator::visitNewObject(LNewObject *lir)
{
    Register objReg = ToRegister(lir->output());

    RootedVarObject baseObj(gen->cx, lir->mir()->baseObj());
    types::TypeObject *typeObj = lir->mir()->type();

    if (!gen->cx->typeInferenceEnabled() || !typeObj ||
        !baseObj || baseObj->hasDynamicSlots())
    {
        return visitNewObjectVMCall(lir);
    }

    OutOfLineNewObject *ool = new OutOfLineNewObject(lir);
    if (!addOutOfLineCode(ool))
        return false;

    RootedVarObject templateObject(gen->cx, CopyInitializerObject(gen->cx, baseObj));
    if (!templateObject)
        return false;
    templateObject->setType(typeObj);

    masm.getNewObject(gen->cx, objReg, templateObject, ool->entry());
    masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitOutOfLineNewObject(OutOfLineNewObject *ool)
{
    if (!visitNewObjectVMCall(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitInitProp(LInitProp *lir)
{
    Register objReg = ToRegister(lir->getObject());

    typedef bool(*pf)(JSContext *cx, HandleObject obj, HandlePropertyName name, const Value &value);
    static const VMFunction InitPropInfo = FunctionInfo<pf>(InitProp);

    pushArg(ToValue(lir, LInitProp::ValueIndex));
    pushArg(ImmWord(lir->mir()->propertyName()));
    pushArg(objReg);

    return callVM(InitPropInfo, lir);
}

// Out-of-line object allocation for |new|, calling to the VM.
class OutOfLineCreateThis : public OutOfLineCodeBase<CodeGenerator>
{
    LCreateThis *lir_;

  public:
    OutOfLineCreateThis(LCreateThis *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineCreateThis(this);
    }

    LCreateThis *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitCreateThisVMCall(LCreateThis *lir)
{
    Register objReg    = ToRegister(lir->output());
    Register calleeReg = ToRegister(lir->getCallee());
    Register protoReg  = ToRegister(lir->getPrototype());

    // Since LCreateThis is not isCall(), used registers must be saved around the call.
    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    typedef JSObject *(*pf)(JSContext *cx, HandleObject callee, JSObject *proto);
    static const VMFunction CreateThisInfo =
        FunctionInfo<pf>(js_CreateThisForFunctionWithProto);

    pushArg(protoReg);
    pushArg(calleeReg);

    if (!callVM(CreateThisInfo, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);
    return true;
}

bool
CodeGenerator::visitCreateThis(LCreateThis *lir)
{
    Register objReg = ToRegister(lir->output());

    // Attempt to inline allocation if possible.
    if (lir->mir()->hasTemplateObject()) {
        OutOfLineCreateThis *ool = new OutOfLineCreateThis(lir);
        if (!addOutOfLineCode(ool))
            return false;

        masm.getNewObject(gen->cx, objReg, lir->mir()->getTemplateObject(), ool->entry());
        masm.bind(ool->rejoin());
        return true;
    }

    return visitCreateThisVMCall(lir);
}

bool
CodeGenerator::visitOutOfLineCreateThis(OutOfLineCreateThis *ool)
{
    if (!visitCreateThisVMCall(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArrayLength(LArrayLength *lir)
{
    Address length(ToRegister(lir->elements()), ObjectElements::offsetOfLength());
    masm.load32(length, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitTypedArrayLength(LTypedArrayLength *lir)
{
    Register obj = ToRegister(lir->object());
    Register out = ToRegister(lir->output());
    masm.unboxInt32(Address(obj, TypedArray::lengthOffset()), out);
    return true;
}

bool
CodeGenerator::visitTypedArrayElements(LTypedArrayElements *lir)
{
    Register obj = ToRegister(lir->object());
    Register out = ToRegister(lir->output());
    masm.loadPtr(Address(obj, TypedArray::dataOffset()), out);
    return true;
}

bool
CodeGenerator::visitStringLength(LStringLength *lir)
{
    Address lengthAndFlags(ToRegister(lir->string()), JSString::offsetOfLengthAndFlags());
    Register output = ToRegister(lir->output());

    masm.loadPtr(lengthAndFlags, output);
    masm.rshiftPtr(Imm32(JSString::LENGTH_SHIFT), output);
    return true;
}

bool
CodeGenerator::visitAbsI(LAbsI *ins)
{
    Register input = ToRegister(ins->input());
    Label positive;

    JS_ASSERT(input == ToRegister(ins->output()));
    masm.test32(input, input);
    masm.j(Assembler::GreaterThanOrEqual, &positive);
    masm.neg32(input);
    if (!ins->snapshot() || !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
    masm.bind(&positive);

    return true;
}

bool
CodeGenerator::visitBinaryV(LBinaryV *lir)
{
    typedef bool (*pf)(JSContext *, HandleValue, HandleValue, Value *);
    static const VMFunction AddInfo = FunctionInfo<pf>(js::AddValues);
    static const VMFunction SubInfo = FunctionInfo<pf>(js::SubValues);
    static const VMFunction MulInfo = FunctionInfo<pf>(js::MulValues);
    static const VMFunction DivInfo = FunctionInfo<pf>(js::DivValues);
    static const VMFunction ModInfo = FunctionInfo<pf>(js::ModValues);
    static const VMFunction UrshInfo = FunctionInfo<pf>(js::UrshValues);

    pushArg(ToValue(lir, LBinaryV::RhsInput));
    pushArg(ToValue(lir, LBinaryV::LhsInput));

    switch (lir->jsop()) {
      case JSOP_ADD:
        return callVM(AddInfo, lir);

      case JSOP_SUB:
        return callVM(SubInfo, lir);

      case JSOP_MUL:
        return callVM(MulInfo, lir);

      case JSOP_DIV:
        return callVM(DivInfo, lir);

      case JSOP_MOD:
        return callVM(ModInfo, lir);

      case JSOP_URSH:
        return callVM(UrshInfo, lir);

      default:
        JS_NOT_REACHED("Unexpected binary op");
        return false;
    }
}

bool
CodeGenerator::visitCompareV(LCompareV *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, const Value &, JSBool *);
    static const VMFunction EqInfo = FunctionInfo<pf>(ion::LooselyEqual<true>);
    static const VMFunction NeInfo = FunctionInfo<pf>(ion::LooselyEqual<false>);
    static const VMFunction StrictEqInfo = FunctionInfo<pf>(ion::StrictlyEqual<true>);
    static const VMFunction StrictNeInfo = FunctionInfo<pf>(ion::StrictlyEqual<false>);
    static const VMFunction LtInfo = FunctionInfo<pf>(ion::LessThan);
    static const VMFunction LeInfo = FunctionInfo<pf>(ion::LessThanOrEqual);
    static const VMFunction GtInfo = FunctionInfo<pf>(ion::GreaterThan);
    static const VMFunction GeInfo = FunctionInfo<pf>(ion::GreaterThanOrEqual);

    pushArg(ToValue(lir, LBinaryV::RhsInput));
    pushArg(ToValue(lir, LBinaryV::LhsInput));

    switch (lir->jsop()) {
      case JSOP_EQ:
        return callVM(EqInfo, lir);

      case JSOP_NE:
        return callVM(NeInfo, lir);

      case JSOP_STRICTEQ:
        return callVM(StrictEqInfo, lir);

      case JSOP_STRICTNE:
        return callVM(StrictNeInfo, lir);

      case JSOP_LT:
        return callVM(LtInfo, lir);

      case JSOP_LE:
        return callVM(LeInfo, lir);

      case JSOP_GT:
        return callVM(GtInfo, lir);

      case JSOP_GE:
        return callVM(GeInfo, lir);

      default:
        JS_NOT_REACHED("Unexpected compare op");
        return false;
    }
}

bool
CodeGenerator::visitIsNullOrUndefined(LIsNullOrUndefined *lir)
{
    JSOp op = lir->mir()->jsop();
    MIRType specialization = lir->mir()->specialization();
    JS_ASSERT(IsNullOrUndefined(specialization));

    const ValueOperand value = ToValue(lir, LIsNullOrUndefined::Value);
    Register output = ToRegister(lir->output());

    if (op == JSOP_EQ || op == JSOP_NE) {
        Register tag = masm.splitTagForTest(value);

        Label nullOrUndefined, done;
        masm.branchTestNull(Assembler::Equal, tag, &nullOrUndefined);
        masm.branchTestUndefined(Assembler::Equal, tag, &nullOrUndefined);

        masm.move32(Imm32(op == JSOP_NE), output);
        masm.jump(&done);

        masm.bind(&nullOrUndefined);
        masm.move32(Imm32(op == JSOP_EQ), output);
        masm.bind(&done);
        return true;
    }

    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    Assembler::Condition cond = JSOpToCondition(op);
    if (specialization == MIRType_Null)
        cond = masm.testNull(cond, value);
    else
        cond = masm.testUndefined(cond, value);

    emitSet(cond, output);
    return true;
}

bool
CodeGenerator::visitIsNullOrUndefinedAndBranch(LIsNullOrUndefinedAndBranch *lir)
{
    JSOp op = lir->mir()->jsop();
    MIRType specialization = lir->mir()->specialization();
    JS_ASSERT(IsNullOrUndefined(specialization));

    const ValueOperand value = ToValue(lir, LIsNullOrUndefinedAndBranch::Value);

    if (op == JSOP_EQ || op == JSOP_NE) {
        MBasicBlock *ifTrue;
        MBasicBlock *ifFalse;

        if (op == JSOP_EQ) {
            ifTrue = lir->ifTrue();
            ifFalse = lir->ifFalse();
        } else {
            // Swap branches.
            ifTrue = lir->ifFalse();
            ifFalse = lir->ifTrue();
            op = JSOP_EQ;
        }

        Register tag = masm.splitTagForTest(value);
        masm.branchTestNull(Assembler::Equal, tag, ifTrue->lir()->label());

        Assembler::Condition cond = masm.testUndefined(Assembler::Equal, tag);
        emitBranch(cond, ifTrue, ifFalse);
        return true;
    }

    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    Assembler::Condition cond = JSOpToCondition(op);
    if (specialization == MIRType_Null)
        cond = masm.testNull(cond, value);
    else
        cond = masm.testUndefined(cond, value);

    emitBranch(cond, lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGenerator::visitConcat(LConcat *lir)
{
    typedef JSString *(*pf)(JSContext *, HandleString, HandleString);
    static const VMFunction js_ConcatStringsInfo = FunctionInfo<pf>(js_ConcatStrings);

    pushArg(ToRegister(lir->rhs()));
    pushArg(ToRegister(lir->lhs()));
    if (!callVM(js_ConcatStringsInfo, lir))
        return false;
    return true;
}

bool
CodeGenerator::visitCharCodeAt(LCharCodeAt *lir)
{
    Register str = ToRegister(lir->str());
    Register index = ToRegister(lir->index());
    Register output = ToRegister(lir->output());

    typedef bool (*pf)(JSContext *, JSString *);
    static const VMFunction ensureLinearInfo = FunctionInfo<pf>(JSString::ensureLinear);
    OutOfLineCode *ool = oolCallVM(ensureLinearInfo, lir, (ArgList(), str), StoreNothing());
    if (!ool)
        return false;

    Address lengthAndFlagsAddr(str, JSString::offsetOfLengthAndFlags());
    masm.loadPtr(lengthAndFlagsAddr, output);

    JS_ASSERT(JSString::LINEAR_FLAGS == 0);
    masm.branchTest32(Assembler::NonZero, output, Imm32(JSString::LINEAR_MASK), ool->entry());
    masm.bind(ool->rejoin());

    // getChars
    Address charsAddr(str, JSString::offsetOfChars());
    masm.loadPtr(charsAddr, output);
    masm.load16ZeroExtend(BaseIndex(output, index, TimesTwo, 0), output);

    return true;
}

bool
CodeGenerator::visitFromCharCode(LFromCharCode *lir)
{
    Register code = ToRegister(lir->code());
    Register output = ToRegister(lir->output());

    // This static variable would be used by js_NewString as an initial buffer.
    Label fast;
    masm.cmpPtr(code, ImmWord(StaticStrings::UNIT_STATIC_LIMIT));
    masm.j(Assembler::Below, &fast);

    // Store the code in the tmpString. This assume that jitted codes are not
    // running concurently.
    static jschar tmpString[2] = {0, 0};
    Register tmpStringAddr = output;
    masm.movePtr(ImmWord(tmpString), tmpStringAddr);
    masm.store16(code, Address(tmpStringAddr, 0));

    // Copy the tmpString to a newly allocated string.
    typedef JSFixedString *(*pf)(JSContext *, const jschar *, size_t);
    static const VMFunction newStringCopyNInfo = FunctionInfo<pf>(js_NewStringCopyN);
    OutOfLineCode *ool = oolCallVM(newStringCopyNInfo, lir, (ArgList(), tmpStringAddr, Imm32(1)),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    masm.jump(ool->entry());
    masm.bind(&fast);
    masm.movePtr(ImmWord(&gen->cx->runtime->staticStrings.unitStaticTable), output);
    masm.loadPtr(BaseIndex(output, code, ScalePointer), output);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitInitializedLength(LInitializedLength *lir)
{
    Address initLength(ToRegister(lir->elements()), ObjectElements::offsetOfInitializedLength());
    masm.load32(initLength, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitSetInitializedLength(LSetInitializedLength *lir)
{
    Address initLength(ToRegister(lir->elements()), ObjectElements::offsetOfInitializedLength());
    Int32Key index = ToInt32Key(lir->index());

    masm.bumpKey(&index, 1);
    masm.storeKey(index, initLength);
    // Restore register value if it is used/captured after.
    masm.bumpKey(&index, -1);
    return true;
}

bool
CodeGenerator::visitNotV(LNotV *lir)
{
    Label setFalse;
    Label join;
    masm.branchTestValueTruthy(ToValue(lir, LNotV::Input), &setFalse, ToFloatRegister(lir->tempFloat()));

    // fallthrough to set true
    masm.move32(Imm32(1), ToRegister(lir->getDef(0)));
    masm.jump(&join);

    // true case rediercts to setFalse
    masm.bind(&setFalse);
    masm.move32(Imm32(0), ToRegister(lir->getDef(0)));

    // both branches meet here.
    masm.bind(&join);
    return true;
}

bool
CodeGenerator::visitBoundsCheck(LBoundsCheck *lir)
{
    if (lir->index()->isConstant())
        masm.cmp32(ToRegister(lir->length()), Imm32(ToInt32(lir->index())));
    else
        masm.cmp32(ToRegister(lir->length()), ToRegister(lir->index()));
    return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
}

bool
CodeGenerator::visitBoundsCheckRange(LBoundsCheckRange *lir)
{
    int32 min = lir->mir()->minimum();
    int32 max = lir->mir()->maximum();
    JS_ASSERT(max >= min);

    Register temp = ToRegister(lir->getTemp(0));
    if (lir->index()->isConstant()) {
        int32 nmin, nmax;
        int32 index = ToInt32(lir->index());
        if (SafeAdd(index, min, &nmin) && SafeAdd(index, max, &nmax) && nmin >= 0) {
            masm.cmp32(ToRegister(lir->length()), Imm32(nmax));
            return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
        }
        masm.mov(Imm32(index), temp);
    } else {
        masm.mov(ToRegister(lir->index()), temp);
    }

    // If the minimum and maximum differ then do an underflow check first.
    // If the two are the same then doing an unsigned comparison on the
    // length will also catch a negative index.
    if (min != max) {
        if (min != 0) {
            masm.add32(Imm32(min), temp);
            if (!bailoutIf(Assembler::Overflow, lir->snapshot()))
                return false;
            int32 diff;
            if (SafeSub(max, min, &diff))
                max = diff;
            else
                masm.sub32(Imm32(min), temp);
        }

        masm.cmp32(temp, Imm32(0));
        if (!bailoutIf(Assembler::LessThan, lir->snapshot()))
            return false;
    }

    // Compute the maximum possible index. No overflow check is needed when
    // max > 0. We can only wraparound to a negative number, which will test as
    // larger than all nonnegative numbers in the unsigned comparison, and the
    // length is required to be nonnegative (else testing a negative length
    // would succeed on any nonnegative index).
    if (max != 0) {
        masm.add32(Imm32(max), temp);
        if (max < 0 && !bailoutIf(Assembler::Overflow, lir->snapshot()))
            return false;
    }

    masm.cmp32(ToRegister(lir->length()), temp);
    return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
}

bool
CodeGenerator::visitBoundsCheckLower(LBoundsCheckLower *lir)
{
    int32 min = lir->mir()->minimum();
    masm.cmp32(ToRegister(lir->index()), Imm32(min));
    return bailoutIf(Assembler::LessThan, lir->snapshot());
}

class OutOfLineStoreElementHole : public OutOfLineCodeBase<CodeGenerator>
{
    LInstruction *ins_;
    Label rejoinStore_;

  public:
    OutOfLineStoreElementHole(LInstruction *ins)
      : ins_(ins)
    {
        JS_ASSERT(ins->isStoreElementHoleV() || ins->isStoreElementHoleT());
    }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineStoreElementHole(this);
    }
    LInstruction *ins() const {
        return ins_;
    }
    Label *rejoinStore() {
        return &rejoinStore_;
    }
};

bool
CodeGenerator::visitStoreElementT(LStoreElementT *store)
{
    if (store->mir()->needsBarrier())
       emitPreBarrier(ToRegister(store->elements()), store->index(), store->mir()->elementType());

    storeElementTyped(store->value(), store->mir()->value()->type(), store->mir()->elementType(),
                      ToRegister(store->elements()), store->index());
    return true;
}

bool
CodeGenerator::visitStoreElementV(LStoreElementV *lir)
{
    const ValueOperand value = ToValue(lir, LStoreElementV::Value);
    Register elements = ToRegister(lir->elements());

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, lir->index(), MIRType_Value);

    if (lir->index()->isConstant())
        masm.storeValue(value, Address(elements, ToInt32(lir->index()) * sizeof(js::Value)));
    else
        masm.storeValue(value, BaseIndex(elements, ToRegister(lir->index()), TimesEight));
    return true;
}

bool
CodeGenerator::visitStoreElementHoleT(LStoreElementHoleT *lir)
{
    OutOfLineStoreElementHole *ool = new OutOfLineStoreElementHole(lir);
    if (!addOutOfLineCode(ool))
        return false;

    Register elements = ToRegister(lir->elements());
    const LAllocation *index = lir->index();

    // OOL path if index >= initializedLength.
    Address initLength(elements, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::BelowOrEqual, initLength, ToInt32Key(index), ool->entry());

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, index, lir->mir()->elementType());

    masm.bind(ool->rejoinStore());
    storeElementTyped(lir->value(), lir->mir()->value()->type(), lir->mir()->elementType(),
                      elements, index);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitStoreElementHoleV(LStoreElementHoleV *lir)
{
    OutOfLineStoreElementHole *ool = new OutOfLineStoreElementHole(lir);
    if (!addOutOfLineCode(ool))
        return false;

    Register elements = ToRegister(lir->elements());
    const LAllocation *index = lir->index();
    const ValueOperand value = ToValue(lir, LStoreElementHoleV::Value);

    // OOL path if index >= initializedLength.
    Address initLength(elements, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::BelowOrEqual, initLength, ToInt32Key(index), ool->entry());

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, index, lir->mir()->elementType());

    masm.bind(ool->rejoinStore());
    if (lir->index()->isConstant())
        masm.storeValue(value, Address(elements, ToInt32(lir->index()) * sizeof(js::Value)));
    else
        masm.storeValue(value, BaseIndex(elements, ToRegister(lir->index()), TimesEight));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineStoreElementHole(OutOfLineStoreElementHole *ool)
{
    Register object, elements;
    LInstruction *ins = ool->ins();
    const LAllocation *index;
    MIRType valueType;
    ConstantOrRegister value;

    if (ins->isStoreElementHoleV()) {
        LStoreElementHoleV *store = ins->toStoreElementHoleV();
        object = ToRegister(store->object());
        elements = ToRegister(store->elements());
        index = store->index();
        valueType = store->mir()->value()->type();
        value = TypedOrValueRegister(ToValue(store, LStoreElementHoleV::Value));
    } else {
        LStoreElementHoleT *store = ins->toStoreElementHoleT();
        object = ToRegister(store->object());
        elements = ToRegister(store->elements());
        index = store->index();
        valueType = store->mir()->value()->type();
        if (store->value()->isConstant())
            value = ConstantOrRegister(*store->value()->toConstant());
        else
            value = TypedOrValueRegister(valueType, ToAnyRegister(store->value()));
    }

    // If index == initializedLength, try to bump the initialized length inline.
    // If index > initializedLength, call a stub. Note that this relies on the
    // condition flags sticking from the incoming branch.
    Label callStub;
    masm.j(Assembler::NotEqual, &callStub);

    Int32Key key = ToInt32Key(index);

    // Check array capacity.
    masm.branchKey(Assembler::BelowOrEqual, Address(elements, ObjectElements::offsetOfCapacity()),
                   key, &callStub);

    // Update initialized length. The capacity guard above ensures this won't overflow,
    // due to NELEMENTS_LIMIT.
    masm.bumpKey(&key, 1);
    masm.storeKey(key, Address(elements, ObjectElements::offsetOfInitializedLength()));

    // Update length if length < initializedLength.
    Label dontUpdate;
    masm.branchKey(Assembler::AboveOrEqual, Address(elements, ObjectElements::offsetOfLength()),
                   key, &dontUpdate);
    masm.storeKey(key, Address(elements, ObjectElements::offsetOfLength()));
    masm.bind(&dontUpdate);

    masm.bumpKey(&key, -1);

    if (ins->isStoreElementHoleT() && valueType != MIRType_Double) {
        // The inline path for StoreElementHoleT does not always store the type tag,
        // so we do the store on the OOL path. We use MIRType_None for the element type
        // so that storeElementTyped will always store the type tag.
        storeElementTyped(ins->toStoreElementHoleT()->value(), valueType, MIRType_None, elements,
                          index);
        masm.jump(ool->rejoin());
    } else {
        // Jump to the inline path where we will store the value.
        masm.jump(ool->rejoinStore());
    }

    masm.bind(&callStub);
    saveLive(ins);

    typedef bool (*pf)(JSContext *, JSObject *, const Value &, const Value &, JSBool strict);
    static const VMFunction Info = FunctionInfo<pf>(SetObjectElement);

    pushArg(Imm32(current->mir()->strictModeCode()));
    pushArg(value);
    if (index->isConstant())
        pushArg(*index->toConstant());
    else
        pushArg(TypedOrValueRegister(MIRType_Int32, ToAnyRegister(index)));
    pushArg(object);
    if (!callVM(Info, ins))
        return false;

    restoreLive(ins);
    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::emitArrayPopShift(LInstruction *lir, const MArrayPopShift *mir, Register obj,
                                 Register elementsTemp, Register lengthTemp, TypedOrValueRegister out)
{
    OutOfLineCode *ool;
    typedef bool (*pf)(JSContext *, JSObject *, Value *);

    if (mir->mode() == MArrayPopShift::Pop) {
        static const VMFunction Info = FunctionInfo<pf>(ion::ArrayPopDense);
        ool = oolCallVM(Info, lir, (ArgList(), obj), StoreValueTo(out));
        if (!ool)
            return false;
    } else {
        JS_ASSERT(mir->mode() == MArrayPopShift::Shift);
        static const VMFunction Info = FunctionInfo<pf>(ion::ArrayShiftDense);
        ool = oolCallVM(Info, lir, (ArgList(), obj), StoreValueTo(out));
        if (!ool)
            return false;
    }

    // Load elements and length.
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), elementsTemp);
    masm.load32(Address(elementsTemp, ObjectElements::offsetOfLength()), lengthTemp);

    // VM call if length != initializedLength.
    Int32Key key = Int32Key(lengthTemp);
    Address initLength(elementsTemp, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::NotEqual, initLength, key, ool->entry());

    // Test for length != 0. On zero length either take a VM call or generate
    // an undefined value, depending on whether the call is known to produce
    // undefined.
    Label done;
    if (mir->maybeUndefined()) {
        Label notEmpty;
        masm.branchTest32(Assembler::NonZero, lengthTemp, lengthTemp, &notEmpty);
        masm.moveValue(UndefinedValue(), out.valueReg());
        masm.jump(&done);
        masm.bind(&notEmpty);
    } else {
        masm.branchTest32(Assembler::Zero, lengthTemp, lengthTemp, ool->entry());
    }

    masm.bumpKey(&key, -1);

    if (mir->mode() == MArrayPopShift::Pop) {
        masm.loadElementTypedOrValue(BaseIndex(elementsTemp, lengthTemp, TimesEight), out,
                                     mir->needsHoleCheck(), ool->entry());
    } else {
        JS_ASSERT(mir->mode() == MArrayPopShift::Shift);
        masm.loadElementTypedOrValue(Address(elementsTemp, 0), out, mir->needsHoleCheck(),
                                     ool->entry());
    }

    masm.store32(lengthTemp, Address(elementsTemp, ObjectElements::offsetOfLength()));
    masm.store32(lengthTemp, Address(elementsTemp, ObjectElements::offsetOfInitializedLength()));

    if (mir->mode() == MArrayPopShift::Shift) {
        // Don't save the temp registers.
        RegisterSet temps;
        temps.add(elementsTemp);
        temps.add(lengthTemp);

        saveVolatile(temps);
        masm.setupUnalignedABICall(1, lengthTemp);
        masm.passABIArg(obj);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ArrayShiftMoveElements));
        restoreVolatile(temps);
    }

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArrayPopShiftV(LArrayPopShiftV *lir)
{
    Register obj = ToRegister(lir->object());
    Register elements = ToRegister(lir->temp0());
    Register length = ToRegister(lir->temp1());
    TypedOrValueRegister out(ToOutValue(lir));
    return emitArrayPopShift(lir, lir->mir(), obj, elements, length, out);
}

bool
CodeGenerator::visitArrayPopShiftT(LArrayPopShiftT *lir)
{
    Register obj = ToRegister(lir->object());
    Register elements = ToRegister(lir->temp0());
    Register length = ToRegister(lir->temp1());
    TypedOrValueRegister out(lir->mir()->type(), ToAnyRegister(lir->output()));
    return emitArrayPopShift(lir, lir->mir(), obj, elements, length, out);
}

bool
CodeGenerator::emitArrayPush(LInstruction *lir, const MArrayPush *mir, Register obj,
                             ConstantOrRegister value, Register elementsTemp, Register length)
{
    typedef bool (*pf)(JSContext *, JSObject *, const Value &, uint32_t *);
    static const VMFunction Info = FunctionInfo<pf>(ion::ArrayPushDense);
    OutOfLineCode *ool = oolCallVM(Info, lir, (ArgList(), obj, value), StoreRegisterTo(length));
    if (!ool)
        return false;

    // Load elements and length.
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), elementsTemp);
    masm.load32(Address(elementsTemp, ObjectElements::offsetOfLength()), length);

    Int32Key key = Int32Key(length);
    Address initLength(elementsTemp, ObjectElements::offsetOfInitializedLength());
    Address capacity(elementsTemp, ObjectElements::offsetOfCapacity());

    // Guard length == initializedLength.
    masm.branchKey(Assembler::NotEqual, initLength, key, ool->entry());

    // Guard length < capacity.
    masm.branchKey(Assembler::BelowOrEqual, capacity, key, ool->entry());

    masm.storeConstantOrRegister(value, BaseIndex(elementsTemp, length, TimesEight));

    masm.bumpKey(&key, 1);
    masm.store32(length, Address(elementsTemp, ObjectElements::offsetOfLength()));
    masm.store32(length, Address(elementsTemp, ObjectElements::offsetOfInitializedLength()));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArrayPushV(LArrayPushV *lir)
{
    Register obj = ToRegister(lir->object());
    Register elementsTemp = ToRegister(lir->temp());
    Register length = ToRegister(lir->output());
    ConstantOrRegister value = TypedOrValueRegister(ToValue(lir, LArrayPushV::Value));
    return emitArrayPush(lir, lir->mir(), obj, value, elementsTemp, length);
}

bool
CodeGenerator::visitArrayPushT(LArrayPushT *lir)
{
    Register obj = ToRegister(lir->object());
    Register elementsTemp = ToRegister(lir->temp());
    Register length = ToRegister(lir->output());
    ConstantOrRegister value;
    if (lir->value()->isConstant())
        value = ConstantOrRegister(*lir->value()->toConstant());
    else
        value = TypedOrValueRegister(lir->mir()->value()->type(), ToAnyRegister(lir->value()));
    return emitArrayPush(lir, lir->mir(), obj, value, elementsTemp, length);
}

bool
CodeGenerator::visitCallIteratorStart(LCallIteratorStart *lir)
{
    typedef JSObject *(*pf)(JSContext *, HandleObject, uint32_t);
    static const VMFunction Info = FunctionInfo<pf>(GetIteratorObject);

    pushArg(Imm32(lir->mir()->flags()));
    pushArg(ToRegister(lir->object()));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitIteratorStart(LIteratorStart *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register output = ToRegister(lir->output());

    uint32_t flags = lir->mir()->flags();

    typedef JSObject *(*pf)(JSContext *, HandleObject, uint32_t);
    static const VMFunction Info = FunctionInfo<pf>(GetIteratorObject);

    OutOfLineCode *ool = oolCallVM(Info, lir, (ArgList(), obj, Imm32(flags)), StoreRegisterTo(output));
    if (!ool)
        return false;

    const Register temp1 = ToRegister(lir->temp1());
    const Register temp2 = ToRegister(lir->temp2());
    const Register niTemp = ToRegister(lir->temp3()); // Holds the NativeIterator object.

    // Iterators other than for-in should use LCallIteratorStart.
    JS_ASSERT(flags == JSITER_ENUMERATE);

    // Fetch the most recent iterator and ensure it's not NULL.
    masm.loadPtr(AbsoluteAddress(&gen->cx->runtime->nativeIterCache.last), output);
    masm.branchTestPtr(Assembler::Zero, output, output, ool->entry());

    // Load NativeIterator.
    masm.loadObjPrivate(output, JSObject::ITER_CLASS_NFIXED_SLOTS, niTemp);

    // Ensure the |active| and |unreusable| bits are not set.
    masm.branchTest32(Assembler::NonZero, Address(niTemp, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_ACTIVE|JSITER_UNREUSABLE), ool->entry());

    // Load the iterator's shape array.
    masm.loadPtr(Address(niTemp, offsetof(NativeIterator, shapes_array)), temp2);

    // Compare shape of object with the first shape.
    masm.loadObjShape(obj, temp1);
    masm.branchPtr(Assembler::NotEqual, Address(temp2, 0), temp1, ool->entry());

    // Compare shape of object's prototype with the second shape.
    masm.loadObjProto(obj, temp1);
    masm.loadObjShape(temp1, temp1);
    masm.branchPtr(Assembler::NotEqual, Address(temp2, sizeof(Shape *)), temp1, ool->entry());

    // Ensure the object's prototype's prototype is NULL. The last native iterator
    // will always have a prototype chain length of one (i.e. it must be a plain
    // object), so we do not need to generate a loop here.
    masm.loadObjProto(obj, temp1);
    masm.loadObjProto(temp1, temp1);
    masm.branchTestPtr(Assembler::NonZero, temp1, temp1, ool->entry());

    // Write barrier for stores to the iterator. We only need to take a write
    // barrier if NativeIterator::obj is actually going to change.
    if (gen->cx->compartment->needsBarrier()) {
        masm.branchPtr(Assembler::NotEqual, Address(niTemp, offsetof(NativeIterator, obj)),
                       obj, ool->entry());
    }

    // Mark iterator as active.
    masm.storePtr(obj, Address(niTemp, offsetof(NativeIterator, obj)));
    masm.or32(Imm32(JSITER_ACTIVE), Address(niTemp, offsetof(NativeIterator, flags)));

    // Chain onto the active iterator stack.
    masm.loadJSContext(temp1);
    masm.loadPtr(Address(temp1, offsetof(JSContext, enumerators)), temp2);
    masm.storePtr(temp2, Address(niTemp, offsetof(NativeIterator, next)));
    masm.storePtr(output, Address(temp1, offsetof(JSContext, enumerators)));

    masm.bind(ool->rejoin());
    return true;
}

static void
LoadNativeIterator(MacroAssembler &masm, Register obj, Register dest, Label *failures)
{
    JS_ASSERT(obj != dest);

    // Test class.
    masm.branchTestObjClass(Assembler::NotEqual, obj, dest, &IteratorClass, failures);

    // Load NativeIterator object.
    masm.loadObjPrivate(obj, JSObject::ITER_CLASS_NFIXED_SLOTS, dest);

    // Test for value iterators.
    masm.branchTest32(Assembler::NonZero, Address(dest, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_FOREACH), failures);
}

bool
CodeGenerator::visitIteratorNext(LIteratorNext *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register temp = ToRegister(lir->temp());
    const ValueOperand output = ToOutValue(lir);

    typedef bool (*pf)(JSContext *, JSObject *, Value *);
    static const VMFunction Info = FunctionInfo<pf>(js_IteratorNext);

    OutOfLineCode *ool = oolCallVM(Info, lir, (ArgList(), obj), StoreValueTo(output));
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, temp, ool->entry());

    // Get cursor, next string.
    masm.loadPtr(Address(temp, offsetof(NativeIterator, props_cursor)), output.scratchReg());
    masm.loadPtr(Address(output.scratchReg(), 0), output.scratchReg());
    masm.tagValue(JSVAL_TYPE_STRING, output.scratchReg(), output);

    // Increase the cursor.
    masm.addPtr(Imm32(sizeof(JSString *)), Address(temp, offsetof(NativeIterator, props_cursor)));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitIteratorMore(LIteratorMore *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register output = ToRegister(lir->output());
    const Register temp = ToRegister(lir->temp());

    typedef bool (*pf)(JSContext *, HandleObject, JSBool *);
    static const VMFunction Info = FunctionInfo<pf>(ion::IteratorMore);
    OutOfLineCode *ool = oolCallVM(Info, lir, (ArgList(), obj), StoreRegisterTo(output));
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, output, ool->entry());

    // Set output to true if props_cursor < props_end.
    masm.loadPtr(Address(output, offsetof(NativeIterator, props_end)), temp);
    masm.cmpPtr(Address(output, offsetof(NativeIterator, props_cursor)), temp);
    emitSet(Assembler::LessThan, output);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitIteratorEnd(LIteratorEnd *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register temp1 = ToRegister(lir->temp1());
    const Register temp2 = ToRegister(lir->temp2());

    typedef bool (*pf)(JSContext *, JSObject *);
    static const VMFunction Info = FunctionInfo<pf>(CloseIterator);

    OutOfLineCode *ool = oolCallVM(Info, lir, (ArgList(), obj), StoreNothing());
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, temp1, ool->entry());

    // Clear active bit.
    masm.and32(Imm32(~JSITER_ACTIVE), Address(temp1, offsetof(NativeIterator, flags)));

    // Reset property cursor.
    masm.loadPtr(Address(temp1, offsetof(NativeIterator, props_array)), temp2);
    masm.storePtr(temp2, Address(temp1, offsetof(NativeIterator, props_cursor)));

    // Advance enumerators list.
    masm.loadJSContext(temp2);
    masm.loadPtr(Address(temp1, offsetof(NativeIterator, next)), temp1);
    masm.storePtr(temp1, Address(temp2, offsetof(JSContext, enumerators)));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::generate()
{
    JSContext *cx = gen->cx;

    if (!safepoints_.init(graph.localSlotCount()))
        return false;

    // Before generating any code, we generate type checks for all parameters.
    // This comes before deoptTable_, because we can't use deopt tables without
    // creating the actual frame.
    if (!generateArgumentsChecks())
        return false;

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
    if (!generateInvalidateEpilogue())
        return false;
    if (!generateOutOfLineCode())
        return false;

    if (masm.oom())
        return false;

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    // We encode safepoints after the OSI-point offsets have been determined.
    encodeSafepoints();

    JSScript *script = gen->info().script();
    JS_ASSERT(!script->ion);

    uint32 scriptFrameSize = frameClass_ == FrameSizeClass::None()
                           ? frameDepth_
                           : FrameSizeClass::FromDepth(frameDepth_).frameSize();

    script->ion = IonScript::New(cx, graph.localSlotCount(), scriptFrameSize, snapshots_.size(),
                                 bailouts_.length(), graph.numConstants(),
                                 safepointIndices_.length(), osiIndices_.length(),
                                 cacheList_.length(), safepoints_.size());
    if (!script->ion)
        return false;
    invalidateEpilogueData_.fixup(&masm);
    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, invalidateEpilogueData_),
                                       ImmWord(uintptr_t(script->ion)),
                                       ImmWord(uintptr_t(-1)));

    IonSpew(IonSpew_Codegen, "Created IonScript %p (raw %p)",
            (void *) script->ion, (void *) code->raw());

    script->ion->setInvalidationEpilogueDataOffset(invalidateEpilogueData_.offset());
    script->ion->setOsrPc(gen->info().osrPc());
    script->ion->setOsrEntryOffset(getOsrEntryOffset());
    ptrdiff_t real_invalidate = masm.actualOffset(invalidate_.offset());
    script->ion->setInvalidationEpilogueOffset(real_invalidate);

    script->ion->setMethod(code);
    script->ion->setDeoptTable(deoptTable_);
    if (snapshots_.size())
        script->ion->copySnapshots(&snapshots_);
    if (bailouts_.length())
        script->ion->copyBailoutTable(&bailouts_[0]);
    if (graph.numConstants())
        script->ion->copyConstants(graph.constantPool());
    if (safepointIndices_.length())
        script->ion->copySafepointIndices(&safepointIndices_[0], masm);
    if (osiIndices_.length())
        script->ion->copyOsiIndices(&osiIndices_[0], masm);
    if (cacheList_.length())
        script->ion->copyCacheEntries(&cacheList_[0], masm);
    if (safepoints_.size())
        script->ion->copySafepoints(&safepoints_);

    linkAbsoluteLabels();
    
    return true;
}

// An out-of-line path to convert a boxed int32 to a double.
class OutOfLineUnboxDouble : public OutOfLineCodeBase<CodeGenerator>
{
    LUnboxDouble *unboxDouble_;

  public:
    OutOfLineUnboxDouble(LUnboxDouble *unboxDouble)
      : unboxDouble_(unboxDouble)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineUnboxDouble(this);
    }

    LUnboxDouble *unboxDouble() const {
        return unboxDouble_;
    }
};

bool
CodeGenerator::visitUnboxDouble(LUnboxDouble *lir)
{
    const ValueOperand box = ToValue(lir, LUnboxDouble::Input);
    const LDefinition *result = lir->output();

    // Out-of-line path to convert int32 to double or bailout
    // if this instruction is fallible.
    OutOfLineUnboxDouble *ool = new OutOfLineUnboxDouble(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTestDouble(Assembler::NotEqual, box, ool->entry());
    masm.unboxDouble(box, ToFloatRegister(result));
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineUnboxDouble(OutOfLineUnboxDouble *ool)
{
    LUnboxDouble *ins = ool->unboxDouble();
    const ValueOperand value = ToValue(ins, LUnboxDouble::Input);

    if (ins->mir()->fallible()) {
        Assembler::Condition cond = masm.testInt32(Assembler::NotEqual, value);
        if (!bailoutIf(cond, ins->snapshot()))
            return false;
    }
    masm.int32ValueToDouble(value, ToFloatRegister(ins->output()));
    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*GetPropertyOrNameFn)(JSContext *, HandleObject, HandlePropertyName, Value *);

bool
CodeGenerator::visitCallGetProperty(LCallGetProperty *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, PropertyName *, Value *);
    static const VMFunction Info = FunctionInfo<pf>(GetProperty);

    pushArg(ImmGCPtr(lir->mir()->atom()));
    pushArg(ToValue(lir, LCallGetProperty::Value));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitCallGetName(LCallGetName *lir)
{
    static const VMFunction Info = FunctionInfo<GetPropertyOrNameFn>(GetScopeName);

    pushArg(ImmGCPtr(lir->mir()->atom()));
    pushArg(ToRegister(lir->getOperand(0)));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitCallGetNameTypeOf(LCallGetNameTypeOf *lir)
{
    static const VMFunction Info = FunctionInfo<GetPropertyOrNameFn>(GetScopeNameForTypeOf);

    pushArg(ImmGCPtr(lir->mir()->atom()));
    pushArg(ToRegister(lir->getOperand(0)));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitCallGetElement(LCallGetElement *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, const Value &, Value *);
    static const VMFunction GetElementInfo = FunctionInfo<pf>(js::GetElement);
    static const VMFunction CallElementInfo = FunctionInfo<pf>(js::CallElement);

    pushArg(ToValue(lir, LCallGetElement::RhsInput));
    pushArg(ToValue(lir, LCallGetElement::LhsInput));

    JSOp op = JSOp(*lir->mir()->resumePoint()->pc());

    if (op == JSOP_GETELEM) {
        return callVM(GetElementInfo, lir);
    } else {
        JS_ASSERT(op == JSOP_CALLELEM);
        return callVM(CallElementInfo, lir);
    }
}

bool
CodeGenerator::visitCallSetElement(LCallSetElement *lir)
{
    typedef bool (*pf)(JSContext *, JSObject *, const Value &, const Value &, JSBool strict);
    static const VMFunction SetObjectElementInfo = FunctionInfo<pf>(js::SetObjectElement);

    pushArg(Imm32(current->mir()->strictModeCode()));
    pushArg(ToValue(lir, LCallSetElement::Value));
    pushArg(ToValue(lir, LCallSetElement::Index));
    pushArg(ToRegister(lir->getOperand(0)));
    return callVM(SetObjectElementInfo, lir);
}

bool
CodeGenerator::visitLoadFixedSlotV(LLoadFixedSlotV *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();
    ValueOperand result = GetValueOutput(ins);

    masm.loadValue(Address(obj, JSObject::getFixedSlotOffset(slot)), result);
    return true;
}

bool
CodeGenerator::visitLoadFixedSlotT(LLoadFixedSlotT *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();
    AnyRegister result = ToAnyRegister(ins->getDef(0));

    masm.loadUnboxedValue(Address(obj, JSObject::getFixedSlotOffset(slot)), result);
    return true;
}

bool
CodeGenerator::visitStoreFixedSlotV(LStoreFixedSlotV *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();

    const ValueOperand value = ToValue(ins, LStoreFixedSlotV::Value);

    Address address(obj, JSObject::getFixedSlotOffset(slot));
    if (ins->mir()->needsBarrier())
        masm.emitPreBarrier(address, MIRType_Value);

    masm.storeValue(value, address);

    return true;
}

bool
CodeGenerator::visitStoreFixedSlotT(LStoreFixedSlotT *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();

    const LAllocation *value = ins->value();
    MIRType valueType = ins->mir()->value()->type();

    ConstantOrRegister nvalue = value->isConstant()
                              ? ConstantOrRegister(*value->toConstant())
                              : TypedOrValueRegister(valueType, ToAnyRegister(value));

    Address address(obj, JSObject::getFixedSlotOffset(slot));
    if (ins->mir()->needsBarrier())
        masm.emitPreBarrier(address, MIRType_Value);

    masm.storeConstantOrRegister(nvalue, address);

    return true;
}

// An out-of-line path to call an inline cache function.
class OutOfLineCache : public OutOfLineCodeBase<CodeGenerator>
{
    LInstruction *ins;

    CodeOffsetJump inlineJump;
    CodeOffsetLabel inlineLabel;

  public:
    OutOfLineCache(LInstruction *ins)
      : ins(ins)
    {}

    void setInlineJump(CodeOffsetJump jump, CodeOffsetLabel label) {
        inlineJump = jump;
        inlineLabel = label;
    }

    CodeOffsetJump getInlineJump() const {
        return inlineJump;
    }

    CodeOffsetLabel getInlineLabel() const {
        return inlineLabel;
    }

    bool accept(CodeGenerator *codegen) {
        switch (ins->op()) {
          case LInstruction::LOp_GetPropertyCacheT:
          case LInstruction::LOp_GetPropertyCacheV:
            return codegen->visitOutOfLineCacheGetProperty(this);
          case LInstruction::LOp_GetElementCacheV:
            return codegen->visitOutOfLineGetElementCache(this);
          case LInstruction::LOp_SetPropertyCacheT:
          case LInstruction::LOp_SetPropertyCacheV:
            return codegen->visitOutOfLineSetPropertyCache(this);
          case LInstruction::LOp_BindNameCache:
            return codegen->visitOutOfLineBindNameCache(this);
          default:
            JS_NOT_REACHED("Bad instruction");
            return false;
        }
    }

    LInstruction *cache() {
        return ins;
    }
};

bool
CodeGenerator::visitCache(LInstruction *ins)
{
    OutOfLineCache *ool = new OutOfLineCache(ins);
    if (!addOutOfLineCode(ool))
        return false;

    CodeOffsetJump jump = masm.jumpWithPatch(ool->entry());
    CodeOffsetLabel label = masm.labelForPatch();
    masm.bind(ool->rejoin());

    ool->setInlineJump(jump, label);
    return true;
}

bool
CodeGenerator::visitOutOfLineCacheGetProperty(OutOfLineCache *ool)
{
    Register objReg = ToRegister(ool->cache()->getOperand(0));
    RegisterSet liveRegs = ool->cache()->safepoint()->liveRegs();

    LInstruction *ins_ = ool->cache();
    const MGetPropertyCache *mir;

    TypedOrValueRegister output;

    if (ins_->op() == LInstruction::LOp_GetPropertyCacheT) {
        LGetPropertyCacheT *ins = (LGetPropertyCacheT *) ins_;
        output = TypedOrValueRegister(ins->mir()->type(), ToAnyRegister(ins->getDef(0)));
        mir = ins->mir();
    } else {
        LGetPropertyCacheV *ins = (LGetPropertyCacheV *) ins_;
        output = TypedOrValueRegister(GetValueOutput(ins));
        mir = ins->mir();
    }

    IonCacheGetProperty cache(ool->getInlineJump(), ool->getInlineLabel(),
                              masm.labelForPatch(), liveRegs,
                              objReg, mir->atom(), output);

    cache.setScriptedLocation(mir->block()->info().script(), mir->resumePoint()->pc());
    size_t cacheIndex = allocateCache(cache);

    saveLive(ins_);

    typedef bool (*pf)(JSContext *, size_t, JSObject *, Value *);
    static const VMFunction GetPropertyCacheInfo = FunctionInfo<pf>(GetPropertyCache);

    pushArg(objReg);
    pushArg(Imm32(cacheIndex));
    if (!callVM(GetPropertyCacheInfo, ins_))
        return false;

    masm.storeCallResultValue(output);
    restoreLive(ins_);

    masm.jump(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitOutOfLineGetElementCache(OutOfLineCache *ool)
{
    LGetElementCacheV *ins = ool->cache()->toGetElementCacheV();
    const MGetElementCache *mir = ins->mir();

    Register obj = ToRegister(ins->object());
    ConstantOrRegister index = TypedOrValueRegister(ToValue(ins, LGetElementCacheV::Index));
    TypedOrValueRegister output = TypedOrValueRegister(GetValueOutput(ins));

    RegisterSet liveRegs = ins->safepoint()->liveRegs();

    IonCacheGetElement cache(ool->getInlineJump(), ool->getInlineLabel(),
                             masm.labelForPatch(), liveRegs,
                             obj, index, output,
                             mir->monitoredResult());

    cache.setScriptedLocation(mir->block()->info().script(), mir->resumePoint()->pc());
    size_t cacheIndex = allocateCache(cache);

    saveLive(ins);

    typedef bool (*pf)(JSContext *, size_t, JSObject *, const Value &, Value *);
    static const VMFunction Info = FunctionInfo<pf>(GetElementCache);

    pushArg(index);
    pushArg(obj);
    pushArg(Imm32(cacheIndex));
    if (!callVM(Info, ins))
        return false;

    masm.storeCallResultValue(output);
    restoreLive(ins);

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineBindNameCache(OutOfLineCache *ool)
{
    LBindNameCache *ins = ool->cache()->toBindNameCache();
    Register scopeChain = ToRegister(ins->scopeChain());
    Register output = ToRegister(ins->output());

    RegisterSet liveRegs = ins->safepoint()->liveRegs();

    const MBindNameCache *mir = ins->mir();
    IonCacheBindName cache(ool->getInlineJump(), ool->getInlineLabel(),
                           masm.labelForPatch(), liveRegs,
                           scopeChain, mir->name(), output);
    cache.setScriptedLocation(mir->script(), mir->pc());
    size_t cacheIndex = allocateCache(cache);

    saveLive(ins);

    typedef JSObject *(*pf)(JSContext *, size_t, HandleObject);
    static const VMFunction BindNameCacheInfo = FunctionInfo<pf>(BindNameCache);

    pushArg(scopeChain);
    pushArg(Imm32(cacheIndex));
    if (!callVM(BindNameCacheInfo, ins))
        return false;

    masm.storeCallResult(output);
    restoreLive(ins);

    masm.jump(ool->rejoin());
    return true;
}

ConstantOrRegister
CodeGenerator::getSetPropertyValue(LInstruction *ins)
{
    if (ins->getOperand(1)->isConstant()) {
        JS_ASSERT(ins->isSetPropertyCacheT());
        return ConstantOrRegister(*ins->getOperand(1)->toConstant());
    }

    switch (ins->op()) {
      case LInstruction::LOp_CallSetProperty:
        return TypedOrValueRegister(ToValue(ins, LCallSetProperty::Value));
      case LInstruction::LOp_SetPropertyCacheV:
        return TypedOrValueRegister(ToValue(ins, LSetPropertyCacheV::Value));
      case LInstruction::LOp_SetPropertyCacheT: {
        LSetPropertyCacheT *ins_ = ins->toSetPropertyCacheT();
        return TypedOrValueRegister(ins_->valueType(), ToAnyRegister(ins->getOperand(1)));
      }
      default:
        JS_NOT_REACHED("Bad opcode");
        return ConstantOrRegister(UndefinedValue());
    }
}

bool
CodeGenerator::visitCallSetProperty(LCallSetProperty *ins)
{
    ConstantOrRegister value = getSetPropertyValue(ins);

    const Register objReg = ToRegister(ins->getOperand(0));
    bool isSetName = JSOp(*ins->mir()->resumePoint()->pc()) == JSOP_SETNAME;

    pushArg(Imm32(isSetName));
    pushArg(Imm32(ins->mir()->strict()));

    pushArg(value);
    pushArg(ImmGCPtr(ins->mir()->atom()));
    pushArg(objReg);

    typedef bool (*pf)(JSContext *, HandleObject, JSAtom *, const HandleValue, bool, bool);
    static const VMFunction info = FunctionInfo<pf>(SetProperty);

    return callVM(info, ins);
}

bool
CodeGenerator::visitCallDeleteProperty(LCallDeleteProperty *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, PropertyName *, JSBool *);

    pushArg(ImmGCPtr(lir->mir()->atom()));
    pushArg(ToValue(lir, LCallDeleteProperty::Value));

    if (lir->mir()->block()->info().script()->strictModeCode) {
        static const VMFunction Info = FunctionInfo<pf>(DeleteProperty<true>);
        return callVM(Info, lir);
    } else {
        static const VMFunction Info = FunctionInfo<pf>(DeleteProperty<false>);
        return callVM(Info, lir);
    }
}

bool
CodeGenerator::visitOutOfLineSetPropertyCache(OutOfLineCache *ool)
{
    LInstruction *ins = ool->cache();

    Register objReg = ToRegister(ins->getOperand(0));
    RegisterSet liveRegs = ins->safepoint()->liveRegs();

    ConstantOrRegister value = getSetPropertyValue(ins);
    const MSetPropertyCache *mir = ins->mirRaw()->toSetPropertyCache();

    IonCacheSetProperty cache(ool->getInlineJump(), ool->getInlineLabel(),
                              masm.labelForPatch(), liveRegs,
                              objReg, mir->atom(), value,
                              mir->strict());

    size_t cacheIndex = allocateCache(cache);
    bool isSetName = JSOp(*mir->resumePoint()->pc()) == JSOP_SETNAME;

    saveLive(ins);

    pushArg(Imm32(isSetName));
    pushArg(value);
    pushArg(objReg);
    pushArg(Imm32(cacheIndex));

    typedef bool (*pf)(JSContext *, size_t, HandleObject, HandleValue, bool);
    static const VMFunction info = FunctionInfo<pf>(ion::SetPropertyCache);

    if (!callVM(info, ool->cache()))
        return false;

    restoreLive(ins);

    masm.jump(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitThrow(LThrow *lir)
{
    typedef bool (*pf)(JSContext *, const Value &);
    static const VMFunction ThrowInfo = FunctionInfo<pf>(js::Throw);

    pushArg(ToValue(lir, LThrow::Value));
    return callVM(ThrowInfo, lir);
}

bool
CodeGenerator::visitBitNotV(LBitNotV *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, int *p);
    static const VMFunction info = FunctionInfo<pf>(BitNot);

    pushArg(ToValue(lir, LBitNotV::Input));
    return callVM(info, lir);
}

bool
CodeGenerator::visitBitOpV(LBitOpV *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, const Value &, int *p);
    static const VMFunction BitAndInfo = FunctionInfo<pf>(BitAnd);
    static const VMFunction BitOrInfo = FunctionInfo<pf>(BitOr);
    static const VMFunction BitXorInfo = FunctionInfo<pf>(BitXor);
    static const VMFunction BitLhsInfo = FunctionInfo<pf>(BitLsh);
    static const VMFunction BitRhsInfo = FunctionInfo<pf>(BitRsh);

    pushArg(ToValue(lir, LBitOpV::RhsInput));
    pushArg(ToValue(lir, LBitOpV::LhsInput));

    switch (lir->jsop()) {
      case JSOP_BITAND:
        return callVM(BitAndInfo, lir);
      case JSOP_BITOR:
        return callVM(BitOrInfo, lir);
      case JSOP_BITXOR:
        return callVM(BitXorInfo, lir);
      case JSOP_LSH:
        return callVM(BitLhsInfo, lir);
      case JSOP_RSH:
        return callVM(BitRhsInfo, lir);
      default:
        break;
    }
    JS_NOT_REACHED("unexpected bitop");
    return false;
}

class OutOfLineTypeOfV : public OutOfLineCodeBase<CodeGenerator>
{
    LTypeOfV *ins_;

  public:
    OutOfLineTypeOfV(LTypeOfV *ins)
      : ins_(ins)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineTypeOfV(this);
    }
    LTypeOfV *ins() const {
        return ins_;
    }
};

bool
CodeGenerator::visitTypeOfV(LTypeOfV *lir)
{
    const ValueOperand value = ToValue(lir, LTypeOfV::Input);
    Register output = ToRegister(lir->output());
    Register tag = masm.splitTagForTest(value);

    OutOfLineTypeOfV *ool = new OutOfLineTypeOfV(lir);
    if (!addOutOfLineCode(ool))
        return false;

    PropertyName **typeAtoms = gen->cx->runtime->atomState.typeAtoms;

    // Jump to the OOL path if the value is an object. Objects are complicated
    // since they may have a typeof hook.
    masm.branchTestObject(Assembler::Equal, tag, ool->entry());

    Label done;

    Label notNumber;
    masm.branchTestNumber(Assembler::NotEqual, tag, &notNumber);
    masm.movePtr(ImmGCPtr(typeAtoms[JSTYPE_NUMBER]), output);
    masm.jump(&done);
    masm.bind(&notNumber);

    Label notUndefined;
    masm.branchTestUndefined(Assembler::NotEqual, tag, &notUndefined);
    masm.movePtr(ImmGCPtr(typeAtoms[JSTYPE_VOID]), output);
    masm.jump(&done);
    masm.bind(&notUndefined);

    Label notNull;
    masm.branchTestNull(Assembler::NotEqual, tag, &notNull);
    masm.movePtr(ImmGCPtr(typeAtoms[JSTYPE_OBJECT]), output);
    masm.jump(&done);
    masm.bind(&notNull);

    Label notBoolean;
    masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
    masm.movePtr(ImmGCPtr(typeAtoms[JSTYPE_BOOLEAN]), output);
    masm.jump(&done);
    masm.bind(&notBoolean);

    masm.movePtr(ImmGCPtr(typeAtoms[JSTYPE_STRING]), output);

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool)
{
    typedef JSString *(*pf)(JSContext *, const Value &);
    static const VMFunction Info = FunctionInfo<pf>(TypeOfOperation);

    LTypeOfV *ins = ool->ins();
    saveLive(ins);

    pushArg(ToValue(ins, LTypeOfV::Input));
    if (!callVM(Info, ins))
        return false;

    masm.storeCallResult(ToRegister(ins->output()));
    restoreLive(ins);

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitToIdV(LToIdV *lir)
{
    typedef bool (*pf)(JSContext *, const Value &, const Value &, Value *);
    static const VMFunction Info = FunctionInfo<pf>(ToIdOperation);

    pushArg(ToValue(lir, LToIdV::Index));
    pushArg(ToValue(lir, LToIdV::Object));
    return callVM(Info, lir);
}

bool
CodeGenerator::visitLoadElementV(LLoadElementV *load)
{
    Register elements = ToRegister(load->elements());
    const ValueOperand out = ToOutValue(load);

    if (load->index()->isConstant())
        masm.loadValue(Address(elements, ToInt32(load->index()) * sizeof(Value)), out);
    else
        masm.loadValue(BaseIndex(elements, ToRegister(load->index()), TimesEight), out);

    if (load->mir()->needsHoleCheck()) {
        Assembler::Condition cond = masm.testMagic(Assembler::Equal, out);
        if (!bailoutIf(cond, load->snapshot()))
            return false;
    }

    return true;
}

bool
CodeGenerator::visitLoadElementHole(LLoadElementHole *lir)
{
    Register elements = ToRegister(lir->elements());
    Register initLength = ToRegister(lir->initLength());
    const ValueOperand out = ToOutValue(lir);

    // If the index is out of bounds, load |undefined|. Otherwise, load the
    // value.
    Label undefined, done;
    if (lir->index()->isConstant()) {
        masm.branch32(Assembler::BelowOrEqual, initLength, Imm32(ToInt32(lir->index())), &undefined);
        masm.loadValue(Address(elements, ToInt32(lir->index()) * sizeof(Value)), out);
    } else {
        masm.branch32(Assembler::BelowOrEqual, initLength, ToRegister(lir->index()), &undefined);
        masm.loadValue(BaseIndex(elements, ToRegister(lir->index()), TimesEight), out);
    }

    // If a hole check is needed, and the value wasn't a hole, we're done.
    // Otherwise, we'll load undefined.
    if (lir->mir()->needsHoleCheck())
        masm.branchTestMagic(Assembler::NotEqual, out, &done);
    else
        masm.jump(&done);

    masm.bind(&undefined);
    masm.moveValue(UndefinedValue(), out);
    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitLoadTypedArrayElement(LLoadTypedArrayElement *lir)
{
    Register elements = ToRegister(lir->elements());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());
    AnyRegister out = ToAnyRegister(lir->output());

    int arrayType = lir->mir()->arrayType();
    int shift = TypedArray::slotWidth(arrayType);

    Label fail;
    if (lir->index()->isConstant()) {
        Address source(elements, ToInt32(lir->index()) * shift);
        masm.loadFromTypedArray(arrayType, source, out, temp, &fail);
    } else {
        BaseIndex source(elements, ToRegister(lir->index()), ScaleFromShift(shift));
        masm.loadFromTypedArray(arrayType, source, out, temp, &fail);
    }

    if (fail.used() && !bailoutFrom(&fail, lir->snapshot()))
        return false;

    return true;
}

class OutOfLineLoadTypedArray : public OutOfLineCodeBase<CodeGenerator>
{
    LLoadTypedArrayElementHole *ins_;

  public:
    OutOfLineLoadTypedArray(LLoadTypedArrayElementHole *ins)
      : ins_(ins)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineLoadTypedArray(this);
    }

    LLoadTypedArrayElementHole *ins() const {
        return ins_;
    }
};

bool
CodeGenerator::visitLoadTypedArrayElementHole(LLoadTypedArrayElementHole *lir)
{
    Register object = ToRegister(lir->object());
    const ValueOperand out = ToOutValue(lir);

    OutOfLineLoadTypedArray *ool = new OutOfLineLoadTypedArray(lir);
    if (!addOutOfLineCode(ool))
        return false;

    // Load the length.
    Register scratch = out.scratchReg();
    Int32Key key = ToInt32Key(lir->index());
    masm.unboxInt32(Address(object, TypedArray::lengthOffset()), scratch);

    // OOL path if index >= length.
    masm.branchKey(Assembler::BelowOrEqual, scratch, key, ool->entry());

    // Load the elements vector.
    masm.loadPtr(Address(object, TypedArray::dataOffset()), scratch);

    int arrayType = lir->mir()->arrayType();
    int shift = TypedArray::slotWidth(arrayType);

    Label fail;
    if (key.isConstant()) {
        Address source(scratch, key.constant() * shift);
        masm.loadFromTypedArray(arrayType, source, out, lir->mir()->allowDouble(), &fail);
    } else {
        BaseIndex source(scratch, key.reg(), ScaleFromShift(shift));
        masm.loadFromTypedArray(arrayType, source, out, lir->mir()->allowDouble(), &fail);
    }

    if (fail.used() && !bailoutFrom(&fail, lir->snapshot()))
        return false;

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineLoadTypedArray(OutOfLineLoadTypedArray *ool)
{
    LLoadTypedArrayElementHole *ins = ool->ins();
    saveLive(ins);

    Register object = ToRegister(ins->object());
    ValueOperand out = ToOutValue(ins);

    typedef bool (*pf)(JSContext *, const Value &, const Value &, Value *);
    static const VMFunction Info = FunctionInfo<pf>(js::GetElementMonitored);

    if (ins->index()->isConstant())
        pushArg(*ins->index()->toConstant());
    else
        pushArg(TypedOrValueRegister(MIRType_Int32, ToAnyRegister(ins->index())));
    pushArg(TypedOrValueRegister(MIRType_Object, AnyRegister(object)));
    if (!callVM(Info, ins))
        return false;

    masm.storeCallResultValue(out);
    restoreLive(ins);

    masm.jump(ool->rejoin());
    return true;
}

template <typename T>
static inline void
StoreToTypedArray(MacroAssembler &masm, int arrayType, const LAllocation *value, const T &dest)
{
    if (arrayType == TypedArray::TYPE_FLOAT32 || arrayType == TypedArray::TYPE_FLOAT64) {
        masm.storeToTypedFloatArray(arrayType, ToFloatRegister(value), dest);
    } else {
        if (value->isConstant())
            masm.storeToTypedIntArray(arrayType, Imm32(ToInt32(value)), dest);
        else
            masm.storeToTypedIntArray(arrayType, ToRegister(value), dest);
    }
}

bool
CodeGenerator::visitStoreTypedArrayElement(LStoreTypedArrayElement *lir)
{
    Register elements = ToRegister(lir->elements());
    const LAllocation *value = lir->value();

    int arrayType = lir->mir()->arrayType();
    int shift = TypedArray::slotWidth(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * shift);
        StoreToTypedArray(masm, arrayType, value, dest);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromShift(shift));
        StoreToTypedArray(masm, arrayType, value, dest);
    }

    return true;
}

bool
CodeGenerator::visitClampIToUint8(LClampIToUint8 *lir)
{
    Register input = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.clampIntToUint8(input, output);
    return true;
}

bool
CodeGenerator::visitClampDToUint8(LClampDToUint8 *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.clampDoubleToUint8(input, output);
    return true;
}

bool
CodeGenerator::visitClampVToUint8(LClampVToUint8 *lir)
{
    ValueOperand input = ToValue(lir, LClampVToUint8::Input);
    FloatRegister tempFloat = ToFloatRegister(lir->tempFloat());
    Register output = ToRegister(lir->output());

    Register tag = masm.splitTagForTest(input);

    Label done;
    Label isInt32, isDouble, isBoolean;
    masm.branchTestInt32(Assembler::Equal, tag, &isInt32);
    masm.branchTestDouble(Assembler::Equal, tag, &isDouble);
    masm.branchTestBoolean(Assembler::Equal, tag, &isBoolean);

    // Undefined, null and objects are always 0.
    Label isZero;
    masm.branchTestUndefined(Assembler::Equal, tag, &isZero);
    masm.branchTestNull(Assembler::Equal, tag, &isZero);
    masm.branchTestObject(Assembler::Equal, tag, &isZero);

    // Bailout for everything else (strings).
    if (!bailout(lir->snapshot()))
        return false;

    masm.bind(&isInt32);
    masm.unboxInt32(input, output);
    masm.clampIntToUint8(output, output);
    masm.jump(&done);

    masm.bind(&isDouble);
    masm.unboxDouble(input, tempFloat);
    masm.clampDoubleToUint8(tempFloat, output);
    masm.jump(&done);

    masm.bind(&isBoolean);
    masm.unboxBoolean(input, output);
    masm.jump(&done);

    masm.bind(&isZero);
    masm.move32(Imm32(0), output);

    masm.bind(&done);
    return true;
}

} // namespace ion
} // namespace js

