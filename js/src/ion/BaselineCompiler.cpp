/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineJIT.h"
#include "BaselineIC.h"
#include "BaselineHelpers.h"
#include "BaselineCompiler.h"
#include "FixedList.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "VMFunctions.h"
#include "IonFrames-inl.h"

using namespace js;
using namespace js::ion;

BaselineCompiler::BaselineCompiler(JSContext *cx, HandleScript script)
  : BaselineCompilerSpecific(cx, script),
    return_(new HeapLabel()),
    autoRooter_(cx, this)
{
}

bool
BaselineCompiler::init()
{
    if (!labels_.init(script->length))
        return false;

    for (size_t i = 0; i < script->length; i++)
        new (&labels_[i]) Label();

    if (!frame.init())
        return false;

    return true;
}

MethodStatus
BaselineCompiler::compile()
{
    IonSpew(IonSpew_BaselineScripts, "Baseline compiling script %s:%d (%p)",
            script->filename, script->lineno, script.get());

    if (script->needsArgsObj()) {
        IonSpew(IonSpew_BaselineAbort, "Script needs arguments object");
        return Method_CantCompile;
    }

    if (function() && function()->isHeavyweight()) {
        IonSpew(IonSpew_BaselineAbort, "FIXME compile heavy weight functions");
        return Method_CantCompile;
    }

    // Pin analysis info during compilation.
    analyze::AutoEnterAnalysis autoEnterAnalysis(cx);

    if (!emitPrologue())
        return Method_Error;

    MethodStatus status = emitBody();
    if (status != Method_Compiled)
        return status;

    if (!emitEpilogue())
        return Method_Error;

    if (masm.oom())
        return Method_Error;

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return Method_Error;

    JS_ASSERT(!script->hasBaselineScript());

    BaselineScript *baselineScript = BaselineScript::New(cx, icEntries_.length());
    if (!baselineScript)
        return Method_Error;
    script->baseline = baselineScript;

    IonSpew(IonSpew_BaselineScripts, "Created BaselineScript %p (raw %p) for %s:%d",
            (void *) script->baseline, (void *) code->raw(),
            script->filename, script->lineno);

    script->baseline->setMethod(code);

    // Copy IC entries
    if (icEntries_.length())
        baselineScript->copyICEntries(&icEntries_[0], masm);

    // Adopt fallback stubs from the compiler into the baseline script.
    baselineScript->adoptFallbackStubs(&stubSpace_);

    // Patch IC loads using IC entries
    for (size_t i = 0; i < icLoadLabels_.length(); i++) {
        CodeOffsetLabel label = icLoadLabels_[i].label;
        label.fixup(&masm);
        size_t icEntry = icLoadLabels_[i].icEntry;
        ICEntry *entryAddr = &(baselineScript->icEntry(icEntry));
        Assembler::patchDataWithValueCheck(CodeLocationLabel(code, label),
                                           ImmWord(uintptr_t(entryAddr)),
                                           ImmWord(uintptr_t(-1)));
    }

    return Method_Compiled;
}

void
BaselineCompiler::trace(JSTracer *trc)
{
    // The stubcodes that have been allocated for the ICs that have been compiled
    // so far need to marked.  There should only be fallback stubs in any IC chains.
    for (size_t i = 0; i < icEntries_.length(); i++) {
        ICEntry &entry = icEntries_[i];
        if (!entry.hasStub())
            continue;
        JS_ASSERT(entry.firstStub() != NULL);
        JS_ASSERT(entry.firstStub()->isFallback());
        JS_ASSERT(entry.firstStub()->next() == NULL);
        entry.firstStub()->trace(trc);
    }
}

#ifdef DEBUG
#define SPEW_OPCODE()                                                         \
    JS_BEGIN_MACRO                                                            \
        if (IsJaegerSpewChannelActive(JSpew_JSOps)) {                         \
            Sprinter sprinter(cx);                                            \
            sprinter.init();                                                  \
            RootedScript script_(cx, script);                                 \
            js_Disassemble1(cx, script_, pc, pc - script_->code,              \
                            JS_TRUE, &sprinter);                              \
            JaegerSpew(JSpew_JSOps, "    %2u %s",                             \
                       (unsigned)frame.stackDepth(), sprinter.string());      \
        }                                                                     \
    JS_END_MACRO;
