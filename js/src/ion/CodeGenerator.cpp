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
        emitTruncateDouble(temp, output, &fails);
        break;
      default:
        JS_ASSERT(lir->mode() == LValueToInt32::NORMAL);
        emitDoubleToInt32(temp, output, &fails);
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
    emitDoubleToInt32(input, output, &fail);
    if (!bailoutFrom(&fail, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitTestVAndBranch(LTestVAndBranch *lir)
{
    const ValueOperand value = ToValue(lir, LTestVAndBranch::Input);

    Register tag = masm.splitTagForTest(value);

    Assembler::Condition cond;

    // Eventually we will want some sort of type filter here. For now, just
    // emit all easy cases. For speed we use the cached tag for all comparison,
    // except for doubles, which we test last (as the operation can clobber the
    // tag, which may be in ScratchReg).
    masm.branchTestUndefined(Assembler::Equal, tag, lir->ifFalse());

    masm.branchTestNull(Assembler::Equal, tag, lir->ifFalse());
    masm.branchTestObject(Assembler::Equal, tag, lir->ifTrue());

    Label notBoolean;
    masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
    masm.branchTestBooleanTruthy(false, value, lir->ifFalse());
    masm.jump(lir->ifTrue());
    masm.bind(&notBoolean);

    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, tag, &notInt32);
    cond = masm.testInt32Truthy(false, value);
    masm.j(cond, lir->ifFalse());
    masm.jump(lir->ifTrue());
    masm.bind(&notInt32);

    // Test if a string is non-empty.
    Label notString;
    masm.branchTestString(Assembler::NotEqual, tag, &notString);
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
CodeGenerator::visitIntToString(LIntToString *lir)
{
    typedef JSString *(*pf)(JSContext *, jsint);
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
    static const VMFunction js_CloneRegExpObjectInfo = FunctionInfo<pf>(js_CloneRegExpObject);

    pushArg(ImmGCPtr(proto));
    pushArg(ImmGCPtr(lir->mir()->source()));
    return callVM(js_CloneRegExpObjectInfo, lir);
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

    uint32 osiReturnPointOffset;
    if (!markOsiPoint(lir, &osiReturnPointOffset))
        return false;

    LSafepoint *safepoint = lir->associatedSafepoint();
    JS_ASSERT(!safepoint->osiReturnPointOffset());
    safepoint->setOsiReturnPointOffset(osiReturnPointOffset);
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
    //  bool (*)(JSContext *, uintN, Value *vp)
    // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Allocate space for the outparam, moving the StackPointer to what will be &vp[1].
    masm.adjustStack(unusedStack);

    // Push a Value containing the callee object: natives are allowed to access their callee before
    // setting the return value. The StackPointer is moved to &vp[0].
    masm.Push(ObjectValue(*target));

    // Preload arguments into registers.
    masm.loadJSContext(gen->cx->runtime, argJSContextReg);
    masm.move32(Imm32(call->nargs()), argUintNReg);
    masm.movePtr(StackPointer, argVpReg);

    // Construct exit frame.
    uint32 safepointOffset = masm.buildFakeExitFrame(tempReg);
    masm.linkExitFrame();
    if (!markSafepointAt(safepointOffset, call))
        return false;

    // Construct and execute call.
    masm.setupUnalignedABICall(3, tempReg);
    masm.setABIArg(0, argJSContextReg);
    masm.setABIArg(1, argUintNReg);
    masm.setABIArg(2, argVpReg);
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

    // As a temporary hack for JSOP_NEW support, always call out to InvokeConstructor
    // in the case of a constructing call.
    // TODO: Bug 701692: performant support for JSOP_NEW.
    if (call->mir()->isConstruct()) {
        typedef bool (*pf)(JSContext *, JSFunction *, uint32, Value *, Value *);
        static const VMFunction InvokeConstructorFunctionInfo =
            FunctionInfo<pf>(InvokeConstructorFunction);

        // Nestle %esp up to the argument vector.
        // Each path must account for framePushed_ separately, for callVM to be valid.
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

    Label end, invoke;

    // Guard that calleereg is a non-native function:
    // Non-native iff (callee->flags & JSFUN_KINDMASK >= JSFUN_INTERPRETED).
    // This is equivalent to testing if any of the bits in JSFUN_KINDMASK are set.
    if (!call->hasSingleTarget()) {
        masm.branchTest32(Assembler::Zero, Address(calleereg, offsetof(JSFunction, flags)),
                          Imm32(JSFUN_INTERPRETED), &invoke);
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
        masm.load16(Address(calleereg, offsetof(JSFunction, nargs)), nargsreg);
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

        pushArg(StackPointer);          // argv.
        pushArg(Imm32(call->nargs()));  // argc.
        pushArg(calleereg);             // JSFunction *.

        if (!callVM(InvokeFunctionInfo, call))
            return false;

        // Un-nestle %esp from the argument vector. No prefix was pushed.
        masm.reserveStack(unusedStack);
    }

    masm.bind(&end);

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

bool
CodeGenerator::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint
    // patching to occur. Otherwise, we could overwrite the invalidation
    // epilogue.
    for (size_t i = 0; i < sizeof(void *); i++)
        masm.nop();

    masm.bind(&invalidate_);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    IonCode *thunk = gen->cx->compartment->ionCompartment()->getOrCreateInvalidationThunk(gen->cx);
    masm.call(thunk);
#ifdef DEBUG
    // We should never reach this point in JIT code -- the invalidation thunk should
    // pop the invalidated JS frame and return directly to its caller.
    masm.breakpoint();
#endif
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
    masm.loadPtr(ImmWord(limitAddr), limitReg);

    CheckOverRecursedFailure *ool = new CheckOverRecursedFailure(lir);
    if (!addOutOfLineCode(ool))
        return false;

    // Conditional forward (unlikely) branch to failure.
    masm.branchPtr(Assembler::BelowOrEqual, StackPointer, limitReg, ool->entry());

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

bool
CodeGenerator::visitNewArray(LNewArray *ins)
{
    typedef JSObject *(*pf)(JSContext *, uint32, types::TypeObject *);
    static const VMFunction NewInitArrayInfo = FunctionInfo<pf>(NewInitArray);

    // ReturnReg is used for the returned value, so we don't care using it
    // because it would be erased by the function call.
    const Register type = ReturnReg;
    masm.movePtr(ImmWord(ins->mir()->type()), type);

    pushArg(type);
    pushArg(Imm32(ins->mir()->count()));
    if (!callVM(NewInitArrayInfo, ins))
        return false;
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
    typedef bool (*pf)(JSContext *, const Value &, const Value &, Value *);
    static const VMFunction AddInfo = FunctionInfo<pf>(js::AddValues);
    static const VMFunction SubInfo = FunctionInfo<pf>(js::SubValues);
    static const VMFunction MulInfo = FunctionInfo<pf>(js::MulValues);
    static const VMFunction DivInfo = FunctionInfo<pf>(js::DivValues);
    static const VMFunction ModInfo = FunctionInfo<pf>(js::ModValues);

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
CodeGenerator::visitConcat(LConcat *lir)
{
    typedef JSString *(*pf)(JSContext *, JSString *, JSString *);
    static const VMFunction js_ConcatStringsInfo = FunctionInfo<pf>(js_ConcatStrings);

    pushArg(ToRegister(lir->rhs()));
    pushArg(ToRegister(lir->lhs()));
    if (!callVM(js_ConcatStringsInfo, lir))
        return false;
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
    storeElementTyped(store->value(), store->mir()->value()->type(), store->mir()->elementType(),
                      ToRegister(store->elements()), store->index());
    return true;
}

bool
CodeGenerator::visitStoreElementV(LStoreElementV *lir)
{
    const ValueOperand value = ToValue(lir, LStoreElementV::Value);
    Register elements = ToRegister(lir->elements());

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

    typedef bool (*pf)(JSContext *, JSObject *, const Value &, const Value &);
    static const VMFunction Info = FunctionInfo<pf>(SetObjectElement);

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
CodeGenerator::visitCallIteratorStart(LCallIteratorStart *lir)
{
    typedef JSObject *(*pf)(JSContext *, JSObject *, uint32_t);
    static const VMFunction Info = FunctionInfo<pf>(GetIteratorObject);

    const Register objReg = ToRegister(lir->getOperand(0));

    pushArg(Imm32(lir->mir()->flags()));
    pushArg(objReg);
    return callVM(Info, lir);
}

bool
CodeGenerator::visitCallIteratorNext(LCallIteratorNext *lir)
{
    typedef bool (*pf)(JSContext *, JSObject *, Value *);
    static const VMFunction Info = FunctionInfo<pf>(js_IteratorNext);

    const Register objReg = ToRegister(lir->getOperand(0));

    pushArg(objReg);
    return callVM(Info, lir);
}

bool
CodeGenerator::visitCallIteratorMore(LCallIteratorMore *lir)
{
    typedef bool (*pf)(JSContext *, JSObject *, Value *);
    static const VMFunction Info = FunctionInfo<pf>(js_IteratorMore);

    const Register objReg = ToRegister(lir->getOperand(0));

    pushArg(objReg);
    if (!callVM(Info, lir))
        return false;

    // Unbox the boolean value produced by IteratorMore to the output register.
    Register output = ToRegister(lir->getDef(0));
    masm.unboxValue(JSReturnOperand, AnyRegister(output));

    return true;
}

bool
CodeGenerator::visitCallIteratorEnd(LCallIteratorEnd *lir)
{
    typedef bool (*pf)(JSContext *, JSObject *);
    static const VMFunction Info = FunctionInfo<pf>(js_CloseIterator);

    const Register objReg = ToRegister(lir->getOperand(0));

    pushArg(objReg);
    return callVM(Info, lir);
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

    // We encode safepoints after the OSI-point offsets have been determined.
    encodeSafepoints();

    if (masm.oom())
        return false;

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

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

    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, invalidateEpilogueData_),
                                       ImmWord(uintptr_t(script->ion)),
                                       ImmWord(uintptr_t(-1)));

    IonSpew(IonSpew_Codegen, "Created IonScript %p (raw %p)",
            (void *) script->ion, (void *) code->raw());

    script->ion->setInvalidationEpilogueDataOffset(invalidateEpilogueData_.offset());
    script->ion->setOsrPc(gen->info().osrPc());
    script->ion->setOsrEntryOffset(getOsrEntryOffset());
    script->ion->setInvalidationEpilogueOffset(invalidate_.offset());

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
        script->ion->copyOsiIndices(&osiIndices_[0]);
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

typedef bool (*GetPropertyOrNameFn)(JSContext *, JSObject *, PropertyName *, Value *);

bool
CodeGenerator::visitCallGetProperty(LCallGetProperty *lir)
{
    static const VMFunction Info = FunctionInfo<GetPropertyOrNameFn>(GetObjectProperty);

    pushArg(ImmGCPtr(lir->mir()->atom()));
    pushArg(ToRegister(lir->getOperand(0)));
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

    pushArg(ToValue(lir, LCallGetElement::RhsInput));
    pushArg(ToValue(lir, LCallGetElement::LhsInput));
    return callVM(GetElementInfo, lir);
}

bool
CodeGenerator::visitCallSetElement(LCallSetElement *lir)
{
    typedef bool (*pf)(JSContext *, JSObject *, const Value &, const Value &);
    static const VMFunction SetObjectElementInfo = FunctionInfo<pf>(js::SetObjectElement);

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
    masm.storeValue(value, Address(obj, JSObject::getFixedSlotOffset(slot)));

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

    masm.storeConstantOrRegister(nvalue, Address(obj, JSObject::getFixedSlotOffset(slot)));

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
          case LInstruction::LOp_SetPropertyCacheT:
          case LInstruction::LOp_SetPropertyCacheV:
            return codegen->visitOutOfLineSetPropertyCache(this);
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

    liveRegs.maybeTake(output);

    IonCacheGetProperty cache(ool->getInlineJump(), ool->getInlineLabel(),
                              masm.labelForPatch(), liveRegs,
                              objReg, mir->atom(), output);

    cache.setScriptedLocation(mir->script(), mir->pc());
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

    pushArg(value);
    pushArg(ImmGCPtr(ins->mir()->atom()));
    pushArg(objReg);

    typedef bool (*pf)(JSContext *, JSObject *, JSAtom *, Value);
    if (ins->mir()->strict()) {
        static const VMFunction info = FunctionInfo<pf>(SetProperty<true>);
        if (!callVM(info, ins))
            return false;
    } else {
        static const VMFunction info = FunctionInfo<pf>(SetProperty<false>);
        if (!callVM(info, ins))
            return false;
    }

    return true;
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

    saveLive(ins);

    pushArg(value);
    pushArg(objReg);
    pushArg(Imm32(cacheIndex));

    typedef bool (*pf)(JSContext *, size_t, JSObject *, Value);
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

    PropertyName **typeAtoms = GetIonContext()->cx->runtime->atomState.typeAtoms;

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

} // namespace ion
} // namespace js