#else
#define SPEW_OPCODE()
#endif /* DEBUG */

bool
BaselineCompiler::emitPrologue()
{
    masm.push(BaselineFrameReg);
    masm.mov(BaselineStackReg, BaselineFrameReg);

    masm.subPtr(Imm32(BaselineFrame::Size()), BaselineStackReg);

    masm.checkStackAlignment();

    // Initialize locals to |undefined|. Use R0 to minimize code size.
    masm.moveValue(UndefinedValue(), R0);
    for (size_t i = 0; i < frame.nlocals(); i++)
        masm.pushValue(R0);

    return true;
}

bool
BaselineCompiler::emitEpilogue()
{
    masm.bind(return_);

    masm.mov(BaselineFrameReg, BaselineStackReg);
    masm.pop(BaselineFrameReg);

    masm.ret();
    return true;
}

bool
BaselineCompiler::emitIC(ICStub *stub)
{
    ICEntry *entry = allocateICEntry(stub);
    if (!entry)
        return false;

    CodeOffsetLabel patchOffset;
    EmitCallIC(&patchOffset, masm);
    entry->setReturnOffset(masm.currentOffset());
    if (!addICLoadLabel(patchOffset))
        return false;

    return true;
}

bool
BaselineCompiler::initScopeChain()
{
    RootedFunction fun(cx, function());
    if (fun) {
        // Use callee->environment as scope chain. Note that we do
        // this also for heavy-weight functions, so that the scope
        // chain slot is properly initialized if the call triggers GC.
        Register callee = R0.scratchReg();
        Register scope = R1.scratchReg();
        masm.loadPtr(frame.addressOfCallee(), callee);
        masm.loadPtr(Address(callee, JSFunction::offsetOfEnvironment()), scope);
        masm.storePtr(scope, frame.addressOfScopeChain());

        // Once we compile heavy-weight functions, we should create a new
        // call object here.
        JS_ASSERT(!fun->isHeavyweight());
    } else {
        // Once we compile eval scripts, we should create a call object.
        JS_ASSERT(!script->isForEval());

        // For global scripts, the scope chain is the global object.
        masm.storePtr(ImmGCPtr(&script->global()), frame.addressOfScopeChain());
    }

    return true;
}

bool
BaselineCompiler::emitStackCheck()
{
    Label skipIC;
    uintptr_t *limitAddr = &cx->runtime->ionStackLimit;
    masm.loadPtr(AbsoluteAddress(limitAddr), R0.scratchReg());
    masm.branchPtr(Assembler::AboveOrEqual, BaselineStackReg, R0.scratchReg(), &skipIC);

    ICStackCheck_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    masm.bind(&skipIC);
    return true;
}

typedef bool (*InterruptCheckFn)(JSContext *);
static const VMFunction InterruptCheckInfo = FunctionInfo<InterruptCheckFn>(InterruptCheck);

bool
BaselineCompiler::emitInterruptCheck()
{
    Label done;
    void *interrupt = (void *)&cx->compartment->rt->interrupt;
    masm.branch32(Assembler::Equal, AbsoluteAddress(interrupt), Imm32(0), &done);

    prepareVMCall();
    if (!callVM(InterruptCheckInfo))
        return false;

    masm.bind(&done);
    return true;
}

bool
BaselineCompiler::emitUseCountIncrement()
{
    // Emit no use count increments or bailouts if Ion is not
    // enabled, or if the script will never be Ion-compileable

    if (!ionCompileable_)
        return true;

    Register scriptReg = R2.scratchReg();
    Register countReg = R0.scratchReg();
    Address useCountAddr(scriptReg, JSScript::offsetOfUseCount());
    Label lowCount;

    masm.movePtr(ImmGCPtr(script), scriptReg);
    masm.load32(useCountAddr, countReg);
    masm.add32(Imm32(1), countReg);
    masm.store32(countReg, useCountAddr);
    masm.branch32(Assembler::LessThan, countReg, Imm32(js_IonOptions.usesBeforeCompile), &lowCount);

    // Call IC.
    ICUseCount_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    masm.bind(&lowCount);

    return true;
}

MethodStatus
BaselineCompiler::emitBody()
{
    pc = script->code;

    // Initialize the scope chain before any operation that may
    // call into the VM and trigger a GC.
    if (!initScopeChain())
        return Method_Error;

    if (!emitStackCheck())
        return Method_Error;

    if (!emitUseCountIncrement())
        return Method_Error;

    while (true) {
        SPEW_OPCODE();
        JSOp op = JSOp(*pc);
        IonSpew(IonSpew_BaselineOp, "Compiling op: %s", js_CodeName[op]);

        // Fully sync the stack if there are incoming jumps.
        analyze::Bytecode *code = script->analysis()->maybeCode(pc);
        if (code && code->jumpTarget) {
            frame.syncStack(0);
            frame.setStackDepth(code->stackDepth);
        }

        frame.assertValidState(pc);

        masm.bind(labelOf(pc));

        switch (op) {
          default:
            // Ignore fat opcodes, we compile the decomposed version instead.
            if (js_CodeSpec[op].format & JOF_DECOMPOSE)
                break;
            IonSpew(IonSpew_BaselineAbort, "Unhandled op: %s", js_CodeName[op]);
            return Method_CantCompile;

#define EMIT_OP(OP)                            \
          case OP:                             \
            if (!this->emit_##OP())            \
                return Method_Error;           \
            break;
OPCODE_LIST(EMIT_OP)
#undef EMIT_OP
        }

        if (op == JSOP_STOP)
            break;

        pc += GetBytecodeLength(pc);
    }

    JS_ASSERT(JSOp(*pc) == JSOP_STOP);
    JS_ASSERT(frame.stackDepth() == 0);
    return Method_Compiled;
}

bool
BaselineCompiler::emit_JSOP_NOP()
{
    return true;
}

bool
BaselineCompiler::emit_JSOP_LABEL()
{
    return true;
}

bool
BaselineCompiler::emit_JSOP_NOTEARG()
{
    return true;
}

bool
BaselineCompiler::emit_JSOP_POP()
{
    frame.pop();
    return true;
}

bool
BaselineCompiler::emit_JSOP_DUP()
{
    // Keep top stack value in R0, sync the rest so that we can use R1. We use
    // separate registers because every register can be used by at most one
    // StackValue.
    frame.popRegsAndSync(1);
    masm.moveValue(R0, R1);

    // inc/dec ops use DUP followed by ONE, ADD. Push R0 last to avoid a move.
    frame.push(R1);
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_DUP2()
{
    frame.syncStack(0);

    masm.loadValue(frame.addressOfStackValue(frame.peek(-2)), R0);
    masm.loadValue(frame.addressOfStackValue(frame.peek(-1)), R1);

    frame.push(R0);
    frame.push(R1);
    return true;
}

bool
BaselineCompiler::emit_JSOP_SWAP()
{
    // Keep top stack values in R0 and R1.
    frame.popRegsAndSync(2);

    frame.push(R1);
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_PICK()
{
    frame.syncStack(0);

    // Pick takes a value on the stack and moves it to the top.
    // For instance, pick 2:
    //     before: A B C D E
    //     after : A B D E C

    // First, move value at -(amount + 1) into R0.
    int depth = -(GET_INT8(pc) + 1);
    masm.loadValue(frame.addressOfStackValue(frame.peek(depth)), R0);

    // Move the other values down.
    depth++;
    for (; depth < 0; depth++) {
        Address source = frame.addressOfStackValue(frame.peek(depth));
        Address dest = frame.addressOfStackValue(frame.peek(depth - 1));
        masm.loadValue(source, R1);
        masm.storeValue(R1, dest);
    }

    // Push R0.
    frame.pop();
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_GOTO()
{
    frame.syncStack(0);

    jsbytecode *target = pc + GET_JUMP_OFFSET(pc);
    masm.jump(labelOf(target));
    return true;
}

bool
BaselineCompiler::emitToBoolean()
{
    Label skipIC;
    masm.branchTestBoolean(Assembler::Equal, R0, &skipIC);

    // Call IC
    ICToBool_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    masm.bind(&skipIC);
    return true;
}

bool
BaselineCompiler::emitTest(bool branchIfTrue)
{
    // Keep top stack value in R0.
    frame.popRegsAndSync(1);

    if (!emitToBoolean())
        return false;

    // IC will leave a JSBool value (guaranteed) in R0, just need to branch on it.
    masm.branchTestBooleanTruthy(branchIfTrue, R0, labelOf(pc + GET_JUMP_OFFSET(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_IFEQ()
{
    return emitTest(false);
}

bool
BaselineCompiler::emit_JSOP_IFNE()
{
    return emitTest(true);
}

bool
BaselineCompiler::emitAndOr(bool branchIfTrue)
{
    // AND and OR leave the original value on the stack.
    frame.syncStack(0);

    masm.loadValue(frame.addressOfStackValue(frame.peek(-1)), R0);
    if (!emitToBoolean())
        return false;

    masm.branchTestBooleanTruthy(branchIfTrue, R0, labelOf(pc + GET_JUMP_OFFSET(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_AND()
{
    return emitAndOr(false);
}

bool
BaselineCompiler::emit_JSOP_OR()
{
    return emitAndOr(true);
}

bool
BaselineCompiler::emit_JSOP_NOT()
{
    // Keep top stack value in R0.
    frame.popRegsAndSync(1);

    if (!emitToBoolean())
        return false;

    masm.notBoolean(R0);

    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_POS()
{
    // Keep top stack value in R0.
    frame.popRegsAndSync(1);

    // Inline path for int32 and double.
    Label done;
    masm.branchTestNumber(Assembler::Equal, R0, &done);

    // Call IC.
    ICToNumber_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    masm.bind(&done);
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_LOOPHEAD()
{
    return emitInterruptCheck();
}

bool
BaselineCompiler::emit_JSOP_LOOPENTRY()
{
    frame.syncStack(0);
    return emitUseCountIncrement();
}

bool
BaselineCompiler::emit_JSOP_VOID()
{
    frame.pop();
    frame.push(UndefinedValue());
    return true;
}

bool
BaselineCompiler::emit_JSOP_UNDEFINED()
{
    frame.push(UndefinedValue());
    return true;
}

bool
BaselineCompiler::emit_JSOP_HOLE()
{
    frame.push(MagicValue(JS_ARRAY_HOLE));
    return true;
}

bool
BaselineCompiler::emit_JSOP_NULL()
{
    frame.push(NullValue());
    return true;
}

bool
BaselineCompiler::emit_JSOP_THIS()
{
    // Keep this value in R0
    frame.pushThis();

    // In strict mode function or self-hosted function, |this| is left alone.
    if (!function() || function()->strict() || function()->isSelfHostedBuiltin())
        return true;

    Label skipIC;
    // Keep |thisv| in R0
    frame.popRegsAndSync(1);
    // If |this| is already an object, skip the IC.
    masm.branchTestObject(Assembler::Equal, R0, &skipIC);

    // Call IC
    ICThis_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    masm.storeValue(R0, frame.addressOfThis());

    // R0 is new pushed |this| value.
    masm.bind(&skipIC);
    frame.push(R0);

    return true;
}

bool
BaselineCompiler::emit_JSOP_TRUE()
{
    frame.push(BooleanValue(true));
    return true;
}

bool
BaselineCompiler::emit_JSOP_FALSE()
{
    frame.push(BooleanValue(false));
    return true;
}

bool
BaselineCompiler::emit_JSOP_ZERO()
{
    frame.push(Int32Value(0));
    return true;
}

bool
BaselineCompiler::emit_JSOP_ONE()
{
    frame.push(Int32Value(1));
    return true;
}

bool
BaselineCompiler::emit_JSOP_INT8()
{
    frame.push(Int32Value(GET_INT8(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_INT32()
{
    frame.push(Int32Value(GET_INT32(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_UINT16()
{
    frame.push(Int32Value(GET_UINT16(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_UINT24()
{
    frame.push(Int32Value(GET_UINT24(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_DOUBLE()
{
    frame.push(script->getConst(GET_UINT32_INDEX(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_STRING()
{
    frame.push(StringValue(script->getAtom(pc)));
    return true;
}

bool
BaselineCompiler::emit_JSOP_OBJECT()
{
    frame.push(ObjectValue(*script->getObject(pc)));
    return true;
}

typedef JSObject *(*CloneRegExpObjectFn)(JSContext *, JSObject *, JSObject *);
static const VMFunction CloneRegExpObjectInfo =
    FunctionInfo<CloneRegExpObjectFn>(CloneRegExpObject);

bool
BaselineCompiler::emit_JSOP_REGEXP()
{
    RootedObject reObj(cx, script->getRegExp(GET_UINT32_INDEX(pc)));
    RootedObject proto(cx, script->global().getOrCreateRegExpPrototype(cx));
    if (!proto)
        return false;

    prepareVMCall();

    pushArg(ImmGCPtr(proto));
    pushArg(ImmGCPtr(reObj));

    if (!callVM(CloneRegExpObjectInfo))
        return false;

    // Box and push return value.
    masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
    frame.push(R0);
    return true;
}

typedef JSObject *(*LambdaFn)(JSContext *, HandleFunction, HandleObject);
static const VMFunction LambdaInfo = FunctionInfo<LambdaFn>(js::Lambda);

bool
BaselineCompiler::emit_JSOP_LAMBDA()
{
    RootedFunction fun(cx, script->getFunction(GET_UINT32_INDEX(pc)));

    prepareVMCall();
    masm.loadPtr(frame.addressOfScopeChain(), R0.scratchReg());

    pushArg(R0.scratchReg());
    pushArg(ImmGCPtr(fun));

    if (!callVM(LambdaInfo))
        return false;

    // Box and push return value.
    masm.tagValue(JSVAL_TYPE_OBJECT, ReturnReg, R0);
    frame.push(R0);
    return true;
}

void
BaselineCompiler::storeValue(const StackValue *source, const Address &dest,
                             const ValueOperand &scratch)
{
    switch (source->kind()) {
      case StackValue::Constant:
        masm.storeValue(source->constant(), dest);
        break;
      case StackValue::Register:
        masm.storeValue(source->reg(), dest);
        break;
      case StackValue::LocalSlot:
        masm.loadValue(frame.addressOfLocal(source->localSlot()), scratch);
        masm.storeValue(scratch, dest);
        break;
      case StackValue::ArgSlot:
        masm.loadValue(frame.addressOfArg(source->argSlot()), scratch);
        masm.storeValue(scratch, dest);
        break;
      case StackValue::ThisSlot:
        masm.loadValue(frame.addressOfThis(), scratch);
        masm.storeValue(scratch, dest);
        break;
      case StackValue::Stack:
        masm.loadValue(frame.addressOfStackValue(source), scratch);
        masm.storeValue(scratch, dest);
        break;
      default:
        JS_NOT_REACHED("Invalid kind");
    }
}

bool
BaselineCompiler::emit_JSOP_BITOR()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_BITXOR()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_BITAND()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_LSH()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_RSH()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_URSH()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_ADD()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_SUB()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_MUL()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_DIV()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emit_JSOP_MOD()
{
    return emitBinaryArith();
}

bool
BaselineCompiler::emitBinaryArith()
{
    // Keep top JSStack value in R0 and R2
    frame.popRegsAndSync(2);

    // Call IC
    ICBinaryArith_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emitUnaryArith()
{
    // Keep top stack value in R0.
    frame.popRegsAndSync(1);

    // Call IC
    ICUnaryArith_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_BITNOT()
{
    return emitUnaryArith();
}

bool
BaselineCompiler::emit_JSOP_NEG()
{
    return emitUnaryArith();
}

bool
BaselineCompiler::emit_JSOP_LT()
{
    return emitCompare();
}

bool
BaselineCompiler::emit_JSOP_LE()
{
    return emitCompare();
}

bool
BaselineCompiler::emit_JSOP_GT()
{
    return emitCompare();
}

bool
BaselineCompiler::emit_JSOP_GE()
{
    return emitCompare();
}

bool
BaselineCompiler::emit_JSOP_EQ()
{
    return emitCompare();
}

bool
BaselineCompiler::emit_JSOP_NE()
{
    return emitCompare();
}

bool
BaselineCompiler::emitCompare()
{
    // CODEGEN

    // Keep top JSStack value in R0 and R2.
    frame.popRegsAndSync(2);

    // Call IC.
    ICCompare_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_NEWARRAY()
{
    frame.syncStack(0);

    uint32_t length = GET_UINT24(pc);
    RootedTypeObject type(cx);
    if (!types::UseNewTypeForInitializer(cx, script, pc, JSProto_Array)) {
        type = types::TypeScript::InitObject(cx, script, pc, JSProto_Array);
        if (!type)
            return false;
    }

    // Pass length in R0, type in R1.
    masm.move32(Imm32(length), R0.scratchReg());
    masm.movePtr(ImmGCPtr(type), R1.scratchReg());

    ICNewArray_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_INITELEM_ARRAY()
{
    // Ensure array is on the stack.
    frame.syncStack(1);

    StackValue *array = frame.peek(-2);
    StackValue *element = frame.peek(-1);

    uint32_t index = GET_UINT24(pc);

    // Load obj->elements.
    Register scratch = R2.scratchReg();
    masm.extractObject(frame.addressOfStackValue(array), scratch);
    masm.loadPtr(Address(scratch, JSObject::offsetOfElements()), scratch);

    // Update initialized length.
    masm.store32(Imm32(index + 1), Address(scratch, ObjectElements::offsetOfInitializedLength()));

    // Perform the store.
    storeValue(element, Address(scratch, index * sizeof(Value)), R0);

    frame.pop();
    return true;
}

bool
BaselineCompiler::emit_JSOP_ENDINIT()
{
    return true;
}

bool
BaselineCompiler::emit_JSOP_GETELEM()
{
    // Keep top two stack values in R0 and R1.
    frame.popRegsAndSync(2);

    // Call IC.
    ICGetElem_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_SETELEM()
{
    // Store RHS in the scratch slot.
    storeValue(frame.peek(-1), frame.addressOfScratchValue(), R2);
    frame.pop();

    // Keep object and index in R0 and R1.
    frame.popRegsAndSync(2);

    // Keep RHS on the stack.
    frame.pushScratchValue();

    // Call IC.
    ICSetElem_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    return true;
}

bool
BaselineCompiler::emit_JSOP_GETGNAME()
{
    RootedPropertyName name(cx, script->getName(pc));

    if (name == cx->names().undefined) {
        frame.push(UndefinedValue());
        return true;
    }
    if (name == cx->names().NaN) {
        frame.push(cx->runtime->NaNValue);
        return true;
    }
    if (name == cx->names().Infinity) {
        frame.push(cx->runtime->positiveInfinityValue);
        return true;
    }

    frame.syncStack(0);

    masm.movePtr(ImmGCPtr(&script->global()), R0.scratchReg());

    // Call IC.
    ICGetName_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_CALLGNAME()
{
    return emit_JSOP_GETGNAME();
}

bool
BaselineCompiler::emit_JSOP_BINDGNAME()
{
    frame.push(ObjectValue(script->global()));
    return true;
}

bool
BaselineCompiler::emit_JSOP_SETPROP()
{
    // Keep lhs in R0, rhs in R1.
    frame.popRegsAndSync(2);

    // Call IC.
    ICSetProp_Fallback::Compiler compiler(cx);
    if (!emitIC(compiler.getStub(&stubSpace_)))
        return false;

    // The IC will return the RHS value in R0, mark it as pushed value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_SETNAME()
{
    return emit_JSOP_SETPROP();
}

bool
BaselineCompiler::emit_JSOP_SETGNAME()
{
    return emit_JSOP_SETPROP();
}

bool
BaselineCompiler::emit_JSOP_GETPROP()
{
    // Keep object in R0.
    frame.popRegsAndSync(1);

    // Call IC.
    ICGetProp_Fallback::Compiler compiler(cx);
    if (!emitIC(compiler.getStub(&stubSpace_)))
        return false;

    // Mark R0 as pushed stack value.
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_CALLPROP()
{
    return emit_JSOP_GETPROP();
}

bool
BaselineCompiler::emit_JSOP_LENGTH()
{
    return emit_JSOP_GETPROP();
}

typedef bool (*DefVarOrConstFn)(JSContext *, HandlePropertyName, unsigned, HandleObject);
static const VMFunction DefVarOrConstInfo = FunctionInfo<DefVarOrConstFn>(DefVarOrConst);

bool
BaselineCompiler::emit_JSOP_DEFVAR()
{
    frame.syncStack(0);

    unsigned attrs = JSPROP_ENUMERATE;
    if (!script->isForEval())
        attrs |= JSPROP_PERMANENT;
    if (JSOp(*pc) == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;
    JS_ASSERT(attrs <= UINT32_MAX);

    masm.loadPtr(frame.addressOfScopeChain(), R0.scratchReg());

    prepareVMCall();

    pushArg(R0.scratchReg());
    pushArg(Imm32(attrs));
    pushArg(ImmGCPtr(script->getName(pc)));

    return callVM(DefVarOrConstInfo);
}

bool
BaselineCompiler::emit_JSOP_DEFCONST()
{
    return emit_JSOP_DEFVAR();
}

bool
BaselineCompiler::emit_JSOP_GETLOCAL()
{
    uint32_t local = GET_SLOTNO(pc);

    if (local >= frame.nlocals()) {
        // Destructuring assignments may use GETLOCAL to access stack values.
        frame.syncStack(0);
        masm.loadValue(Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfLocal(local)), R0);
        frame.push(R0);
        return true;
    }

    frame.pushLocal(local);
    return true;
}

bool
BaselineCompiler::emit_JSOP_CALLLOCAL()
{
    return emit_JSOP_GETLOCAL();
}

bool
BaselineCompiler::emit_JSOP_SETLOCAL()
{
    // Ensure no other StackValue refers to the old value, for instance i + (i = 3).
    // This also allows us to use R0 as scratch below.
    frame.syncStack(1);

    uint32_t local = GET_SLOTNO(pc);
    storeValue(frame.peek(-1), frame.addressOfLocal(local), R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_GETARG()
{
    // Arguments object is not yet supported.
    JS_ASSERT(!script->argsObjAliasesFormals());

    uint32_t arg = GET_SLOTNO(pc);
    frame.pushArg(arg);
    return true;
}

bool
BaselineCompiler::emit_JSOP_CALLARG()
{
    return emit_JSOP_GETARG();
}

bool
BaselineCompiler::emit_JSOP_SETARG()
{
    // Arguments object is not yet supported.
    JS_ASSERT(!script->argsObjAliasesFormals());

    // See the comment in JSOP_SETLOCAL.
    frame.syncStack(1);

    uint32_t arg = GET_SLOTNO(pc);
    storeValue(frame.peek(-1), frame.addressOfArg(arg), R0);
    return true;
}

bool
BaselineCompiler::emitCall()
{
    JS_ASSERT(js_CodeSpec[*pc].format & JOF_INVOKE);

    uint32_t argc = GET_ARGC(pc);

    frame.syncStack(0);
    masm.mov(Imm32(argc), R0.scratchReg());

    // Call IC
    ICCall_Fallback::Compiler stubCompiler(cx);
    if (!emitIC(stubCompiler.getStub(&stubSpace_)))
        return false;

    // Update FrameInfo.
    frame.popn(argc + 2);
    frame.push(R0);
    return true;
}

bool
BaselineCompiler::emit_JSOP_CALL()
{
    return emitCall();
}

bool
BaselineCompiler::emit_JSOP_NEW()
{
    return emitCall();
}

bool
BaselineCompiler::emit_JSOP_FUNCALL()
{
    return emitCall();
}

bool
BaselineCompiler::emit_JSOP_FUNAPPLY()
{
    return emitCall();
}

typedef bool (*ThrowFn)(JSContext *, HandleValue);
static const VMFunction ThrowInfo = FunctionInfo<ThrowFn>(js::Throw);

bool
BaselineCompiler::emit_JSOP_THROW()
{
    // Keep value to throw in R0.
    frame.popRegsAndSync(1);

    prepareVMCall();
    pushArg(R0);

    return callVM(ThrowInfo);
}

bool
BaselineCompiler::emit_JSOP_RETURN()
{
    JS_ASSERT(frame.stackDepth() == 1);

    JS_GC(cx->runtime);

    frame.popValue(JSReturnOperand);
    masm.jump(return_);
    return true;
}

bool
BaselineCompiler::emit_JSOP_STOP()
{
    JS_ASSERT(frame.stackDepth() == 0);

    masm.moveValue(UndefinedValue(), JSReturnOperand);

    // No need to jump to return_, epilogue follows immediately.
    return true;
}
