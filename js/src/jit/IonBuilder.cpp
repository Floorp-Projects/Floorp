/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonBuilder.h"

#include "mozilla/DebugOnly.h"

#include "jsautooplen.h"

#include "builtin/Eval.h"
#include "builtin/TypedObject.h"
#include "builtin/TypeRepresentation.h"
#include "frontend/SourceNotes.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineInspector.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/Ion.h"
#include "jit/IonSpewer.h"
#include "jit/Lowering.h"
#include "jit/MIRGraph.h"

#include "vm/ArgumentsObject.h"
#include "vm/RegExpStatics.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "jit/CompileInfo-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::Maybe;

IonBuilder::IonBuilder(JSContext *cx, TempAllocator *temp, MIRGraph *graph,
                       types::CompilerConstraintList *constraints,
                       BaselineInspector *inspector, CompileInfo *info, BaselineFrame *baselineFrame,
                       size_t inliningDepth, uint32_t loopDepth)
  : MIRGenerator(cx->compartment(), temp, graph, info),
    backgroundCodegen_(nullptr),
    cx(cx),
    baselineFrame_(baselineFrame),
    abortReason_(AbortReason_Disable),
    constraints_(constraints),
    analysis_(info->script()),
    thisTypes(nullptr),
    argTypes(nullptr),
    typeArray(nullptr),
    typeArrayHint(0),
    loopDepth_(loopDepth),
    callerResumePoint_(nullptr),
    callerBuilder_(nullptr),
    inspector(inspector),
    inliningDepth_(inliningDepth),
    numLoopRestarts_(0),
    failedBoundsCheck_(info->script()->failedBoundsCheck),
    failedShapeGuard_(info->script()->failedShapeGuard),
    nonStringIteration_(false),
    lazyArguments_(nullptr),
    inlineCallInfo_(nullptr)
{
    script_.init(info->script());
    pc = info->startPC();

    JS_ASSERT(script()->hasBaselineScript());
}

void
IonBuilder::clearForBackEnd()
{
    cx = nullptr;
    baselineFrame_ = nullptr;

    // The GSN cache allocates data from the malloc heap. Release this before
    // later phases of compilation to avoid leaks, as the top level IonBuilder
    // is not explicitly destroyed. Note that builders for inner scripts are
    // constructed on the stack and will release this memory on destruction.
    gsn.purge();
}

bool
IonBuilder::abort(const char *message, ...)
{
    // Don't call PCToLineNumber in release builds.
#ifdef DEBUG
    va_list ap;
    va_start(ap, message);
    abortFmt(message, ap);
    va_end(ap);
    IonSpew(IonSpew_Abort, "aborted @ %s:%d", script()->filename(), PCToLineNumber(script(), pc));
#endif
    return false;
}

void
IonBuilder::spew(const char *message)
{
    // Don't call PCToLineNumber in release builds.
#ifdef DEBUG
    IonSpew(IonSpew_MIR, "%s @ %s:%d", message, script()->filename(), PCToLineNumber(script(), pc));
#endif
}

static inline int32_t
GetJumpOffset(jsbytecode *pc)
{
    JS_ASSERT(js_CodeSpec[JSOp(*pc)].type() == JOF_JUMP);
    return GET_JUMP_OFFSET(pc);
}

IonBuilder::CFGState
IonBuilder::CFGState::If(jsbytecode *join, MBasicBlock *ifFalse)
{
    CFGState state;
    state.state = IF_TRUE;
    state.stopAt = join;
    state.branch.ifFalse = ifFalse;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::IfElse(jsbytecode *trueEnd, jsbytecode *falseEnd, MBasicBlock *ifFalse)
{
    CFGState state;
    // If the end of the false path is the same as the start of the
    // false path, then the "else" block is empty and we can devolve
    // this to the IF_TRUE case. We handle this here because there is
    // still an extra GOTO on the true path and we want stopAt to point
    // there, whereas the IF_TRUE case does not have the GOTO.
    state.state = (falseEnd == ifFalse->pc())
                  ? IF_TRUE_EMPTY_ELSE
                  : IF_ELSE_TRUE;
    state.stopAt = trueEnd;
    state.branch.falseEnd = falseEnd;
    state.branch.ifFalse = ifFalse;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::AndOr(jsbytecode *join, MBasicBlock *joinStart)
{
    CFGState state;
    state.state = AND_OR;
    state.stopAt = join;
    state.branch.ifFalse = joinStart;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::TableSwitch(jsbytecode *exitpc, MTableSwitch *ins)
{
    CFGState state;
    state.state = TABLE_SWITCH;
    state.stopAt = exitpc;
    state.tableswitch.exitpc = exitpc;
    state.tableswitch.breaks = nullptr;
    state.tableswitch.ins = ins;
    state.tableswitch.currentBlock = 0;
    return state;
}

JSFunction *
IonBuilder::getSingleCallTarget(types::TemporaryTypeSet *calleeTypes)
{
    if (!calleeTypes)
        return nullptr;

    JSObject *obj = calleeTypes->getSingleton();
    if (!obj || !obj->is<JSFunction>())
        return nullptr;

    return &obj->as<JSFunction>();
}

bool
IonBuilder::getPolyCallTargets(types::TemporaryTypeSet *calleeTypes, bool constructing,
                               ObjectVector &targets, uint32_t maxTargets, bool *gotLambda)
{
    JS_ASSERT(targets.length() == 0);
    JS_ASSERT(gotLambda);
    *gotLambda = false;

    if (!calleeTypes)
        return true;

    if (calleeTypes->baseFlags() != 0)
        return true;

    unsigned objCount = calleeTypes->getObjectCount();

    if (objCount == 0 || objCount > maxTargets)
        return true;

    if (!targets.reserve(objCount))
        return false;
    for(unsigned i = 0; i < objCount; i++) {
        JSObject *obj = calleeTypes->getSingleObject(i);
        JSFunction *fun;
        if (obj) {
            if (!obj->is<JSFunction>()) {
                targets.clear();
                return true;
            }
            fun = &obj->as<JSFunction>();
        } else {
            types::TypeObject *typeObj = calleeTypes->getTypeObject(i);
            JS_ASSERT(typeObj);
            if (!typeObj->interpretedFunction) {
                targets.clear();
                return true;
            }

            fun = typeObj->interpretedFunction;
            *gotLambda = true;
        }

        // Don't optimize if we're constructing and the callee is not a
        // constructor, so that CallKnown does not have to handle this case
        // (it should always throw).
        if (constructing && !fun->isInterpretedConstructor() && !fun->isNativeConstructor()) {
            targets.clear();
            return true;
        }

        DebugOnly<bool> appendOk = targets.append(fun);
        JS_ASSERT(appendOk);
    }

    // For now, only inline "singleton" lambda calls
    if (*gotLambda && targets.length() > 1)
        targets.clear();

    return true;
}

bool
IonBuilder::canEnterInlinedFunction(JSFunction *target)
{
    if (target->isHeavyweight())
        return false;

    JSScript *targetScript = target->nonLazyScript();

    if (targetScript->uninlineable)
        return false;

    if (!targetScript->analyzedArgsUsage())
        return false;

    if (targetScript->needsArgsObj())
        return false;

    if (!targetScript->compileAndGo)
        return false;

    types::TypeObjectKey *targetType = types::TypeObjectKey::get(target);
    if (targetType->unknownProperties())
        return false;

    return true;
}

bool
IonBuilder::canInlineTarget(JSFunction *target, CallInfo &callInfo)
{
    if (!target->isInterpreted()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-interpreted");
        return false;
    }

    if (target->getParent() != &script()->global()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to scope mismatch");
        return false;
    }

    // Allow constructing lazy scripts when performing the definite properties
    // analysis, as baseline has not been used to warm the caller up yet.
    if (target->isInterpreted() && info().executionMode() == DefinitePropertiesAnalysis) {
        if (!target->getOrCreateScript(context()))
            return false;

        RootedScript script(context(), target->nonLazyScript());
        if (!script->hasBaselineScript() && script->canBaselineCompile()) {
            MethodStatus status = BaselineCompile(context(), script);
            if (status != Method_Compiled)
                return false;
        }
    }

    if (!target->hasScript()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to lack of Non-Lazy script");
        return false;
    }

    if (callInfo.constructing() && !target->isInterpretedConstructor()) {
        IonSpew(IonSpew_Inlining, "Cannot inline because callee is not a constructor");
        return false;
    }

    JSScript *inlineScript = target->nonLazyScript();
    ExecutionMode executionMode = info().executionMode();
    if (!CanIonCompile(inlineScript, executionMode)) {
        IonSpew(IonSpew_Inlining, "%s:%d Cannot inline due to disable Ion compilation",
                                  inlineScript->filename(), inlineScript->lineno);
        return false;
    }

    // Don't inline functions which don't have baseline scripts.
    if (!inlineScript->hasBaselineScript()) {
        IonSpew(IonSpew_Inlining, "%s:%d Cannot inline target with no baseline jitcode",
                                  inlineScript->filename(), inlineScript->lineno);
        return false;
    }

    if (TooManyArguments(target->nargs)) {
        IonSpew(IonSpew_Inlining, "%s:%d Cannot inline too many args",
                                  inlineScript->filename(), inlineScript->lineno);
        return false;
    }

    if (TooManyArguments(callInfo.argc())) {
        IonSpew(IonSpew_Inlining, "%s:%d Cannot inline too many args",
                                  inlineScript->filename(), inlineScript->lineno);
        return false;
    }

    // Allow inlining of recursive calls, but only one level deep.
    IonBuilder *builder = callerBuilder_;
    while (builder) {
        if (builder->script() == inlineScript) {
            IonSpew(IonSpew_Inlining, "%s:%d Not inlining recursive call",
                                       inlineScript->filename(), inlineScript->lineno);
            return false;
        }
        builder = builder->callerBuilder_;
    }

    if (!canEnterInlinedFunction(target)) {
        IonSpew(IonSpew_Inlining, "%s:%d Cannot inline due to oracle veto %d",
                                  inlineScript->filename(), inlineScript->lineno,
                                  script()->lineno);
        return false;
    }

    return true;
}

void
IonBuilder::popCfgStack()
{
    if (cfgStack_.back().isLoop())
        loops_.popBack();
    if (cfgStack_.back().state == CFGState::LABEL)
        labels_.popBack();
    cfgStack_.popBack();
}

void
IonBuilder::analyzeNewLoopTypes(MBasicBlock *entry, jsbytecode *start, jsbytecode *end)
{
    // The phi inputs at the loop head only reflect types for variables that
    // were present at the start of the loop. If the variable changes to a new
    // type within the loop body, and that type is carried around to the loop
    // head, then we need to know about the new type up front.
    //
    // Since SSA information hasn't been constructed for the loop body yet, we
    // need a separate analysis to pick out the types that might flow around
    // the loop header. This is a best-effort analysis that may either over-
    // or under-approximate the set of such types.
    //
    // Over-approximating the types may lead to inefficient generated code, and
    // under-approximating the types will cause the loop body to be analyzed
    // multiple times as the correct types are deduced (see finishLoop).

    // If we restarted processing of an outer loop then get loop header types
    // directly from the last time we have previously processed this loop. This
    // both avoids repeated work from the bytecode traverse below, and will
    // also pick up types discovered while previously building the loop body.
    for (size_t i = 0; i < loopHeaders_.length(); i++) {
        if (loopHeaders_[i].pc == start) {
            MBasicBlock *oldEntry = loopHeaders_[i].header;
            for (MPhiIterator oldPhi = oldEntry->phisBegin();
                 oldPhi != oldEntry->phisEnd();
                 oldPhi++)
            {
                MPhi *newPhi = entry->getSlot(oldPhi->slot())->toPhi();
                newPhi->addBackedgeType(oldPhi->type(), oldPhi->resultTypeSet());
            }
            // Update the most recent header for this loop encountered, in case
            // new types flow to the phis and the loop is processed at least
            // three times.
            loopHeaders_[i].header = entry;
            return;
        }
    }
    loopHeaders_.append(LoopHeader(start, entry));

    jsbytecode *last = nullptr, *earlier = nullptr;
    for (jsbytecode *pc = start; pc != end; earlier = last, last = pc, pc += GetBytecodeLength(pc)) {
        uint32_t slot;
        if (*pc == JSOP_SETLOCAL)
            slot = info().localSlot(GET_SLOTNO(pc));
        else if (*pc == JSOP_SETARG)
            slot = info().argSlotUnchecked(GET_SLOTNO(pc));
        else
            continue;
        if (slot >= info().firstStackSlot())
            continue;
        if (!analysis().maybeInfo(pc))
            continue;

        MPhi *phi = entry->getSlot(slot)->toPhi();

        if (*last == JSOP_POS)
            last = earlier;

        if (js_CodeSpec[*last].format & JOF_TYPESET) {
            types::TemporaryTypeSet *typeSet = bytecodeTypes(last);
            if (!typeSet->empty()) {
                MIRType type = MIRTypeFromValueType(typeSet->getKnownTypeTag());
                phi->addBackedgeType(type, typeSet);
            }
        } else if (*last == JSOP_GETLOCAL || *last == JSOP_GETARG) {
            uint32_t slot = (*last == JSOP_GETLOCAL)
                            ? info().localSlot(GET_SLOTNO(last))
                            : info().argSlotUnchecked(GET_SLOTNO(last));
            if (slot < info().firstStackSlot()) {
                MPhi *otherPhi = entry->getSlot(slot)->toPhi();
                if (otherPhi->hasBackedgeType())
                    phi->addBackedgeType(otherPhi->type(), otherPhi->resultTypeSet());
            }
        } else {
            MIRType type = MIRType_None;
            switch (*last) {
              case JSOP_VOID:
              case JSOP_UNDEFINED:
                type = MIRType_Undefined;
                break;
              case JSOP_NULL:
                type = MIRType_Null;
                break;
              case JSOP_ZERO:
              case JSOP_ONE:
              case JSOP_INT8:
              case JSOP_INT32:
              case JSOP_UINT16:
              case JSOP_UINT24:
              case JSOP_BITAND:
              case JSOP_BITOR:
              case JSOP_BITXOR:
              case JSOP_BITNOT:
              case JSOP_RSH:
              case JSOP_LSH:
              case JSOP_URSH:
                type = MIRType_Int32;
                break;
              case JSOP_FALSE:
              case JSOP_TRUE:
              case JSOP_EQ:
              case JSOP_NE:
              case JSOP_LT:
              case JSOP_LE:
              case JSOP_GT:
              case JSOP_GE:
              case JSOP_NOT:
              case JSOP_STRICTEQ:
              case JSOP_STRICTNE:
              case JSOP_IN:
              case JSOP_INSTANCEOF:
                type = MIRType_Boolean;
                break;
              case JSOP_DOUBLE:
                type = MIRType_Double;
                break;
              case JSOP_STRING:
              case JSOP_TYPEOF:
              case JSOP_TYPEOFEXPR:
              case JSOP_ITERNEXT:
                type = MIRType_String;
                break;
              case JSOP_ADD:
              case JSOP_SUB:
              case JSOP_MUL:
              case JSOP_DIV:
              case JSOP_MOD:
              case JSOP_NEG:
                type = inspector->expectedResultType(last);
              default:
                break;
            }
            if (type != MIRType_None)
                phi->addBackedgeType(type, nullptr);
        }
    }
}

bool
IonBuilder::pushLoop(CFGState::State initial, jsbytecode *stopAt, MBasicBlock *entry, bool osr,
                     jsbytecode *loopHead, jsbytecode *initialPc,
                     jsbytecode *bodyStart, jsbytecode *bodyEnd, jsbytecode *exitpc,
                     jsbytecode *continuepc)
{
    if (!continuepc)
        continuepc = entry->pc();

    ControlFlowInfo loop(cfgStack_.length(), continuepc);
    if (!loops_.append(loop))
        return false;

    CFGState state;
    state.state = initial;
    state.stopAt = stopAt;
    state.loop.bodyStart = bodyStart;
    state.loop.bodyEnd = bodyEnd;
    state.loop.exitpc = exitpc;
    state.loop.continuepc = continuepc;
    state.loop.entry = entry;
    state.loop.osr = osr;
    state.loop.successor = nullptr;
    state.loop.breaks = nullptr;
    state.loop.continues = nullptr;
    state.loop.initialState = initial;
    state.loop.initialPc = initialPc;
    state.loop.initialStopAt = stopAt;
    state.loop.loopHead = loopHead;
    return cfgStack_.append(state);
}

bool
IonBuilder::init()
{
    if (!types::TypeScript::FreezeTypeSets(constraints(), script(),
                                           &thisTypes, &argTypes, &typeArray))
    {
        return false;
    }

    if (!analysis().init(gsn))
        return false;

    return true;
}

bool
IonBuilder::build()
{
    if (!init())
        return false;

    setCurrentAndSpecializePhis(newBlock(pc));
    if (!current)
        return false;

    IonSpew(IonSpew_Scripts, "Analyzing script %s:%d (%p) (usecount=%d)",
            script()->filename(), script()->lineno, (void *)script(), (int)script()->getUseCount());

    if (!initParameters())
        return false;

    // Initialize local variables.
    for (uint32_t i = 0; i < info().nlocals(); i++) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        current->initSlot(info().localSlot(i), undef);
    }

    // Initialize something for the scope chain. We can bail out before the
    // start instruction, but the snapshot is encoded *at* the start
    // instruction, which means generating any code that could load into
    // registers is illegal.
    MInstruction *scope = MConstant::New(UndefinedValue());
    current->add(scope);
    current->initSlot(info().scopeChainSlot(), scope);

    // Initialize the return value.
    MInstruction *returnValue = MConstant::New(UndefinedValue());
    current->add(returnValue);
    current->initSlot(info().returnValueSlot(), returnValue);

    // Initialize the arguments object slot to undefined if necessary.
    if (info().hasArguments()) {
        MInstruction *argsObj = MConstant::New(UndefinedValue());
        current->add(argsObj);
        current->initSlot(info().argsObjSlot(), argsObj);
    }

    // Emit the start instruction, so we can begin real instructions.
    current->makeStart(MStart::New(MStart::StartType_Default));
    if (instrumentedProfiling())
        current->add(MFunctionBoundary::New(script(), MFunctionBoundary::Enter));

    // Parameters have been checked to correspond to the typeset, now we unbox
    // what we can in an infallible manner.
    rewriteParameters();

    // It's safe to start emitting actual IR, so now build the scope chain.
    if (!initScopeChain())
        return false;

    if (info().needsArgsObj() && !initArgumentsObject())
        return false;

    // Guard against over-recursion.
    MCheckOverRecursed *check = new MCheckOverRecursed;
    current->add(check);
    check->setResumePoint(current->entryResumePoint());

    // Prevent |this| from being DCE'd: necessary for constructors.
    if (info().fun())
        current->getSlot(info().thisSlot())->setGuard();

    // The type analysis phase attempts to insert unbox operations near
    // definitions of values. It also attempts to replace uses in resume points
    // with the narrower, unboxed variants. However, we must prevent this
    // replacement from happening on values in the entry snapshot. Otherwise we
    // could get this:
    //
    //       v0 = MParameter(0)
    //       v1 = MParameter(1)
    //       --   ResumePoint(v2, v3)
    //       v2 = Unbox(v0, INT32)
    //       v3 = Unbox(v1, INT32)
    //
    // So we attach the initial resume point to each parameter, which the type
    // analysis explicitly checks (this is the same mechanism used for
    // effectful operations).
    for (uint32_t i = 0; i < info().endArgSlot(); i++) {
        MInstruction *ins = current->getEntrySlot(i)->toInstruction();
        if (ins->type() == MIRType_Value)
            ins->setResumePoint(current->entryResumePoint());
    }

    // lazyArguments should never be accessed in |argsObjAliasesFormals| scripts.
    if (info().hasArguments() && !info().argsObjAliasesFormals()) {
        lazyArguments_ = MConstant::New(MagicValue(JS_OPTIMIZED_ARGUMENTS));
        current->add(lazyArguments_);
    }

    if (!traverseBytecode())
        return false;

    if (!processIterators())
        return false;

    JS_ASSERT(loopDepth_ == 0);
    abortReason_ = AbortReason_NoAbort;
    return true;
}

bool
IonBuilder::processIterators()
{
    // Find phis that must directly hold an iterator live.
    Vector<MPhi *, 0, SystemAllocPolicy> worklist;
    for (size_t i = 0; i < iterators_.length(); i++) {
        MInstruction *ins = iterators_[i];
        for (MUseDefIterator iter(ins); iter; iter++) {
            if (iter.def()->isPhi()) {
                if (!worklist.append(iter.def()->toPhi()))
                    return false;
            }
        }
    }

    // Propagate the iterator and live status of phis to all other connected
    // phis.
    while (!worklist.empty()) {
        MPhi *phi = worklist.popCopy();
        phi->setIterator();
        phi->setFoldedUnchecked();

        for (MUseDefIterator iter(phi); iter; iter++) {
            if (iter.def()->isPhi()) {
                MPhi *other = iter.def()->toPhi();
                if (!other->isIterator() && !worklist.append(other))
                    return false;
            }
        }
    }

    return true;
}

bool
IonBuilder::buildInline(IonBuilder *callerBuilder, MResumePoint *callerResumePoint,
                        CallInfo &callInfo)
{
    if (!init())
        return false;

    inlineCallInfo_ = &callInfo;

    IonSpew(IonSpew_Scripts, "Inlining script %s:%d (%p)",
            script()->filename(), script()->lineno, (void *)script());

    callerBuilder_ = callerBuilder;
    callerResumePoint_ = callerResumePoint;

    if (callerBuilder->failedBoundsCheck_)
        failedBoundsCheck_ = true;

    if (callerBuilder->failedShapeGuard_)
        failedShapeGuard_ = true;

    // Generate single entrance block.
    setCurrentAndSpecializePhis(newBlock(pc));
    if (!current)
        return false;

    current->setCallerResumePoint(callerResumePoint);

    // Connect the entrance block to the last block in the caller's graph.
    MBasicBlock *predecessor = callerBuilder->current;
    JS_ASSERT(predecessor == callerResumePoint->block());

    // All further instructions generated in from this scope should be
    // considered as part of the function that we're inlining. We also need to
    // keep track of the inlining depth because all scripts inlined on the same
    // level contiguously have only one Inline_Exit node.
    if (instrumentedProfiling())
        predecessor->add(MFunctionBoundary::New(script(),
                                                MFunctionBoundary::Inline_Enter,
                                                inliningDepth_));

    predecessor->end(MGoto::New(current));
    if (!current->addPredecessorWithoutPhis(predecessor))
        return false;

    // Initialize scope chain slot to Undefined.  It's set later by |initScopeChain|.
    MInstruction *scope = MConstant::New(UndefinedValue());
    current->add(scope);
    current->initSlot(info().scopeChainSlot(), scope);

    // Initialize |return value| slot.
    MInstruction *returnValue = MConstant::New(UndefinedValue());
    current->add(returnValue);
    current->initSlot(info().returnValueSlot(), returnValue);

    // Initialize |arguments| slot.
    if (info().hasArguments()) {
        MInstruction *argsObj = MConstant::New(UndefinedValue());
        current->add(argsObj);
        current->initSlot(info().argsObjSlot(), argsObj);
    }

    // Initialize |this| slot.
    current->initSlot(info().thisSlot(), callInfo.thisArg());

    IonSpew(IonSpew_Inlining, "Initializing %u arg slots", info().nargs());

    // NB: Ion does not inline functions which |needsArgsObj|.  So using argSlot()
    // instead of argSlotUnchecked() below is OK
    JS_ASSERT(!info().needsArgsObj());

    // Initialize actually set arguments.
    uint32_t existing_args = Min<uint32_t>(callInfo.argc(), info().nargs());
    for (size_t i = 0; i < existing_args; ++i) {
        MDefinition *arg = callInfo.getArg(i);
        current->initSlot(info().argSlot(i), arg);
    }

    // Pass Undefined for missing arguments
    for (size_t i = callInfo.argc(); i < info().nargs(); ++i) {
        MConstant *arg = MConstant::New(UndefinedValue());
        current->add(arg);
        current->initSlot(info().argSlot(i), arg);
    }

    // Initialize the scope chain now that args are initialized.
    if (!initScopeChain(callInfo.fun()))
        return false;

    IonSpew(IonSpew_Inlining, "Initializing %u local slots", info().nlocals());

    // Initialize local variables.
    for (uint32_t i = 0; i < info().nlocals(); i++) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        current->initSlot(info().localSlot(i), undef);
    }

    IonSpew(IonSpew_Inlining, "Inline entry block MResumePoint %p, %u operands",
            (void *) current->entryResumePoint(), current->entryResumePoint()->numOperands());

    // +2 for the scope chain and |this|, maybe another +1 for arguments object slot.
    JS_ASSERT(current->entryResumePoint()->numOperands() == info().totalSlots());

    if (script_->argumentsHasVarBinding()) {
        lazyArguments_ = MConstant::New(MagicValue(JS_OPTIMIZED_ARGUMENTS));
        current->add(lazyArguments_);
    }

    if (!traverseBytecode())
        return false;

    return true;
}

void
IonBuilder::rewriteParameter(uint32_t slotIdx, MDefinition *param, int32_t argIndex)
{
    JS_ASSERT(param->isParameter() || param->isGetArgumentsObjectArg());

    types::TemporaryTypeSet *types = param->resultTypeSet();
    JSValueType definiteType = types->getKnownTypeTag();
    if (definiteType == JSVAL_TYPE_UNKNOWN)
        return;

    MInstruction *actual = nullptr;
    switch (definiteType) {
      case JSVAL_TYPE_UNDEFINED:
        param->setFoldedUnchecked();
        actual = MConstant::New(UndefinedValue());
        break;

      case JSVAL_TYPE_NULL:
        param->setFoldedUnchecked();
        actual = MConstant::New(NullValue());
        break;

      default:
        actual = MUnbox::New(param, MIRTypeFromValueType(definiteType), MUnbox::Infallible);
        break;
    }

    // Careful! We leave the original MParameter in the entry resume point. The
    // arguments still need to be checked unless proven otherwise at the call
    // site, and these checks can bailout. We can end up:
    //   v0 = Parameter(0)
    //   v1 = Unbox(v0, INT32)
    //   --   ResumePoint(v0)
    //
    // As usual, it would be invalid for v1 to be captured in the initial
    // resume point, rather than v0.
    current->add(actual);
    current->rewriteSlot(slotIdx, actual);
}

// Apply Type Inference information to parameters early on, unboxing them if
// they have a definitive type. The actual guards will be emitted by the code
// generator, explicitly, as part of the function prologue.
void
IonBuilder::rewriteParameters()
{
    JS_ASSERT(info().scopeChainSlot() == 0);

    if (!info().fun())
        return;

    for (uint32_t i = info().startArgSlot(); i < info().endArgSlot(); i++) {
        MDefinition *param = current->getSlot(i);
        rewriteParameter(i, param, param->toParameter()->index());
    }
}

bool
IonBuilder::initParameters()
{
    if (!info().fun())
        return true;

    // If we are doing OSR on a frame which initially executed in the
    // interpreter and didn't accumulate type information, try to use that OSR
    // frame to determine possible initial types for 'this' and parameters.

    if (thisTypes->empty() && baselineFrame_) {
        if (!thisTypes->addType(types::GetValueType(baselineFrame_->thisValue()), temp_->lifoAlloc()))
            return false;
    }

    MParameter *param = MParameter::New(MParameter::THIS_SLOT, thisTypes);
    current->add(param);
    current->initSlot(info().thisSlot(), param);

    for (uint32_t i = 0; i < info().nargs(); i++) {
        types::TemporaryTypeSet *types = &argTypes[i];
        if (types->empty() && baselineFrame_ &&
            !script_->baselineScript()->modifiesArguments())
        {
            if (!types->addType(types::GetValueType(baselineFrame_->argv()[i]), temp_->lifoAlloc()))
                return false;
        }

        param = MParameter::New(i, types);
        current->add(param);
        current->initSlot(info().argSlotUnchecked(i), param);
    }

    return true;
}

bool
IonBuilder::initScopeChain(MDefinition *callee)
{
    MInstruction *scope = nullptr;

    // If the script doesn't use the scopechain, then it's already initialized
    // from earlier.  However, always make a scope chain when |needsArgsObj| is true
    // for the script, since arguments object construction requires the scope chain
    // to be passed in.
    if (!info().needsArgsObj() && !analysis().usesScopeChain())
        return true;

    // The scope chain is only tracked in scripts that have NAME opcodes which
    // will try to access the scope. For other scripts, the scope instructions
    // will be held live by resume points and code will still be generated for
    // them, so just use a constant undefined value.
    if (!script()->compileAndGo)
        return abort("non-CNG global scripts are not supported");

    if (JSFunction *fun = info().fun()) {
        if (!callee) {
            MCallee *calleeIns = MCallee::New();
            current->add(calleeIns);
            callee = calleeIns;
        }
        scope = MFunctionEnvironment::New(callee);
        current->add(scope);

        // This reproduce what is done in CallObject::createForFunction
        if (fun->isHeavyweight()) {
            if (fun->isNamedLambda()) {
                scope = createDeclEnvObject(callee, scope);
                if (!scope)
                    return false;
            }

            scope = createCallObject(callee, scope);
            if (!scope)
                return false;
        }
    } else {
        scope = MConstant::New(ObjectValue(script()->global()));
        current->add(scope);
    }

    current->setScopeChain(scope);
    return true;
}

bool
IonBuilder::initArgumentsObject()
{
    IonSpew(IonSpew_MIR, "%s:%d - Emitting code to initialize arguments object! block=%p",
                              script()->filename(), script()->lineno, current);
    JS_ASSERT(info().needsArgsObj());
    MCreateArgumentsObject *argsObj = MCreateArgumentsObject::New(current->scopeChain());
    current->add(argsObj);
    current->setArgumentsObject(argsObj);
    return true;
}

bool
IonBuilder::addOsrValueTypeBarrier(uint32_t slot, MInstruction **def_,
                                   MIRType type, types::TemporaryTypeSet *typeSet)
{
    MInstruction *&def = *def_;
    MBasicBlock *osrBlock = def->block();

    // Clear bogus type information added in newOsrPreheader().
    def->setResultType(MIRType_Value);
    def->setResultTypeSet(nullptr);

    if (typeSet && !typeSet->unknown()) {
        MInstruction *barrier = MTypeBarrier::New(def, typeSet);
        osrBlock->insertBefore(osrBlock->lastIns(), barrier);
        osrBlock->rewriteSlot(slot, barrier);
        def = barrier;
    } else if (type == MIRType_Null ||
               type == MIRType_Undefined ||
               type == MIRType_Magic)
    {
        // No unbox instruction will be added below, so check the type by
        // adding a type barrier for a singleton type set.
        types::Type ntype = types::Type::PrimitiveType(ValueTypeFromMIRType(type));
        typeSet = temp_->lifoAlloc()->new_<types::TemporaryTypeSet>(ntype);
        if (!typeSet)
            return false;
        MInstruction *barrier = MTypeBarrier::New(def, typeSet);
        osrBlock->insertBefore(osrBlock->lastIns(), barrier);
        osrBlock->rewriteSlot(slot, barrier);
        def = barrier;
    }

    if (type != def->type()) {
        switch (type) {
          case MIRType_Boolean:
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_String:
          case MIRType_Object:
          {
            MUnbox *unbox = MUnbox::New(def, type, MUnbox::Fallible);
            osrBlock->insertBefore(osrBlock->lastIns(), unbox);
            osrBlock->rewriteSlot(slot, unbox);
            def = unbox;
            break;
          }

          case MIRType_Null:
          {
            MConstant *c = MConstant::New(NullValue());
            osrBlock->insertBefore(osrBlock->lastIns(), c);
            osrBlock->rewriteSlot(slot, c);
            def = c;
            break;
          }

          case MIRType_Undefined:
          {
            MConstant *c = MConstant::New(UndefinedValue());
            osrBlock->insertBefore(osrBlock->lastIns(), c);
            osrBlock->rewriteSlot(slot, c);
            def = c;
            break;
          }

          case MIRType_Magic:
            JS_ASSERT(lazyArguments_);
            osrBlock->rewriteSlot(slot, lazyArguments_);
            def = lazyArguments_;
            break;

          default:
            break;
        }
    }

    JS_ASSERT(def == osrBlock->getSlot(slot));
    return true;
}

bool
IonBuilder::maybeAddOsrTypeBarriers()
{
    if (!info().osrPc())
        return true;

    // The loop has successfully been processed, and the loop header phis
    // have their final type. Add unboxes and type barriers in the OSR
    // block to check that the values have the appropriate type, and update
    // the types in the preheader.

    MBasicBlock *osrBlock = graph().osrBlock();
    if (!osrBlock) {
        // Because IonBuilder does not compile catch blocks, it's possible to
        // end up without an OSR block if the OSR pc is only reachable via a
        // break-statement inside the catch block. For instance:
        //
        //   for (;;) {
        //       try {
        //           throw 3;
        //       } catch(e) {
        //           break;
        //       }
        //   }
        //   while (..) { } // <= OSR here, only reachable via catch block.
        //
        // For now we just abort in this case.
        JS_ASSERT(graph().hasTryBlock());
        return abort("OSR block only reachable through catch block");
    }

    MBasicBlock *preheader = osrBlock->getSuccessor(0);
    MBasicBlock *header = preheader->getSuccessor(0);
    static const size_t OSR_PHI_POSITION = 1;
    JS_ASSERT(preheader->getPredecessor(OSR_PHI_POSITION) == osrBlock);

    MPhiIterator headerPhi = header->phisBegin();
    while (headerPhi != header->phisEnd() && headerPhi->slot() < info().startArgSlot())
        headerPhi++;

    for (uint32_t i = info().startArgSlot(); i < osrBlock->stackDepth(); i++, headerPhi++) {

        // Aliased slots are never accessed, since they need to go through
        // the callobject. The typebarriers are added there and can be
        // discared here.
        if (info().isSlotAliased(i))
            continue;

        MInstruction *def = osrBlock->getSlot(i)->toInstruction();

        JS_ASSERT(headerPhi->slot() == i);
        MPhi *preheaderPhi = preheader->getSlot(i)->toPhi();

        MIRType type = headerPhi->type();
        types::TemporaryTypeSet *typeSet = headerPhi->resultTypeSet();

        if (!addOsrValueTypeBarrier(i, &def, type, typeSet))
            return false;

        preheaderPhi->replaceOperand(OSR_PHI_POSITION, def);
        preheaderPhi->setResultType(type);
        preheaderPhi->setResultTypeSet(typeSet);
    }

    return true;
}

// We try to build a control-flow graph in the order that it would be built as
// if traversing the AST. This leads to a nice ordering and lets us build SSA
// in one pass, since the bytecode is structured.
//
// We traverse the bytecode iteratively, maintaining a current basic block.
// Each basic block has a mapping of local slots to instructions, as well as a
// stack depth. As we encounter instructions we mutate this mapping in the
// current block.
//
// Things get interesting when we encounter a control structure. This can be
// either an IFEQ, downward GOTO, or a decompiler hint stashed away in source
// notes. Once we encounter such an opcode, we recover the structure of the
// control flow (its branches and bounds), and push it on a stack.
//
// As we continue traversing the bytecode, we look for points that would
// terminate the topmost control flow path pushed on the stack. These are:
//  (1) The bounds of the current structure (end of a loop or join/edge of a
//      branch).
//  (2) A "return", "break", or "continue" statement.
//
// For (1), we expect that there is a current block in the progress of being
// built, and we complete the necessary edges in the CFG. For (2), we expect
// that there is no active block.
//
// For normal diamond join points, we construct Phi nodes as we add
// predecessors. For loops, care must be taken to propagate Phi nodes back
// through uses in the loop body.
bool
IonBuilder::traverseBytecode()
{
    for (;;) {
        JS_ASSERT(pc < info().limitPC());

        for (;;) {
            if (!temp().ensureBallast())
                return false;

            // Check if we've hit an expected join point or edge in the bytecode.
            // Leaving one control structure could place us at the edge of another,
            // thus |while| instead of |if| so we don't skip any opcodes.
            if (!cfgStack_.empty() && cfgStack_.back().stopAt == pc) {
                ControlStatus status = processCfgStack();
                if (status == ControlStatus_Error)
                    return false;
                if (status == ControlStatus_Abort)
                    return abort("Aborted while processing control flow");
                if (!current)
                    return maybeAddOsrTypeBarriers();
                continue;
            }

            // Some opcodes need to be handled early because they affect control
            // flow, terminating the current basic block and/or instructing the
            // traversal algorithm to continue from a new pc.
            //
            //   (1) If the opcode does not affect control flow, then the opcode
            //       is inspected and transformed to IR. This is the process_opcode
            //       label.
            //   (2) A loop could be detected via a forward GOTO. In this case,
            //       we don't want to process the GOTO, but the following
            //       instruction.
            //   (3) A RETURN, STOP, BREAK, or CONTINUE may require processing the
            //       CFG stack to terminate open branches.
            //
            // Similar to above, snooping control flow could land us at another
            // control flow point, so we iterate until it's time to inspect a real
            // opcode.
            ControlStatus status;
            if ((status = snoopControlFlow(JSOp(*pc))) == ControlStatus_None)
                break;
            if (status == ControlStatus_Error)
                return false;
            if (status == ControlStatus_Abort)
                return abort("Aborted while processing control flow");
            if (!current)
                return maybeAddOsrTypeBarriers();
        }

#ifdef DEBUG
        // In debug builds, after compiling this op, check that all values
        // popped by this opcode either:
        //
        //   (1) Have the Folded flag set on them.
        //   (2) Have more uses than before compiling this op (the value is
        //       used as operand of a new MIR instruction).
        //
        // This is used to catch problems where IonBuilder pops a value without
        // adding any SSA uses and doesn't call setFoldedUnchecked on it.
        Vector<MDefinition *, 4, IonAllocPolicy> popped;
        Vector<size_t, 4, IonAllocPolicy> poppedUses;
        unsigned nuses = GetUseCount(script_, pc - script_->code);

        for (unsigned i = 0; i < nuses; i++) {
            MDefinition *def = current->peek(-int32_t(i + 1));
            if (!popped.append(def) || !poppedUses.append(def->defUseCount()))
                return false;
        }
#endif

        // Nothing in inspectOpcode() is allowed to advance the pc.
        JSOp op = JSOp(*pc);
        if (!inspectOpcode(op))
            return false;

#ifdef DEBUG
        for (size_t i = 0; i < popped.length(); i++) {
            // Call instructions can discard PassArg instructions. Ignore them.
            if (popped[i]->isPassArg() && !popped[i]->hasUses())
                continue;

            switch (op) {
              case JSOP_POP:
              case JSOP_POPN:
              case JSOP_DUP:
              case JSOP_DUP2:
              case JSOP_PICK:
              case JSOP_SWAP:
              case JSOP_SETARG:
              case JSOP_SETLOCAL:
              case JSOP_SETRVAL:
              case JSOP_POPV:
              case JSOP_VOID:
                // Don't require SSA uses for values popped by these ops.
                break;

              case JSOP_POS:
              case JSOP_TOID:
                // These ops may leave their input on the stack without setting
                // the Folded flag. If this value will be popped immediately,
                // we may replace it with |undefined|, but the difference is
                // not observable.
                JS_ASSERT(i == 0);
                if (current->peek(-1) == popped[0])
                    break;
                // FALL THROUGH

              default:
                JS_ASSERT(popped[i]->isFolded() ||

                          // MNewDerivedTypedObject instances are
                          // often dead unless they escape from the
                          // fn. See IonBuilder::loadTypedObjectData()
                          // for more details.
                          popped[i]->isNewDerivedTypedObject() ||

                          popped[i]->defUseCount() > poppedUses[i]);
                break;
            }
        }
#endif

        pc += js_CodeSpec[op].length;
        current->updateTrackedPc(pc);
    }

    return maybeAddOsrTypeBarriers();
}

IonBuilder::ControlStatus
IonBuilder::snoopControlFlow(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return maybeLoop(op, info().getNote(gsn, pc));

      case JSOP_POP:
        return maybeLoop(op, info().getNote(gsn, pc));

      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
        return processReturn(op);

      case JSOP_THROW:
        return processThrow();

      case JSOP_GOTO:
      {
        jssrcnote *sn = info().getNote(gsn, pc);
        switch (sn ? SN_TYPE(sn) : SRC_NULL) {
          case SRC_BREAK:
          case SRC_BREAK2LABEL:
            return processBreak(op, sn);

          case SRC_CONTINUE:
            return processContinue(op);

          case SRC_SWITCHBREAK:
            return processSwitchBreak(op);

          case SRC_WHILE:
          case SRC_FOR_IN:
          case SRC_FOR_OF:
            // while (cond) { }
            return whileOrForInLoop(sn);

          default:
            // Hard assert for now - make an error later.
            MOZ_ASSUME_UNREACHABLE("unknown goto case");
        }
        break;
      }

      case JSOP_TABLESWITCH:
        return tableSwitch(op, info().getNote(gsn, pc));

      case JSOP_IFNE:
        // We should never reach an IFNE, it's a stopAt point, which will
        // trigger closing the loop.
        MOZ_ASSUME_UNREACHABLE("we should never reach an ifne!");

      default:
        break;
    }
    return ControlStatus_None;
}

bool
IonBuilder::inspectOpcode(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
      case JSOP_LINENO:
      case JSOP_LOOPENTRY:
        return true;

      case JSOP_LABEL:
        return jsop_label();

      case JSOP_UNDEFINED:
        return pushConstant(UndefinedValue());

      case JSOP_IFEQ:
        return jsop_ifeq(JSOP_IFEQ);

      case JSOP_TRY:
        return jsop_try();

      case JSOP_CONDSWITCH:
        return jsop_condswitch();

      case JSOP_BITNOT:
        return jsop_bitnot();

      case JSOP_BITAND:
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_LSH:
      case JSOP_RSH:
      case JSOP_URSH:
        return jsop_bitop(op);

      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_DIV:
      case JSOP_MOD:
        return jsop_binary(op);

      case JSOP_POS:
        return jsop_pos();

      case JSOP_NEG:
        return jsop_neg();

      case JSOP_AND:
      case JSOP_OR:
        return jsop_andor(op);

      case JSOP_DEFVAR:
      case JSOP_DEFCONST:
        return jsop_defvar(GET_UINT32_INDEX(pc));

      case JSOP_DEFFUN:
        return jsop_deffun(GET_UINT32_INDEX(pc));

      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
        return jsop_compare(op);

      case JSOP_DOUBLE:
        return pushConstant(info().getConst(pc));

      case JSOP_STRING:
        return pushConstant(StringValue(info().getAtom(pc)));

      case JSOP_ZERO:
        return pushConstant(Int32Value(0));

      case JSOP_ONE:
        return pushConstant(Int32Value(1));

      case JSOP_NULL:
        return pushConstant(NullValue());

      case JSOP_VOID:
        current->pop();
        return pushConstant(UndefinedValue());

      case JSOP_HOLE:
        return pushConstant(MagicValue(JS_ELEMENTS_HOLE));

      case JSOP_FALSE:
        return pushConstant(BooleanValue(false));

      case JSOP_TRUE:
        return pushConstant(BooleanValue(true));

      case JSOP_ARGUMENTS:
        return jsop_arguments();

      case JSOP_RUNONCE:
        return jsop_runonce();

      case JSOP_REST:
        return jsop_rest();

      case JSOP_NOTEARG:
        return jsop_notearg();

      case JSOP_GETARG:
      case JSOP_CALLARG:
        if (info().argsObjAliasesFormals()) {
            MGetArgumentsObjectArg *getArg = MGetArgumentsObjectArg::New(current->argumentsObject(),
                                                                         GET_SLOTNO(pc));
            current->add(getArg);
            current->push(getArg);
        } else {
            current->pushArg(GET_SLOTNO(pc));
        }
        return true;

      case JSOP_SETARG:
        return jsop_setarg(GET_SLOTNO(pc));

      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL:
        current->pushLocal(GET_SLOTNO(pc));
        return true;

      case JSOP_SETLOCAL:
        current->setLocal(GET_SLOTNO(pc));
        return true;

      case JSOP_POP:
        current->pop();

        // POP opcodes frequently appear where values are killed, e.g. after
        // SET* opcodes. Place a resume point afterwards to avoid capturing
        // the dead value in later snapshots, except in places where that
        // resume point is obviously unnecessary.
        if (pc[JSOP_POP_LENGTH] == JSOP_POP)
            return true;
        return maybeInsertResume();

      case JSOP_POPN:
        for (uint32_t i = 0, n = GET_UINT16(pc); i < n; i++)
            current->pop();
        return true;

      case JSOP_NEWINIT:
        if (GET_UINT8(pc) == JSProto_Array)
            return jsop_newarray(0);
        return jsop_newobject();

      case JSOP_NEWARRAY:
        return jsop_newarray(GET_UINT24(pc));

      case JSOP_NEWOBJECT:
        return jsop_newobject();

      case JSOP_INITELEM:
        return jsop_initelem();

      case JSOP_INITELEM_ARRAY:
        return jsop_initelem_array();

      case JSOP_INITPROP:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_initprop(name);
      }

      case JSOP_INITPROP_GETTER:
      case JSOP_INITPROP_SETTER: {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_initprop_getter_setter(name);
      }

      case JSOP_INITELEM_GETTER:
      case JSOP_INITELEM_SETTER:
        return jsop_initelem_getter_setter();

      case JSOP_ENDINIT:
        return true;

      case JSOP_FUNCALL:
        return jsop_funcall(GET_ARGC(pc));

      case JSOP_FUNAPPLY:
        return jsop_funapply(GET_ARGC(pc));

      case JSOP_CALL:
      case JSOP_NEW:
        return jsop_call(GET_ARGC(pc), (JSOp)*pc == JSOP_NEW);

      case JSOP_EVAL:
        return jsop_eval(GET_ARGC(pc));

      case JSOP_INT8:
        return pushConstant(Int32Value(GET_INT8(pc)));

      case JSOP_UINT16:
        return pushConstant(Int32Value(GET_UINT16(pc)));

      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        JSObject *obj = &script()->global();
        bool succeeded;
        if (!getStaticName(obj, name, &succeeded))
            return false;
        return succeeded || jsop_getname(name);
      }

      case JSOP_BINDGNAME:
        return pushConstant(ObjectValue(script()->global()));

      case JSOP_SETGNAME:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        JSObject *obj = &script()->global();
        return setStaticName(obj, name);
      }

      case JSOP_NAME:
      case JSOP_CALLNAME:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_getname(name);
      }

      case JSOP_GETINTRINSIC:
      case JSOP_CALLINTRINSIC:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_intrinsic(name);
      }

      case JSOP_BINDNAME:
        return jsop_bindname(info().getName(pc));

      case JSOP_DUP:
        current->pushSlot(current->stackDepth() - 1);
        return true;

      case JSOP_DUP2:
        return jsop_dup2();

      case JSOP_SWAP:
        current->swapAt(-1);
        return true;

      case JSOP_PICK:
        current->pick(-GET_INT8(pc));
        return true;

      case JSOP_GETALIASEDVAR:
      case JSOP_CALLALIASEDVAR:
        return jsop_getaliasedvar(ScopeCoordinate(pc));

      case JSOP_SETALIASEDVAR:
        return jsop_setaliasedvar(ScopeCoordinate(pc));

      case JSOP_UINT24:
        return pushConstant(Int32Value(GET_UINT24(pc)));

      case JSOP_INT32:
        return pushConstant(Int32Value(GET_INT32(pc)));

      case JSOP_LOOPHEAD:
        // JSOP_LOOPHEAD is handled when processing the loop header.
        MOZ_ASSUME_UNREACHABLE("JSOP_LOOPHEAD outside loop");

      case JSOP_GETELEM:
      case JSOP_CALLELEM:
        return jsop_getelem();

      case JSOP_SETELEM:
        return jsop_setelem();

      case JSOP_LENGTH:
        return jsop_length();

      case JSOP_NOT:
        return jsop_not();

      case JSOP_THIS:
        return jsop_this();

      case JSOP_CALLEE:
      {
        MDefinition *callee;
        if (inliningDepth_ == 0) {
            MInstruction *calleeIns = MCallee::New();
            current->add(calleeIns);
            callee = calleeIns;
        } else {
            callee = inlineCallInfo_->fun();
        }
        current->push(callee);
        return true;
      }

      case JSOP_GETPROP:
      case JSOP_CALLPROP:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_getprop(name);
      }

      case JSOP_SETPROP:
      case JSOP_SETNAME:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_setprop(name);
      }

      case JSOP_DELPROP:
      {
        PropertyName *name = info().getAtom(pc)->asPropertyName();
        return jsop_delprop(name);
      }

      case JSOP_DELELEM:
        return jsop_delelem();

      case JSOP_REGEXP:
        return jsop_regexp(info().getRegExp(pc));

      case JSOP_OBJECT:
        return jsop_object(info().getObject(pc));

      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        return jsop_typeof();

      case JSOP_TOID:
        return jsop_toid();

      case JSOP_LAMBDA:
        return jsop_lambda(info().getFunction(pc));

      case JSOP_ITER:
        return jsop_iter(GET_INT8(pc));

      case JSOP_ITERNEXT:
        return jsop_iternext();

      case JSOP_MOREITER:
        return jsop_itermore();

      case JSOP_ENDITER:
        return jsop_iterend();

      case JSOP_IN:
        return jsop_in();

      case JSOP_SETRVAL:
      case JSOP_POPV:
        JS_ASSERT(!script()->noScriptRval);
        current->setSlot(info().returnValueSlot(), current->pop());
        return true;

      case JSOP_INSTANCEOF:
        return jsop_instanceof();

      default:
#ifdef DEBUG
        return abort("Unsupported opcode: %s (line %d)", js_CodeName[op], info().lineno(pc));
#else
        return abort("Unsupported opcode: %d (line %d)", op, info().lineno(pc));
#endif
    }
}

// Given that the current control flow structure has ended forcefully,
// via a return, break, or continue (rather than joining), propagate the
// termination up. For example, a return nested 5 loops deep may terminate
// every outer loop at once, if there are no intervening conditionals:
//
// for (...) {
//   for (...) {
//     return x;
//   }
// }
//
// If |current| is nullptr when this function returns, then there is no more
// control flow to be processed.
IonBuilder::ControlStatus
IonBuilder::processControlEnd()
{
    JS_ASSERT(!current);

    if (cfgStack_.empty()) {
        // If there is no more control flow to process, then this is the
        // last return in the function.
        return ControlStatus_Ended;
    }

    return processCfgStack();
}

// Processes the top of the CFG stack. This is used from two places:
// (1) processControlEnd(), whereby a break, continue, or return may interrupt
//     an in-progress CFG structure before reaching its actual termination
//     point in the bytecode.
// (2) traverseBytecode(), whereby we reach the last instruction in a CFG
//     structure.
IonBuilder::ControlStatus
IonBuilder::processCfgStack()
{
    ControlStatus status = processCfgEntry(cfgStack_.back());

    // If this terminated a CFG structure, act like processControlEnd() and
    // keep propagating upward.
    while (status == ControlStatus_Ended) {
        popCfgStack();
        if (cfgStack_.empty())
            return status;
        status = processCfgEntry(cfgStack_.back());
    }

    // If some join took place, the current structure is finished.
    if (status == ControlStatus_Joined)
        popCfgStack();

    return status;
}

IonBuilder::ControlStatus
IonBuilder::processCfgEntry(CFGState &state)
{
    switch (state.state) {
      case CFGState::IF_TRUE:
      case CFGState::IF_TRUE_EMPTY_ELSE:
        return processIfEnd(state);

      case CFGState::IF_ELSE_TRUE:
        return processIfElseTrueEnd(state);

      case CFGState::IF_ELSE_FALSE:
        return processIfElseFalseEnd(state);

      case CFGState::DO_WHILE_LOOP_BODY:
        return processDoWhileBodyEnd(state);

      case CFGState::DO_WHILE_LOOP_COND:
        return processDoWhileCondEnd(state);

      case CFGState::WHILE_LOOP_COND:
        return processWhileCondEnd(state);

      case CFGState::WHILE_LOOP_BODY:
        return processWhileBodyEnd(state);

      case CFGState::FOR_LOOP_COND:
        return processForCondEnd(state);

      case CFGState::FOR_LOOP_BODY:
        return processForBodyEnd(state);

      case CFGState::FOR_LOOP_UPDATE:
        return processForUpdateEnd(state);

      case CFGState::TABLE_SWITCH:
        return processNextTableSwitchCase(state);

      case CFGState::COND_SWITCH_CASE:
        return processCondSwitchCase(state);

      case CFGState::COND_SWITCH_BODY:
        return processCondSwitchBody(state);

      case CFGState::AND_OR:
        return processAndOrEnd(state);

      case CFGState::LABEL:
        return processLabelEnd(state);

      case CFGState::TRY:
        return processTryEnd(state);

      default:
        MOZ_ASSUME_UNREACHABLE("unknown cfgstate");
    }
}

IonBuilder::ControlStatus
IonBuilder::processIfEnd(CFGState &state)
{
    if (current) {
        // Here, the false block is the join point. Create an edge from the
        // current block to the false block. Note that a RETURN opcode
        // could have already ended the block.
        current->end(MGoto::New(state.branch.ifFalse));

        if (!state.branch.ifFalse->addPredecessor(current))
            return ControlStatus_Error;
    }

    setCurrentAndSpecializePhis(state.branch.ifFalse);
    graph().moveBlockToEnd(current);
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processIfElseTrueEnd(CFGState &state)
{
    // We've reached the end of the true branch of an if-else. Don't
    // create an edge yet, just transition to parsing the false branch.
    state.state = CFGState::IF_ELSE_FALSE;
    state.branch.ifTrue = current;
    state.stopAt = state.branch.falseEnd;
    pc = state.branch.ifFalse->pc();
    setCurrentAndSpecializePhis(state.branch.ifFalse);
    graph().moveBlockToEnd(current);
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processIfElseFalseEnd(CFGState &state)
{
    // Update the state to have the latest block from the false path.
    state.branch.ifFalse = current;

    // To create the join node, we need an incoming edge that has not been
    // terminated yet.
    MBasicBlock *pred = state.branch.ifTrue
                        ? state.branch.ifTrue
                        : state.branch.ifFalse;
    MBasicBlock *other = (pred == state.branch.ifTrue) ? state.branch.ifFalse : state.branch.ifTrue;

    if (!pred)
        return ControlStatus_Ended;

    // Create a new block to represent the join.
    MBasicBlock *join = newBlock(pred, state.branch.falseEnd);
    if (!join)
        return ControlStatus_Error;

    // Create edges from the true and false blocks as needed.
    pred->end(MGoto::New(join));

    if (other) {
        other->end(MGoto::New(join));
        if (!join->addPredecessor(other))
            return ControlStatus_Error;
    }

    // Ignore unreachable remainder of false block if existent.
    setCurrentAndSpecializePhis(join);
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processBrokenLoop(CFGState &state)
{
    JS_ASSERT(!current);

    JS_ASSERT(loopDepth_);
    loopDepth_--;

    // A broken loop is not a real loop (it has no header or backedge), so
    // reset the loop depth.
    for (MBasicBlockIterator i(graph().begin(state.loop.entry)); i != graph().end(); i++) {
        if (i->loopDepth() > loopDepth_)
            i->setLoopDepth(i->loopDepth() - 1);
    }

    // If the loop started with a condition (while/for) then even if the
    // structure never actually loops, the condition itself can still fail and
    // thus we must resume at the successor, if one exists.
    setCurrentAndSpecializePhis(state.loop.successor);
    if (current) {
        JS_ASSERT(current->loopDepth() == loopDepth_);
        graph().moveBlockToEnd(current);
    }

    // Join the breaks together and continue parsing.
    if (state.loop.breaks) {
        MBasicBlock *block = createBreakCatchBlock(state.loop.breaks, state.loop.exitpc);
        if (!block)
            return ControlStatus_Error;

        if (current) {
            current->end(MGoto::New(block));
            if (!block->addPredecessor(current))
                return ControlStatus_Error;
        }

        setCurrentAndSpecializePhis(block);
    }

    // If the loop is not gated on a condition, and has only returns, we'll
    // reach this case. For example:
    // do { ... return; } while ();
    if (!current)
        return ControlStatus_Ended;

    // Otherwise, the loop is gated on a condition and/or has breaks so keep
    // parsing at the successor.
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::finishLoop(CFGState &state, MBasicBlock *successor)
{
    JS_ASSERT(current);

    JS_ASSERT(loopDepth_);
    loopDepth_--;
    JS_ASSERT_IF(successor, successor->loopDepth() == loopDepth_);

    // Compute phis in the loop header and propagate them throughout the loop,
    // including the successor.
    AbortReason r = state.loop.entry->setBackedge(current);
    if (r == AbortReason_Alloc)
        return ControlStatus_Error;
    if (r == AbortReason_Disable) {
        // If there are types for variables on the backedge that were not
        // present at the original loop header, then uses of the variables'
        // phis may have generated incorrect nodes. The new types have been
        // incorporated into the header phis, so remove all blocks for the
        // loop body and restart with the new types.
        return restartLoop(state);
    }

    if (successor) {
        graph().moveBlockToEnd(successor);
        successor->inheritPhis(state.loop.entry);
    }

    if (state.loop.breaks) {
        // Propagate phis placed in the header to individual break exit points.
        DeferredEdge *edge = state.loop.breaks;
        while (edge) {
            edge->block->inheritPhis(state.loop.entry);
            edge = edge->next;
        }

        // Create a catch block to join all break exits.
        MBasicBlock *block = createBreakCatchBlock(state.loop.breaks, state.loop.exitpc);
        if (!block)
            return ControlStatus_Error;

        if (successor) {
            // Finally, create an unconditional edge from the successor to the
            // catch block.
            successor->end(MGoto::New(block));
            if (!block->addPredecessor(successor))
                return ControlStatus_Error;
        }
        successor = block;
    }

    setCurrentAndSpecializePhis(successor);

    // An infinite loop (for (;;) { }) will not have a successor.
    if (!current)
        return ControlStatus_Ended;

    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::restartLoop(CFGState state)
{
    spew("New types at loop header, restarting loop body");

    if (js_IonOptions.limitScriptSize) {
        if (++numLoopRestarts_ >= MAX_LOOP_RESTARTS)
            return ControlStatus_Abort;
    }

    MBasicBlock *header = state.loop.entry;

    // Remove all blocks in the loop body other than the header, which has phis
    // of the appropriate type and incoming edges to preserve.
    graph().removeBlocksAfter(header);

    // Remove all instructions from the header itself, and all resume points
    // except the entry resume point.
    header->discardAllInstructions();
    header->discardAllResumePoints(/* discardEntry = */ false);
    header->setStackDepth(header->getPredecessor(0)->stackDepth());

    popCfgStack();

    loopDepth_++;

    if (!pushLoop(state.loop.initialState, state.loop.initialStopAt, header, state.loop.osr,
                  state.loop.loopHead, state.loop.initialPc,
                  state.loop.bodyStart, state.loop.bodyEnd,
                  state.loop.exitpc, state.loop.continuepc))
    {
        return ControlStatus_Error;
    }

    CFGState &nstate = cfgStack_.back();

    nstate.loop.condpc = state.loop.condpc;
    nstate.loop.updatepc = state.loop.updatepc;
    nstate.loop.updateEnd = state.loop.updateEnd;

    // Don't specializePhis(), as the header has been visited before and the
    // phis have already had their type set.
    setCurrent(header);

    if (!jsop_loophead(nstate.loop.loopHead))
        return ControlStatus_Error;

    pc = nstate.loop.initialPc;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processDoWhileBodyEnd(CFGState &state)
{
    if (!processDeferredContinues(state))
        return ControlStatus_Error;

    // No current means control flow cannot reach the condition, so this will
    // never loop.
    if (!current)
        return processBrokenLoop(state);

    MBasicBlock *header = newBlock(current, state.loop.updatepc);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    state.state = CFGState::DO_WHILE_LOOP_COND;
    state.stopAt = state.loop.updateEnd;
    pc = state.loop.updatepc;
    setCurrentAndSpecializePhis(header);
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processDoWhileCondEnd(CFGState &state)
{
    JS_ASSERT(JSOp(*pc) == JSOP_IFNE);

    // We're guaranteed a |current|, it's impossible to break or return from
    // inside the conditional expression.
    JS_ASSERT(current);

    // Pop the last value, and create the successor block.
    MDefinition *vins = current->pop();
    MBasicBlock *successor = newBlock(current, GetNextPc(pc), loopDepth_ - 1);
    if (!successor)
        return ControlStatus_Error;

    // Create the test instruction and end the current block.
    MTest *test = MTest::New(vins, state.loop.entry, successor);
    current->end(test);
    return finishLoop(state, successor);
}

IonBuilder::ControlStatus
IonBuilder::processWhileCondEnd(CFGState &state)
{
    JS_ASSERT(JSOp(*pc) == JSOP_IFNE || JSOp(*pc) == JSOP_IFEQ);

    // Balance the stack past the IFNE.
    MDefinition *ins = current->pop();

    // Create the body and successor blocks.
    MBasicBlock *body = newBlock(current, state.loop.bodyStart);
    state.loop.successor = newBlock(current, state.loop.exitpc, loopDepth_ - 1);
    if (!body || !state.loop.successor)
        return ControlStatus_Error;

    MTest *test;
    if (JSOp(*pc) == JSOP_IFNE)
        test = MTest::New(ins, body, state.loop.successor);
    else
        test = MTest::New(ins, state.loop.successor, body);
    current->end(test);

    state.state = CFGState::WHILE_LOOP_BODY;
    state.stopAt = state.loop.bodyEnd;
    pc = state.loop.bodyStart;
    setCurrentAndSpecializePhis(body);
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processWhileBodyEnd(CFGState &state)
{
    if (!processDeferredContinues(state))
        return ControlStatus_Error;

    if (!current)
        return processBrokenLoop(state);

    current->end(MGoto::New(state.loop.entry));
    return finishLoop(state, state.loop.successor);
}

IonBuilder::ControlStatus
IonBuilder::processForCondEnd(CFGState &state)
{
    JS_ASSERT(JSOp(*pc) == JSOP_IFNE);

    // Balance the stack past the IFNE.
    MDefinition *ins = current->pop();

    // Create the body and successor blocks.
    MBasicBlock *body = newBlock(current, state.loop.bodyStart);
    state.loop.successor = newBlock(current, state.loop.exitpc, loopDepth_ - 1);
    if (!body || !state.loop.successor)
        return ControlStatus_Error;

    MTest *test = MTest::New(ins, body, state.loop.successor);
    current->end(test);

    state.state = CFGState::FOR_LOOP_BODY;
    state.stopAt = state.loop.bodyEnd;
    pc = state.loop.bodyStart;
    setCurrentAndSpecializePhis(body);
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processForBodyEnd(CFGState &state)
{
    if (!processDeferredContinues(state))
        return ControlStatus_Error;

    // If there is no updatepc, just go right to processing what would be the
    // end of the update clause. Otherwise, |current| might be nullptr; if this is
    // the case, the udpate is unreachable anyway.
    if (!state.loop.updatepc || !current)
        return processForUpdateEnd(state);

    pc = state.loop.updatepc;

    state.state = CFGState::FOR_LOOP_UPDATE;
    state.stopAt = state.loop.updateEnd;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processForUpdateEnd(CFGState &state)
{
    // If there is no current, we couldn't reach the loop edge and there was no
    // update clause.
    if (!current)
        return processBrokenLoop(state);

    current->end(MGoto::New(state.loop.entry));
    return finishLoop(state, state.loop.successor);
}

IonBuilder::DeferredEdge *
IonBuilder::filterDeadDeferredEdges(DeferredEdge *edge)
{
    DeferredEdge *head = edge, *prev = nullptr;

    while (edge) {
        if (edge->block->isDead()) {
            if (prev)
                prev->next = edge->next;
            else
                head = edge->next;
        } else {
            prev = edge;
        }
        edge = edge->next;
    }

    // There must be at least one deferred edge from a block that was not
    // deleted; blocks are deleted when restarting processing of a loop, and
    // the final version of the loop body will have edges from live blocks.
    JS_ASSERT(head);

    return head;
}

bool
IonBuilder::processDeferredContinues(CFGState &state)
{
    // If there are any continues for this loop, and there is an update block,
    // then we need to create a new basic block to house the update.
    if (state.loop.continues) {
        DeferredEdge *edge = filterDeadDeferredEdges(state.loop.continues);

        MBasicBlock *update = newBlock(edge->block, loops_.back().continuepc);
        if (!update)
            return false;

        if (current) {
            current->end(MGoto::New(update));
            if (!update->addPredecessor(current))
                return ControlStatus_Error;
        }

        // No need to use addPredecessor for first edge,
        // because it is already predecessor.
        edge->block->end(MGoto::New(update));
        edge = edge->next;

        // Remaining edges
        while (edge) {
            edge->block->end(MGoto::New(update));
            if (!update->addPredecessor(edge->block))
                return ControlStatus_Error;
            edge = edge->next;
        }
        state.loop.continues = nullptr;

        setCurrentAndSpecializePhis(update);
    }

    return true;
}

MBasicBlock *
IonBuilder::createBreakCatchBlock(DeferredEdge *edge, jsbytecode *pc)
{
    edge = filterDeadDeferredEdges(edge);

    // Create block, using the first break statement as predecessor
    MBasicBlock *successor = newBlock(edge->block, pc);
    if (!successor)
        return nullptr;

    // No need to use addPredecessor for first edge,
    // because it is already predecessor.
    edge->block->end(MGoto::New(successor));
    edge = edge->next;

    // Finish up remaining breaks.
    while (edge) {
        edge->block->end(MGoto::New(successor));
        if (!successor->addPredecessor(edge->block))
            return nullptr;
        edge = edge->next;
    }

    return successor;
}

IonBuilder::ControlStatus
IonBuilder::processNextTableSwitchCase(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::TABLE_SWITCH);

    state.tableswitch.currentBlock++;

    // Test if there are still unprocessed successors (cases/default)
    if (state.tableswitch.currentBlock >= state.tableswitch.ins->numBlocks())
        return processSwitchEnd(state.tableswitch.breaks, state.tableswitch.exitpc);

    // Get the next successor
    MBasicBlock *successor = state.tableswitch.ins->getBlock(state.tableswitch.currentBlock);

    // Add current block as predecessor if available.
    // This means the previous case didn't have a break statement.
    // So flow will continue in this block.
    if (current) {
        current->end(MGoto::New(successor));
        successor->addPredecessor(current);
    }

    // Insert successor after the current block, to maintain RPO.
    graph().moveBlockToEnd(successor);

    // If this is the last successor the block should stop at the end of the tableswitch
    // Else it should stop at the start of the next successor
    if (state.tableswitch.currentBlock+1 < state.tableswitch.ins->numBlocks())
        state.stopAt = state.tableswitch.ins->getBlock(state.tableswitch.currentBlock+1)->pc();
    else
        state.stopAt = state.tableswitch.exitpc;

    setCurrentAndSpecializePhis(successor);
    pc = current->pc();
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processAndOrEnd(CFGState &state)
{
    // We just processed the RHS of an && or || expression.
    // Now jump to the join point (the false block).
    current->end(MGoto::New(state.branch.ifFalse));

    if (!state.branch.ifFalse->addPredecessor(current))
        return ControlStatus_Error;

    setCurrentAndSpecializePhis(state.branch.ifFalse);
    graph().moveBlockToEnd(current);
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processLabelEnd(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::LABEL);

    // If there are no breaks and no current, controlflow is terminated.
    if (!state.label.breaks && !current)
        return ControlStatus_Ended;

    // If there are no breaks to this label, there's nothing to do.
    if (!state.label.breaks)
        return ControlStatus_Joined;

    MBasicBlock *successor = createBreakCatchBlock(state.label.breaks, state.stopAt);
    if (!successor)
        return ControlStatus_Error;

    if (current) {
        current->end(MGoto::New(successor));
        successor->addPredecessor(current);
    }

    pc = state.stopAt;
    setCurrentAndSpecializePhis(successor);
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processTryEnd(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::TRY);

    if (!state.try_.successor) {
        JS_ASSERT(!current);
        return ControlStatus_Ended;
    }

    if (current) {
        current->end(MGoto::New(state.try_.successor));

        if (!state.try_.successor->addPredecessor(current))
            return ControlStatus_Error;
    }

    // Start parsing the code after this try-catch statement.
    setCurrentAndSpecializePhis(state.try_.successor);
    graph().moveBlockToEnd(current);
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processBreak(JSOp op, jssrcnote *sn)
{
    JS_ASSERT(op == JSOP_GOTO);

    JS_ASSERT(SN_TYPE(sn) == SRC_BREAK ||
              SN_TYPE(sn) == SRC_BREAK2LABEL);

    // Find the break target.
    jsbytecode *target = pc + GetJumpOffset(pc);
    DebugOnly<bool> found = false;

    if (SN_TYPE(sn) == SRC_BREAK2LABEL) {
        for (size_t i = labels_.length() - 1; i < labels_.length(); i--) {
            CFGState &cfg = cfgStack_[labels_[i].cfgEntry];
            JS_ASSERT(cfg.state == CFGState::LABEL);
            if (cfg.stopAt == target) {
                cfg.label.breaks = new DeferredEdge(current, cfg.label.breaks);
                found = true;
                break;
            }
        }
    } else {
        for (size_t i = loops_.length() - 1; i < loops_.length(); i--) {
            CFGState &cfg = cfgStack_[loops_[i].cfgEntry];
            JS_ASSERT(cfg.isLoop());
            if (cfg.loop.exitpc == target) {
                cfg.loop.breaks = new DeferredEdge(current, cfg.loop.breaks);
                found = true;
                break;
            }
        }
    }

    JS_ASSERT(found);

    setCurrent(nullptr);
    pc += js_CodeSpec[op].length;
    return processControlEnd();
}

static inline jsbytecode *
EffectiveContinue(jsbytecode *pc)
{
    if (JSOp(*pc) == JSOP_GOTO)
        return pc + GetJumpOffset(pc);
    return pc;
}

IonBuilder::ControlStatus
IonBuilder::processContinue(JSOp op)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target loop.
    CFGState *found = nullptr;
    jsbytecode *target = pc + GetJumpOffset(pc);
    for (size_t i = loops_.length() - 1; i < loops_.length(); i--) {
        if (loops_[i].continuepc == target ||
            EffectiveContinue(loops_[i].continuepc) == target)
        {
            found = &cfgStack_[loops_[i].cfgEntry];
            break;
        }
    }

    // There must always be a valid target loop structure. If not, there's
    // probably an off-by-something error in which pc we track.
    JS_ASSERT(found);
    CFGState &state = *found;

    state.loop.continues = new DeferredEdge(current, state.loop.continues);

    setCurrent(nullptr);
    pc += js_CodeSpec[op].length;
    return processControlEnd();
}

IonBuilder::ControlStatus
IonBuilder::processSwitchBreak(JSOp op)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target switch.
    CFGState *found = nullptr;
    jsbytecode *target = pc + GetJumpOffset(pc);
    for (size_t i = switches_.length() - 1; i < switches_.length(); i--) {
        if (switches_[i].continuepc == target) {
            found = &cfgStack_[switches_[i].cfgEntry];
            break;
        }
    }

    // There must always be a valid target loop structure. If not, there's
    // probably an off-by-something error in which pc we track.
    JS_ASSERT(found);
    CFGState &state = *found;

    DeferredEdge **breaks = nullptr;
    switch (state.state) {
      case CFGState::TABLE_SWITCH:
        breaks = &state.tableswitch.breaks;
        break;
      case CFGState::COND_SWITCH_BODY:
        breaks = &state.condswitch.breaks;
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected switch state.");
    }

    *breaks = new DeferredEdge(current, *breaks);

    setCurrent(nullptr);
    pc += js_CodeSpec[op].length;
    return processControlEnd();
}

IonBuilder::ControlStatus
IonBuilder::processSwitchEnd(DeferredEdge *breaks, jsbytecode *exitpc)
{
    // No break statements, no current.
    // This means that control flow is cut-off from this point
    // (e.g. all cases have return statements).
    if (!breaks && !current)
        return ControlStatus_Ended;

    // Create successor block.
    // If there are breaks, create block with breaks as predecessor
    // Else create a block with current as predecessor
    MBasicBlock *successor = nullptr;
    if (breaks)
        successor = createBreakCatchBlock(breaks, exitpc);
    else
        successor = newBlock(current, exitpc);

    if (!successor)
        return ControlStatus_Ended;

    // If there is current, the current block flows into this one.
    // So current is also a predecessor to this block
    if (current) {
        current->end(MGoto::New(successor));
        if (breaks)
            successor->addPredecessor(current);
    }

    pc = exitpc;
    setCurrentAndSpecializePhis(successor);
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::maybeLoop(JSOp op, jssrcnote *sn)
{
    // This function looks at the opcode and source note and tries to
    // determine the structure of the loop. For some opcodes, like
    // POP/NOP which are not explicitly control flow, this source note is
    // optional. For opcodes with control flow, like GOTO, an unrecognized
    // or not-present source note is a compilation failure.
    switch (op) {
      case JSOP_POP:
        // for (init; ; update?) ...
        if (sn && SN_TYPE(sn) == SRC_FOR) {
            current->pop();
            return forLoop(op, sn);
        }
        break;

      case JSOP_NOP:
        if (sn) {
            // do { } while (cond)
            if (SN_TYPE(sn) == SRC_WHILE)
                return doWhileLoop(op, sn);
            // Build a mapping such that given a basic block, whose successor
            // has a phi

            // for (; ; update?)
            if (SN_TYPE(sn) == SRC_FOR)
                return forLoop(op, sn);
        }
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
    }

    return ControlStatus_None;
}

void
IonBuilder::assertValidLoopHeadOp(jsbytecode *pc)
{
#ifdef DEBUG
    JS_ASSERT(JSOp(*pc) == JSOP_LOOPHEAD);

    // Make sure this is the next opcode after the loop header,
    // unless the for loop is unconditional.
    CFGState &state = cfgStack_.back();
    JS_ASSERT_IF((JSOp)*(state.loop.entry->pc()) == JSOP_GOTO,
        GetNextPc(state.loop.entry->pc()) == pc);

    // do-while loops have a source note.
    jssrcnote *sn = info().getNote(gsn, pc);
    if (sn) {
        jsbytecode *ifne = pc + js_GetSrcNoteOffset(sn, 0);

        jsbytecode *expected_ifne;
        switch (state.state) {
          case CFGState::DO_WHILE_LOOP_BODY:
            expected_ifne = state.loop.updateEnd;
            break;

          default:
            MOZ_ASSUME_UNREACHABLE("JSOP_LOOPHEAD unexpected source note");
        }

        // Make sure this loop goes to the same ifne as the loop header's
        // source notes or GOTO.
        JS_ASSERT(ifne == expected_ifne);
    } else {
        JS_ASSERT(state.state != CFGState::DO_WHILE_LOOP_BODY);
    }
#endif
}

IonBuilder::ControlStatus
IonBuilder::doWhileLoop(JSOp op, jssrcnote *sn)
{
    // do { } while() loops have the following structure:
    //    NOP         ; SRC_WHILE (offset to COND)
    //    LOOPHEAD    ; SRC_WHILE (offset to IFNE)
    //    LOOPENTRY
    //    ...         ; body
    //    ...
    //    COND        ; start of condition
    //    ...
    //    IFNE ->     ; goes to LOOPHEAD
    int condition_offset = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *conditionpc = pc + condition_offset;

    jssrcnote *sn2 = info().getNote(gsn, pc+1);
    int offset = js_GetSrcNoteOffset(sn2, 0);
    jsbytecode *ifne = pc + offset + 1;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a loophead op.
    jsbytecode *loopHead = GetNextPc(pc);
    JS_ASSERT(JSOp(*loopHead) == JSOP_LOOPHEAD);
    JS_ASSERT(loopHead == ifne + GetJumpOffset(ifne));

    jsbytecode *loopEntry = GetNextPc(loopHead);
    bool osr = info().hasOsrAt(loopEntry);

    if (osr) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        setCurrentAndSpecializePhis(preheader);
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc, osr);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    jsbytecode *loophead = GetNextPc(pc);
    jsbytecode *bodyStart = GetNextPc(loophead);
    jsbytecode *bodyEnd = conditionpc;
    jsbytecode *exitpc = GetNextPc(ifne);
    analyzeNewLoopTypes(header, bodyStart, exitpc);
    if (!pushLoop(CFGState::DO_WHILE_LOOP_BODY, conditionpc, header, osr,
                  loopHead, bodyStart, bodyStart, bodyEnd, exitpc, conditionpc))
    {
        return ControlStatus_Error;
    }

    CFGState &state = cfgStack_.back();
    state.loop.updatepc = conditionpc;
    state.loop.updateEnd = ifne;

    setCurrentAndSpecializePhis(header);
    if (!jsop_loophead(loophead))
        return ControlStatus_Error;

    pc = bodyStart;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::whileOrForInLoop(jssrcnote *sn)
{
    // while (cond) { } loops have the following structure:
    //    GOTO cond   ; SRC_WHILE (offset to IFNE)
    //    LOOPHEAD
    //    ...
    //  cond:
    //    LOOPENTRY
    //    ...
    //    IFNE        ; goes to LOOPHEAD
    // for (x in y) { } loops are similar; the cond will be a MOREITER.
    JS_ASSERT(SN_TYPE(sn) == SRC_FOR_OF || SN_TYPE(sn) == SRC_FOR_IN || SN_TYPE(sn) == SRC_WHILE);
    int ifneOffset = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *ifne = pc + ifneOffset;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a loophead op.
    JS_ASSERT(JSOp(*GetNextPc(pc)) == JSOP_LOOPHEAD);
    JS_ASSERT(GetNextPc(pc) == ifne + GetJumpOffset(ifne));

    jsbytecode *loopEntry = pc + GetJumpOffset(pc);
    bool osr = info().hasOsrAt(loopEntry);

    if (osr) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        setCurrentAndSpecializePhis(preheader);
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc, osr);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    // Skip past the JSOP_LOOPHEAD for the body start.
    jsbytecode *loopHead = GetNextPc(pc);
    jsbytecode *bodyStart = GetNextPc(loopHead);
    jsbytecode *bodyEnd = pc + GetJumpOffset(pc);
    jsbytecode *exitpc = GetNextPc(ifne);
    analyzeNewLoopTypes(header, bodyStart, exitpc);
    if (!pushLoop(CFGState::WHILE_LOOP_COND, ifne, header, osr,
                  loopHead, bodyEnd, bodyStart, bodyEnd, exitpc))
    {
        return ControlStatus_Error;
    }

    // Parse the condition first.
    setCurrentAndSpecializePhis(header);
    if (!jsop_loophead(loopHead))
        return ControlStatus_Error;

    pc = bodyEnd;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::forLoop(JSOp op, jssrcnote *sn)
{
    // Skip the NOP or POP.
    JS_ASSERT(op == JSOP_POP || op == JSOP_NOP);
    pc = GetNextPc(pc);

    jsbytecode *condpc = pc + js_GetSrcNoteOffset(sn, 0);
    jsbytecode *updatepc = pc + js_GetSrcNoteOffset(sn, 1);
    jsbytecode *ifne = pc + js_GetSrcNoteOffset(sn, 2);
    jsbytecode *exitpc = GetNextPc(ifne);

    // for loops have the following structures:
    //
    //   NOP or POP
    //   [GOTO cond | NOP]
    //   LOOPHEAD
    // body:
    //    ; [body]
    // [increment:]
    //    ; [increment]
    // [cond:]
    //   LOOPENTRY
    //   GOTO body
    //
    // If there is a condition (condpc != ifne), this acts similar to a while
    // loop otherwise, it acts like a do-while loop.
    jsbytecode *bodyStart = pc;
    jsbytecode *bodyEnd = updatepc;
    jsbytecode *loopEntry = condpc;
    if (condpc != ifne) {
        JS_ASSERT(JSOp(*bodyStart) == JSOP_GOTO);
        JS_ASSERT(bodyStart + GetJumpOffset(bodyStart) == condpc);
        bodyStart = GetNextPc(bodyStart);
    } else {
        // No loop condition, such as for(j = 0; ; j++)
        if (op != JSOP_NOP) {
            // If the loop starts with POP, we have to skip a NOP.
            JS_ASSERT(JSOp(*bodyStart) == JSOP_NOP);
            bodyStart = GetNextPc(bodyStart);
        }
        loopEntry = GetNextPc(bodyStart);
    }
    jsbytecode *loopHead = bodyStart;
    JS_ASSERT(JSOp(*bodyStart) == JSOP_LOOPHEAD);
    JS_ASSERT(ifne + GetJumpOffset(ifne) == bodyStart);
    bodyStart = GetNextPc(bodyStart);

    bool osr = info().hasOsrAt(loopEntry);

    if (osr) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        setCurrentAndSpecializePhis(preheader);
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc, osr);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    // If there is no condition, we immediately parse the body. Otherwise, we
    // parse the condition.
    jsbytecode *stopAt;
    CFGState::State initial;
    if (condpc != ifne) {
        pc = condpc;
        stopAt = ifne;
        initial = CFGState::FOR_LOOP_COND;
    } else {
        pc = bodyStart;
        stopAt = bodyEnd;
        initial = CFGState::FOR_LOOP_BODY;
    }

    analyzeNewLoopTypes(header, bodyStart, exitpc);
    if (!pushLoop(initial, stopAt, header, osr,
                  loopHead, pc, bodyStart, bodyEnd, exitpc, updatepc))
    {
        return ControlStatus_Error;
    }

    CFGState &state = cfgStack_.back();
    state.loop.condpc = (condpc != ifne) ? condpc : nullptr;
    state.loop.updatepc = (updatepc != condpc) ? updatepc : nullptr;
    if (state.loop.updatepc)
        state.loop.updateEnd = condpc;

    setCurrentAndSpecializePhis(header);
    if (!jsop_loophead(loopHead))
        return ControlStatus_Error;

    return ControlStatus_Jumped;
}

int
IonBuilder::CmpSuccessors(const void *a, const void *b)
{
    const MBasicBlock *a0 = * (MBasicBlock * const *)a;
    const MBasicBlock *b0 = * (MBasicBlock * const *)b;
    if (a0->pc() == b0->pc())
        return 0;

    return (a0->pc() > b0->pc()) ? 1 : -1;
}

IonBuilder::ControlStatus
IonBuilder::tableSwitch(JSOp op, jssrcnote *sn)
{
    // TableSwitch op contains the following data
    // (length between data is JUMP_OFFSET_LEN)
    //
    // 0: Offset of default case
    // 1: Lowest number in tableswitch
    // 2: Highest number in tableswitch
    // 3: Offset of case low
    // 4: Offset of case low+1
    // .: ...
    // .: Offset of case high

    JS_ASSERT(op == JSOP_TABLESWITCH);
    JS_ASSERT(SN_TYPE(sn) == SRC_TABLESWITCH);

    // Pop input.
    MDefinition *ins = current->pop();

    // Get the default and exit pc
    jsbytecode *exitpc = pc + js_GetSrcNoteOffset(sn, 0);
    jsbytecode *defaultpc = pc + GET_JUMP_OFFSET(pc);

    JS_ASSERT(defaultpc > pc && defaultpc <= exitpc);

    // Get the low and high from the tableswitch
    jsbytecode *pc2 = pc;
    pc2 += JUMP_OFFSET_LEN;
    int low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    int high = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;

    // Create MIR instruction
    MTableSwitch *tableswitch = MTableSwitch::New(ins, low, high);

    // Create default case
    MBasicBlock *defaultcase = newBlock(current, defaultpc);
    if (!defaultcase)
        return ControlStatus_Error;
    tableswitch->addDefault(defaultcase);
    tableswitch->addBlock(defaultcase);

    // Create cases
    jsbytecode *casepc = nullptr;
    for (int i = 0; i < high-low+1; i++) {
        casepc = pc + GET_JUMP_OFFSET(pc2);

        JS_ASSERT(casepc >= pc && casepc <= exitpc);

        MBasicBlock *caseblock = newBlock(current, casepc);
        if (!caseblock)
            return ControlStatus_Error;

        // If the casepc equals the current pc, it is not a written case,
        // but a filled gap. That way we can use a tableswitch instead of
        // condswitch, even if not all numbers are consecutive.
        // In that case this block goes to the default case
        if (casepc == pc) {
            caseblock->end(MGoto::New(defaultcase));
            defaultcase->addPredecessor(caseblock);
        }

        tableswitch->addCase(tableswitch->addSuccessor(caseblock));

        // If this is an actual case (not filled gap),
        // add this block to the list that still needs to get processed
        if (casepc != pc)
            tableswitch->addBlock(caseblock);

        pc2 += JUMP_OFFSET_LEN;
    }

    // Move defaultcase to the end, to maintain RPO.
    graph().moveBlockToEnd(defaultcase);

    JS_ASSERT(tableswitch->numCases() == (uint32_t)(high - low + 1));
    JS_ASSERT(tableswitch->numSuccessors() > 0);

    // Sort the list of blocks that still needs to get processed by pc
    qsort(tableswitch->blocks(), tableswitch->numBlocks(),
          sizeof(MBasicBlock*), CmpSuccessors);

    // Create info
    ControlFlowInfo switchinfo(cfgStack_.length(), exitpc);
    if (!switches_.append(switchinfo))
        return ControlStatus_Error;

    // Use a state to retrieve some information
    CFGState state = CFGState::TableSwitch(exitpc, tableswitch);

    // Save the MIR instruction as last instruction of this block.
    current->end(tableswitch);

    // If there is only one successor the block should stop at the end of the switch
    // Else it should stop at the start of the next successor
    if (tableswitch->numBlocks() > 1)
        state.stopAt = tableswitch->getBlock(1)->pc();
    setCurrentAndSpecializePhis(tableswitch->getBlock(0));

    if (!cfgStack_.append(state))
        return ControlStatus_Error;

    pc = current->pc();
    return ControlStatus_Jumped;
}

bool
IonBuilder::jsop_label()
{
    JS_ASSERT(JSOp(*pc) == JSOP_LABEL);

    jsbytecode *endpc = pc + GET_JUMP_OFFSET(pc);
    JS_ASSERT(endpc > pc);

    ControlFlowInfo label(cfgStack_.length(), endpc);
    if (!labels_.append(label))
        return false;

    return cfgStack_.append(CFGState::Label(endpc));
}

bool
IonBuilder::jsop_condswitch()
{
    // CondSwitch op looks as follows:
    //   condswitch [length +exit_pc; first case offset +next-case ]
    //   {
    //     {
    //       ... any code ...
    //       case (+jump) [pcdelta offset +next-case]
    //     }+
    //     default (+jump)
    //     ... jump targets ...
    //   }
    //
    // The default case is always emitted even if there is no default case in
    // the source.  The last case statement pcdelta source note might have a 0
    // offset on the last case (not all the time).
    //
    // A conditional evaluate the condition of each case and compare it to the
    // switch value with a strict equality.  Cases conditions are iterated
    // linearly until one is matching. If one case succeeds, the flow jumps into
    // the corresponding body block.  The body block might alias others and
    // might continue in the next body block if the body is not terminated with
    // a break.
    //
    // Algorithm:
    //  1/ Loop over the case chain to reach the default target
    //   & Estimate the number of uniq bodies.
    //  2/ Generate code for all cases (see processCondSwitchCase).
    //  3/ Generate code for all bodies (see processCondSwitchBody).

    JS_ASSERT(JSOp(*pc) == JSOP_CONDSWITCH);
    jssrcnote *sn = info().getNote(gsn, pc);
    JS_ASSERT(SN_TYPE(sn) == SRC_CONDSWITCH);

    // Get the exit pc
    jsbytecode *exitpc = pc + js_GetSrcNoteOffset(sn, 0);
    jsbytecode *firstCase = pc + js_GetSrcNoteOffset(sn, 1);

    // Iterate all cases in the conditional switch.
    // - Stop at the default case. (always emitted after the last case)
    // - Estimate the number of uniq bodies. This estimation might be off by 1
    //   if the default body alias a case body.
    jsbytecode *curCase = firstCase;
    jsbytecode *lastTarget = GetJumpOffset(curCase) + curCase;
    size_t nbBodies = 2; // default target and the first body.

    JS_ASSERT(pc < curCase && curCase <= exitpc);
    while (JSOp(*curCase) == JSOP_CASE) {
        // Fetch the next case.
        jssrcnote *caseSn = info().getNote(gsn, curCase);
        JS_ASSERT(caseSn && SN_TYPE(caseSn) == SRC_NEXTCASE);
        ptrdiff_t off = js_GetSrcNoteOffset(caseSn, 0);
        curCase = off ? curCase + off : GetNextPc(curCase);
        JS_ASSERT(pc < curCase && curCase <= exitpc);

        // Count non-aliased cases.
        jsbytecode *curTarget = GetJumpOffset(curCase) + curCase;
        if (lastTarget < curTarget)
            nbBodies++;
        lastTarget = curTarget;
    }

    // The current case now be the default case which jump to the body of the
    // default case, which might be behind the last target.
    JS_ASSERT(JSOp(*curCase) == JSOP_DEFAULT);
    jsbytecode *defaultTarget = GetJumpOffset(curCase) + curCase;
    JS_ASSERT(curCase < defaultTarget && defaultTarget <= exitpc);

    // Allocate the current graph state.
    CFGState state = CFGState::CondSwitch(this, exitpc, defaultTarget);
    if (!state.condswitch.bodies || !state.condswitch.bodies->init(nbBodies))
        return ControlStatus_Error;

    // We loop on case conditions with processCondSwitchCase.
    JS_ASSERT(JSOp(*firstCase) == JSOP_CASE);
    state.stopAt = firstCase;
    state.state = CFGState::COND_SWITCH_CASE;

    return cfgStack_.append(state);
}

IonBuilder::CFGState
IonBuilder::CFGState::CondSwitch(IonBuilder *builder, jsbytecode *exitpc, jsbytecode *defaultTarget)
{
    CFGState state;
    state.state = COND_SWITCH_CASE;
    state.stopAt = nullptr;
    state.condswitch.bodies = (FixedList<MBasicBlock *> *)builder->temp_->allocate(
        sizeof(FixedList<MBasicBlock *>));
    state.condswitch.currentIdx = 0;
    state.condswitch.defaultTarget = defaultTarget;
    state.condswitch.defaultIdx = uint32_t(-1);
    state.condswitch.exitpc = exitpc;
    state.condswitch.breaks = nullptr;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::Label(jsbytecode *exitpc)
{
    CFGState state;
    state.state = LABEL;
    state.stopAt = exitpc;
    state.label.breaks = nullptr;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::Try(jsbytecode *exitpc, MBasicBlock *successor)
{
    CFGState state;
    state.state = TRY;
    state.stopAt = exitpc;
    state.try_.successor = successor;
    return state;
}

IonBuilder::ControlStatus
IonBuilder::processCondSwitchCase(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::COND_SWITCH_CASE);
    JS_ASSERT(!state.condswitch.breaks);
    JS_ASSERT(current);
    JS_ASSERT(JSOp(*pc) == JSOP_CASE);
    FixedList<MBasicBlock *> &bodies = *state.condswitch.bodies;
    jsbytecode *defaultTarget = state.condswitch.defaultTarget;
    uint32_t &currentIdx = state.condswitch.currentIdx;
    jsbytecode *lastTarget = currentIdx ? bodies[currentIdx - 1]->pc() : nullptr;

    // Fetch the following case in which we will continue.
    jssrcnote *sn = info().getNote(gsn, pc);
    ptrdiff_t off = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *casePc = off ? pc + off : GetNextPc(pc);
    bool caseIsDefault = JSOp(*casePc) == JSOP_DEFAULT;
    JS_ASSERT(JSOp(*casePc) == JSOP_CASE || caseIsDefault);

    // Allocate the block of the matching case.
    bool bodyIsNew = false;
    MBasicBlock *bodyBlock = nullptr;
    jsbytecode *bodyTarget = pc + GetJumpOffset(pc);
    if (lastTarget < bodyTarget) {
        // If the default body is in the middle or aliasing the current target.
        if (lastTarget < defaultTarget && defaultTarget <= bodyTarget) {
            JS_ASSERT(state.condswitch.defaultIdx == uint32_t(-1));
            state.condswitch.defaultIdx = currentIdx;
            bodies[currentIdx] = nullptr;
            // If the default body does not alias any and it would be allocated
            // later and stored in the defaultIdx location.
            if (defaultTarget < bodyTarget)
                currentIdx++;
        }

        bodyIsNew = true;
        // Pop switch and case operands.
        bodyBlock = newBlockPopN(current, bodyTarget, 2);
        bodies[currentIdx++] = bodyBlock;
    } else {
        // This body alias the previous one.
        JS_ASSERT(lastTarget == bodyTarget);
        JS_ASSERT(currentIdx > 0);
        bodyBlock = bodies[currentIdx - 1];
    }

    if (!bodyBlock)
        return ControlStatus_Error;

    lastTarget = bodyTarget;

    // Allocate the block of the non-matching case.  This can either be a normal
    // case or the default case.
    bool caseIsNew = false;
    MBasicBlock *caseBlock = nullptr;
    if (!caseIsDefault) {
        caseIsNew = true;
        // Pop the case operand.
        caseBlock = newBlockPopN(current, GetNextPc(pc), 1);
    } else {
        // The non-matching case is the default case, which jump directly to its
        // body. Skip the creation of a default case block and directly create
        // the default body if it does not alias any previous body.

        if (state.condswitch.defaultIdx == uint32_t(-1)) {
            // The default target is the last target.
            JS_ASSERT(lastTarget < defaultTarget);
            state.condswitch.defaultIdx = currentIdx++;
            caseIsNew = true;
        } else if (bodies[state.condswitch.defaultIdx] == nullptr) {
            // The default target is in the middle and it does not alias any
            // case target.
            JS_ASSERT(defaultTarget < lastTarget);
            caseIsNew = true;
        } else {
            // The default target is in the middle and it alias a case target.
            JS_ASSERT(defaultTarget <= lastTarget);
            caseBlock = bodies[state.condswitch.defaultIdx];
        }

        // Allocate and register the default body.
        if (caseIsNew) {
            // Pop the case & switch operands.
            caseBlock = newBlockPopN(current, defaultTarget, 2);
            bodies[state.condswitch.defaultIdx] = caseBlock;
        }
    }

    if (!caseBlock)
        return ControlStatus_Error;

    // Terminate the last case condition block by emitting the code
    // corresponding to JSOP_CASE bytecode.
    if (bodyBlock != caseBlock) {
        MDefinition *caseOperand = current->pop();
        MDefinition *switchOperand = current->peek(-1);
        MCompare *cmpResult = MCompare::New(switchOperand, caseOperand, JSOP_STRICTEQ);
        cmpResult->infer(inspector, pc);
        JS_ASSERT(!cmpResult->isEffectful());
        current->add(cmpResult);
        current->end(MTest::New(cmpResult, bodyBlock, caseBlock));

        // Add last case as predecessor of the body if the body is aliasing
        // the previous case body.
        if (!bodyIsNew && !bodyBlock->addPredecessorPopN(current, 1))
            return ControlStatus_Error;

        // Add last case as predecessor of the non-matching case if the
        // non-matching case is an aliased default case. We need to pop the
        // switch operand as we skip the default case block and use the default
        // body block directly.
        JS_ASSERT_IF(!caseIsNew, caseIsDefault);
        if (!caseIsNew && !caseBlock->addPredecessorPopN(current, 1))
            return ControlStatus_Error;
    } else {
        // The default case alias the last case body.
        JS_ASSERT(caseIsDefault);
        current->pop(); // Case operand
        current->pop(); // Switch operand
        current->end(MGoto::New(bodyBlock));
        if (!bodyIsNew && !bodyBlock->addPredecessor(current))
            return ControlStatus_Error;
    }

    if (caseIsDefault) {
        // The last case condition is finished.  Loop in processCondSwitchBody,
        // with potential stops in processSwitchBreak.  Check that the bodies
        // fixed list is over-estimate by at most 1, and shrink the size such as
        // length can be used as an upper bound while iterating bodies.
        JS_ASSERT(currentIdx == bodies.length() || currentIdx + 1 == bodies.length());
        bodies.shrink(bodies.length() - currentIdx);

        // Handle break statements in processSwitchBreak while processing
        // bodies.
        ControlFlowInfo breakInfo(cfgStack_.length() - 1, state.condswitch.exitpc);
        if (!switches_.append(breakInfo))
            return ControlStatus_Error;

        // Jump into the first body.
        currentIdx = 0;
        setCurrent(nullptr);
        state.state = CFGState::COND_SWITCH_BODY;
        return processCondSwitchBody(state);
    }

    // Continue until the case condition.
    setCurrentAndSpecializePhis(caseBlock);
    pc = current->pc();
    state.stopAt = casePc;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processCondSwitchBody(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::COND_SWITCH_BODY);
    JS_ASSERT(pc <= state.condswitch.exitpc);
    FixedList<MBasicBlock *> &bodies = *state.condswitch.bodies;
    uint32_t &currentIdx = state.condswitch.currentIdx;

    JS_ASSERT(currentIdx <= bodies.length());
    if (currentIdx == bodies.length()) {
        JS_ASSERT_IF(current, pc == state.condswitch.exitpc);
        return processSwitchEnd(state.condswitch.breaks, state.condswitch.exitpc);
    }

    // Get the next body
    MBasicBlock *nextBody = bodies[currentIdx++];
    JS_ASSERT_IF(current, pc == nextBody->pc());

    // Fix the reverse post-order iteration.
    graph().moveBlockToEnd(nextBody);

    // The last body continue into the new one.
    if (current) {
        current->end(MGoto::New(nextBody));
        nextBody->addPredecessor(current);
    }

    // Continue in the next body.
    setCurrentAndSpecializePhis(nextBody);
    pc = current->pc();

    if (currentIdx < bodies.length())
        state.stopAt = bodies[currentIdx]->pc();
    else
        state.stopAt = state.condswitch.exitpc;
    return ControlStatus_Jumped;
}

bool
IonBuilder::jsop_andor(JSOp op)
{
    JS_ASSERT(op == JSOP_AND || op == JSOP_OR);

    jsbytecode *rhsStart = pc + js_CodeSpec[op].length;
    jsbytecode *joinStart = pc + GetJumpOffset(pc);
    JS_ASSERT(joinStart > pc);

    // We have to leave the LHS on the stack.
    MDefinition *lhs = current->peek(-1);

    MBasicBlock *evalRhs = newBlock(current, rhsStart);
    MBasicBlock *join = newBlock(current, joinStart);
    if (!evalRhs || !join)
        return false;

    MTest *test = (op == JSOP_AND)
                  ? MTest::New(lhs, evalRhs, join)
                  : MTest::New(lhs, join, evalRhs);
    test->infer();
    current->end(test);

    if (!cfgStack_.append(CFGState::AndOr(joinStart, join)))
        return false;

    setCurrentAndSpecializePhis(evalRhs);
    return true;
}

bool
IonBuilder::jsop_dup2()
{
    uint32_t lhsSlot = current->stackDepth() - 2;
    uint32_t rhsSlot = current->stackDepth() - 1;
    current->pushSlot(lhsSlot);
    current->pushSlot(rhsSlot);
    return true;
}

bool
IonBuilder::jsop_loophead(jsbytecode *pc)
{
    assertValidLoopHeadOp(pc);

    current->add(MInterruptCheck::New());

    return true;
}

bool
IonBuilder::jsop_ifeq(JSOp op)
{
    // IFEQ always has a forward offset.
    jsbytecode *trueStart = pc + js_CodeSpec[op].length;
    jsbytecode *falseStart = pc + GetJumpOffset(pc);
    JS_ASSERT(falseStart > pc);

    // We only handle cases that emit source notes.
    jssrcnote *sn = info().getNote(gsn, pc);
    if (!sn)
        return abort("expected sourcenote");

    MDefinition *ins = current->pop();

    // Create true and false branches.
    MBasicBlock *ifTrue = newBlock(current, trueStart);
    MBasicBlock *ifFalse = newBlock(current, falseStart);
    if (!ifTrue || !ifFalse)
        return false;

    MTest *test = MTest::New(ins, ifTrue, ifFalse);
    current->end(test);

    // The bytecode for if/ternary gets emitted either like this:
    //
    //    IFEQ X  ; src note (IF_ELSE, COND) points to the GOTO
    //    ...
    //    GOTO Z
    // X: ...     ; else/else if
    //    ...
    // Z:         ; join
    //
    // Or like this:
    //
    //    IFEQ X  ; src note (IF) has no offset
    //    ...
    // Z: ...     ; join
    //
    // We want to parse the bytecode as if we were parsing the AST, so for the
    // IF_ELSE/COND cases, we use the source note and follow the GOTO. For the
    // IF case, the IFEQ offset is the join point.
    switch (SN_TYPE(sn)) {
      case SRC_IF:
        if (!cfgStack_.append(CFGState::If(falseStart, ifFalse)))
            return false;
        break;

      case SRC_IF_ELSE:
      case SRC_COND:
      {
        // Infer the join point from the JSOP_GOTO[X] sitting here, then
        // assert as we much we can that this is the right GOTO.
        jsbytecode *trueEnd = pc + js_GetSrcNoteOffset(sn, 0);
        JS_ASSERT(trueEnd > pc);
        JS_ASSERT(trueEnd < falseStart);
        JS_ASSERT(JSOp(*trueEnd) == JSOP_GOTO);
        JS_ASSERT(!info().getNote(gsn, trueEnd));

        jsbytecode *falseEnd = trueEnd + GetJumpOffset(trueEnd);
        JS_ASSERT(falseEnd > trueEnd);
        JS_ASSERT(falseEnd >= falseStart);

        if (!cfgStack_.append(CFGState::IfElse(trueEnd, falseEnd, ifFalse)))
            return false;
        break;
      }

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected source note type");
    }

    // Switch to parsing the true branch. Note that no PC update is needed,
    // it's the next instruction.
    setCurrentAndSpecializePhis(ifTrue);

    return true;
}

bool
IonBuilder::jsop_try()
{
    JS_ASSERT(JSOp(*pc) == JSOP_TRY);

    if (!js_IonOptions.compileTryCatch)
        return abort("Try-catch support disabled");

    // Try-finally is not yet supported.
    if (analysis().hasTryFinally())
        return abort("Has try-finally");

    // Try-catch within inline frames is not yet supported.
    if (isInlineBuilder())
        return abort("try-catch within inline frame");

    graph().setHasTryBlock();

    jssrcnote *sn = info().getNote(gsn, pc);
    JS_ASSERT(SN_TYPE(sn) == SRC_TRY);

    // Get the pc of the last instruction in the try block. It's a JSOP_GOTO to
    // jump over the catch block.
    jsbytecode *endpc = pc + js_GetSrcNoteOffset(sn, 0);
    JS_ASSERT(JSOp(*endpc) == JSOP_GOTO);
    JS_ASSERT(GetJumpOffset(endpc) > 0);

    jsbytecode *afterTry = endpc + GetJumpOffset(endpc);

    // If controlflow in the try body is terminated (by a return or throw
    // statement), the code after the try-statement may still be reachable
    // via the catch block (which we don't compile) and OSR can enter it.
    // For example:
    //
    //     try {
    //         throw 3;
    //     } catch(e) { }
    //
    //     for (var i=0; i<1000; i++) {}
    //
    // To handle this, we create two blocks: one for the try block and one
    // for the code following the try-catch statement. Both blocks are
    // connected to the graph with an MTest instruction that always jumps to
    // the try block. This ensures the successor block always has a predecessor
    // and later passes will optimize this MTest to a no-op.
    //
    // If the code after the try block is unreachable (control flow in both the
    // try and catch blocks is terminated), only create the try block, to avoid
    // parsing unreachable code.

    MBasicBlock *tryBlock = newBlock(current, GetNextPc(pc));
    if (!tryBlock)
        return false;

    MBasicBlock *successor;
    if (analysis().maybeInfo(afterTry)) {
        successor = newBlock(current, afterTry);
        if (!successor)
            return false;

        // Add MTest(true, tryBlock, successorBlock).
        MConstant *true_ = MConstant::New(BooleanValue(true));
        current->add(true_);
        current->end(MTest::New(true_, tryBlock, successor));
    } else {
        successor = nullptr;
        current->end(MGoto::New(tryBlock));
    }

    if (!cfgStack_.append(CFGState::Try(endpc, successor)))
        return false;

    // The baseline compiler should not attempt to enter the catch block
    // via OSR.
    JS_ASSERT(info().osrPc() < endpc || info().osrPc() >= afterTry);

    // Start parsing the try block.
    setCurrentAndSpecializePhis(tryBlock);
    return true;
}

IonBuilder::ControlStatus
IonBuilder::processReturn(JSOp op)
{
    MDefinition *def;
    switch (op) {
      case JSOP_RETURN:
        // Return the last instruction.
        def = current->pop();
        break;

      case JSOP_STOP:
        // Return undefined eagerly if script doesn't use return value.
        if (script()->noScriptRval) {
            MInstruction *ins = MConstant::New(UndefinedValue());
            current->add(ins);
            def = ins;
            break;
        }

        // Fall through
      case JSOP_RETRVAL:
        // Return the value in the return value slot.
        def = current->getSlot(info().returnValueSlot());
        break;

      default:
        def = nullptr;
        MOZ_ASSUME_UNREACHABLE("unknown return op");
    }

    if (instrumentedProfiling())
        current->add(MFunctionBoundary::New(script(), MFunctionBoundary::Exit));
    MReturn *ret = MReturn::New(def);
    current->end(ret);

    if (!graph().addExit(current))
        return ControlStatus_Error;

    // Make sure no one tries to use this block now.
    setCurrent(nullptr);
    return processControlEnd();
}

IonBuilder::ControlStatus
IonBuilder::processThrow()
{
    // JSOP_THROW can't be compiled within inlined frames.
    if (isInlineBuilder())
        return ControlStatus_Abort;

    MDefinition *def = current->pop();

    if (graph().hasTryBlock()) {
        // MThrow is not marked as effectful. This means when it throws and we
        // are inside a try block, we could use an earlier resume point and this
        // resume point may not be up-to-date, for example:
        //
        // (function() {
        //     try {
        //         var x = 1;
        //         foo(); // resume point
        //         x = 2;
        //         throw foo;
        //     } catch(e) {
        //         print(x);
        //     }
        // ])();
        //
        // If we use the resume point after the call, this will print 1 instead
        // of 2. To fix this, we create a resume point right before the MThrow.
        //
        // Note that this is not a problem for instructions other than MThrow
        // because they are either marked as effectful (have their own resume
        // point) or cannot throw a catchable exception.
        MNop *ins = MNop::New();
        current->add(ins);

        if (!resumeAfter(ins))
            return ControlStatus_Error;
    }

    MThrow *ins = MThrow::New(def);
    current->end(ins);

    if (!graph().addExit(current))
        return ControlStatus_Error;

    // Make sure no one tries to use this block now.
    setCurrent(nullptr);
    return processControlEnd();
}

bool
IonBuilder::pushConstant(const Value &v)
{
    MConstant *ins = MConstant::New(v);
    current->add(ins);
    current->push(ins);
    return true;
}

bool
IonBuilder::jsop_bitnot()
{
    MDefinition *input = current->pop();
    MBitNot *ins = MBitNot::New(input);

    current->add(ins);
    ins->infer();

    current->push(ins);
    if (ins->isEffectful() && !resumeAfter(ins))
        return false;
    return true;
}
bool
IonBuilder::jsop_bitop(JSOp op)
{
    // Pop inputs.
    MDefinition *right = current->pop();
    MDefinition *left = current->pop();

    MBinaryBitwiseInstruction *ins;
    switch (op) {
      case JSOP_BITAND:
        ins = MBitAnd::New(left, right);
        break;

      case JSOP_BITOR:
        ins = MBitOr::New(left, right);
        break;

      case JSOP_BITXOR:
        ins = MBitXor::New(left, right);
        break;

      case JSOP_LSH:
        ins = MLsh::New(left, right);
        break;

      case JSOP_RSH:
        ins = MRsh::New(left, right);
        break;

      case JSOP_URSH:
        ins = MUrsh::New(left, right);
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected bitop");
    }

    current->add(ins);
    ins->infer(inspector, pc);

    current->push(ins);
    if (ins->isEffectful() && !resumeAfter(ins))
        return false;

    return true;
}

bool
IonBuilder::jsop_binary(JSOp op, MDefinition *left, MDefinition *right)
{
    // Do a string concatenation if adding two inputs that are int or string
    // and at least one is a string.
    if (op == JSOP_ADD &&
        ((left->type() == MIRType_String &&
          (right->type() == MIRType_String ||
           right->type() == MIRType_Int32 ||
           right->type() == MIRType_Double)) ||
         (left->type() == MIRType_Int32 &&
          right->type() == MIRType_String) ||
         (left->type() == MIRType_Double &&
          right->type() == MIRType_String)))
    {
        MConcat *ins = MConcat::New(left, right);
        current->add(ins);
        current->push(ins);
        return maybeInsertResume();
    }

    MBinaryArithInstruction *ins;
    switch (op) {
      case JSOP_ADD:
        ins = MAdd::New(left, right);
        break;

      case JSOP_SUB:
        ins = MSub::New(left, right);
        break;

      case JSOP_MUL:
        ins = MMul::New(left, right);
        break;

      case JSOP_DIV:
        ins = MDiv::New(left, right);
        break;

      case JSOP_MOD:
        ins = MMod::New(left, right);
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected binary opcode");
    }

    current->add(ins);
    ins->infer(inspector, pc);
    current->push(ins);

    if (ins->isEffectful())
        return resumeAfter(ins);
    return maybeInsertResume();
}

bool
IonBuilder::jsop_binary(JSOp op)
{
    MDefinition *right = current->pop();
    MDefinition *left = current->pop();

    return jsop_binary(op, left, right);
}

bool
IonBuilder::jsop_pos()
{
    if (IsNumberType(current->peek(-1)->type())) {
        // Already int32 or double.
        return true;
    }

    // Compile +x as x * 1.
    MDefinition *value = current->pop();
    MConstant *one = MConstant::New(Int32Value(1));
    current->add(one);

    return jsop_binary(JSOP_MUL, value, one);
}

bool
IonBuilder::jsop_neg()
{
    // Since JSOP_NEG does not use a slot, we cannot push the MConstant.
    // The MConstant is therefore passed to JSOP_MUL without slot traffic.
    MConstant *negator = MConstant::New(Int32Value(-1));
    current->add(negator);

    MDefinition *right = current->pop();

    if (!jsop_binary(JSOP_MUL, negator, right))
        return false;
    return true;
}

bool
IonBuilder::jsop_notearg()
{
    // JSOP_NOTEARG notes that the value in current->pop() has just
    // been pushed onto the stack for use in calling a function.
    MDefinition *def = current->pop();
    MPassArg *arg = MPassArg::New(def);

    current->add(arg);
    current->push(arg);
    return true;
}

class AutoAccumulateExits
{
    MIRGraph &graph_;
    MIRGraphExits *prev_;

  public:
    AutoAccumulateExits(MIRGraph &graph, MIRGraphExits &exits)
      : graph_(graph)
    {
        prev_ = graph_.exitAccumulator();
        graph_.setExitAccumulator(&exits);
    }
    ~AutoAccumulateExits() {
        graph_.setExitAccumulator(prev_);
    }
};

bool
IonBuilder::inlineScriptedCall(CallInfo &callInfo, JSFunction *target)
{
    JS_ASSERT(target->isInterpreted());
    JS_ASSERT(IsIonInlinablePC(pc));

    // Remove any MPassArgs.
    if (callInfo.isWrapped())
        callInfo.unwrapArgs();

    // Ensure sufficient space in the slots: needed for inlining from FUNAPPLY.
    uint32_t depth = current->stackDepth() + callInfo.numFormals();
    if (depth > current->nslots()) {
        if (!current->increaseSlots(depth - current->nslots()))
            return false;
    }

    // Create new |this| on the caller-side for inlined constructors.
    if (callInfo.constructing()) {
        MDefinition *thisDefn = createThis(target, callInfo.fun());
        if (!thisDefn)
            return false;
        callInfo.setThis(thisDefn);
    }

    // Capture formals in the outer resume point.
    callInfo.pushFormals(current);

    MResumePoint *outerResumePoint =
        MResumePoint::New(current, pc, callerResumePoint_, MResumePoint::Outer);
    if (!outerResumePoint)
        return false;

    // Pop formals again, except leave |fun| on stack for duration of call.
    callInfo.popFormals(current);
    current->push(callInfo.fun());

    JSScript *calleeScript = target->nonLazyScript();
    BaselineInspector inspector(calleeScript);

    // Improve type information of |this| when not set.
    if (callInfo.constructing() &&
        !callInfo.thisArg()->resultTypeSet() &&
        calleeScript->types)
    {
        types::StackTypeSet *types = types::TypeScript::ThisTypes(calleeScript);
        if (!types->unknown()) {
            MTypeBarrier *barrier =
                MTypeBarrier::New(callInfo.thisArg(), types->clone(temp_->lifoAlloc()));
            current->add(barrier);
            callInfo.setThis(barrier);
        }
    }

    // Start inlining.
    LifoAlloc *alloc = temp_->lifoAlloc();
    CompileInfo *info = alloc->new_<CompileInfo>(calleeScript, target,
                                                 (jsbytecode *)nullptr, callInfo.constructing(),
                                                 this->info().executionMode());
    if (!info)
        return false;

    MIRGraphExits saveExits;
    AutoAccumulateExits aae(graph(), saveExits);

    // Build the graph.
    JS_ASSERT(!cx->isExceptionPending());
    IonBuilder inlineBuilder(cx, &temp(), &graph(), constraints(), &inspector, info, nullptr,
                             inliningDepth_ + 1, loopDepth_);
    if (!inlineBuilder.buildInline(this, outerResumePoint, callInfo)) {
        if (cx->isExceptionPending()) {
            IonSpew(IonSpew_Abort, "Inline builder raised exception.");
            abortReason_ = AbortReason_Error;
            return false;
        }

        // Inlining the callee failed. Mark the callee as uninlineable only if
        // the inlining was aborted for a non-exception reason.
        if (inlineBuilder.abortReason_ == AbortReason_Disable) {
            calleeScript->uninlineable = true;
            abortReason_ = AbortReason_Inlining;
        } else if (inlineBuilder.abortReason_ == AbortReason_Inlining) {
            abortReason_ = AbortReason_Inlining;
        }

        return false;
    }

    // Create return block.
    jsbytecode *postCall = GetNextPc(pc);
    MBasicBlock *returnBlock = newBlock(nullptr, postCall);
    if (!returnBlock)
        return false;
    returnBlock->setCallerResumePoint(callerResumePoint_);

    // When profiling add Inline_Exit instruction to indicate end of inlined function.
    if (instrumentedProfiling())
        returnBlock->add(MFunctionBoundary::New(nullptr, MFunctionBoundary::Inline_Exit));

    // Inherit the slots from current and pop |fun|.
    returnBlock->inheritSlots(current);
    returnBlock->pop();

    // Accumulate return values.
    MIRGraphExits &exits = *inlineBuilder.graph().exitAccumulator();
    if (exits.length() == 0) {
        // Inlining of functions that have no exit is not supported.
        calleeScript->uninlineable = true;
        abortReason_ = AbortReason_Inlining;
        return false;
    }
    MDefinition *retvalDefn = patchInlinedReturns(callInfo, exits, returnBlock);
    if (!retvalDefn)
        return false;
    returnBlock->push(retvalDefn);

    // Initialize entry slots now that the stack has been fixed up.
    if (!returnBlock->initEntrySlots())
        return false;

    setCurrentAndSpecializePhis(returnBlock);
    return true;
}

MDefinition *
IonBuilder::patchInlinedReturn(CallInfo &callInfo, MBasicBlock *exit, MBasicBlock *bottom)
{
    // Replaces the MReturn in the exit block with an MGoto.
    MDefinition *rdef = exit->lastIns()->toReturn()->input();
    exit->discardLastIns();

    // Constructors must be patched by the caller to always return an object.
    if (callInfo.constructing()) {
        if (rdef->type() == MIRType_Value) {
            // Unknown return: dynamically detect objects.
            MReturnFromCtor *filter = MReturnFromCtor::New(rdef, callInfo.thisArg());
            exit->add(filter);
            rdef = filter;
        } else if (rdef->type() != MIRType_Object) {
            // Known non-object return: force |this|.
            rdef = callInfo.thisArg();
        }
    } else if (callInfo.isSetter()) {
        // Setters return their argument, not whatever value is returned.
        rdef = callInfo.getArg(0);
    }

    MGoto *replacement = MGoto::New(bottom);
    exit->end(replacement);
    if (!bottom->addPredecessorWithoutPhis(exit))
        return nullptr;

    return rdef;
}

MDefinition *
IonBuilder::patchInlinedReturns(CallInfo &callInfo, MIRGraphExits &exits, MBasicBlock *bottom)
{
    // Replaces MReturns with MGotos, returning the MDefinition
    // representing the return value, or nullptr.
    JS_ASSERT(exits.length() > 0);

    if (exits.length() == 1)
        return patchInlinedReturn(callInfo, exits[0], bottom);

    // Accumulate multiple returns with a phi.
    MPhi *phi = MPhi::New(bottom->stackDepth());
    if (!phi->reserveLength(exits.length()))
        return nullptr;

    for (size_t i = 0; i < exits.length(); i++) {
        MDefinition *rdef = patchInlinedReturn(callInfo, exits[i], bottom);
        if (!rdef)
            return nullptr;
        phi->addInput(rdef);
    }

    bottom->addPhi(phi);
    return phi;
}

static bool
IsSmallFunction(JSScript *script)
{
    return script->length <= js_IonOptions.smallFunctionMaxBytecodeLength;
}

bool
IonBuilder::makeInliningDecision(JSFunction *target, CallInfo &callInfo)
{
    // Only inline when inlining is enabled.
    if (!inliningEnabled())
        return false;

    // When there is no target, inlining is impossible.
    if (target == nullptr)
        return false;

    // Native functions provide their own detection in inlineNativeCall().
    if (target->isNative())
        return true;

    // Determine whether inlining is possible at callee site
    if (!canInlineTarget(target, callInfo))
        return false;

    // Heuristics!
    JSScript *targetScript = target->nonLazyScript();

    // Skip heuristics if we have an explicit hint to inline.
    if (!targetScript->shouldInline) {
        // Cap the inlining depth.
        if (IsSmallFunction(targetScript)) {
            if (inliningDepth_ >= js_IonOptions.smallFunctionMaxInlineDepth) {
                IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: exceeding allowed inline depth",
                        targetScript->filename(), targetScript->lineno);
                return false;
            }
        } else {
            if (inliningDepth_ >= js_IonOptions.maxInlineDepth) {
                IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: exceeding allowed inline depth",
                        targetScript->filename(), targetScript->lineno);
                return false;
            }

            if (targetScript->hasLoops()) {
                IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: big function that contains a loop",
                        targetScript->filename(), targetScript->lineno);
                return false;
            }
        }

        // Callee must not be excessively large.
        // This heuristic also applies to the callsite as a whole.
        if (targetScript->length > js_IonOptions.inlineMaxTotalBytecodeLength) {
            IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: callee excessively large.",
                    targetScript->filename(), targetScript->lineno);
            return false;
        }

        // Caller must be... somewhat hot. Ignore use counts when inlining for
        // the definite properties analysis, as the caller has not run yet.
        uint32_t callerUses = script()->getUseCount();
        if (callerUses < js_IonOptions.usesBeforeInlining() &&
            info().executionMode() != DefinitePropertiesAnalysis)
        {
            IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: caller is insufficiently hot.",
                    targetScript->filename(), targetScript->lineno);
            return false;
        }

        // Callee must be hot relative to the caller.
        if (targetScript->getUseCount() * js_IonOptions.inlineUseCountRatio < callerUses &&
            info().executionMode() != DefinitePropertiesAnalysis)
        {
            IonSpew(IonSpew_Inlining, "%s:%d - Vetoed: callee is not hot.",
                    targetScript->filename(), targetScript->lineno);
            return false;
        }
    }

    // TI calls ObjectStateChange to trigger invalidation of the caller.
    types::TypeObjectKey *targetType = types::TypeObjectKey::get(target);
    targetType->watchStateChangeForInlinedCall(constraints());

    return true;
}

uint32_t
IonBuilder::selectInliningTargets(ObjectVector &targets, CallInfo &callInfo, BoolVector &choiceSet)
{
    uint32_t totalSize = 0;
    uint32_t numInlineable = 0;

    // For each target, ask whether it may be inlined.
    if (!choiceSet.reserve(targets.length()))
        return false;
    for (size_t i = 0; i < targets.length(); i++) {
        JSFunction *target = &targets[i]->as<JSFunction>();
        bool inlineable = makeInliningDecision(target, callInfo);

        // Enforce a maximum inlined bytecode limit at the callsite.
        if (inlineable && target->isInterpreted()) {
            totalSize += target->nonLazyScript()->length;
            if (totalSize > js_IonOptions.inlineMaxTotalBytecodeLength)
                inlineable = false;
        }

        choiceSet.append(inlineable);
        if (inlineable)
            numInlineable++;
    }

    JS_ASSERT(choiceSet.length() == targets.length());
    return numInlineable;
}

static bool
CanInlineGetPropertyCache(MGetPropertyCache *cache, MDefinition *thisDef)
{
    JS_ASSERT(cache->object()->type() == MIRType_Object);
    if (cache->object() != thisDef)
        return false;

    InlinePropertyTable *table = cache->propTable();
    if (!table)
        return false;
    if (table->numEntries() == 0)
        return false;
    return true;
}

MGetPropertyCache *
IonBuilder::getInlineableGetPropertyCache(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return nullptr;

    MDefinition *thisDef = callInfo.thisArg();
    if (thisDef->type() != MIRType_Object)
        return nullptr;

    // Unwrap thisDef for pointer comparison purposes.
    if (thisDef->isPassArg())
        thisDef = thisDef->toPassArg()->getArgument();

    MDefinition *funcDef = callInfo.fun();
    if (funcDef->type() != MIRType_Object)
        return nullptr;

    // MGetPropertyCache with no uses may be optimized away.
    if (funcDef->isGetPropertyCache()) {
        MGetPropertyCache *cache = funcDef->toGetPropertyCache();
        if (cache->hasUses())
            return nullptr;
        if (!CanInlineGetPropertyCache(cache, thisDef))
            return nullptr;
        return cache;
    }

    // Optimize away the following common pattern:
    // MTypeBarrier[MIRType_Object] <- MGetPropertyCache
    if (funcDef->isTypeBarrier()) {
        MTypeBarrier *barrier = funcDef->toTypeBarrier();
        if (barrier->hasUses())
            return nullptr;
        if (barrier->type() != MIRType_Object)
            return nullptr;
        if (!barrier->input()->isGetPropertyCache())
            return nullptr;

        MGetPropertyCache *cache = barrier->input()->toGetPropertyCache();
        if (cache->hasUses() && !cache->hasOneUse())
            return nullptr;
        if (!CanInlineGetPropertyCache(cache, thisDef))
            return nullptr;
        return cache;
    }

    return nullptr;
}

IonBuilder::InliningStatus
IonBuilder::inlineSingleCall(CallInfo &callInfo, JSFunction *target)
{
    // Expects formals to be popped and wrapped.
    if (target->isNative())
        return inlineNativeCall(callInfo, target->native());

    if (!inlineScriptedCall(callInfo, target))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineCallsite(ObjectVector &targets, ObjectVector &originals,
                           bool lambda, CallInfo &callInfo)
{
    if (!inliningEnabled())
        return InliningStatus_NotInlined;

    if (targets.length() == 0)
        return InliningStatus_NotInlined;

    // Is the function provided by an MGetPropertyCache?
    // If so, the cache may be movable to a fallback path, with a dispatch
    // instruction guarding on the incoming TypeObject.
    MGetPropertyCache *propCache = getInlineableGetPropertyCache(callInfo);

    // Inline single targets -- unless they derive from a cache, in which case
    // avoiding the cache and guarding is still faster.
    if (!propCache && targets.length() == 1) {
        JSFunction *target = &targets[0]->as<JSFunction>();
        if (!makeInliningDecision(target, callInfo))
            return InliningStatus_NotInlined;

        // Inlining will elminate uses of the original callee, but it needs to
        // be preserved in phis if we bail out.  Mark the old callee definition as
        // folded to ensure this happens.
        callInfo.fun()->setFoldedUnchecked();

        // If the callee is not going to be a lambda (which may vary across
        // different invocations), then the callee definition can be replaced by a
        // constant.
        if (!lambda) {
            // Replace the function with an MConstant.
            MConstant *constFun = MConstant::New(ObjectValue(*target));
            current->add(constFun);
            callInfo.setFun(constFun);
        }

        return inlineSingleCall(callInfo, target);
    }

    // Choose a subset of the targets for polymorphic inlining.
    BoolVector choiceSet;
    uint32_t numInlined = selectInliningTargets(targets, callInfo, choiceSet);
    if (numInlined == 0)
        return InliningStatus_NotInlined;

    // Perform a polymorphic dispatch.
    if (!inlineCalls(callInfo, targets, originals, choiceSet, propCache))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

bool
IonBuilder::inlineGenericFallback(JSFunction *target, CallInfo &callInfo, MBasicBlock *dispatchBlock,
                                  bool clonedAtCallsite)
{
    // Generate a new block with all arguments on-stack.
    MBasicBlock *fallbackBlock = newBlock(dispatchBlock, pc);
    if (!fallbackBlock)
        return false;

    // Create a new CallInfo to track modified state within this block.
    CallInfo fallbackInfo(callInfo.constructing());
    if (!fallbackInfo.init(callInfo))
        return false;
    fallbackInfo.popFormals(fallbackBlock);
    fallbackInfo.wrapArgs(fallbackBlock);

    // Generate an MCall, which uses stateful |current|.
    setCurrentAndSpecializePhis(fallbackBlock);
    if (!makeCall(target, fallbackInfo, clonedAtCallsite))
        return false;

    // Pass return block to caller as |current|.
    return true;
}

bool
IonBuilder::inlineTypeObjectFallback(CallInfo &callInfo, MBasicBlock *dispatchBlock,
                                     MTypeObjectDispatch *dispatch, MGetPropertyCache *cache,
                                     MBasicBlock **fallbackTarget)
{
    // Getting here implies the following:
    // 1. The call function is an MGetPropertyCache, or an MGetPropertyCache
    //    followed by an MTypeBarrier.
    JS_ASSERT(callInfo.fun()->isGetPropertyCache() || callInfo.fun()->isTypeBarrier());

    // 2. The MGetPropertyCache has inlineable cases by guarding on the TypeObject.
    JS_ASSERT(dispatch->numCases() > 0);

    // 3. The MGetPropertyCache (and, if applicable, MTypeBarrier) only
    //    have at most a single use.
    JS_ASSERT_IF(callInfo.fun()->isGetPropertyCache(), !cache->hasUses());
    JS_ASSERT_IF(callInfo.fun()->isTypeBarrier(), cache->hasOneUse());

    // This means that no resume points yet capture the MGetPropertyCache,
    // so everything from the MGetPropertyCache up until the call is movable.
    // We now move the MGetPropertyCache and friends into a fallback path.

    // Create a new CallInfo to track modified state within the fallback path.
    CallInfo fallbackInfo(callInfo.constructing());
    if (!fallbackInfo.init(callInfo))
        return false;

    // Capture stack prior to the call operation. This captures the function.
    MResumePoint *preCallResumePoint =
        MResumePoint::New(dispatchBlock, pc, callerResumePoint_, MResumePoint::ResumeAt);
    if (!preCallResumePoint)
        return false;

    DebugOnly<size_t> preCallFuncIndex = preCallResumePoint->numOperands() - callInfo.numFormals();
    JS_ASSERT(preCallResumePoint->getOperand(preCallFuncIndex) == fallbackInfo.fun());

    // In the dispatch block, replace the function's slot entry with Undefined.
    MConstant *undefined = MConstant::New(UndefinedValue());
    dispatchBlock->add(undefined);
    dispatchBlock->rewriteAtDepth(-int(callInfo.numFormals()), undefined);

    // Construct a block that does nothing but remove formals from the stack.
    // This is effectively changing the entry resume point of the later fallback block.
    MBasicBlock *prepBlock = newBlock(dispatchBlock, pc);
    if (!prepBlock)
        return false;
    fallbackInfo.popFormals(prepBlock);

    // Construct a block into which the MGetPropertyCache can be moved.
    // This is subtle: the pc and resume point are those of the MGetPropertyCache!
    InlinePropertyTable *propTable = cache->propTable();
    JS_ASSERT(propTable->pc() != nullptr);
    JS_ASSERT(propTable->priorResumePoint() != nullptr);
    MBasicBlock *getPropBlock = newBlock(prepBlock, propTable->pc(), propTable->priorResumePoint());
    if (!getPropBlock)
        return false;

    prepBlock->end(MGoto::New(getPropBlock));

    // Since the getPropBlock inherited the stack from right before the MGetPropertyCache,
    // the target of the MGetPropertyCache is still on the stack.
    DebugOnly<MDefinition *> checkObject = getPropBlock->pop();
    JS_ASSERT(checkObject == cache->object());

    // Move the MGetPropertyCache and friends into the getPropBlock.
    if (fallbackInfo.fun()->isGetPropertyCache()) {
        JS_ASSERT(fallbackInfo.fun()->toGetPropertyCache() == cache);
        getPropBlock->addFromElsewhere(cache);
        getPropBlock->push(cache);
    } else {
        MTypeBarrier *barrier = callInfo.fun()->toTypeBarrier();
        JS_ASSERT(barrier->type() == MIRType_Object);
        JS_ASSERT(barrier->input()->isGetPropertyCache());
        JS_ASSERT(barrier->input()->toGetPropertyCache() == cache);

        getPropBlock->addFromElsewhere(cache);
        getPropBlock->addFromElsewhere(barrier);
        getPropBlock->push(barrier);
    }

    // Construct an end block with the correct resume point.
    MBasicBlock *preCallBlock = newBlock(getPropBlock, pc, preCallResumePoint);
    if (!preCallBlock)
        return false;
    getPropBlock->end(MGoto::New(preCallBlock));

    // Now inline the MCallGeneric, using preCallBlock as the dispatch point.
    if (!inlineGenericFallback(nullptr, fallbackInfo, preCallBlock, false))
        return false;

    // inlineGenericFallback() set the return block as |current|.
    preCallBlock->end(MGoto::New(current));
    *fallbackTarget = prepBlock;
    return true;
}

bool
IonBuilder::inlineCalls(CallInfo &callInfo, ObjectVector &targets,
                        ObjectVector &originals, BoolVector &choiceSet,
                        MGetPropertyCache *maybeCache)
{
    // Only handle polymorphic inlining.
    JS_ASSERT(IsIonInlinablePC(pc));
    JS_ASSERT(choiceSet.length() == targets.length());
    JS_ASSERT_IF(!maybeCache, targets.length() >= 2);
    JS_ASSERT_IF(maybeCache, targets.length() >= 1);

    MBasicBlock *dispatchBlock = current;

    // Unwrap the arguments.
    JS_ASSERT(callInfo.isWrapped());
    callInfo.unwrapArgs();
    callInfo.pushFormals(dispatchBlock);

    // Patch any InlinePropertyTable to only contain functions that are inlineable.
    //
    // Note that we trim using originals, as callsite clones are not user
    // visible. We don't patch the entries inside the table with the cloned
    // targets, as the entries should only be used for comparison.
    //
    // The InlinePropertyTable will also be patched at the end to exclude native functions
    // that vetoed inlining.
    if (maybeCache) {
        InlinePropertyTable *propTable = maybeCache->propTable();
        propTable->trimToTargets(originals);
        if (propTable->numEntries() == 0)
            maybeCache = nullptr;
    }

    // Generate a dispatch based on guard kind.
    MDispatchInstruction *dispatch;
    if (maybeCache) {
        dispatch = MTypeObjectDispatch::New(maybeCache->object(), maybeCache->propTable());
        callInfo.fun()->setFoldedUnchecked();
    } else {
        dispatch = MFunctionDispatch::New(callInfo.fun());
    }

    // Generate a return block to host the rval-collecting MPhi.
    jsbytecode *postCall = GetNextPc(pc);
    MBasicBlock *returnBlock = newBlock(nullptr, postCall);
    if (!returnBlock)
        return false;
    returnBlock->setCallerResumePoint(callerResumePoint_);

    // Set up stack, used to manually create a post-call resume point.
    returnBlock->inheritSlots(dispatchBlock);
    callInfo.popFormals(returnBlock);

    MPhi *retPhi = MPhi::New(returnBlock->stackDepth());
    returnBlock->addPhi(retPhi);
    returnBlock->push(retPhi);

    // Create a resume point from current stack state.
    returnBlock->initEntrySlots();

    // Reserve the capacity for the phi.
    // Note: this is an upperbound. Unreachable targets and uninlineable natives are also counted.
    uint32_t count = 1; // Possible fallback block.
    for (uint32_t i = 0; i < targets.length(); i++) {
        if (choiceSet[i])
            count++;
    }
    retPhi->reserveLength(count);

    // During inlining the 'this' value is assigned a type set which is
    // specialized to the type objects which can generate that inlining target.
    // After inlining the original type set is restored.
    types::TemporaryTypeSet *cacheObjectTypeSet =
        maybeCache ? maybeCache->object()->resultTypeSet() : nullptr;

    // Inline each of the inlineable targets.
    JS_ASSERT(targets.length() == originals.length());
    for (uint32_t i = 0; i < targets.length(); i++) {
        // When original != target, the target is a callsite clone. The
        // original should be used for guards, and the target should be the
        // actual function inlined.
        JSFunction *original = &originals[i]->as<JSFunction>();
        JSFunction *target = &targets[i]->as<JSFunction>();

        // Target must be inlineable.
        if (!choiceSet[i])
            continue;

        // Target must be reachable by the MDispatchInstruction.
        if (maybeCache && !maybeCache->propTable()->hasFunction(original)) {
            choiceSet[i] = false;
            continue;
        }

        MBasicBlock *inlineBlock = newBlock(dispatchBlock, pc);
        if (!inlineBlock)
            return false;

        // Create a function MConstant to use in the entry ResumePoint.
        MConstant *funcDef = MConstant::New(ObjectValue(*target));
        funcDef->setFoldedUnchecked();
        dispatchBlock->add(funcDef);

        // Use the MConstant in the inline resume point and on stack.
        int funIndex = inlineBlock->entryResumePoint()->numOperands() - callInfo.numFormals();
        inlineBlock->entryResumePoint()->replaceOperand(funIndex, funcDef);
        inlineBlock->rewriteSlot(funIndex, funcDef);

        // Create a new CallInfo to track modified state within the inline block.
        CallInfo inlineInfo(callInfo.constructing());
        if (!inlineInfo.init(callInfo))
            return false;
        inlineInfo.popFormals(inlineBlock);
        inlineInfo.setFun(funcDef);
        inlineInfo.wrapArgs(inlineBlock);

        if (maybeCache) {
            JS_ASSERT(callInfo.thisArg() == maybeCache->object());
            types::TemporaryTypeSet *targetThisTypes =
                maybeCache->propTable()->buildTypeSetForFunction(original);
            if (!targetThisTypes)
                return false;
            maybeCache->object()->setResultTypeSet(targetThisTypes);
        }

        // Inline the call into the inlineBlock.
        setCurrentAndSpecializePhis(inlineBlock);
        InliningStatus status = inlineSingleCall(inlineInfo, target);
        if (status == InliningStatus_Error)
            return false;

        // Natives may veto inlining.
        if (status == InliningStatus_NotInlined) {
            JS_ASSERT(target->isNative());
            JS_ASSERT(current == inlineBlock);
            inlineBlock->discardAllResumePoints();
            graph().removeBlock(inlineBlock);
            choiceSet[i] = false;
            continue;
        }

        // inlineSingleCall() changed |current| to the inline return block.
        MBasicBlock *inlineReturnBlock = current;
        setCurrent(dispatchBlock);

        // Connect the inline path to the returnBlock.
        //
        // Note that guarding is on the original function pointer even
        // if there is a clone, since cloning occurs at the callsite.
        dispatch->addCase(original, inlineBlock);

        MDefinition *retVal = inlineReturnBlock->peek(-1);
        retPhi->addInput(retVal);
        inlineReturnBlock->end(MGoto::New(returnBlock));
        if (!returnBlock->addPredecessorWithoutPhis(inlineReturnBlock))
            return false;
    }

    // Patch the InlinePropertyTable to not dispatch to vetoed paths.
    //
    // Note that like above, we trim using originals instead of targets.
    if (maybeCache) {
        maybeCache->object()->setResultTypeSet(cacheObjectTypeSet);

        InlinePropertyTable *propTable = maybeCache->propTable();
        propTable->trimTo(originals, choiceSet);

        // If all paths were vetoed, output only a generic fallback path.
        if (propTable->numEntries() == 0) {
            JS_ASSERT(dispatch->numCases() == 0);
            maybeCache = nullptr;
        }
    }

    // If necessary, generate a fallback path.
    // MTypeObjectDispatch always uses a fallback path.
    if (maybeCache || dispatch->numCases() < targets.length()) {
        // Generate fallback blocks, and set |current| to the fallback return block.
        if (maybeCache) {
            MBasicBlock *fallbackTarget;
            if (!inlineTypeObjectFallback(callInfo, dispatchBlock, (MTypeObjectDispatch *)dispatch,
                                          maybeCache, &fallbackTarget))
            {
                return false;
            }
            dispatch->addFallback(fallbackTarget);
        } else {
            JSFunction *remaining = nullptr;
            bool clonedAtCallsite = false;

            // If there is only 1 remaining case, we can annotate the fallback call
            // with the target information.
            if (dispatch->numCases() + 1 == originals.length()) {
                for (uint32_t i = 0; i < originals.length(); i++) {
                    if (choiceSet[i])
                        continue;

                    remaining = &targets[i]->as<JSFunction>();
                    clonedAtCallsite = targets[i] != originals[i];
                    break;
                }
            }

            if (!inlineGenericFallback(remaining, callInfo, dispatchBlock, clonedAtCallsite))
                return false;
            dispatch->addFallback(current);
        }

        MBasicBlock *fallbackReturnBlock = current;

        // Connect fallback case to return infrastructure.
        MDefinition *retVal = fallbackReturnBlock->peek(-1);
        retPhi->addInput(retVal);
        fallbackReturnBlock->end(MGoto::New(returnBlock));
        if (!returnBlock->addPredecessorWithoutPhis(fallbackReturnBlock))
            return false;
    }

    // Finally add the dispatch instruction.
    // This must be done at the end so that add() may be called above.
    dispatchBlock->end(dispatch);

    // Check the depth change: +1 for retval
    JS_ASSERT(returnBlock->stackDepth() == dispatchBlock->stackDepth() - callInfo.numFormals() + 1);

    graph().moveBlockToEnd(returnBlock);
    setCurrentAndSpecializePhis(returnBlock);
    return true;
}

MInstruction *
IonBuilder::createDeclEnvObject(MDefinition *callee, MDefinition *scope)
{
    // Get a template CallObject that we'll use to generate inline object
    // creation.
    DeclEnvObject *templateObj = inspector->templateDeclEnvObject();

    // One field is added to the function to handle its name.  This cannot be a
    // dynamic slot because there is still plenty of room on the DeclEnv object.
    JS_ASSERT(!templateObj->hasDynamicSlots());

    // Allocate the actual object. It is important that no intervening
    // instructions could potentially bailout, thus leaking the dynamic slots
    // pointer.
    MInstruction *declEnvObj = MNewDeclEnvObject::New(templateObj);
    current->add(declEnvObj);

    // Initialize the object's reserved slots. No post barrier is needed here:
    // the object will be allocated in the nursery if possible, and if the
    // tenured heap is used instead, a minor collection will have been performed
    // that moved scope/callee to the tenured heap.
    current->add(MStoreFixedSlot::New(declEnvObj, DeclEnvObject::enclosingScopeSlot(), scope));
    current->add(MStoreFixedSlot::New(declEnvObj, DeclEnvObject::lambdaSlot(), callee));

    return declEnvObj;
}

MInstruction *
IonBuilder::createCallObject(MDefinition *callee, MDefinition *scope)
{
    // Get a template CallObject that we'll use to generate inline object
    // creation.
    CallObject *templateObj = inspector->templateCallObject();

    // If the CallObject needs dynamic slots, allocate those now.
    MInstruction *slots;
    if (templateObj->hasDynamicSlots()) {
        size_t nslots = JSObject::dynamicSlotsCount(templateObj->numFixedSlots(),
                                                    templateObj->slotSpan());
        slots = MNewSlots::New(nslots);
    } else {
        slots = MConstant::New(NullValue());
    }
    current->add(slots);

    // Allocate the actual object. It is important that no intervening
    // instructions could potentially bailout, thus leaking the dynamic slots
    // pointer. Run-once scripts need a singleton type, so always do a VM call
    // in such cases.
    MInstruction *callObj = MNewCallObject::New(templateObj, script()->treatAsRunOnce, slots);
    current->add(callObj);

    // Insert a post barrier to protect the following writes if we allocated
    // the new call object directly into tenured storage.
    if (templateObj->type()->isLongLivedForJITAlloc())
        current->add(MPostWriteBarrier::New(callObj));

    // Initialize the object's reserved slots. No post barrier is needed here,
    // for the same reason as in createDeclEnvObject.
    current->add(MStoreFixedSlot::New(callObj, CallObject::enclosingScopeSlot(), scope));
    current->add(MStoreFixedSlot::New(callObj, CallObject::calleeSlot(), callee));

    // Initialize argument slots.
    for (AliasedFormalIter i(script()); i; i++) {
        unsigned slot = i.scopeSlot();
        unsigned formal = i.frameIndex();
        MDefinition *param = current->getSlot(info().argSlotUnchecked(formal));
        if (slot >= templateObj->numFixedSlots())
            current->add(MStoreSlot::New(slots, slot - templateObj->numFixedSlots(), param));
        else
            current->add(MStoreFixedSlot::New(callObj, slot, param));
    }

    return callObj;
}

MDefinition *
IonBuilder::createThisScripted(MDefinition *callee)
{
    // Get callee.prototype.
    //
    // This instruction MUST be idempotent: since it does not correspond to an
    // explicit operation in the bytecode, we cannot use resumeAfter().
    // Getters may not override |prototype| fetching, so this operation is indeed idempotent.
    // - First try an idempotent property cache.
    // - Upon failing idempotent property cache, we can't use a non-idempotent cache,
    //   therefore we fallback to CallGetProperty
    //
    // Note: both CallGetProperty and GetPropertyCache can trigger a GC,
    //       and thus invalidation.
    MInstruction *getProto;
    if (!invalidatedIdempotentCache()) {
        MGetPropertyCache *getPropCache = MGetPropertyCache::New(callee, names().prototype);
        getPropCache->setIdempotent();
        getProto = getPropCache;
    } else {
        MCallGetProperty *callGetProp = MCallGetProperty::New(callee, names().prototype,
                                                              /* callprop = */ false);
        callGetProp->setIdempotent();
        getProto = callGetProp;
    }
    current->add(getProto);

    // Create this from prototype
    MCreateThisWithProto *createThis = MCreateThisWithProto::New(callee, getProto);
    current->add(createThis);

    return createThis;
}

JSObject *
IonBuilder::getSingletonPrototype(JSFunction *target)
{
    if (!target || !target->hasSingletonType())
        return nullptr;
    types::TypeObjectKey *targetType = types::TypeObjectKey::get(target);
    if (targetType->unknownProperties())
        return nullptr;

    jsid protoid = NameToId(names().prototype);
    types::HeapTypeSetKey protoProperty = targetType->property(protoid);

    return protoProperty.singleton(constraints());
}

MDefinition *
IonBuilder::createThisScriptedSingleton(JSFunction *target, MDefinition *callee)
{
    // Get the singleton prototype (if exists)
    JSObject *proto = getSingletonPrototype(target);
    if (!proto)
        return nullptr;

    if (!target->nonLazyScript()->types)
        return nullptr;

    JSObject *templateObject = inspector->getTemplateObject(pc);
    if (!templateObject || !templateObject->is<JSObject>())
        return nullptr;
    if (templateObject->getProto() != proto)
        return nullptr;

    if (!types::TypeScript::ThisTypes(target->nonLazyScript())->hasType(types::Type::ObjectType(templateObject)))
        return nullptr;

    // For template objects with NewScript info, the appropriate allocation
    // kind to use may change due to dynamic property adds. In these cases
    // calling Ion code will be invalidated, but any baseline template object
    // may be stale. Update to the correct template object in this case.
    types::TypeObject *templateType = templateObject->type();
    if (templateType->hasNewScript()) {
        templateObject = templateType->newScript()->templateObject;
        JS_ASSERT(templateObject->type() == templateType);

        // Trigger recompilation if the templateObject changes.
        types::TypeObjectKey::get(templateType)->watchStateChangeForNewScriptTemplate(constraints());
    }

    // Generate an inline path to create a new |this| object with
    // the given singleton prototype.
    MCreateThisWithTemplate *createThis = MCreateThisWithTemplate::New(templateObject);
    current->add(createThis);

    return createThis;
}

MDefinition *
IonBuilder::createThis(JSFunction *target, MDefinition *callee)
{
    // Create this for unknown target
    if (!target) {
        MCreateThis *createThis = MCreateThis::New(callee);
        current->add(createThis);
        return createThis;
    }

    // Native constructors build the new Object themselves.
    if (target->isNative()) {
        if (!target->isNativeConstructor())
            return nullptr;

        MConstant *magic = MConstant::New(MagicValue(JS_IS_CONSTRUCTING));
        current->add(magic);
        return magic;
    }

    // Try baking in the prototype.
    MDefinition *createThis = createThisScriptedSingleton(target, callee);
    if (createThis)
        return createThis;

    return createThisScripted(callee);
}

bool
IonBuilder::jsop_funcall(uint32_t argc)
{
    // Stack for JSOP_FUNCALL:
    // 1:      MPassArg(arg0)
    // ...
    // argc:   MPassArg(argN)
    // argc+1: MPassArg(JSFunction *), the 'f' in |f.call()|, in |this| position.
    // argc+2: The native 'call' function.

    int calleeDepth = -((int)argc + 2);
    int funcDepth = -((int)argc + 1);

    // If |Function.prototype.call| may be overridden, don't optimize callsite.
    types::TemporaryTypeSet *calleeTypes = current->peek(calleeDepth)->resultTypeSet();
    JSFunction *native = getSingleCallTarget(calleeTypes);
    if (!native || !native->isNative() || native->native() != &js_fun_call) {
        CallInfo callInfo(false);
        if (!callInfo.init(current, argc))
            return false;
        return makeCall(native, callInfo, false);
    }
    current->peek(calleeDepth)->setFoldedUnchecked();

    // Extract call target.
    types::TemporaryTypeSet *funTypes = current->peek(funcDepth)->resultTypeSet();
    JSFunction *target = getSingleCallTarget(funTypes);

    // Unwrap the (JSFunction *) parameter.
    MPassArg *passFunc = current->peek(funcDepth)->toPassArg();
    current->rewriteAtDepth(funcDepth, passFunc->getArgument());

    // Remove the MPassArg(JSFunction *).
    passFunc->replaceAllUsesWith(passFunc->getArgument());
    passFunc->block()->discard(passFunc);

    // Shimmy the slots down to remove the native 'call' function.
    current->shimmySlots(funcDepth - 1);

    // If no |this| argument was provided, explicitly pass Undefined.
    // Pushing is safe here, since one stack slot has been removed.
    if (argc == 0) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        MPassArg *pass = MPassArg::New(undef);
        current->add(pass);
        current->push(pass);
    } else {
        // |this| becomes implicit in the call.
        argc -= 1;
    }

    CallInfo callInfo(false);
    if (!callInfo.init(current, argc))
        return false;

    // Try inlining call
    if (argc > 0 && makeInliningDecision(target, callInfo) && target->isInterpreted())
        return inlineScriptedCall(callInfo, target);

    // Call without inlining.
    return makeCall(target, callInfo, false);
}

bool
IonBuilder::jsop_funapply(uint32_t argc)
{
    int calleeDepth = -((int)argc + 2);

    types::TemporaryTypeSet *calleeTypes = current->peek(calleeDepth)->resultTypeSet();
    JSFunction *native = getSingleCallTarget(calleeTypes);
    if (argc != 2) {
        CallInfo callInfo(false);
        if (!callInfo.init(current, argc))
            return false;
        return makeCall(native, callInfo, false);
    }

    // Disable compilation if the second argument to |apply| cannot be guaranteed
    // to be either definitely |arguments| or definitely not |arguments|.
    MDefinition *argument = current->peek(-1);
    if (script()->argumentsHasVarBinding() &&
        argument->mightBeType(MIRType_Magic) &&
        argument->type() != MIRType_Magic)
    {
        return abort("fun.apply with MaybeArguments");
    }

    // Fallback to regular call if arg 2 is not definitely |arguments|.
    if (argument->type() != MIRType_Magic) {
        CallInfo callInfo(false);
        if (!callInfo.init(current, argc))
            return false;
        return makeCall(native, callInfo, false);
    }

    if (!native ||
        !native->isNative() ||
        native->native() != js_fun_apply)
    {
        return abort("fun.apply speculation failed");
    }

    current->peek(calleeDepth)->setFoldedUnchecked();

    // Use funapply that definitely uses |arguments|
    return jsop_funapplyarguments(argc);
}

bool
IonBuilder::jsop_funapplyarguments(uint32_t argc)
{
    // Stack for JSOP_FUNAPPLY:
    // 1:      MPassArg(Vp)
    // 2:      MPassArg(This)
    // argc+1: MPassArg(JSFunction *), the 'f' in |f.call()|, in |this| position.
    // argc+2: The native 'apply' function.

    int funcDepth = -((int)argc + 1);

    // Extract call target.
    types::TemporaryTypeSet *funTypes = current->peek(funcDepth)->resultTypeSet();
    JSFunction *target = getSingleCallTarget(funTypes);

    // When this script isn't inlined, use MApplyArgs,
    // to copy the arguments from the stack and call the function
    if (inliningDepth_ == 0 && info().executionMode() != DefinitePropertiesAnalysis) {

        // Vp
        MPassArg *passVp = current->pop()->toPassArg();
        passVp->getArgument()->setFoldedUnchecked();
        passVp->replaceAllUsesWith(passVp->getArgument());
        passVp->block()->discard(passVp);

        // This
        MPassArg *passThis = current->pop()->toPassArg();
        MDefinition *argThis = passThis->getArgument();
        passThis->replaceAllUsesWith(argThis);
        passThis->block()->discard(passThis);

        // Unwrap the (JSFunction *) parameter.
        MPassArg *passFunc = current->pop()->toPassArg();
        MDefinition *argFunc = passFunc->getArgument();
        passFunc->replaceAllUsesWith(argFunc);
        passFunc->block()->discard(passFunc);

        // Pop apply function.
        current->pop();

        MArgumentsLength *numArgs = MArgumentsLength::New();
        current->add(numArgs);

        MApplyArgs *apply = MApplyArgs::New(target, argFunc, numArgs, argThis);
        current->add(apply);
        current->push(apply);
        if (!resumeAfter(apply))
            return false;

        types::TemporaryTypeSet *types = bytecodeTypes(pc);
        return pushTypeBarrier(apply, types, true);
    }

    // When inlining we have the arguments the function gets called with
    // and can optimize even more, by just calling the functions with the args.
    // We also try this path when doing the definite properties analysis, as we
    // can inline the apply() target and don't care about the actual arguments
    // that were passed in.

    CallInfo callInfo(false);

    // Vp
    MPassArg *passVp = current->pop()->toPassArg();
    passVp->getArgument()->setFoldedUnchecked();
    passVp->replaceAllUsesWith(passVp->getArgument());
    passVp->block()->discard(passVp);

    // Arguments
    MDefinitionVector args;
    if (inliningDepth_) {
        if (!args.append(inlineCallInfo_->argv().begin(), inlineCallInfo_->argv().end()))
            return false;
    }
    callInfo.setArgs(&args);

    // This
    MPassArg *passThis = current->pop()->toPassArg();
    MDefinition *argThis = passThis->getArgument();
    passThis->replaceAllUsesWith(argThis);
    passThis->block()->discard(passThis);
    callInfo.setThis(argThis);

    // Unwrap the (JSFunction *) parameter.
    MPassArg *passFunc = current->pop()->toPassArg();
    MDefinition *argFunc = passFunc->getArgument();
    passFunc->replaceAllUsesWith(argFunc);
    passFunc->block()->discard(passFunc);

    callInfo.setFun(argFunc);

    // Pop apply function.
    current->pop();

    // Try inlining call
    if (makeInliningDecision(target, callInfo) && target->isInterpreted())
        return inlineScriptedCall(callInfo, target);

    callInfo.wrapArgs(current);
    return makeCall(target, callInfo, false);
}

bool
IonBuilder::jsop_call(uint32_t argc, bool constructing)
{
    // If this call has never executed, try to seed the observed type set
    // based on how the call result is used.
    types::TemporaryTypeSet *observed = bytecodeTypes(pc);
    if (observed->empty()) {
        if (BytecodeFlowsToBitop(pc)) {
            if (!observed->addType(types::Type::Int32Type(), temp_->lifoAlloc()))
                return false;
        } else if (*GetNextPc(pc) == JSOP_POS) {
            // Note: this is lame, overspecialized on the code patterns used
            // by asm.js and should be replaced by a more general mechanism.
            // See bug 870847.
            if (!observed->addType(types::Type::DoubleType(), temp_->lifoAlloc()))
                return false;
        }
    }

    int calleeDepth = -((int)argc + 2);

    // Acquire known call target if existent.
    ObjectVector originals;
    bool gotLambda = false;
    types::TemporaryTypeSet *calleeTypes = current->peek(calleeDepth)->resultTypeSet();
    if (calleeTypes) {
        if (!getPolyCallTargets(calleeTypes, constructing, originals, 4, &gotLambda))
            return false;
    }
    JS_ASSERT_IF(gotLambda, originals.length() <= 1);

    // If any call targets need to be cloned, clone them. Keep track of the
    // originals as we need to case on them for poly inline.
    bool hasClones = false;
    ObjectVector targets;
    RootedFunction fun(cx);
    RootedScript scriptRoot(cx, script());
    for (uint32_t i = 0; i < originals.length(); i++) {
        fun = &originals[i]->as<JSFunction>();
        if (fun->hasScript() && fun->nonLazyScript()->shouldCloneAtCallsite) {
            fun = CloneFunctionAtCallsite(cx, fun, scriptRoot, pc);
            if (!fun)
                return false;
            hasClones = true;
        }
        if (!targets.append(fun))
            return false;
    }

    CallInfo callInfo(constructing);
    if (!callInfo.init(current, argc))
        return false;

    // Try inlining
    InliningStatus status = inlineCallsite(targets, originals, gotLambda, callInfo);
    if (status == InliningStatus_Inlined)
        return true;
    if (status == InliningStatus_Error)
        return false;

    // No inline, just make the call.
    JSFunction *target = nullptr;
    if (targets.length() == 1)
        target = &targets[0]->as<JSFunction>();

    return makeCall(target, callInfo, hasClones);
}

MDefinition *
IonBuilder::makeCallsiteClone(JSFunction *target, MDefinition *fun)
{
    // Bake in the clone eagerly if we have a known target. We have arrived here
    // because TI told us that the known target is a should-clone-at-callsite
    // function, which means that target already is the clone. Make sure to ensure
    // that the old definition remains in resume points.
    if (target) {
        MConstant *constant = MConstant::New(ObjectValue(*target));
        current->add(constant);
        fun->setFoldedUnchecked();
        return constant;
    }

    // Add a callsite clone IC if we have multiple targets. Note that we
    // should have checked already that at least some targets are marked as
    // should-clone-at-callsite.
    MCallsiteCloneCache *clone = MCallsiteCloneCache::New(fun, pc);
    current->add(clone);
    return clone;
}

static bool
TestShouldDOMCall(JSContext *cx, types::TypeSet *inTypes, JSFunction *func,
                  JSJitInfo::OpType opType)
{
    if (!func->isNative() || !func->jitInfo())
        return false;
    // If all the DOM objects flowing through are legal with this
    // property, we can bake in a call to the bottom half of the DOM
    // accessor
    DOMInstanceClassMatchesProto instanceChecker =
        GetDOMCallbacks(cx->runtime())->instanceClassMatchesProto;

    const JSJitInfo *jinfo = func->jitInfo();
    if (jinfo->type != opType)
        return false;

    for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
        types::TypeObjectKey *curType = inTypes->getObject(i);
        if (!curType)
            continue;

        RootedObject protoRoot(cx, curType->proto().toObjectOrNull());
        if (!instanceChecker(protoRoot, jinfo->protoID, jinfo->depth))
            return false;
    }

    return true;
}

static bool
TestAreKnownDOMTypes(JSContext *cx, types::TypeSet *inTypes)
{
    if (inTypes->unknownObject())
        return false;

    // First iterate to make sure they all are DOM objects, then freeze all of
    // them as such if they are.
    for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
        types::TypeObjectKey *curType = inTypes->getObject(i);
        if (!curType)
            continue;

        if (curType->unknownProperties())
            return false;
        if (!(curType->clasp()->flags & JSCLASS_IS_DOMJSCLASS))
            return false;
    }

    // If we didn't check anything, no reason to say yes.
    if (inTypes->getObjectCount() > 0)
        return true;

    return false;
}


static bool
ArgumentTypesMatch(MDefinition *def, types::StackTypeSet *calleeTypes)
{
    if (def->resultTypeSet()) {
        JS_ASSERT(def->type() == MIRType_Value || def->mightBeType(def->type()));
        return def->resultTypeSet()->isSubset(calleeTypes);
    }

    if (def->type() == MIRType_Value)
        return false;

    if (def->type() == MIRType_Object)
        return calleeTypes->unknownObject();

    return calleeTypes->mightBeType(ValueTypeFromMIRType(def->type()));
}

bool
IonBuilder::testNeedsArgumentCheck(JSContext *cx, JSFunction *target, CallInfo &callInfo)
{
    // If we have a known target, check if the caller arg types are a subset of callee.
    // Since typeset accumulates and can't decrease that means we don't need to check
    // the arguments anymore.
    if (!target->hasScript())
        return true;

    JSScript *targetScript = target->nonLazyScript();
    if (!targetScript->types)
        return true;

    if (!ArgumentTypesMatch(callInfo.thisArg(), types::TypeScript::ThisTypes(targetScript)))
        return true;
    uint32_t expected_args = Min<uint32_t>(callInfo.argc(), target->nargs);
    for (size_t i = 0; i < expected_args; i++) {
        if (!ArgumentTypesMatch(callInfo.getArg(i), types::TypeScript::ArgTypes(targetScript, i)))
            return true;
    }
    for (size_t i = callInfo.argc(); i < target->nargs; i++) {
        if (!types::TypeScript::ArgTypes(targetScript, i)->mightBeType(JSVAL_TYPE_UNDEFINED))
            return true;
    }

    return false;
}

MCall *
IonBuilder::makeCallHelper(JSFunction *target, CallInfo &callInfo, bool cloneAtCallsite)
{
    JS_ASSERT(callInfo.isWrapped());

    // This function may be called with mutated stack.
    // Querying TI for popped types is invalid.

    uint32_t targetArgs = callInfo.argc();

    // Collect number of missing arguments provided that the target is
    // scripted. Native functions are passed an explicit 'argc' parameter.
    if (target && !target->isNative())
        targetArgs = Max<uint32_t>(target->nargs, callInfo.argc());

    MCall *call =
        MCall::New(target, targetArgs + 1, callInfo.argc(), callInfo.constructing());
    if (!call)
        return nullptr;

    // Explicitly pad any missing arguments with |undefined|.
    // This permits skipping the argumentsRectifier.
    for (int i = targetArgs; i > (int)callInfo.argc(); i--) {
        JS_ASSERT_IF(target, !target->isNative());
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        MPassArg *pass = MPassArg::New(undef);
        current->add(pass);
        call->addArg(i, pass);
    }

    // Add explicit arguments.
    // Skip addArg(0) because it is reserved for this
    for (int32_t i = callInfo.argc() - 1; i >= 0; i--) {
        JS_ASSERT(callInfo.getArg(i)->isPassArg());
        call->addArg(i + 1, callInfo.getArg(i)->toPassArg());
    }

    // Place an MPrepareCall before the first passed argument, before we
    // potentially perform rearrangement.
    JS_ASSERT(callInfo.thisArg()->isPassArg());
    MPassArg *thisArg = callInfo.thisArg()->toPassArg();
    MPrepareCall *start = new MPrepareCall;
    thisArg->block()->insertBefore(thisArg, start);
    call->initPrepareCall(start);

    // Inline the constructor on the caller-side.
    if (callInfo.constructing()) {
        MDefinition *create = createThis(target, callInfo.fun());
        if (!create) {
            abort("Failure inlining constructor for call.");
            return nullptr;
        }

        // Unwrap the MPassArg before discarding: it may have been captured by an MResumePoint.
        thisArg->replaceAllUsesWith(thisArg->getArgument());
        thisArg->block()->discard(thisArg);

        MPassArg *newThis = MPassArg::New(create);
        current->add(newThis);

        thisArg = newThis;
        callInfo.setThis(newThis);
    }

    // Pass |this| and function.
    call->addArg(0, thisArg);

    // Add a callsite clone IC for multiple targets which all should be
    // callsite cloned, or bake in the clone for a single target.
    if (cloneAtCallsite) {
        MDefinition *fun = makeCallsiteClone(target, callInfo.fun());
        callInfo.setFun(fun);
    }

    if (target && JSOp(*pc) == JSOP_CALL) {
        // We know we have a single call target.  Check whether the "this" types
        // are DOM types and our function a DOM function, and if so flag the
        // MCall accordingly.
        types::TemporaryTypeSet *thisTypes = thisArg->resultTypeSet();
        if (thisTypes &&
            TestAreKnownDOMTypes(cx, thisTypes) &&
            TestShouldDOMCall(cx, thisTypes, target, JSJitInfo::Method))
        {
            call->setDOMFunction();
        }
    }

    if (target && !testNeedsArgumentCheck(cx, target, callInfo))
        call->disableArgCheck();

    call->initFunction(callInfo.fun());

    current->add(call);
    return call;
}

static bool
DOMCallNeedsBarrier(const JSJitInfo* jitinfo, types::TemporaryTypeSet *types)
{
    // If the return type of our DOM native is in "types" already, we don't
    // actually need a barrier.
    if (jitinfo->returnType == JSVAL_TYPE_UNKNOWN)
        return true;

    // JSVAL_TYPE_OBJECT doesn't tell us much; we still have to barrier on the
    // actual type of the object.
    if (jitinfo->returnType == JSVAL_TYPE_OBJECT)
        return true;

    // No need for a barrier if we're already expecting the type we'll produce.
    return jitinfo->returnType != types->getKnownTypeTag();
}

bool
IonBuilder::makeCall(JSFunction *target, CallInfo &callInfo, bool cloneAtCallsite)
{
    // Constructor calls to non-constructors should throw. We don't want to use
    // CallKnown in this case.
    JS_ASSERT_IF(callInfo.constructing() && target,
                 target->isInterpretedConstructor() || target->isNativeConstructor());

    MCall *call = makeCallHelper(target, callInfo, cloneAtCallsite);
    if (!call)
        return false;

    current->push(call);
    if (!resumeAfter(call))
        return false;

    types::TemporaryTypeSet *types = bytecodeTypes(pc);

    bool barrier = true;
    if (call->isDOMFunction()) {
        JSFunction* target = call->getSingleTarget();
        JS_ASSERT(target && target->isNative() && target->jitInfo());
        barrier = DOMCallNeedsBarrier(target->jitInfo(), types);
    }

    return pushTypeBarrier(call, types, barrier);
}

bool
IonBuilder::jsop_eval(uint32_t argc)
{
    int calleeDepth = -((int)argc + 2);
    types::TemporaryTypeSet *calleeTypes = current->peek(calleeDepth)->resultTypeSet();

    // Emit a normal call if the eval has never executed. This keeps us from
    // disabling compilation for the script when testing with --ion-eager.
    if (calleeTypes && calleeTypes->empty())
        return jsop_call(argc, /* constructing = */ false);

    JSFunction *singleton = getSingleCallTarget(calleeTypes);
    if (!singleton)
        return abort("No singleton callee for eval()");

    if (IsBuiltinEvalForScope(&script()->global(), ObjectValue(*singleton))) {
        if (argc != 1)
            return abort("Direct eval with more than one argument");

        if (!info().fun())
            return abort("Direct eval in global code");

        // The 'this' value for the outer and eval scripts must be the
        // same. This is not guaranteed if a primitive string/number/etc.
        // is passed through to the eval invoke as the primitive may be
        // boxed into different objects if accessed via 'this'.
        JSValueType type = thisTypes->getKnownTypeTag();
        if (type != JSVAL_TYPE_OBJECT && type != JSVAL_TYPE_NULL && type != JSVAL_TYPE_UNDEFINED)
            return abort("Direct eval from script with maybe-primitive 'this'");

        CallInfo callInfo(/* constructing = */ false);
        if (!callInfo.init(current, argc))
            return false;
        callInfo.unwrapArgs();

        callInfo.fun()->setFoldedUnchecked();

        MDefinition *scopeChain = current->scopeChain();
        MDefinition *string = callInfo.getArg(0);

        current->pushSlot(info().thisSlot());
        MDefinition *thisValue = current->pop();

        // Try to pattern match 'eval(v + "()")'. In this case v is likely a
        // name on the scope chain and the eval is performing a call on that
        // value. Use a dynamic scope chain lookup rather than a full eval.
        if (string->isConcat() &&
            string->getOperand(1)->isConstant() &&
            string->getOperand(1)->toConstant()->value().isString())
        {
            JSString *str = string->getOperand(1)->toConstant()->value().toString();

            bool match;
            if (!JS_StringEqualsAscii(cx, str, "()", &match))
                return false;
            if (match) {
                MDefinition *name = string->getOperand(0);
                MInstruction *dynamicName = MGetDynamicName::New(scopeChain, name);
                current->add(dynamicName);

                MInstruction *thisv = MPassArg::New(thisValue);
                current->add(thisv);

                current->push(dynamicName);
                current->push(thisv);

                CallInfo evalCallInfo(/* constructing = */ false);
                if (!evalCallInfo.init(current, /* argc = */ 0))
                    return false;

                return makeCall(nullptr, evalCallInfo, false);
            }
        }

        MInstruction *filterArguments = MFilterArgumentsOrEval::New(string);
        current->add(filterArguments);

        MInstruction *ins = MCallDirectEval::New(scopeChain, string, thisValue, pc);
        current->add(ins);
        current->push(ins);

        types::TemporaryTypeSet *types = bytecodeTypes(pc);
        return resumeAfter(ins) && pushTypeBarrier(ins, types, true);
    }

    return jsop_call(argc, /* constructing = */ false);
}

bool
IonBuilder::jsop_compare(JSOp op)
{
    MDefinition *right = current->pop();
    MDefinition *left = current->pop();

    MCompare *ins = MCompare::New(left, right, op);
    current->add(ins);
    current->push(ins);

    ins->infer(inspector, pc);

    if (ins->isEffectful() && !resumeAfter(ins))
        return false;
    return true;
}

bool
IonBuilder::jsop_newarray(uint32_t count)
{
    JS_ASSERT(script()->compileAndGo);

    JSObject *templateObject = inspector->getTemplateObject(pc);
    if (!templateObject)
        return abort("No template object for NEWARRAY");

    JS_ASSERT(templateObject->is<ArrayObject>());
    if (templateObject->type()->unknownProperties()) {
        // We will get confused in jsop_initelem_array if we can't find the
        // type object being initialized.
        return abort("New array has unknown properties");
    }

    types::TemporaryTypeSet::DoubleConversion conversion =
        bytecodeTypes(pc)->convertDoubleElements(constraints());
    if (conversion == types::TemporaryTypeSet::AlwaysConvertToDoubles)
        templateObject->setShouldConvertDoubleElements();

    MNewArray *ins = new MNewArray(count, templateObject, MNewArray::NewArray_Allocating);

    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_newobject()
{
    // Don't bake in the TypeObject for non-CNG scripts.
    JS_ASSERT(script()->compileAndGo);

    JSObject *templateObject = inspector->getTemplateObject(pc);
    if (!templateObject)
        return abort("No template object for NEWOBJECT");

    JS_ASSERT(templateObject->is<JSObject>());
    MNewObject *ins = MNewObject::New(templateObject,
                                      /* templateObjectIsClassPrototype = */ false);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_initelem()
{
    MDefinition *value = current->pop();
    MDefinition *id = current->pop();
    MDefinition *obj = current->peek(-1);

    MInitElem *initElem = MInitElem::New(obj, id, value);
    current->add(initElem);

    return resumeAfter(initElem);
}

bool
IonBuilder::jsop_initelem_array()
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->peek(-1);

    // Make sure that arrays have the type being written to them by the
    // intializer, and that arrays are marked as non-packed when writing holes
    // to them during initialization.
    bool needStub = false;
    types::TypeObjectKey *initializer = obj->resultTypeSet()->getObject(0);
    if (value->isConstant() && value->toConstant()->value().isMagic(JS_ELEMENTS_HOLE)) {
        if (!initializer->hasFlags(constraints(), types::OBJECT_FLAG_NON_PACKED))
            needStub = true;
    } else if (!initializer->unknownProperties()) {
        types::HeapTypeSetKey elemTypes = initializer->property(JSID_VOID);
        if (!TypeSetIncludes(elemTypes.maybeTypes(), value->type(), value->resultTypeSet())) {
            elemTypes.freeze(constraints());
            needStub = true;
        }
    }

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(obj, value));

    if (needStub) {
        MCallInitElementArray *store = MCallInitElementArray::New(obj, GET_UINT24(pc), value);
        current->add(store);
        return resumeAfter(store);
    }

    MConstant *id = MConstant::New(Int32Value(GET_UINT24(pc)));
    current->add(id);

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    if (obj->toNewArray()->templateObject()->shouldConvertDoubleElements()) {
        MInstruction *valueDouble = MToDouble::New(value);
        current->add(valueDouble);
        value = valueDouble;
    }

    // Store the value.
    MStoreElement *store = MStoreElement::New(elements, id, value, /* needsHoleCheck = */ false);
    current->add(store);

    // Update the length.
    MSetInitializedLength *initLength = MSetInitializedLength::New(elements, id);
    current->add(initLength);

    if (!resumeAfter(initLength))
        return false;

   return true;
}

static bool
CanEffectlesslyCallLookupGenericOnObject(JSContext *cx, JSObject *obj, PropertyName *name)
{
    while (obj) {
        if (!obj->isNative())
            return false;
        if (obj->getClass()->ops.lookupGeneric)
            return false;
        if (obj->nativeLookup(cx, NameToId(name)))
            return true;
        if (obj->getClass()->resolve != JS_ResolveStub &&
            obj->getClass()->resolve != (JSResolveOp)fun_resolve)
            return false;
        obj = obj->getProto();
    }
    return true;
}

bool
IonBuilder::jsop_initprop(PropertyName *name)
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->peek(-1);

    RootedObject templateObject(cx, obj->toNewObject()->templateObject());

    if (!CanEffectlesslyCallLookupGenericOnObject(cx, templateObject, name))
        return abort("INITPROP template object is special");

    RootedObject holder(cx);
    RootedShape shape(cx);
    RootedId id(cx, NameToId(name));
    bool res = LookupPropertyWithFlags(cx, templateObject, id,
                                       0, &holder, &shape);
    if (!res)
        return false;

    if (!shape || holder != templateObject) {
        // JSOP_NEWINIT becomes an MNewObject without preconfigured properties.
        MInitProp *init = MInitProp::New(obj, name, value);
        current->add(init);
        return resumeAfter(init);
    }

    if (PropertyWriteNeedsTypeBarrier(constraints(), current,
                                      &obj, name, &value, /* canModify = */ true))
    {
        // JSOP_NEWINIT becomes an MNewObject without preconfigured properties.
        MInitProp *init = MInitProp::New(obj, name, value);
        current->add(init);
        return resumeAfter(init);
    }

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(obj, value));

    bool needsBarrier = true;
    if (obj->resultTypeSet() &&
        !obj->resultTypeSet()->propertyNeedsBarrier(constraints(), id))
    {
        needsBarrier = false;
    }

    // In parallel execution, we never require write barriers.  See
    // forkjoin.cpp for more information.
    switch (info().executionMode()) {
      case SequentialExecution:
      case DefinitePropertiesAnalysis:
        break;
      case ParallelExecution:
        needsBarrier = false;
        break;
    }

    if (templateObject->isFixedSlot(shape->slot())) {
        MStoreFixedSlot *store = MStoreFixedSlot::New(obj, shape->slot(), value);
        if (needsBarrier)
            store->setNeedsBarrier();

        current->add(store);
        return resumeAfter(store);
    }

    MSlots *slots = MSlots::New(obj);
    current->add(slots);

    uint32_t slot = templateObject->dynamicSlotIndex(shape->slot());
    MStoreSlot *store = MStoreSlot::New(slots, slot, value);
    if (needsBarrier)
        store->setNeedsBarrier();

    current->add(store);
    return resumeAfter(store);
}

bool
IonBuilder::jsop_initprop_getter_setter(PropertyName *name)
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->peek(-1);

    MInitPropGetterSetter *init = MInitPropGetterSetter::New(obj, name, value);
    current->add(init);
    return resumeAfter(init);
}

bool
IonBuilder::jsop_initelem_getter_setter()
{
    MDefinition *value = current->pop();
    MDefinition *id = current->pop();
    MDefinition *obj = current->peek(-1);

    MInitElemGetterSetter *init = MInitElemGetterSetter::New(obj, id, value);
    current->add(init);
    return resumeAfter(init);
}

MBasicBlock *
IonBuilder::addBlock(MBasicBlock *block, uint32_t loopDepth)
{
    if (!block)
        return nullptr;
    graph().addBlock(block);
    block->setLoopDepth(loopDepth);
    return block;
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(graph(), &analysis(), info(),
                                          predecessor, pc, MBasicBlock::NORMAL);
    return addBlock(block, loopDepth_);
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc, MResumePoint *priorResumePoint)
{
    MBasicBlock *block = MBasicBlock::NewWithResumePoint(graph(), info(), predecessor, pc,
                                                         priorResumePoint);
    return addBlock(block, loopDepth_);
}

MBasicBlock *
IonBuilder::newBlockPopN(MBasicBlock *predecessor, jsbytecode *pc, uint32_t popped)
{
    MBasicBlock *block = MBasicBlock::NewPopN(graph(), info(), predecessor, pc, MBasicBlock::NORMAL, popped);
    return addBlock(block, loopDepth_);
}

MBasicBlock *
IonBuilder::newBlockAfter(MBasicBlock *at, MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(graph(), &analysis(), info(),
                                          predecessor, pc, MBasicBlock::NORMAL);
    if (!block)
        return nullptr;
    graph().insertBlockAfter(at, block);
    return block;
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc, uint32_t loopDepth)
{
    MBasicBlock *block = MBasicBlock::New(graph(), &analysis(), info(),
                                          predecessor, pc, MBasicBlock::NORMAL);
    return addBlock(block, loopDepth);
}

MBasicBlock *
IonBuilder::newOsrPreheader(MBasicBlock *predecessor, jsbytecode *loopEntry)
{
    JS_ASSERT((JSOp)*loopEntry == JSOP_LOOPENTRY);
    JS_ASSERT(loopEntry == info().osrPc());

    // Create two blocks: one for the OSR entry with no predecessors, one for
    // the preheader, which has the OSR entry block as a predecessor. The
    // OSR block is always the second block (with id 1).
    MBasicBlock *osrBlock  = newBlockAfter(*graph().begin(), loopEntry);
    MBasicBlock *preheader = newBlock(predecessor, loopEntry);
    if (!osrBlock || !preheader)
        return nullptr;

    MOsrEntry *entry = MOsrEntry::New();
    osrBlock->add(entry);

    // Initialize |scopeChain|.
    {
        uint32_t slot = info().scopeChainSlot();

        MInstruction *scopev;
        if (analysis().usesScopeChain()) {
            scopev = MOsrScopeChain::New(entry);
        } else {
            // Use an undefined value if the script does not need its scope
            // chain, to match the type that is already being tracked for the
            // slot.
            scopev = MConstant::New(UndefinedValue());
        }

        osrBlock->add(scopev);
        osrBlock->initSlot(slot, scopev);
    }
    // Initialize |return value|
    {
        MInstruction *returnValue;
        if (!script()->noScriptRval)
            returnValue = MOsrReturnValue::New(entry);
        else
            returnValue = MConstant::New(UndefinedValue());
        osrBlock->add(returnValue);
        osrBlock->initSlot(info().returnValueSlot(), returnValue);
    }

    // Initialize arguments object.
    bool needsArgsObj = info().needsArgsObj();
    MInstruction *argsObj = nullptr;
    if (info().hasArguments()) {
        if (needsArgsObj)
            argsObj = MOsrArgumentsObject::New(entry);
        else
            argsObj = MConstant::New(UndefinedValue());
        osrBlock->add(argsObj);
        osrBlock->initSlot(info().argsObjSlot(), argsObj);
    }

    if (info().fun()) {
        // Initialize |this| parameter.
        MParameter *thisv = MParameter::New(MParameter::THIS_SLOT, nullptr);
        osrBlock->add(thisv);
        osrBlock->initSlot(info().thisSlot(), thisv);

        // Initialize arguments.
        for (uint32_t i = 0; i < info().nargs(); i++) {
            uint32_t slot = needsArgsObj ? info().argSlotUnchecked(i) : info().argSlot(i);

            if (needsArgsObj) {
                JS_ASSERT(argsObj && argsObj->isOsrArgumentsObject());
                // If this is an aliased formal, then the arguments object
                // contains a hole at this index.  Any references to this
                // variable in the jitcode will come from JSOP_*ALIASEDVAR
                // opcodes, so the slot itself can be set to undefined.  If
                // it's not aliased, it must be retrieved from the arguments
                // object.
                MInstruction *osrv;
                if (script()->formalIsAliased(i))
                    osrv = MConstant::New(UndefinedValue());
                else
                    osrv = MGetArgumentsObjectArg::New(argsObj, i);

                osrBlock->add(osrv);
                osrBlock->initSlot(slot, osrv);
            } else {
                MParameter *arg = MParameter::New(i, nullptr);
                osrBlock->add(arg);
                osrBlock->initSlot(slot, arg);
            }
        }
    }

    // Initialize locals.
    for (uint32_t i = 0; i < info().nlocals(); i++) {
        uint32_t slot = info().localSlot(i);
        ptrdiff_t offset = BaselineFrame::reverseOffsetOfLocal(i);

        MOsrValue *osrv = MOsrValue::New(entry, offset);
        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
    }

    // Initialize stack.
    uint32_t numStackSlots = preheader->stackDepth() - info().firstStackSlot();
    for (uint32_t i = 0; i < numStackSlots; i++) {
        uint32_t slot = info().stackSlot(i);
        ptrdiff_t offset = BaselineFrame::reverseOffsetOfLocal(info().nlocals() + i);

        MOsrValue *osrv = MOsrValue::New(entry, offset);
        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
    }

    // Create an MStart to hold the first valid MResumePoint.
    MStart *start = MStart::New(MStart::StartType_Osr);
    osrBlock->add(start);
    graph().setOsrStart(start);

    // MOsrValue instructions are infallible, so the first MResumePoint must
    // occur after they execute, at the point of the MStart.
    if (!resumeAt(start, loopEntry))
        return nullptr;

    // Link the same MResumePoint from the MStart to each MOsrValue.
    // This causes logic in ShouldSpecializeInput() to not replace Uses with
    // Unboxes in the MResumePiont, so that the MStart always sees Values.
    osrBlock->linkOsrValues(start);

    // Clone types of the other predecessor of the pre-header to the osr block,
    // such as pre-header phi's won't discard specialized type of the
    // predecessor.
    JS_ASSERT(predecessor->stackDepth() == osrBlock->stackDepth());
    JS_ASSERT(info().scopeChainSlot() == 0);

    // Treat the OSR values as having the same type as the existing values
    // coming in to the loop. These will be fixed up with appropriate
    // unboxing and type barriers in finishLoop, once the possible types
    // at the loop header are known.
    for (uint32_t i = info().startArgSlot(); i < osrBlock->stackDepth(); i++) {
        MDefinition *existing = current->getSlot(i);
        MDefinition *def = osrBlock->getSlot(i);
        JS_ASSERT_IF(!needsArgsObj || !info().isSlotAliased(i), def->type() == MIRType_Value);

        // Aliased slots are never accessed, since they need to go through
        // the callobject. No need to type them here.
        if (info().isSlotAliased(i))
            continue;

        def->setResultType(existing->type());
        def->setResultTypeSet(existing->resultTypeSet());
    }

    // Finish the osrBlock.
    osrBlock->end(MGoto::New(preheader));
    preheader->addPredecessor(osrBlock);
    graph().setOsrBlock(osrBlock);

    // Wrap |this| with a guaranteed use, to prevent instruction elimination.
    // Prevent |this| from being DCE'd: necessary for constructors.
    if (info().fun())
        preheader->getSlot(info().thisSlot())->setGuard();

    return preheader;
}

MBasicBlock *
IonBuilder::newPendingLoopHeader(MBasicBlock *predecessor, jsbytecode *pc, bool osr)
{
    loopDepth_++;
    MBasicBlock *block = MBasicBlock::NewPendingLoopHeader(graph(), info(), predecessor, pc);
    if (!addBlock(block, loopDepth_))
        return nullptr;

    if (osr) {
        // Incorporate type information from the OSR frame into the loop
        // header. The OSR frame may have unexpected types due to type changes
        // within the loop body or due to incomplete profiling information,
        // in which case this may avoid restarts of loop analysis or bailouts
        // during the OSR itself.

        // Unbox the MOsrValue if it is known to be unboxable.
        for (uint32_t i = info().startArgSlot(); i < block->stackDepth(); i++) {

            // The value of aliased args and slots are in the callobject. So we can't
            // the value from the baseline frame.
            if (info().isSlotAliased(i))
                continue;

            // Don't bother with expression stack values. The stack should be
            // empty except for let variables (not Ion-compiled) or iterators.
            if (i >= info().firstStackSlot())
                continue;

            MPhi *phi = block->getSlot(i)->toPhi();

            // Get the value from the baseline frame.
            Value existingValue;
            uint32_t arg = i - info().firstArgSlot();
            uint32_t var = i - info().firstLocalSlot();
            if (info().fun() && i == info().thisSlot()) {
                existingValue = baselineFrame_->thisValue();
            } else if (arg < info().nargs()) {
                if (info().needsArgsObj())
                    existingValue = baselineFrame_->argsObj().arg(arg);
                else
                    existingValue = baselineFrame_->unaliasedFormal(arg);
            } else {
                existingValue = baselineFrame_->unaliasedVar(var);
            }

            // Extract typeset from value.
            MIRType type = existingValue.isDouble()
                         ? MIRType_Double
                         : MIRTypeFromValueType(existingValue.extractNonDoubleType());
            types::Type ntype = types::GetValueType(existingValue);
            types::TemporaryTypeSet *typeSet =
                temp_->lifoAlloc()->new_<types::TemporaryTypeSet>(ntype);
            if (!typeSet)
                return nullptr;
            phi->addBackedgeType(type, typeSet);
        }
    }

    return block;
}

// A resume point is a mapping of stack slots to MDefinitions. It is used to
// capture the environment such that if a guard fails, and IonMonkey needs
// to exit back to the interpreter, the interpreter state can be
// reconstructed.
//
// We capture stack state at critical points:
//   * (1) At the beginning of every basic block.
//   * (2) After every effectful operation.
//
// As long as these two properties are maintained, instructions can
// be moved, hoisted, or, eliminated without problems, and ops without side
// effects do not need to worry about capturing state at precisely the
// right point in time.
//
// Effectful instructions, of course, need to capture state after completion,
// where the interpreter will not attempt to repeat the operation. For this,
// ResumeAfter must be used. The state is attached directly to the effectful
// instruction to ensure that no intermediate instructions could be injected
// in between by a future analysis pass.
//
// During LIR construction, if an instruction can bail back to the interpreter,
// we create an LSnapshot, which uses the last known resume point to request
// register/stack assignments for every live value.
bool
IonBuilder::resume(MInstruction *ins, jsbytecode *pc, MResumePoint::Mode mode)
{
    JS_ASSERT(ins->isEffectful() || !ins->isMovable());

    MResumePoint *resumePoint = MResumePoint::New(ins->block(), pc, callerResumePoint_, mode);
    if (!resumePoint)
        return false;
    ins->setResumePoint(resumePoint);
    resumePoint->setInstruction(ins);
    return true;
}

bool
IonBuilder::resumeAt(MInstruction *ins, jsbytecode *pc)
{
    return resume(ins, pc, MResumePoint::ResumeAt);
}

bool
IonBuilder::resumeAfter(MInstruction *ins)
{
    return resume(ins, pc, MResumePoint::ResumeAfter);
}

bool
IonBuilder::maybeInsertResume()
{
    // Create a resume point at the current position, without an existing
    // effectful instruction. This resume point is not necessary for correct
    // behavior (see above), but is added to avoid holding any values from the
    // previous resume point which are now dead. This shortens the live ranges
    // of such values and improves register allocation.
    //
    // This optimization is not performed outside of loop bodies, where good
    // register allocation is not as critical, in order to avoid creating
    // excessive resume points.

    if (loopDepth_ == 0)
        return true;

    MNop *ins = MNop::New();
    current->add(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::testSingletonProperty(JSObject *obj, JSObject *singleton,
                                  PropertyName *name, bool *isKnownConstant)
{
    // We would like to completely no-op property/global accesses which can
    // produce only a particular JSObject. When indicating the access result is
    // definitely an object, type inference does not account for the
    // possibility that the property is entirely missing from the input object
    // and its prototypes (if this happens, a semantic trigger would be hit and
    // the pushed types updated, even if there is no type barrier).
    //
    // If the access definitely goes through obj, either directly or on the
    // prototype chain, then if obj has a defined property now, and the
    // property has a default or method shape, then the property is not missing
    // and the only way it can become missing in the future is if it is deleted.
    // Deletion causes type properties to be explicitly marked with undefined.

    *isKnownConstant = false;

    if (!CanEffectlesslyCallLookupGenericOnObject(cx, obj, name))
        return true;

    RootedObject objRoot(cx, obj);
    RootedId idRoot(cx, NameToId(name));
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!JSObject::lookupGeneric(cx, objRoot, idRoot, &holder, &shape))
        return false;
    if (!shape)
        return true;

    if (!shape->hasDefaultGetter())
        return true;
    if (!shape->hasSlot())
        return true;
    if (holder->getSlot(shape->slot()).isUndefined())
        return true;

    // Ensure the property does not appear anywhere on the prototype chain
    // before |holder|, and that |holder| only has the result object for its
    // property.
    while (true) {
        types::TypeObjectKey *objType = types::TypeObjectKey::get(obj);
        if (objType->unknownProperties())
            return true;

        types::HeapTypeSetKey property = objType->property(NameToId(name), context());
        if (obj != holder) {
            if (property.notEmpty(constraints()))
                return true;
        } else {
            if (property.singleton(constraints()) != singleton)
                return true;
            break;
        }

        obj = obj->getProto();
    }

    *isKnownConstant = true;
    return true;
}

bool
IonBuilder::testSingletonPropertyTypes(MDefinition *obj, JSObject *singleton,
                                       JSObject *globalObj, PropertyName *name,
                                       bool *isKnownConstant, bool *testObject,
                                       bool *testString)
{
    // As for TestSingletonProperty, but the input is any value in a type set
    // rather than a specific object. If testObject is set then the constant
    // result can only be used after ensuring the input is an object.

    *isKnownConstant = false;
    *testObject = false;
    *testString = false;

    types::TemporaryTypeSet *types = obj->resultTypeSet();

    if (!types && obj->type() != MIRType_String)
        return true;

    if (types && types->unknownObject())
        return true;

    JSObject *objectSingleton = types ? types->getSingleton() : nullptr;
    if (objectSingleton)
        return testSingletonProperty(objectSingleton, singleton, name, isKnownConstant);

    if (!globalObj)
        return true;

    JSProtoKey key;
    switch (obj->type()) {
      case MIRType_String:
        key = JSProto_String;
        break;

      case MIRType_Int32:
      case MIRType_Double:
        key = JSProto_Number;
        break;

      case MIRType_Boolean:
        key = JSProto_Boolean;
        break;

      case MIRType_Object:
      case MIRType_Value: {
        if (types->hasType(types::Type::StringType())) {
            key = JSProto_String;
            *testString = true;
            break;
        }

        if (!types->maybeObject())
            return true;

        // For property accesses which may be on many objects, we just need to
        // find a prototype common to all the objects; if that prototype
        // has the singleton property, the access will not be on a missing property.
        for (unsigned i = 0; i < types->getObjectCount(); i++) {
            types::TypeObjectKey *object = types->getObject(i);
            if (!object)
                continue;

            if (object->unknownProperties())
                return true;
            types::HeapTypeSetKey property = object->property(NameToId(name), context());
            if (property.notEmpty(constraints()))
                return true;

            if (JSObject *proto = object->proto().toObjectOrNull()) {
                // Test this type.
                bool thoughtConstant = false;
                if (!testSingletonProperty(proto, singleton, name, &thoughtConstant))
                    return false;
                if (!thoughtConstant)
                    return true;
            } else {
                // Can't be on the prototype chain with no prototypes...
                return true;
            }
        }
        // If this is not a known object, a test will be needed.
        *testObject = (obj->type() != MIRType_Object);
        *isKnownConstant = true;
        return true;
      }
      default:
        return true;
    }

    RootedObject proto(cx);
    if (!js_GetClassPrototype(cx, key, &proto, nullptr))
        return false;

    return testSingletonProperty(proto, singleton, name, isKnownConstant);
}

// Given an observed type set, annotates the IR as much as possible:
// (1) If no type information is provided, the value on the top of the stack is
//     left in place.
// (2) If a single type definitely exists, and no type barrier is needed,
//     then an infallible unbox instruction replaces the value on the top of
//     the stack.
// (3) If a type barrier is needed, but has an unknown type set, leave the
//     value at the top of the stack.
// (4) If a type barrier is needed, and has a single type, an unbox
//     instruction replaces the top of the stack.
// (5) Lastly, a type barrier instruction replaces the top of the stack.
bool
IonBuilder::pushTypeBarrier(MInstruction *ins, types::TemporaryTypeSet *observed, bool needsBarrier)
{
    // Barriers are never needed for instructions whose result will not be used.
    if (BytecodeIsPopped(pc))
        return true;

    // If the instruction has no side effects, we'll resume the entire operation.
    // The actual type barrier will occur in the interpreter. If the
    // instruction is effectful, even if it has a singleton type, there
    // must be a resume point capturing the original def, and resuming
    // to that point will explicitly monitor the new type.

    if (!needsBarrier) {
        JSValueType type = observed->getKnownTypeTag();
        MInstruction *replace = nullptr;
        switch (type) {
          case JSVAL_TYPE_UNDEFINED:
            ins->setFoldedUnchecked();
            replace = MConstant::New(UndefinedValue());
            break;
          case JSVAL_TYPE_NULL:
            ins->setFoldedUnchecked();
            replace = MConstant::New(NullValue());
            break;
          case JSVAL_TYPE_UNKNOWN:
            break;
          default: {
            MIRType replaceType = MIRTypeFromValueType(type);
            if (ins->type() == MIRType_Value)
                replace = MUnbox::New(ins, replaceType, MUnbox::Infallible);
            else
                JS_ASSERT(ins->type() == replaceType);
            break;
          }
        }
        if (replace) {
            current->pop();
            current->add(replace);
            current->push(replace);
            replace->setResultTypeSet(observed);
        } else {
            ins->setResultTypeSet(observed);
        }
        return true;
    }

    if (observed->unknown())
        return true;

    current->pop();

    MInstruction *barrier = MTypeBarrier::New(ins, observed);
    current->add(barrier);

    if (barrier->type() == MIRType_Undefined)
        return pushConstant(UndefinedValue());
    if (barrier->type() == MIRType_Null)
        return pushConstant(NullValue());

    current->push(barrier);
    return true;
}

bool
IonBuilder::getStaticName(JSObject *staticObject, PropertyName *name, bool *psucceeded)
{
    jsid id = NameToId(name);

    JS_ASSERT(staticObject->is<GlobalObject>() || staticObject->is<CallObject>());

    *psucceeded = true;

    if (staticObject->is<GlobalObject>()) {
        // Optimize undefined, NaN, and Infinity.
        if (name == names().undefined)
            return pushConstant(UndefinedValue());
        if (name == names().NaN)
            return pushConstant(compartment->runtimeFromAnyThread()->NaNValue);
        if (name == names().Infinity)
            return pushConstant(compartment->runtimeFromAnyThread()->positiveInfinityValue);
    }

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    Shape *shape = staticObject->nativeLookup(cx, id);
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot()) {
        *psucceeded = false;
        return true;
    }

    types::TypeObjectKey *staticType = types::TypeObjectKey::get(staticObject);
    Maybe<types::HeapTypeSetKey> propertyTypes;
    if (!staticType->unknownProperties()) {
        propertyTypes.construct(staticType->property(id, context()));
        if (propertyTypes.ref().configured(constraints(), staticType)) {
            // The property has been reconfigured as non-configurable, non-enumerable
            // or non-writable.
            *psucceeded = false;
            return true;
        }
    }

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    bool barrier = PropertyReadNeedsTypeBarrier(cx, context(), constraints(), staticType,
                                                name, types, /* updateObserved = */ true);

    // If the property is permanent, a shape guard isn't necessary.

    JSObject *singleton = types->getSingleton();

    JSValueType knownType = types->getKnownTypeTag();
    if (!barrier) {
        if (singleton) {
            // Try to inline a known constant value.
            bool isKnownConstant;
            if (!testSingletonProperty(staticObject, singleton, name, &isKnownConstant))
                return false;
            if (isKnownConstant)
                return pushConstant(ObjectValue(*singleton));
        }
        if (knownType == JSVAL_TYPE_UNDEFINED)
            return pushConstant(UndefinedValue());
        if (knownType == JSVAL_TYPE_NULL)
            return pushConstant(NullValue());
    }

    MInstruction *obj = MConstant::New(ObjectValue(*staticObject));
    current->add(obj);

    // If we have a property typeset, the HeapTypeSetIsConfigured call will
    // trigger recompilation if the property is deleted or reconfigured.
    if (propertyTypes.empty() && shape->configurable())
        obj = addShapeGuard(obj, staticObject->lastProperty(), Bailout_ShapeGuard);

    MIRType rvalType = MIRTypeFromValueType(types->getKnownTypeTag());
    if (barrier)
        rvalType = MIRType_Value;

    return loadSlot(obj, shape, rvalType, barrier, types);
}

// Whether 'types' includes all possible values represented by input/inputTypes.
bool
jit::TypeSetIncludes(types::TypeSet *types, MIRType input, types::TypeSet *inputTypes)
{
    if (!types)
        return inputTypes && inputTypes->empty();

    switch (input) {
      case MIRType_Undefined:
      case MIRType_Null:
      case MIRType_Boolean:
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Float32:
      case MIRType_String:
      case MIRType_Magic:
        return types->hasType(types::Type::PrimitiveType(ValueTypeFromMIRType(input)));

      case MIRType_Object:
        return types->unknownObject() || (inputTypes && inputTypes->isSubset(types));

      case MIRType_Value:
        return types->unknown() || (inputTypes && inputTypes->isSubset(types));

      default:
        MOZ_ASSUME_UNREACHABLE("Bad input type");
    }
}

// Whether a write of the given value may need a post-write barrier for GC purposes.
bool
jit::NeedsPostBarrier(CompileInfo &info, MDefinition *value)
{
    return info.executionMode() != ParallelExecution && value->mightBeType(MIRType_Object);
}

bool
IonBuilder::setStaticName(JSObject *staticObject, PropertyName *name)
{
    jsid id = NameToId(name);

    JS_ASSERT(staticObject->is<GlobalObject>() || staticObject->is<CallObject>());

    MDefinition *value = current->peek(-1);

    if (staticObject->watched())
        return jsop_setprop(name);

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    Shape *shape = staticObject->nativeLookup(cx, id);
    if (!shape || !shape->hasDefaultSetter() || !shape->writable() || !shape->hasSlot())
        return jsop_setprop(name);

    types::TypeObjectKey *staticType = types::TypeObjectKey::get(staticObject);
    if (staticType->unknownProperties())
        return jsop_setprop(name);

    types::HeapTypeSetKey propertyTypes = staticType->property(id);
    if (propertyTypes.configured(constraints(), staticType)) {
        // The property has been reconfigured as non-configurable, non-enumerable
        // or non-writable.
        return jsop_setprop(name);
    }

    if (!TypeSetIncludes(propertyTypes.maybeTypes(), value->type(), value->resultTypeSet()))
        return jsop_setprop(name);

    current->pop();

    // Pop the bound object on the stack.
    MDefinition *obj = current->pop();
    JS_ASSERT(&obj->toConstant()->value().toObject() == staticObject);

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(obj, value));

    // If the property has a known type, we may be able to optimize typed stores by not
    // storing the type tag. This only works if the property does not have its initial
    // |undefined| value; if |undefined| is assigned at a later point, it will be added
    // to the type set.
    MIRType slotType = MIRType_None;
    if (!staticObject->getSlot(shape->slot()).isUndefined()) {
        JSValueType knownType = propertyTypes.knownTypeTag(constraints());
        if (knownType != JSVAL_TYPE_UNKNOWN)
            slotType = MIRTypeFromValueType(knownType);
    }

    bool needsBarrier = propertyTypes.needsBarrier(constraints());
    return storeSlot(obj, shape, value, needsBarrier, slotType);
}

bool
IonBuilder::jsop_getname(PropertyName *name)
{
    MDefinition *object;
    if (js_CodeSpec[*pc].format & JOF_GNAME) {
        MInstruction *global = MConstant::New(ObjectValue(script()->global()));
        current->add(global);
        object = global;
    } else {
        current->push(current->scopeChain());
        object = current->pop();
    }

    MGetNameCache *ins;
    if (JSOp(*GetNextPc(pc)) == JSOP_TYPEOF)
        ins = MGetNameCache::New(object, name, MGetNameCache::NAMETYPEOF);
    else
        ins = MGetNameCache::New(object, name, MGetNameCache::NAME);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    return pushTypeBarrier(ins, types, true);
}

bool
IonBuilder::jsop_intrinsic(PropertyName *name)
{
    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    JSValueType type = types->getKnownTypeTag();

    // If we haven't executed this opcode yet, we need to get the intrinsic
    // value and monitor the result.
    if (type == JSVAL_TYPE_UNKNOWN) {
        MCallGetIntrinsicValue *ins = MCallGetIntrinsicValue::New(name);

        current->add(ins);
        current->push(ins);

        if (!resumeAfter(ins))
            return false;

        return pushTypeBarrier(ins, types, true);
    }

    // Bake in the intrinsic. Make sure that TI agrees with us on the type.
    RootedPropertyName nameRoot(cx, name);
    RootedValue vp(cx, UndefinedValue());
    if (!cx->global()->getIntrinsicValue(cx, nameRoot, &vp))
        return false;

    JS_ASSERT(types->hasType(types::GetValueType(vp)));

    MConstant *ins = MConstant::New(vp);
    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_bindname(PropertyName *name)
{
    JS_ASSERT(analysis().usesScopeChain());

    MDefinition *scopeChain = current->scopeChain();
    MBindNameCache *ins = MBindNameCache::New(scopeChain, name, script(), pc);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

static JSValueType
GetElemKnownType(bool needsHoleCheck, types::TemporaryTypeSet *types)
{
    JSValueType knownType = types->getKnownTypeTag();

    // Null and undefined have no payload so they can't be specialized.
    // Since folding null/undefined while building SSA is not safe (see the
    // comment in IsPhiObservable), we just add an untyped load instruction
    // and rely on pushTypeBarrier and DCE to replace it with a null/undefined
    // constant.
    if (knownType == JSVAL_TYPE_UNDEFINED || knownType == JSVAL_TYPE_NULL)
        knownType = JSVAL_TYPE_UNKNOWN;

    // Different architectures may want typed element reads which require
    // hole checks to be done as either value or typed reads.
    if (needsHoleCheck && !LIRGenerator::allowTypedElementHoleCheck())
        knownType = JSVAL_TYPE_UNKNOWN;

    return knownType;
}

bool
IonBuilder::jsop_getelem()
{
    MDefinition *index = current->pop();
    MDefinition *obj = current->pop();

    bool emitted = false;

    if (!getElemTryDense(&emitted, obj, index) || emitted)
        return emitted;

    if (!getElemTryTypedStatic(&emitted, obj, index) || emitted)
        return emitted;

    if (!getElemTryTyped(&emitted, obj, index) || emitted)
        return emitted;

    if (!getElemTryString(&emitted, obj, index) || emitted)
        return emitted;

    if (!getElemTryArguments(&emitted, obj, index) || emitted)
        return emitted;

    if (!getElemTryArgumentsInlined(&emitted, obj, index) || emitted)
        return emitted;

    if (script()->argumentsHasVarBinding() && obj->mightBeType(MIRType_Magic))
        return abort("Type is not definitely lazy arguments.");

    if (!getElemTryCache(&emitted, obj, index) || emitted)
        return emitted;

    // Emit call.
    MInstruction *ins = MCallGetElement::New(obj, index);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    return pushTypeBarrier(ins, types, true);
}

bool
IonBuilder::getElemTryDense(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    if (!ElementAccessIsDenseNative(obj, index))
        return true;

    // Don't generate a fast path if there have been bounds check failures
    // and this access might be on a sparse property.
    if (ElementAccessHasExtraIndexedProperty(constraints(), obj) && failedBoundsCheck_)
        return true;

    // Don't generate a fast path if this pc has seen negative indexes accessed,
    // which will not appear to be extra indexed properties.
    if (inspector->hasSeenNegativeIndexGetElement(pc))
        return true;

    // Emit dense getelem variant.
    if (!jsop_getelem_dense(obj, index))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getElemTryTypedStatic(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    ScalarTypeRepresentation::Type arrayType;
    if (!ElementAccessIsTypedArray(obj, index, &arrayType))
        return true;

    if (!LIRGenerator::allowStaticTypedArrayAccesses())
        return true;

    if (ElementAccessHasExtraIndexedProperty(constraints(), obj))
        return true;

    if (!obj->resultTypeSet())
        return true;

    JSObject *tarrObj = obj->resultTypeSet()->getSingleton();
    if (!tarrObj)
        return true;

    TypedArrayObject *tarr = &tarrObj->as<TypedArrayObject>();
    ArrayBufferView::ViewType viewType = JS_GetArrayBufferViewType(tarr);

    // LoadTypedArrayElementStatic currently treats uint32 arrays as int32.
    if (viewType == ArrayBufferView::TYPE_UINT32)
        return true;

    MDefinition *ptr = convertShiftToMaskForStaticTypedArray(index, viewType);
    if (!ptr)
        return true;

    // Emit LoadTypedArrayElementStatic.

    obj->setFoldedUnchecked();
    index->setFoldedUnchecked();

    MLoadTypedArrayElementStatic *load = MLoadTypedArrayElementStatic::New(tarr, ptr);
    current->add(load);
    current->push(load);

    // The load is infallible if an undefined result will be coerced to the
    // appropriate numeric type if the read is out of bounds. The truncation
    // analysis picks up some of these cases, but is incomplete with respect
    // to others. For now, sniff the bytecode for simple patterns following
    // the load which guarantee a truncation or numeric conversion.
    if (viewType == ArrayBufferView::TYPE_FLOAT32 || viewType == ArrayBufferView::TYPE_FLOAT64) {
        jsbytecode *next = pc + JSOP_GETELEM_LENGTH;
        if (*next == JSOP_POS)
            load->setInfallible();
    } else {
        jsbytecode *next = pc + JSOP_GETELEM_LENGTH;
        if (*next == JSOP_ZERO && *(next + JSOP_ZERO_LENGTH) == JSOP_BITOR)
            load->setInfallible();
    }

    *emitted = true;
    return true;
}

bool
IonBuilder::getElemTryTyped(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    ScalarTypeRepresentation::Type arrayType;
    if (!ElementAccessIsTypedArray(obj, index, &arrayType))
        return true;

    // Emit typed getelem variant.
    if (!jsop_getelem_typed(obj, index, arrayType))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getElemTryString(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    if (obj->type() != MIRType_String)
        return true;

    // Emit fast path for string[index].
    MInstruction *idInt32 = MToInt32::New(index);
    current->add(idInt32);
    index = idInt32;

    MStringLength *length = MStringLength::New(obj);
    current->add(length);

    index = addBoundsCheck(index, length);

    MCharCodeAt *charCode = MCharCodeAt::New(obj, index);
    current->add(charCode);

    MFromCharCode *result = MFromCharCode::New(charCode);
    current->add(result);
    current->push(result);

    *emitted = true;
    return true;
}

bool
IonBuilder::getElemTryArguments(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    if (inliningDepth_ > 0)
        return true;

    if (obj->type() != MIRType_Magic)
        return true;

    // Emit GetFrameArgument.

    JS_ASSERT(!info().argsObjAliasesFormals());

    // Type Inference has guaranteed this is an optimized arguments object.
    obj->setFoldedUnchecked();

    // To ensure that we are not looking above the number of actual arguments.
    MArgumentsLength *length = MArgumentsLength::New();
    current->add(length);

    // Ensure index is an integer.
    MInstruction *idInt32 = MToInt32::New(index);
    current->add(idInt32);
    index = idInt32;

    // Bailouts if we read more than the number of actual arguments.
    index = addBoundsCheck(index, length);

    // Load the argument from the actual arguments.
    MGetFrameArgument *load = MGetFrameArgument::New(index, analysis_.hasSetArg());
    current->add(load);
    current->push(load);

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    if (!pushTypeBarrier(load, types, true))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getElemTryArgumentsInlined(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    if (inliningDepth_ == 0)
        return true;

    if (obj->type() != MIRType_Magic)
        return true;

    // Emit inlined arguments.
    obj->setFoldedUnchecked();

    JS_ASSERT(!info().argsObjAliasesFormals());

    // When the id is constant, we can just return the corresponding inlined argument
    if (index->isConstant() && index->toConstant()->value().isInt32()) {
        JS_ASSERT(inliningDepth_ > 0);

        int32_t id = index->toConstant()->value().toInt32();
        index->setFoldedUnchecked();

        if (id < (int32_t)inlineCallInfo_->argc() && id >= 0)
            current->push(inlineCallInfo_->getArg(id));
        else
            pushConstant(UndefinedValue());

        *emitted = true;
        return true;
    }

    // inlined not constant not supported, yet.
    return abort("NYI inlined not constant get argument element");
}

bool
IonBuilder::getElemTryCache(bool *emitted, MDefinition *obj, MDefinition *index)
{
    JS_ASSERT(*emitted == false);

    // Make sure we have at least an object.
    if (!obj->mightBeType(MIRType_Object))
        return true;

    // Don't cache for strings.
    if (obj->mightBeType(MIRType_String))
        return true;

    // Index should be integer or string
    if (!index->mightBeType(MIRType_Int32) && !index->mightBeType(MIRType_String))
        return true;

    // Turn off cacheing if the element is int32 and we've seen non-native objects as the target
    // of this getelem.
    bool nonNativeGetElement = inspector->hasSeenNonNativeGetElement(pc);
    if (index->mightBeType(MIRType_Int32) && nonNativeGetElement)
        return true;

    // Emit GetElementCache.

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    bool barrier = PropertyReadNeedsTypeBarrier(cx, context(), constraints(), obj, nullptr, types);

    // Always add a barrier if the index might be a string, so that the cache
    // can attach stubs for particular properties.
    if (index->mightBeType(MIRType_String))
        barrier = true;

    // See note about always needing a barrier in jsop_getprop.
    if (needsToMonitorMissingProperties(types))
        barrier = true;

    MInstruction *ins = MGetElementCache::New(obj, index, barrier);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    // Spice up type information.
    if (index->type() == MIRType_Int32 && !barrier) {
        bool needHoleCheck = !ElementAccessIsPacked(constraints(), obj);
        JSValueType knownType = GetElemKnownType(needHoleCheck, types);

        if (knownType != JSVAL_TYPE_UNKNOWN && knownType != JSVAL_TYPE_DOUBLE)
            ins->setResultType(MIRTypeFromValueType(knownType));
    }

    if (!pushTypeBarrier(ins, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::jsop_getelem_dense(MDefinition *obj, MDefinition *index)
{
    types::TemporaryTypeSet *types = bytecodeTypes(pc);

    if (JSOp(*pc) == JSOP_CALLELEM && !index->mightBeType(MIRType_String)) {
        // Indexed call on an element of an array. Populate the observed types
        // with any objects that could be in the array, to avoid extraneous
        // type barriers.
        if (!AddObjectsForPropertyRead(obj, nullptr, types))
            return false;
    }

    bool barrier = PropertyReadNeedsTypeBarrier(cx, context(), constraints(), obj, nullptr, types);
    bool needsHoleCheck = !ElementAccessIsPacked(constraints(), obj);

    // Reads which are on holes in the object do not have to bail out if
    // undefined values have been observed at this access site and the access
    // cannot hit another indexed property on the object or its prototypes.
    bool readOutOfBounds =
        types->hasType(types::Type::UndefinedType()) &&
        !ElementAccessHasExtraIndexedProperty(constraints(), obj);

    JSValueType knownType = JSVAL_TYPE_UNKNOWN;
    if (!barrier)
        knownType = GetElemKnownType(needsHoleCheck, types);

    // Ensure index is an integer.
    MInstruction *idInt32 = MToInt32::New(index);
    current->add(idInt32);
    index = idInt32;

    // Get the elements vector.
    MInstruction *elements = MElements::New(obj);
    current->add(elements);

    // Note: to help GVN, use the original MElements instruction and not
    // MConvertElementsToDoubles as operand. This is fine because converting
    // elements to double does not change the initialized length.
    MInitializedLength *initLength = MInitializedLength::New(elements);
    current->add(initLength);

    // If we can load the element as a definite double, make sure to check that
    // the array has been converted to homogenous doubles first.
    //
    // NB: We disable this optimization in parallel execution mode
    // because it is inherently not threadsafe (how do you convert the
    // array atomically when there might be concurrent readers)?
    types::TemporaryTypeSet *objTypes = obj->resultTypeSet();
    ExecutionMode executionMode = info().executionMode();
    bool loadDouble =
        executionMode == SequentialExecution &&
        !barrier &&
        loopDepth_ &&
        !readOutOfBounds &&
        !needsHoleCheck &&
        knownType == JSVAL_TYPE_DOUBLE &&
        objTypes &&
        objTypes->convertDoubleElements(constraints()) == types::TemporaryTypeSet::AlwaysConvertToDoubles;
    if (loadDouble)
        elements = addConvertElementsToDoubles(elements);

    MInstruction *load;

    if (!readOutOfBounds) {
        // This load should not return undefined, so likely we're reading
        // in-bounds elements, and the array is packed or its holes are not
        // read. This is the best case: we can separate the bounds check for
        // hoisting.
        index = addBoundsCheck(index, initLength);

        load = MLoadElement::New(elements, index, needsHoleCheck, loadDouble);
        current->add(load);
    } else {
        // This load may return undefined, so assume that we *can* read holes,
        // or that we can read out-of-bounds accesses. In this case, the bounds
        // check is part of the opcode.
        load = MLoadElementHole::New(elements, index, initLength, needsHoleCheck);
        current->add(load);

        // If maybeUndefined was true, the typeset must have undefined, and
        // then either additional types or a barrier. This means we should
        // never have a typed version of LoadElementHole.
        JS_ASSERT(knownType == JSVAL_TYPE_UNKNOWN);
    }

    if (knownType != JSVAL_TYPE_UNKNOWN)
        load->setResultType(MIRTypeFromValueType(knownType));

    current->push(load);
    return pushTypeBarrier(load, types, barrier);
}

MInstruction *
IonBuilder::getTypedArrayLength(MDefinition *obj)
{
    if (obj->isConstant() && obj->toConstant()->value().isObject()) {
        TypedArrayObject *tarr = &obj->toConstant()->value().toObject().as<TypedArrayObject>();
        int32_t length = (int32_t) tarr->length();
        obj->setFoldedUnchecked();
        return MConstant::New(Int32Value(length));
    }
    return MTypedArrayLength::New(obj);
}

MInstruction *
IonBuilder::getTypedArrayElements(MDefinition *obj)
{
    if (obj->isConstant() && obj->toConstant()->value().isObject()) {
        TypedArrayObject *tarr = &obj->toConstant()->value().toObject().as<TypedArrayObject>();
        void *data = tarr->viewData();

        // The 'data' pointer can change in rare circumstances
        // (ArrayBufferObject::changeContents).
        types::TypeObjectKey *tarrType = types::TypeObjectKey::get(tarr);
        tarrType->watchStateChangeForTypedArrayBuffer(constraints());

        obj->setFoldedUnchecked();
        return MConstantElements::New(data);
    }
    return MTypedArrayElements::New(obj);
}

MDefinition *
IonBuilder::convertShiftToMaskForStaticTypedArray(MDefinition *id,
                                                  ArrayBufferView::ViewType viewType)
{
    // No shifting is necessary if the typed array has single byte elements.
    if (TypedArrayShift(viewType) == 0)
        return id;

    // If the index is an already shifted constant, undo the shift to get the
    // absolute offset being accessed.
    if (id->isConstant() && id->toConstant()->value().isInt32()) {
        int32_t index = id->toConstant()->value().toInt32();
        MConstant *offset = MConstant::New(Int32Value(index << TypedArrayShift(viewType)));
        current->add(offset);
        return offset;
    }

    if (!id->isRsh() || id->isEffectful())
        return nullptr;
    if (!id->getOperand(1)->isConstant())
        return nullptr;
    const Value &value = id->getOperand(1)->toConstant()->value();
    if (!value.isInt32() || uint32_t(value.toInt32()) != TypedArrayShift(viewType))
        return nullptr;

    // Instead of shifting, mask off the low bits of the index so that
    // a non-scaled access on the typed array can be performed.
    MConstant *mask = MConstant::New(Int32Value(~((1 << value.toInt32()) - 1)));
    MBitAnd *ptr = MBitAnd::New(id->getOperand(0), mask);

    ptr->infer(nullptr, nullptr);
    JS_ASSERT(!ptr->isEffectful());

    current->add(mask);
    current->add(ptr);

    return ptr;
}

static MIRType
MIRTypeForTypedArrayRead(ScalarTypeRepresentation::Type arrayType,
                         bool observedDouble)
{
    switch (arrayType) {
      case ScalarTypeRepresentation::TYPE_INT8:
      case ScalarTypeRepresentation::TYPE_UINT8:
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
      case ScalarTypeRepresentation::TYPE_INT16:
      case ScalarTypeRepresentation::TYPE_UINT16:
      case ScalarTypeRepresentation::TYPE_INT32:
        return MIRType_Int32;
      case ScalarTypeRepresentation::TYPE_UINT32:
        return observedDouble ? MIRType_Double : MIRType_Int32;
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        return (LIRGenerator::allowFloat32Optimizations()) ? MIRType_Float32 : MIRType_Double;
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        return MIRType_Double;
    }
    MOZ_ASSUME_UNREACHABLE("Unknown typed array type");
}

bool
IonBuilder::jsop_getelem_typed(MDefinition *obj, MDefinition *index,
                               ScalarTypeRepresentation::Type arrayType)
{
    types::TemporaryTypeSet *types = bytecodeTypes(pc);

    bool maybeUndefined = types->hasType(types::Type::UndefinedType());

    // Reading from an Uint32Array will result in a double for values
    // that don't fit in an int32. We have to bailout if this happens
    // and the instruction is not known to return a double.
    bool allowDouble = types->hasType(types::Type::DoubleType());

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(index);
    current->add(idInt32);
    index = idInt32;

    if (!maybeUndefined) {
        // Assume the index is in range, so that we can hoist the length,
        // elements vector and bounds check.

        // If we are reading in-bounds elements, we can use knowledge about
        // the array type to determine the result type, even if the opcode has
        // never executed. The known pushed type is only used to distinguish
        // uint32 reads that may produce either doubles or integers.
        MIRType knownType = MIRTypeForTypedArrayRead(arrayType, allowDouble);

        // Get the length.
        MInstruction *length = getTypedArrayLength(obj);
        current->add(length);

        // Bounds check.
        index = addBoundsCheck(index, length);

        // Get the elements vector.
        MInstruction *elements = getTypedArrayElements(obj);
        current->add(elements);

        // Load the element.
        MLoadTypedArrayElement *load = MLoadTypedArrayElement::New(elements, index, arrayType);
        current->add(load);
        current->push(load);

        // Note: we can ignore the type barrier here, we know the type must
        // be valid and unbarriered.
        load->setResultType(knownType);
        return true;
    } else {
        // We need a type barrier if the array's element type has never been
        // observed (we've only read out-of-bounds values). Note that for
        // Uint32Array, we only check for int32: if allowDouble is false we
        // will bailout when we read a double.
        bool needsBarrier = true;
        switch (arrayType) {
          case ScalarTypeRepresentation::TYPE_INT8:
          case ScalarTypeRepresentation::TYPE_UINT8:
          case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
          case ScalarTypeRepresentation::TYPE_INT16:
          case ScalarTypeRepresentation::TYPE_UINT16:
          case ScalarTypeRepresentation::TYPE_INT32:
          case ScalarTypeRepresentation::TYPE_UINT32:
            if (types->hasType(types::Type::Int32Type()))
                needsBarrier = false;
            break;
          case ScalarTypeRepresentation::TYPE_FLOAT32:
          case ScalarTypeRepresentation::TYPE_FLOAT64:
            if (allowDouble)
                needsBarrier = false;
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unknown typed array type");
        }

        // Assume we will read out-of-bound values. In this case the
        // bounds check will be part of the instruction, and the instruction
        // will always return a Value.
        MLoadTypedArrayElementHole *load =
            MLoadTypedArrayElementHole::New(obj, index, arrayType, allowDouble);
        current->add(load);
        current->push(load);

        return pushTypeBarrier(load, types, needsBarrier);
    }
}

bool
IonBuilder::jsop_setelem()
{
    bool emitted = false;

    MDefinition *value = current->pop();
    MDefinition *index = current->pop();
    MDefinition *object = current->pop();

    if (!setElemTryTypedStatic(&emitted, object, index, value) || emitted)
        return emitted;

    if (!setElemTryTyped(&emitted, object, index, value) || emitted)
        return emitted;

    if (!setElemTryDense(&emitted, object, index, value) || emitted)
        return emitted;

    if (!setElemTryArguments(&emitted, object, index, value) || emitted)
        return emitted;

    if (script()->argumentsHasVarBinding() && object->mightBeType(MIRType_Magic))
        return abort("Type is not definitely lazy arguments.");

    if (!setElemTryCache(&emitted, object, index, value) || emitted)
        return emitted;

    // Emit call.
    MInstruction *ins = MCallSetElement::New(object, index, value);
    current->add(ins);
    current->push(value);

    return resumeAfter(ins);
}

bool
IonBuilder::setElemTryTypedStatic(bool *emitted, MDefinition *object,
                                  MDefinition *index, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    ScalarTypeRepresentation::Type arrayType;
    if (!ElementAccessIsTypedArray(object, index, &arrayType))
        return true;

    if (!LIRGenerator::allowStaticTypedArrayAccesses())
        return true;

    if (ElementAccessHasExtraIndexedProperty(constraints(), object))
        return true;

    if (!object->resultTypeSet())
        return true;
    JSObject *tarrObj = object->resultTypeSet()->getSingleton();
    if (!tarrObj)
        return true;

    TypedArrayObject *tarr = &tarrObj->as<TypedArrayObject>();
    ArrayBufferView::ViewType viewType = JS_GetArrayBufferViewType(tarr);

    MDefinition *ptr = convertShiftToMaskForStaticTypedArray(index, viewType);
    if (!ptr)
        return true;

    // Emit StoreTypedArrayElementStatic.
    object->setFoldedUnchecked();
    index->setFoldedUnchecked();

    // Clamp value to [0, 255] for Uint8ClampedArray.
    MDefinition *toWrite = value;
    if (viewType == ArrayBufferView::TYPE_UINT8_CLAMPED) {
        toWrite = MClampToUint8::New(value);
        current->add(toWrite->toInstruction());
    }

    MInstruction *store = MStoreTypedArrayElementStatic::New(tarr, ptr, toWrite);
    current->add(store);
    current->push(value);

    if (!resumeAfter(store))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setElemTryTyped(bool *emitted, MDefinition *object,
                            MDefinition *index, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    ScalarTypeRepresentation::Type arrayType;
    if (!ElementAccessIsTypedArray(object, index, &arrayType))
        return true;

    // Emit typed setelem variant.
    if (!jsop_setelem_typed(arrayType, SetElem_Normal, object, index, value))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setElemTryDense(bool *emitted, MDefinition *object,
                            MDefinition *index, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    if (!ElementAccessIsDenseNative(object, index))
        return true;
    if (PropertyWriteNeedsTypeBarrier(constraints(), current,
                                      &object, nullptr, &value, /* canModify = */ true))
    {
        return true;
    }
    if (!object->resultTypeSet())
        return true;

    types::TemporaryTypeSet::DoubleConversion conversion =
        object->resultTypeSet()->convertDoubleElements(constraints());

    // If AmbiguousDoubleConversion, only handle int32 values for now.
    if (conversion == types::TemporaryTypeSet::AmbiguousDoubleConversion &&
        value->type() != MIRType_Int32)
    {
        return true;
    }

    // Don't generate a fast path if there have been bounds check failures
    // and this access might be on a sparse property.
    if (ElementAccessHasExtraIndexedProperty(constraints(), object) && failedBoundsCheck_)
        return true;

    // Emit dense setelem variant.
    if (!jsop_setelem_dense(conversion, SetElem_Normal, object, index, value))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setElemTryArguments(bool *emitted, MDefinition *object,
                                MDefinition *index, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    if (object->type() != MIRType_Magic)
        return true;

    // Arguments are not supported yet.
    return abort("NYI arguments[]=");
}

bool
IonBuilder::setElemTryCache(bool *emitted, MDefinition *object,
                            MDefinition *index, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    if (!object->mightBeType(MIRType_Object))
        return true;

    if (!index->mightBeType(MIRType_Int32) && !index->mightBeType(MIRType_String))
        return true;

    // TODO: Bug 876650: remove this check:
    // Temporary disable the cache if non dense native,
    // until the cache supports more ics
    SetElemICInspector icInspect(inspector->setElemICInspector(pc));
    if (!icInspect.sawDenseWrite() && !icInspect.sawTypedArrayWrite())
        return true;

    if (PropertyWriteNeedsTypeBarrier(constraints(), current,
                                      &object, nullptr, &value, /* canModify = */ true))
    {
        return true;
    }

    // We can avoid worrying about holes in the IC if we know a priori we are safe
    // from them. If TI can guard that there are no indexed properties on the prototype
    // chain, we know that we anen't missing any setters by overwriting the hole with
    // another value.
    bool guardHoles = ElementAccessHasExtraIndexedProperty(constraints(), object);

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(object, value));

    // Emit SetElementCache.
    MInstruction *ins = MSetElementCache::New(object, index, value, script()->strict, guardHoles);
    current->add(ins);
    current->push(value);

    if (!resumeAfter(ins))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::jsop_setelem_dense(types::TemporaryTypeSet::DoubleConversion conversion,
                               SetElemSafety safety,
                               MDefinition *obj, MDefinition *id, MDefinition *value)
{
    MIRType elementType = DenseNativeElementType(constraints(), obj);
    bool packed = ElementAccessIsPacked(constraints(), obj);

    // Writes which are on holes in the object do not have to bail out if they
    // cannot hit another indexed property on the object or its prototypes.
    bool writeOutOfBounds = !ElementAccessHasExtraIndexedProperty(constraints(), obj);

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(obj, value));

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    // Ensure the value is a double, if double conversion might be needed.
    MDefinition *newValue = value;
    switch (conversion) {
      case types::TemporaryTypeSet::AlwaysConvertToDoubles:
      case types::TemporaryTypeSet::MaybeConvertToDoubles: {
        MInstruction *valueDouble = MToDouble::New(value);
        current->add(valueDouble);
        newValue = valueDouble;
        break;
      }

      case types::TemporaryTypeSet::AmbiguousDoubleConversion: {
        JS_ASSERT(value->type() == MIRType_Int32);
        MInstruction *maybeDouble = MMaybeToDoubleElement::New(elements, value);
        current->add(maybeDouble);
        newValue = maybeDouble;
        break;
      }

      case types::TemporaryTypeSet::DontConvertToDoubles:
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Unknown double conversion");
    }

    bool writeHole = false;
    if (safety == SetElem_Normal) {
        SetElemICInspector icInspect(inspector->setElemICInspector(pc));
        writeHole = icInspect.sawOOBDenseWrite();
    }

    // Use MStoreElementHole if this SETELEM has written to out-of-bounds
    // indexes in the past. Otherwise, use MStoreElement so that we can hoist
    // the initialized length and bounds check.
    MStoreElementCommon *store;
    if (writeHole && writeOutOfBounds) {
        JS_ASSERT(safety == SetElem_Normal);

        MStoreElementHole *ins = MStoreElementHole::New(obj, elements, id, newValue);
        store = ins;

        current->add(ins);
        current->push(value);

        if (!resumeAfter(ins))
            return false;
    } else {
        MInitializedLength *initLength = MInitializedLength::New(elements);
        current->add(initLength);

        bool needsHoleCheck;
        if (safety == SetElem_Normal) {
            id = addBoundsCheck(id, initLength);
            needsHoleCheck = !packed && !writeOutOfBounds;
        } else {
            needsHoleCheck = false;
        }

        MStoreElement *ins = MStoreElement::New(elements, id, newValue, needsHoleCheck);
        store = ins;

        if (safety == SetElem_Unsafe)
            ins->setRacy();

        current->add(ins);

        if (safety == SetElem_Normal)
            current->push(value);

        if (!resumeAfter(ins))
            return false;
    }

    // Determine whether a write barrier is required.
    if (obj->resultTypeSet()->propertyNeedsBarrier(constraints(), JSID_VOID))
        store->setNeedsBarrier();

    if (elementType != MIRType_None && packed)
        store->setElementType(elementType);

    return true;
}


bool
IonBuilder::jsop_setelem_typed(ScalarTypeRepresentation::Type arrayType,
                               SetElemSafety safety,
                               MDefinition *obj, MDefinition *id, MDefinition *value)
{
    bool expectOOB;
    if (safety == SetElem_Normal) {
        SetElemICInspector icInspect(inspector->setElemICInspector(pc));
        expectOOB = icInspect.sawOOBTypedArrayWrite();
    } else {
        expectOOB = false;
    }

    if (expectOOB)
        spew("Emitting OOB TypedArray SetElem");

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the length.
    MInstruction *length = getTypedArrayLength(obj);
    current->add(length);

    if (!expectOOB && safety == SetElem_Normal) {
        // Bounds check.
        id = addBoundsCheck(id, length);
    }

    // Get the elements vector.
    MInstruction *elements = getTypedArrayElements(obj);
    current->add(elements);

    // Clamp value to [0, 255] for Uint8ClampedArray.
    MDefinition *toWrite = value;
    if (arrayType == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED) {
        toWrite = MClampToUint8::New(value);
        current->add(toWrite->toInstruction());
    }

    // Store the value.
    MInstruction *ins;
    if (expectOOB) {
        ins = MStoreTypedArrayElementHole::New(elements, length, id, toWrite, arrayType);
    } else {
        MStoreTypedArrayElement *store =
            MStoreTypedArrayElement::New(elements, id, toWrite, arrayType);
        if (safety == SetElem_Unsafe)
            store->setRacy();
        ins = store;
    }

    current->add(ins);

    if (safety == SetElem_Normal)
        current->push(value);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_length()
{
    if (jsop_length_fastPath())
        return true;

    PropertyName *name = info().getAtom(pc)->asPropertyName();
    return jsop_getprop(name);
}

bool
IonBuilder::jsop_length_fastPath()
{
    types::TemporaryTypeSet *types = bytecodeTypes(pc);

    if (types->getKnownTypeTag() != JSVAL_TYPE_INT32)
        return false;

    MDefinition *obj = current->peek(-1);

    if (obj->mightBeType(MIRType_String)) {
        if (obj->mightBeType(MIRType_Object))
            return false;
        current->pop();
        MStringLength *ins = MStringLength::New(obj);
        current->add(ins);
        current->push(ins);
        return true;
    }

    if (obj->mightBeType(MIRType_Object)) {
        types::TemporaryTypeSet *objTypes = obj->resultTypeSet();

        if (objTypes &&
            objTypes->getKnownClass() == &ArrayObject::class_ &&
            !objTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_LENGTH_OVERFLOW))
        {
            current->pop();
            MElements *elements = MElements::New(obj);
            current->add(elements);

            // Read length.
            MArrayLength *length = new MArrayLength(elements);
            current->add(length);
            current->push(length);
            return true;
        }

        if (objTypes && objTypes->getTypedArrayType() != ScalarTypeRepresentation::TYPE_MAX) {
            current->pop();
            MInstruction *length = getTypedArrayLength(obj);
            current->add(length);
            current->push(length);
            return true;
        }
    }

    return false;
}

bool
IonBuilder::jsop_arguments()
{
    if (info().needsArgsObj()) {
        current->push(current->argumentsObject());
        return true;
    }
    JS_ASSERT(lazyArguments_);
    current->push(lazyArguments_);
    return true;
}

bool
IonBuilder::jsop_arguments_length()
{
    // Type Inference has guaranteed this is an optimized arguments object.
    MDefinition *args = current->pop();
    args->setFoldedUnchecked();

    // We don't know anything from the callee
    if (inliningDepth_ == 0) {
        MInstruction *ins = MArgumentsLength::New();
        current->add(ins);
        current->push(ins);
        return true;
    }

    // We are inlining and know the number of arguments the callee pushed
    return pushConstant(Int32Value(inlineCallInfo_->argv().length()));
}

static JSObject *
CreateRestArgumentsTemplateObject(JSContext *cx, unsigned length)
{
    JSObject *templateObject = NewDenseUnallocatedArray(cx, length, nullptr, TenuredObject);
    if (templateObject)
        types::FixRestArgumentsType(cx, templateObject);
    return templateObject;
}

bool
IonBuilder::jsop_rest()
{
    // We don't know anything about the callee.
    if (inliningDepth_ == 0) {
        JSObject *templateObject = CreateRestArgumentsTemplateObject(cx, 0);
        if (!templateObject)
            return false;
        MArgumentsLength *numActuals = MArgumentsLength::New();
        current->add(numActuals);

        // Pass in the number of actual arguments, the number of formals (not
        // including the rest parameter slot itself), and the template object.
        MRest *rest = MRest::New(numActuals, info().nargs() - 1, templateObject);
        current->add(rest);
        current->push(rest);
        return true;
    }

    // We know the exact number of arguments the callee pushed.
    unsigned numActuals = inlineCallInfo_->argv().length();
    unsigned numFormals = info().nargs() - 1;
    unsigned numRest = numActuals > numFormals ? numActuals - numFormals : 0;
    JSObject *templateObject = CreateRestArgumentsTemplateObject(cx, numRest);
    if (!templateObject)
        return false;

    MNewArray *array = new MNewArray(numRest, templateObject, MNewArray::NewArray_Allocating);
    current->add(array);

    if (numActuals <= numFormals) {
        current->push(array);
        return true;
    }

    MElements *elements = MElements::New(array);
    current->add(elements);

    // Unroll the argument copy loop. We don't need to do any bounds or hole
    // checking here.
    MConstant *index;
    for (unsigned i = numFormals; i < numActuals; i++) {
        index = MConstant::New(Int32Value(i - numFormals));
        current->add(index);

        MDefinition *arg = inlineCallInfo_->argv()[i];
        MStoreElement *store = MStoreElement::New(elements, index, arg,
                                                  /* needsHoleCheck = */ false);
        current->add(store);
    }

    MSetInitializedLength *initLength = MSetInitializedLength::New(elements, index);
    current->add(initLength);
    current->push(array);

    return true;
}

bool
IonBuilder::getDefiniteSlot(types::TemporaryTypeSet *types, PropertyName *name,
                            types::HeapTypeSetKey *property)
{
    if (!types || types->unknownObject() || types->getObjectCount() != 1)
        return false;

    types::TypeObjectKey *type = types->getObject(0);
    if (type->unknownProperties())
        return false;

    jsid id = NameToId(name);

    *property = type->property(id);
    return property->maybeTypes() &&
           property->maybeTypes()->definiteProperty() &&
           !property->configured(constraints(), type);
}

bool
IonBuilder::jsop_runonce()
{
    MRunOncePrologue *ins = MRunOncePrologue::New();
    current->add(ins);
    return resumeAfter(ins);
}

bool
IonBuilder::jsop_not()
{
    MDefinition *value = current->pop();

    MNot *ins = new MNot(value);
    current->add(ins);
    current->push(ins);
    ins->infer();
    return true;
}

static inline bool
TestClassHasAccessorHook(const Class *clasp, bool isGetter)
{
    if (isGetter && clasp->ops.getGeneric)
        return true;
    if (!isGetter && clasp->ops.setGeneric)
        return true;
    return false;
}

static inline bool
TestTypeHasOwnProperty(types::TypeObjectKey *typeObj, PropertyName *name, bool &cont)
{
    cont = true;
    types::HeapTypeSetKey propSet = typeObj->property(NameToId(name));
    if (propSet.maybeTypes() && !propSet.maybeTypes()->empty())
        cont = false;
    // Note: Callers must explicitly freeze the property type set later on if optimizing.
    return true;
}

static inline bool
TestCommonAccessorProtoChain(JSContext *cx, PropertyName *name,
                             bool isGetter, JSObject *foundProto,
                             JSObject *obj, bool &cont)
{
    cont = false;
    JSObject *curObj = obj;
    JSObject *stopAt = foundProto->getProto();
    while (curObj != stopAt) {
        // Don't optimize if we have a hook that would have to be called.
        if (TestClassHasAccessorHook(curObj->getClass(), isGetter))
            return true;

        // Check here to make sure that everyone has Type Objects with known
        // properties between them and the proto we found the accessor on. We
        // need those to add freezes safely. NOTE: We do not do this above, as
        // we may be able to freeze all the types up to where we found the
        // property, even if there are unknown types higher in the prototype
        // chain.
        if (curObj != foundProto) {
            types::TypeObjectKey *typeObj = types::TypeObjectKey::get(curObj);
            if (typeObj->unknownProperties())
                return true;

            // Check here to make sure that nobody on the prototype chain is
            // marked as having the property as an "own property". This can
            // happen in cases of |delete| having been used, or cases with
            // watched objects. If TI ever decides to be more accurate about
            // |delete| handling, this should go back to curObj->watched().

            // Even though we are not directly accessing the properties on the whole
            // prototype chain, we need to fault in the sets anyway, as we need
            // to freeze on them.
            bool lcont;
            if (!TestTypeHasOwnProperty(typeObj, name, lcont))
                return false;
            if (!lcont)
                return true;
        }

        curObj = curObj->getProto();
    }
    cont = true;
    return true;
}

static inline bool
SearchCommonPropFunc(JSContext *cx, types::TemporaryTypeSet *types,
                     PropertyName *name, bool isGetter,
                     JSObject *&found, JSObject *&foundProto, bool &cont)
{
    cont = false;
    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        RootedObject curObj(cx, types->getSingleObject(i));

        // Non-Singleton type
        if (!curObj) {
            types::TypeObjectKey *typeObj = types->getObject(i);
            if (!typeObj)
                continue;

            if (typeObj->unknownProperties())
                return true;

            // If the class of the object has a hook, we can't
            // inline, as we would need to call the hook.
            if (TestClassHasAccessorHook(typeObj->clasp(), isGetter))
                return true;

            // If the type has an own property, we can't be sure we don't shadow
            // the chain.
            bool lcont;
            if (!TestTypeHasOwnProperty(typeObj, name, lcont))
                return false;
            if (!lcont)
                return true;

            // Otherwise try using the prototype.
            curObj = typeObj->proto().toObjectOrNull();
        } else {
            // We can't optimize setters on watched singleton objects. A getter
            // on an own property can be protected with the prototype
            // shapeguard, though.
            if (!isGetter && curObj->watched())
                return true;
        }

        // Turns out that we need to check for a property lookup op, else we
        // will end up calling it mid-compilation.
        if (!CanEffectlesslyCallLookupGenericOnObject(cx, curObj, name))
            return true;

        RootedId idRoot(cx, NameToId(name));
        RootedObject proto(cx);
        RootedShape shape(cx);
        if (!JSObject::lookupGeneric(cx, curObj, idRoot, &proto, &shape))
            return false;

        if (!shape)
            return true;

        // We want to optimize specialized getters/setters. The defaults will
        // hit the slot optimization.
        if (isGetter) {
            if (shape->hasDefaultGetter() || !shape->hasGetterValue())
                return true;
        } else {
            if (shape->hasDefaultSetter() || !shape->hasSetterValue())
                return true;
        }

        JSObject * curFound = isGetter ? shape->getterObject():
                                         shape->setterObject();

        // Save the first seen, or verify uniqueness.
        if (!found) {
            if (!curFound->is<JSFunction>())
                return true;
            found = curFound;
        } else if (found != curFound) {
            return true;
        }

        // We only support cases with a single prototype shared. This is
        // overwhelmingly more likely than having multiple different prototype
        // chains with the same custom property function.
        if (!foundProto)
            foundProto = proto;
        else if (foundProto != proto)
            return true;

        bool lcont;
        if (!TestCommonAccessorProtoChain(cx, name, isGetter, foundProto, curObj, lcont))
            return false;
        if (!lcont)
            return true;
    }
    cont = true;
    return true;
}

bool
IonBuilder::freezePropTypeSets(types::TemporaryTypeSet *types,
                               JSObject *foundProto, PropertyName *name)
{
    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        // If we found a Singleton object's own-property, there's nothing to
        // freeze.
        if (types->getSingleObject(i) == foundProto)
            continue;

        types::TypeObjectKey *type = types->getObject(i);
        if (!type)
            continue;

        // Walk the prototype chain. Everyone has to have the property, since we
        // just checked, so propSet cannot be nullptr.
        while (true) {
            types::HeapTypeSetKey property = type->property(NameToId(name));
            JS_ALWAYS_TRUE(!property.notEmpty(constraints()));

            // Don't mark the proto. It will be held down by the shape
            // guard. This allows us to use properties found on prototypes
            // with properties unknown to TI.
            if (type->proto() == foundProto)
                break;
            type = types::TypeObjectKey::get(type->proto().toObjectOrNull());
        }
    }
    return true;
}

inline bool
IonBuilder::testCommonPropFunc(JSContext *cx, types::TemporaryTypeSet *types, PropertyName *name,
                               JSFunction **funcp, bool isGetter, bool *isDOM,
                               MDefinition **guardOut)
{
    JSObject *found = nullptr;
    JSObject *foundProto = nullptr;

    *funcp = nullptr;
    *isDOM = false;

    // No sense looking if we don't know what's going on.
    if (!types || types->unknownObject())
        return true;

    // Iterate down all the types to see if they all have the same getter or
    // setter.
    bool cont;
    if (!SearchCommonPropFunc(cx, types, name, isGetter, found, foundProto, cont))
        return false;
    if (!cont)
        return true;

    // No need to add a freeze if we didn't find anything
    if (!found)
        return true;

    JS_ASSERT(foundProto);

    // Add a shape guard on the prototype we found the property on. The rest of
    // the prototype chain is guarded by TI freezes. Note that a shape guard is
    // good enough here, even in the proxy case, because we have ensured there
    // are no lookup hooks for this property.
    MInstruction *wrapper = MConstant::New(ObjectValue(*foundProto));
    current->add(wrapper);
    wrapper = addShapeGuard(wrapper, foundProto->lastProperty(), Bailout_ShapeGuard);

    // Pass the guard back so it can be an operand.
    if (guardOut) {
        JS_ASSERT(wrapper->isGuardShape());
        *guardOut = wrapper;
    }

    // Now we have to freeze all the property typesets to ensure there isn't a
    // lower shadowing getter or setter installed in the future.
    if (!freezePropTypeSets(types, foundProto, name))
        return false;

    *funcp = &found->as<JSFunction>();
    *isDOM = types->isDOMClass();

    return true;
}

bool
IonBuilder::annotateGetPropertyCache(JSContext *cx, MDefinition *obj, MGetPropertyCache *getPropCache,
                                    types::TemporaryTypeSet *objTypes, types::TemporaryTypeSet *pushedTypes)
{
    PropertyName *name = getPropCache->name();

    // Ensure every pushed value is a singleton.
    if (pushedTypes->unknownObject() || pushedTypes->baseFlags() != 0)
        return true;

    for (unsigned i = 0; i < pushedTypes->getObjectCount(); i++) {
        if (pushedTypes->getTypeObject(i) != nullptr)
            return true;
    }

    // Object's typeset should be a proper object
    if (!objTypes || objTypes->baseFlags() || objTypes->unknownObject())
        return true;

    unsigned int objCount = objTypes->getObjectCount();
    if (objCount == 0)
        return true;

    InlinePropertyTable *inlinePropTable = getPropCache->initInlinePropertyTable(pc);
    if (!inlinePropTable)
        return false;

    // Ensure that the relevant property typeset for each type object is
    // is a single-object typeset containing a JSFunction
    for (unsigned int i = 0; i < objCount; i++) {
        types::TypeObject *baseTypeObj = objTypes->getTypeObject(i);
        if (!baseTypeObj)
            continue;
        types::TypeObjectKey *typeObj = types::TypeObjectKey::get(baseTypeObj);
        if (typeObj->unknownProperties() || !typeObj->proto().isObject())
            continue;

        types::HeapTypeSetKey ownTypes = typeObj->property(NameToId(name));
        if (ownTypes.notEmpty(constraints()))
            continue;

        JSObject *singleton = nullptr;
        JSObject *proto = typeObj->proto().toObject();
        while (true) {
            types::TypeObjectKey *protoType = types::TypeObjectKey::get(proto);
            if (!protoType->unknownProperties()) {
                types::HeapTypeSetKey property = protoType->property(NameToId(name));

                singleton = property.singleton(constraints());
                if (singleton) {
                    if (singleton->is<JSFunction>())
                        break;
                    singleton = nullptr;
                }
            }
            TaggedProto taggedProto = proto->getTaggedProto();
            if (!taggedProto.isObject())
                break;
            proto = taggedProto.toObject();
        }
        if (!singleton)
            continue;

        bool knownConstant = false;
        if (!testSingletonProperty(proto, singleton, name, &knownConstant))
            return false;

        // Don't add cases corresponding to non-observed pushes
        if (!pushedTypes->hasType(types::Type::ObjectType(singleton)))
            continue;

        if (!inlinePropTable->addEntry(baseTypeObj, &singleton->as<JSFunction>()))
            return false;
    }

    if (inlinePropTable->numEntries() == 0) {
        getPropCache->clearInlinePropertyTable();
        return true;
    }

#ifdef DEBUG
    if (inlinePropTable->numEntries() > 0)
        IonSpew(IonSpew_Inlining, "Annotated GetPropertyCache with %d/%d inline cases",
                                    (int) inlinePropTable->numEntries(), (int) objCount);
#endif

    // If we successfully annotated the GetPropertyCache and there are inline cases,
    // then keep a resume point of the state right before this instruction for use
    // later when we have to bail out to this point in the fallback case of a
    // PolyInlineDispatch.
    if (inlinePropTable->numEntries() > 0) {
        // Push the object back onto the stack temporarily to capture the resume point.
        current->push(obj);
        MResumePoint *resumePoint = MResumePoint::New(current, pc, callerResumePoint_,
                                                      MResumePoint::ResumeAt);
        if (!resumePoint)
            return false;
        inlinePropTable->setPriorResumePoint(resumePoint);
        current->pop();
    }
    return true;
}

// Returns true if an idempotent cache has ever invalidated this script
// or an outer script.
bool
IonBuilder::invalidatedIdempotentCache()
{
    IonBuilder *builder = this;
    do {
        if (builder->script()->invalidatedIdempotentCache)
            return true;
        builder = builder->callerBuilder_;
    } while (builder);

    return false;
}

bool
IonBuilder::loadSlot(MDefinition *obj, Shape *shape, MIRType rvalType,
                     bool barrier, types::TemporaryTypeSet *types)
{
    JS_ASSERT(shape->hasDefaultGetter());
    JS_ASSERT(shape->hasSlot());

    if (shape->slot() < shape->numFixedSlots()) {
        MLoadFixedSlot *load = MLoadFixedSlot::New(obj, shape->slot());
        current->add(load);
        current->push(load);

        load->setResultType(rvalType);
        return pushTypeBarrier(load, types, barrier);
    }

    MSlots *slots = MSlots::New(obj);
    current->add(slots);

    MLoadSlot *load = MLoadSlot::New(slots, shape->slot() - shape->numFixedSlots());
    current->add(load);
    current->push(load);

    load->setResultType(rvalType);
    return pushTypeBarrier(load, types, barrier);
}

bool
IonBuilder::storeSlot(MDefinition *obj, Shape *shape, MDefinition *value, bool needsBarrier,
                      MIRType slotType /* = MIRType_None */)
{
    JS_ASSERT(shape->hasDefaultSetter());
    JS_ASSERT(shape->writable());
    JS_ASSERT(shape->hasSlot());

    if (shape->slot() < shape->numFixedSlots()) {
        MStoreFixedSlot *store = MStoreFixedSlot::New(obj, shape->slot(), value);
        current->add(store);
        current->push(value);
        if (needsBarrier)
            store->setNeedsBarrier();
        return resumeAfter(store);
    }

    MSlots *slots = MSlots::New(obj);
    current->add(slots);

    MStoreSlot *store = MStoreSlot::New(slots, shape->slot() - shape->numFixedSlots(), value);
    current->add(store);
    current->push(value);
    if (needsBarrier)
        store->setNeedsBarrier();
    if (slotType != MIRType_None)
        store->setSlotType(slotType);
    return resumeAfter(store);
}

bool
IonBuilder::jsop_getprop(PropertyName *name)
{
    bool emitted = false;

    // Try to optimize arguments.length.
    if (!getPropTryArgumentsLength(&emitted) || emitted)
        return emitted;

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    bool barrier = PropertyReadNeedsTypeBarrier(cx, context(), constraints(),
                                                current->peek(-1), name, types);

    // Always use a call if we are doing the definite properties analysis and
    // not actually emitting code, to simplify later analysis. Also skip deeper
    // analysis if there are no known types for this operation, as it will
    // always invalidate when executing.
    if (info().executionMode() == DefinitePropertiesAnalysis || types->empty()) {
        MDefinition *obj = current->peek(-1);
        MCallGetProperty *call = MCallGetProperty::New(obj, name, *pc == JSOP_CALLPROP);
        current->add(call);

        // During the definite properties analysis we can still try to bake in
        // constants read off the prototype chain, to allow inlining later on.
        // In this case we still need the getprop call so that the later
        // analysis knows when the |this| value has been read from.
        if (info().executionMode() == DefinitePropertiesAnalysis) {
            if (!getPropTryConstant(&emitted, name, types) || emitted)
                return emitted;
        }

        current->pop();
        current->push(call);
        return resumeAfter(call) && pushTypeBarrier(call, types, true);
    }

    // Try to hardcode known constants.
    if (!getPropTryConstant(&emitted, name, types) || emitted)
        return emitted;

    // Try to emit loads from known binary data blocks
    if (!getPropTryTypedObject(&emitted, name, types) || emitted)
        return emitted;

    // Try to emit loads from definite slots.
    if (!getPropTryDefiniteSlot(&emitted, name, barrier, types) || emitted)
        return emitted;

    // Try to inline a common property getter, or make a call.
    if (!getPropTryCommonGetter(&emitted, name, types) || emitted)
        return emitted;

    // Try to emit a monomorphic/polymorphic access based on baseline caches.
    if (!getPropTryInlineAccess(&emitted, name, barrier, types) || emitted)
        return emitted;

    // Try to emit a polymorphic cache.
    if (!getPropTryCache(&emitted, name, barrier, types) || emitted)
        return emitted;

    // Emit a call.
    MDefinition *obj = current->pop();
    MCallGetProperty *call = MCallGetProperty::New(obj, name, *pc == JSOP_CALLPROP);
    current->add(call);
    current->push(call);
    if (!resumeAfter(call))
        return false;

    return pushTypeBarrier(call, types, true);
}

bool
IonBuilder::getPropTryArgumentsLength(bool *emitted)
{
    JS_ASSERT(*emitted == false);
    if (current->peek(-1)->type() != MIRType_Magic) {
        if (script()->argumentsHasVarBinding() && current->peek(-1)->mightBeType(MIRType_Magic))
            return abort("Type is not definitely lazy arguments.");
        return true;
    }
    if (JSOp(*pc) != JSOP_LENGTH)
        return true;

    *emitted = true;
    return jsop_arguments_length();
}

bool
IonBuilder::getPropTryConstant(bool *emitted, PropertyName *name,
                               types::TemporaryTypeSet *types)
{
    JS_ASSERT(*emitted == false);
    JSObject *singleton = types ? types->getSingleton() : nullptr;
    if (!singleton)
        return true;

    JSObject *global = &script()->global();

    bool isConstant, testObject, testString;
    if (!testSingletonPropertyTypes(current->peek(-1), singleton, global, name,
                                    &isConstant, &testObject, &testString))
        return false;

    if (!isConstant)
        return true;

    MDefinition *obj = current->pop();

    // Property access is a known constant -- safe to emit.
    JS_ASSERT(!testString || !testObject);
    if (testObject)
        current->add(MGuardObject::New(obj));
    else if (testString)
        current->add(MGuardString::New(obj));
    else
        obj->setFoldedUnchecked();

    MConstant *known = MConstant::New(ObjectValue(*singleton));

    current->add(known);
    current->push(known);

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryTypedObject(bool *emitted, PropertyName *name,
                                  types::TemporaryTypeSet *resultTypes)
{
    TypeRepresentationSet fieldTypeReprs;
    int32_t fieldOffset;
    size_t fieldIndex;
    if (!lookupTypedObjectField(current->peek(-1), name, &fieldOffset,
                                &fieldTypeReprs, &fieldIndex))
        return false;
    if (fieldTypeReprs.empty())
        return true;

    switch (fieldTypeReprs.kind()) {
      case TypeRepresentation::Struct:
      case TypeRepresentation::Array:
        return getPropTryComplexPropOfTypedObject(emitted,
                                                  fieldOffset,
                                                  fieldTypeReprs,
                                                  fieldIndex,
                                                  resultTypes);

      case TypeRepresentation::Scalar:
        return getPropTryScalarPropOfTypedObject(emitted,
                                                 fieldOffset,
                                                 fieldTypeReprs,
                                                 resultTypes);
    }

    MOZ_ASSUME_UNREACHABLE("Bad kind");
}

bool
IonBuilder::getPropTryScalarPropOfTypedObject(bool *emitted,
                                              int32_t fieldOffset,
                                              TypeRepresentationSet fieldTypeReprs,
                                              types::TemporaryTypeSet *resultTypes)
{
    // Must always be loading the same scalar type
    if (fieldTypeReprs.length() != 1)
        return true;
    ScalarTypeRepresentation *fieldTypeRepr = fieldTypeReprs.get(0)->asScalar();

    // OK!
    *emitted = true;

    MDefinition *typedObj = current->pop();

    // Find location within the owner object.
    MDefinition *owner, *ownerOffset;
    loadTypedObjectData(typedObj, fieldOffset, &owner, &ownerOffset);

    // Load the element data.
    MTypedObjectElements *elements = MTypedObjectElements::New(owner);
    current->add(elements);

    // Reading from an Uint32Array will result in a double for values
    // that don't fit in an int32. We have to bailout if this happens
    // and the instruction is not known to return a double.
    bool allowDouble = resultTypes->hasType(types::Type::DoubleType());
    MIRType knownType = MIRTypeForTypedArrayRead(fieldTypeRepr->type(), allowDouble);

    // Typed array offsets are expressed in units of the alignment,
    // and the binary data API guarantees all offsets are properly
    // aligned. So just do the divide.
    MConstant *alignment = MConstant::New(Int32Value(fieldTypeRepr->alignment()));
    current->add(alignment);
    MDiv *scaledOffset = MDiv::NewAsmJS(ownerOffset, alignment, MIRType_Int32);
    current->add(scaledOffset);

    MLoadTypedArrayElement *load =
        MLoadTypedArrayElement::New(elements, scaledOffset,
                                    fieldTypeRepr->type());
    load->setResultType(knownType);
    load->setResultTypeSet(resultTypes);
    current->add(load);
    current->push(load);
    return true;
}

bool
IonBuilder::getPropTryComplexPropOfTypedObject(bool *emitted,
                                               int32_t fieldOffset,
                                               TypeRepresentationSet fieldTypeReprs,
                                               size_t fieldIndex,
                                               types::TemporaryTypeSet *resultTypes)
{
    // Must know the field index so that we can load the new type
    // object for the derived value
    if (fieldIndex == SIZE_MAX)
        return true;

    *emitted = true;
    MDefinition *typedObj = current->pop();

    // Identify the type object for the field.
    MDefinition *type = loadTypedObjectType(typedObj);
    MDefinition *fieldType = typeObjectForFieldFromStructType(type, fieldIndex);

    // Find location within the owner object.
    MDefinition *owner, *ownerOffset;
    loadTypedObjectData(typedObj, fieldOffset, &owner, &ownerOffset);

    // Create the derived type object.
    MInstruction *derived = new MNewDerivedTypedObject(fieldTypeReprs,
                                                       fieldType,
                                                       owner,
                                                       ownerOffset);
    derived->setResultTypeSet(resultTypes);
    current->add(derived);
    current->push(derived);
    return true;
}

bool
IonBuilder::getPropTryDefiniteSlot(bool *emitted, PropertyName *name,
                                   bool barrier, types::TemporaryTypeSet *types)
{
    JS_ASSERT(*emitted == false);
    types::HeapTypeSetKey property;
    if (!getDefiniteSlot(current->peek(-1)->resultTypeSet(), name, &property))
        return true;

    MDefinition *obj = current->pop();
    MDefinition *useObj = obj;
    if (obj->type() != MIRType_Object) {
        MGuardObject *guard = MGuardObject::New(obj);
        current->add(guard);
        useObj = guard;
    }

    MLoadFixedSlot *fixed = MLoadFixedSlot::New(useObj, property.maybeTypes()->definiteSlot());
    if (!barrier)
        fixed->setResultType(MIRTypeFromValueType(types->getKnownTypeTag()));

    current->add(fixed);
    current->push(fixed);

    if (!pushTypeBarrier(fixed, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryCommonGetter(bool *emitted, PropertyName *name,
                                   types::TemporaryTypeSet *types)
{
    JS_ASSERT(*emitted == false);
    JSFunction *commonGetter;
    bool isDOM;
    MDefinition *guard;

    types::TemporaryTypeSet *objTypes = current->peek(-1)->resultTypeSet();

    if (!testCommonPropFunc(cx, objTypes, name, &commonGetter, true, &isDOM, &guard))
        return false;
    if (!commonGetter)
        return true;

#ifdef JSGC_GENERATIONAL
    if (GetIonContext()->runtime->gcNursery.isInside(commonGetter))
        return true;
#endif

    MDefinition *obj = current->pop();

    if (isDOM && TestShouldDOMCall(cx, objTypes, commonGetter, JSJitInfo::Getter)) {
        const JSJitInfo *jitinfo = commonGetter->jitInfo();
        MGetDOMProperty *get = MGetDOMProperty::New(jitinfo, obj, guard);
        current->add(get);
        current->push(get);

        if (get->isEffectful() && !resumeAfter(get))
            return false;
        bool barrier = DOMCallNeedsBarrier(jitinfo, types);
        if (!pushTypeBarrier(get, types, barrier))
            return false;

        *emitted = true;
        return true;
    }

    // Don't call the getter with a primitive value.
    if (objTypes->getKnownTypeTag() != JSVAL_TYPE_OBJECT) {
        MGuardObject *guardObj = MGuardObject::New(obj);
        current->add(guardObj);
        obj = guardObj;
    }

    // Spoof stack to expected state for call.
    pushConstant(ObjectValue(*commonGetter));

    MPassArg *wrapper = MPassArg::New(obj);
    current->add(wrapper);
    current->push(wrapper);

    CallInfo callInfo(false);
    if (!callInfo.init(current, 0))
        return false;

    // Inline if we can, otherwise, forget it and just generate a call.
    if (makeInliningDecision(commonGetter, callInfo) && commonGetter->isInterpreted()) {
        if (!inlineScriptedCall(callInfo, commonGetter))
            return false;
    } else {
        if (!makeCall(commonGetter, callInfo, false))
            return false;
    }

    *emitted = true;
    return true;
}

static bool
CanInlinePropertyOpShapes(const BaselineInspector::ShapeVector &shapes)
{
    for (size_t i = 0; i < shapes.length(); i++) {
        // We inline the property access as long as the shape is not in
        // dictionary made. We cannot be sure that the shape is still a
        // lastProperty, and calling Shape::search() on dictionary mode
        // shapes that aren't lastProperty is invalid.
        if (shapes[i]->inDictionary())
            return false;
    }

    return true;
}

bool
IonBuilder::getPropTryInlineAccess(bool *emitted, PropertyName *name,
                                   bool barrier, types::TemporaryTypeSet *types)
{
    JS_ASSERT(*emitted == false);
    if (current->peek(-1)->type() != MIRType_Object)
        return true;

    BaselineInspector::ShapeVector shapes;
    if (!inspector->maybeShapesForPropertyOp(pc, shapes))
        return false;

    if (shapes.empty() || !CanInlinePropertyOpShapes(shapes))
        return true;

    MIRType rvalType = MIRTypeFromValueType(types->getKnownTypeTag());
    if (barrier || IsNullOrUndefined(rvalType))
        rvalType = MIRType_Value;

    MDefinition *obj = current->pop();
    if (shapes.length() == 1) {
        // In the monomorphic case, use separate ShapeGuard and LoadSlot
        // instructions.
        spew("Inlining monomorphic GETPROP");

        Shape *objShape = shapes[0];
        obj = addShapeGuard(obj, objShape, Bailout_ShapeGuard);

        Shape *shape = objShape->search(cx, NameToId(name));
        JS_ASSERT(shape);

        if (!loadSlot(obj, shape, rvalType, barrier, types))
            return false;
    } else {
        JS_ASSERT(shapes.length() > 1);
        spew("Inlining polymorphic GETPROP");

        MGetPropertyPolymorphic *load = MGetPropertyPolymorphic::New(obj, name);
        current->add(load);
        current->push(load);

        for (size_t i = 0; i < shapes.length(); i++) {
            Shape *objShape = shapes[i];
            Shape *shape =  objShape->search(cx, NameToId(name));
            JS_ASSERT(shape);
            if (!load->addShape(objShape, shape))
                return false;
        }

        if (failedShapeGuard_)
            load->setNotMovable();

        load->setResultType(rvalType);
        if (!pushTypeBarrier(load, types, barrier))
            return false;
    }

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryCache(bool *emitted, PropertyName *name,
                            bool barrier, types::TemporaryTypeSet *types)
{
    JS_ASSERT(*emitted == false);
    bool accessGetter = inspector->hasSeenAccessedGetter(pc);

    MDefinition *obj = current->peek(-1);

    // The input value must either be an object, or we should have strong suspicions
    // that it can be safely unboxed to an object.
    if (obj->type() != MIRType_Object) {
        types::TemporaryTypeSet *types = obj->resultTypeSet();
        if (!types || !types->objectOrSentinel())
            return true;
    }

    current->pop();
    MGetPropertyCache *load = MGetPropertyCache::New(obj, name);

    // Try to mark the cache as idempotent.
    //
    // In parallel execution, idempotency of caches is ignored, since we
    // repeat the entire ForkJoin workload if we bail out. Note that it's
    // overly restrictive to mark everything as idempotent, because we can
    // treat non-idempotent caches in parallel as repeatable.
    if (obj->type() == MIRType_Object && !invalidatedIdempotentCache() &&
        info().executionMode() != ParallelExecution)
    {
        if (PropertyReadIsIdempotent(constraints(), obj, name))
            load->setIdempotent();
    }

    if (JSOp(*pc) == JSOP_CALLPROP) {
        if (!annotateGetPropertyCache(cx, obj, load, obj->resultTypeSet(), types))
            return false;
    }

    // If the cache is known to access getters, then enable generation of getter stubs.
    if (accessGetter)
        load->setAllowGetters();

    current->add(load);
    current->push(load);

    if (load->isEffectful() && !resumeAfter(load))
        return false;

    if (accessGetter)
        barrier = true;

    if (needsToMonitorMissingProperties(types))
        barrier = true;

    // Caches can read values from prototypes, so update the barrier to
    // reflect such possible values.
    if (!barrier)
        barrier = PropertyReadOnPrototypeNeedsTypeBarrier(cx, constraints(), obj, name, types);

    MIRType rvalType = MIRTypeFromValueType(types->getKnownTypeTag());
    if (barrier || IsNullOrUndefined(rvalType))
        rvalType = MIRType_Value;
    load->setResultType(rvalType);

    if (!pushTypeBarrier(load, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::needsToMonitorMissingProperties(types::TemporaryTypeSet *types)
{
    // GetPropertyParIC and GetElementParIC cannot safely call
    // TypeScript::Monitor to ensure that the observed type set contains
    // undefined. To account for possible missing properties, which property
    // types do not track, we must always insert a type barrier.
    return (info().executionMode() == ParallelExecution &&
            !types->hasType(types::Type::UndefinedType()));
}

bool
IonBuilder::jsop_setprop(PropertyName *name)
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->pop();

    bool emitted = false;

    // Always use a call if we are doing the definite properties analysis and
    // not actually emitting code, to simplify later analysis.
    if (info().executionMode() == DefinitePropertiesAnalysis) {
        MInstruction *ins = MCallSetProperty::New(obj, value, name, script()->strict);
        current->add(ins);
        current->push(value);
        return resumeAfter(ins);
    }

    // Add post barrier if needed.
    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(obj, value));

    // Try to inline a common property setter, or make a call.
    if (!setPropTryCommonSetter(&emitted, obj, name, value) || emitted)
        return emitted;

    types::TemporaryTypeSet *objTypes = obj->resultTypeSet();
    bool barrier = PropertyWriteNeedsTypeBarrier(constraints(), current, &obj, name, &value,
                                                 /* canModify = */ true);

    // Try to emit stores to known binary data blocks
    if (!setPropTryTypedObject(&emitted, obj, name, value) || emitted)
        return emitted;

    // Try to emit store from definite slots.
    if (!setPropTryDefiniteSlot(&emitted, obj, name, value, barrier, objTypes) || emitted)
        return emitted;

    // Try to emit a monomorphic/polymorphic store based on baseline caches.
    if (!setPropTryInlineAccess(&emitted, obj, name, value, barrier, objTypes) || emitted)
        return emitted;

    // Try to emit a polymorphic cache.
    if (!setPropTryCache(&emitted, obj, name, value, barrier, objTypes) || emitted)
        return emitted;

    // Emit call.
    MInstruction *ins = MCallSetProperty::New(obj, value, name, script()->strict);
    current->add(ins);
    current->push(value);
    return resumeAfter(ins);
}

bool
IonBuilder::setPropTryCommonSetter(bool *emitted, MDefinition *obj,
                                   PropertyName *name, MDefinition *value)
{
    JS_ASSERT(*emitted == false);

    JSFunction *commonSetter;
    bool isDOM;

    types::TemporaryTypeSet *objTypes = obj->resultTypeSet();
    if (!testCommonPropFunc(cx, objTypes, name, &commonSetter, false, &isDOM, nullptr))
        return false;

    if (!commonSetter)
        return true;

    // Emit common setter.

    // Setters can be called even if the property write needs a type
    // barrier, as calling the setter does not actually write any data
    // properties.

    // Try emitting dom call.
    if (!setPropTryCommonDOMSetter(emitted, obj, value, commonSetter, isDOM))
        return false;

    if (*emitted)
        return true;

    // Don't call the setter with a primitive value.
    if (objTypes->getKnownTypeTag() != JSVAL_TYPE_OBJECT) {
        MGuardObject *guardObj = MGuardObject::New(obj);
        current->add(guardObj);
        obj = guardObj;
    }

    // Dummy up the stack, as in getprop. We are pushing an extra value, so
    // ensure there is enough space.
    uint32_t depth = current->stackDepth() + 3;
    if (depth > current->nslots()) {
        if (!current->increaseSlots(depth - current->nslots()))
            return false;
    }

    pushConstant(ObjectValue(*commonSetter));

    MPassArg *wrapper = MPassArg::New(obj);
    current->push(wrapper);
    current->add(wrapper);

    MPassArg *arg = MPassArg::New(value);
    current->push(arg);
    current->add(arg);

    // Call the setter. Note that we have to push the original value, not
    // the setter's return value.
    CallInfo callInfo(false);
    if (!callInfo.init(current, 1))
        return false;

    // Ensure that we know we are calling a setter in case we inline it.
    callInfo.markAsSetter();

    // Inline the setter if we can.
    if (makeInliningDecision(commonSetter, callInfo) && commonSetter->isInterpreted()) {
        if (!inlineScriptedCall(callInfo, commonSetter))
            return false;

        *emitted = true;
        return true;
    }

    MCall *call = makeCallHelper(commonSetter, callInfo, false);
    if (!call)
        return false;

    current->push(value);
    if (!resumeAfter(call))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setPropTryCommonDOMSetter(bool *emitted, MDefinition *obj,
                                      MDefinition *value, JSFunction *setter,
                                      bool isDOM)
{
    JS_ASSERT(*emitted == false);

    if (!isDOM)
        return true;

    types::TemporaryTypeSet *objTypes = obj->resultTypeSet();
    if (!TestShouldDOMCall(cx, objTypes, setter, JSJitInfo::Setter))
        return true;

    // Emit SetDOMProperty.
    JS_ASSERT(setter->jitInfo()->type == JSJitInfo::Setter);
    MSetDOMProperty *set = MSetDOMProperty::New(setter->jitInfo()->setter, obj, value);

    current->add(set);
    current->push(value);

    if (!resumeAfter(set))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setPropTryTypedObject(bool *emitted, MDefinition *obj,
                                  PropertyName *name, MDefinition *value)
{
    TypeRepresentationSet fieldTypeReprs;
    int32_t fieldOffset;
    size_t fieldIndex;
    if (!lookupTypedObjectField(obj, name, &fieldOffset, &fieldTypeReprs,
                                &fieldIndex))
        return false;
    if (fieldTypeReprs.empty())
        return true;

    switch (fieldTypeReprs.kind()) {
      case TypeRepresentation::Struct:
      case TypeRepresentation::Array:
        // For now, only optimize storing scalars.
        return true;

      case TypeRepresentation::Scalar:
        break;
    }

    // Must always be storing the same scalar type
    if (fieldTypeReprs.length() != 1)
        return true;
    ScalarTypeRepresentation *fieldTypeRepr = fieldTypeReprs.get(0)->asScalar();

    // OK!
    *emitted = true;

    MTypedObjectElements *elements = MTypedObjectElements::New(obj);
    current->add(elements);

    // Typed array offsets are expressed in units of the alignment,
    // and the binary data API guarantees all offsets are properly
    // aligned.
    JS_ASSERT(fieldOffset % fieldTypeRepr->alignment() == 0);
    int32_t scaledFieldOffset = fieldOffset / fieldTypeRepr->alignment();

    MConstant *offset = MConstant::New(Int32Value(scaledFieldOffset));
    current->add(offset);

    // Clamp value to [0, 255] for Uint8ClampedArray.
    MDefinition *toWrite = value;
    if (fieldTypeRepr->type() == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED) {
        toWrite = MClampToUint8::New(value);
        current->add(toWrite->toInstruction());
    }

    MStoreTypedArrayElement *store =
        MStoreTypedArrayElement::New(elements, offset, toWrite,
                                     fieldTypeRepr->type());
    current->add(store);

    current->push(value);

    return true;
}

bool
IonBuilder::setPropTryDefiniteSlot(bool *emitted, MDefinition *obj,
                                   PropertyName *name, MDefinition *value,
                                   bool barrier, types::TemporaryTypeSet *objTypes)
{
    JS_ASSERT(*emitted == false);

    if (barrier)
        return true;

    types::HeapTypeSetKey property;
    if (!getDefiniteSlot(obj->resultTypeSet(), name, &property))
        return true;

    MStoreFixedSlot *fixed = MStoreFixedSlot::New(obj, property.maybeTypes()->definiteSlot(), value);
    current->add(fixed);
    current->push(value);

    if (property.needsBarrier(constraints()))
        fixed->setNeedsBarrier();

    if (!resumeAfter(fixed))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::setPropTryInlineAccess(bool *emitted, MDefinition *obj,
                                   PropertyName *name,
                                   MDefinition *value, bool barrier,
                                   types::TemporaryTypeSet *objTypes)
{
    JS_ASSERT(*emitted == false);

    if (barrier)
        return true;

    BaselineInspector::ShapeVector shapes;
    if (!inspector->maybeShapesForPropertyOp(pc, shapes))
        return false;

    if (shapes.empty())
        return true;

    if (!CanInlinePropertyOpShapes(shapes))
        return true;

    if (shapes.length() == 1) {
        spew("Inlining monomorphic SETPROP");

        // The Baseline IC was monomorphic, so we inline the property access as
        // long as the shape is not in dictionary mode. We cannot be sure
        // that the shape is still a lastProperty, and calling Shape::search
        // on dictionary mode shapes that aren't lastProperty is invalid.
        Shape *objShape = shapes[0];
        obj = addShapeGuard(obj, objShape, Bailout_ShapeGuard);

        Shape *shape = objShape->search(cx, NameToId(name));
        JS_ASSERT(shape);

        bool needsBarrier = objTypes->propertyNeedsBarrier(constraints(), NameToId(name));
        if (!storeSlot(obj, shape, value, needsBarrier))
            return false;
    } else {
        JS_ASSERT(shapes.length() > 1);
        spew("Inlining polymorphic SETPROP");

        MSetPropertyPolymorphic *ins = MSetPropertyPolymorphic::New(obj, value);
        current->add(ins);
        current->push(value);

        for (size_t i = 0; i < shapes.length(); i++) {
            Shape *objShape = shapes[i];
            Shape *shape =  objShape->search(cx, NameToId(name));
            JS_ASSERT(shape);
            if (!ins->addShape(objShape, shape))
                return false;
        }

        if (objTypes->propertyNeedsBarrier(constraints(), NameToId(name)))
            ins->setNeedsBarrier();

        if (!resumeAfter(ins))
            return false;
    }

    *emitted = true;
    return true;
}

bool
IonBuilder::setPropTryCache(bool *emitted, MDefinition *obj,
                            PropertyName *name, MDefinition *value,
                            bool barrier, types::TemporaryTypeSet *objTypes)
{
    JS_ASSERT(*emitted == false);

    // Emit SetPropertyCache.
    MSetPropertyCache *ins = MSetPropertyCache::New(obj, value, name, script()->strict, barrier);

    if (!objTypes || objTypes->propertyNeedsBarrier(constraints(), NameToId(name)))
        ins->setNeedsBarrier();

    current->add(ins);
    current->push(value);

    if (!resumeAfter(ins))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::jsop_delprop(PropertyName *name)
{
    MDefinition *obj = current->pop();

    MInstruction *ins = MDeleteProperty::New(obj, name);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_delelem()
{
    MDefinition *index = current->pop();
    MDefinition *obj = current->pop();

    MDeleteElement *ins = MDeleteElement::New(obj, index);
    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_regexp(RegExpObject *reobj)
{
    JSObject *prototype = script()->global().getOrCreateRegExpPrototype(cx);
    if (!prototype)
        return false;

    JS_ASSERT(&reobj->JSObject::global() == &script()->global());

    // JS semantics require regular expression literals to create different
    // objects every time they execute. We only need to do this cloning if the
    // script could actually observe the effect of such cloning, for instance
    // by getting or setting properties on it.
    //
    // First, make sure the regex is one we can safely optimize. Lowering can
    // then check if this regex object only flows into known natives and can
    // avoid cloning in this case.

    bool mustClone = true;
    types::TypeObjectKey *typeObj = types::TypeObjectKey::get(&script()->global());
    if (!typeObj->hasFlags(constraints(), types::OBJECT_FLAG_REGEXP_FLAGS_SET)) {
        RegExpStatics *res = script()->global().getRegExpStatics();

        DebugOnly<uint32_t> origFlags = reobj->getFlags();
        DebugOnly<uint32_t> staticsFlags = res->getFlags();
        JS_ASSERT((origFlags & staticsFlags) == staticsFlags);

        if (!reobj->global() && !reobj->sticky())
            mustClone = false;
    }

    MRegExp *regexp = MRegExp::New(reobj, prototype, mustClone);
    current->add(regexp);
    current->push(regexp);

    regexp->setMovable();

    // The MRegExp is set to be movable.
    // That would be incorrect for global/sticky, because lastIndex could be wrong.
    // Therefore setting the lastIndex to 0. That is faster than removing the movable flag.
    if (reobj->sticky() || reobj->global()) {
        JS_ASSERT(mustClone);
        MConstant *zero = MConstant::New(Int32Value(0));
        current->add(zero);

        MStoreFixedSlot *lastIndex =
            MStoreFixedSlot::New(regexp, RegExpObject::lastIndexSlot(), zero);
        current->add(lastIndex);
    }

    return true;
}

bool
IonBuilder::jsop_object(JSObject *obj)
{
    MConstant *ins = MConstant::New(ObjectValue(*obj));
    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_lambda(JSFunction *fun)
{
    JS_ASSERT(analysis().usesScopeChain());
    if (fun->isArrow())
        return abort("bound arrow function");
    if (fun->isNative() && IsAsmJSModuleNative(fun->native()))
        return abort("asm.js module function");

    MLambda *ins = MLambda::New(current->scopeChain(), fun);
    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_setarg(uint32_t arg)
{
    // To handle this case, we should spill the arguments to the space where
    // actual arguments are stored. The tricky part is that if we add a MIR
    // to wrap the spilling action, we don't want the spilling to be
    // captured by the GETARG and by the resume point, only by
    // MGetFrameArgument.
    JS_ASSERT(analysis_.hasSetArg());
    MDefinition *val = current->peek(-1);

    // If an arguments object is in use, and it aliases formals, then all SETARGs
    // must go through the arguments object.
    if (info().argsObjAliasesFormals()) {
        current->add(MSetArgumentsObjectArg::New(current->argumentsObject(), GET_SLOTNO(pc), val));
        return true;
    }

    // Otherwise, if a magic arguments is in use, and it aliases formals, and there exist
    // arguments[...] GETELEM expressions in the script, then SetFrameArgument must be used.
    // If no arguments[...] GETELEM expressions are in the script, and an argsobj is not
    // required, then it means that any aliased argument set can never be observed, and
    // the frame does not actually need to be updated with the new arg value.
    if (info().argumentsAliasesFormals()) {
        // Try-catch within inline frames is not yet supported.
        if (isInlineBuilder())
            return abort("JSOP_SETARG with magic arguments in inlined function.");

        MSetFrameArgument *store = MSetFrameArgument::New(arg, val);
        current->add(store);
        current->setArg(arg);
        return true;
    }

    // If this assignment is at the start of the function and is coercing
    // the original value for the argument which was passed in, loosen
    // the type information for that original argument if it is currently
    // empty due to originally executing in the interpreter.
    if (graph().numBlocks() == 1 &&
        (val->isBitOr() || val->isBitAnd() || val->isMul() /* for JSOP_POS */))
     {
         for (size_t i = 0; i < val->numOperands(); i++) {
            MDefinition *op = val->getOperand(i);
            if (op->isParameter() &&
                op->toParameter()->index() == (int32_t)arg &&
                op->resultTypeSet() &&
                op->resultTypeSet()->empty())
            {
                JS_ASSERT(op->resultTypeSet() == &argTypes[arg]);
                if (!argTypes[arg].addType(types::Type::UnknownType(), temp_->lifoAlloc()))
                    return false;
            }
        }
    }

    current->setArg(arg);
    return true;
}

bool
IonBuilder::jsop_defvar(uint32_t index)
{
    JS_ASSERT(JSOp(*pc) == JSOP_DEFVAR || JSOp(*pc) == JSOP_DEFCONST);

    PropertyName *name = script()->getName(index);

    // Bake in attrs.
    unsigned attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT;
    if (JSOp(*pc) == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    // Pass the ScopeChain.
    JS_ASSERT(analysis().usesScopeChain());

    // Bake the name pointer into the MDefVar.
    MDefVar *defvar = MDefVar::New(name, attrs, current->scopeChain());
    current->add(defvar);

    return resumeAfter(defvar);
}

bool
IonBuilder::jsop_deffun(uint32_t index)
{
    JSFunction *fun = script()->getFunction(index);
    if (fun->isNative() && IsAsmJSModuleNative(fun->native()))
        return abort("asm.js module function");

    JS_ASSERT(analysis().usesScopeChain());

    MDefFun *deffun = MDefFun::New(fun, current->scopeChain());
    current->add(deffun);

    return resumeAfter(deffun);
}

bool
IonBuilder::jsop_this()
{
    if (!info().fun())
        return abort("JSOP_THIS outside of a JSFunction.");

    if (script()->strict || info().fun()->isSelfHostedBuiltin()) {
        // No need to wrap primitive |this| in strict mode or self-hosted code.
        current->pushSlot(info().thisSlot());
        return true;
    }

    if (thisTypes->getKnownTypeTag() == JSVAL_TYPE_OBJECT ||
        (thisTypes->empty() && baselineFrame_ && baselineFrame_->thisValue().isObject()))
    {
        // This is safe, because if the entry type of |this| is an object, it
        // will necessarily be an object throughout the entire function. OSR
        // can introduce a phi, but this phi will be specialized.
        current->pushSlot(info().thisSlot());
        return true;
    }

    // If we are doing a definite properties analysis, we don't yet know the
    // |this| type as its type object is being created right now. Instead of
    // bailing out just push the |this| slot, as this code won't actually
    // execute and it does not matter whether |this| is primitive.
    if (info().executionMode() == DefinitePropertiesAnalysis) {
        current->pushSlot(info().thisSlot());
        return true;
    }

    // Hard case: |this| may be a primitive we have to wrap.
    MDefinition *def = current->getSlot(info().thisSlot());

    if (def->type() == MIRType_Object) {
        // If we already computed a |this| object, we can reuse it.
        current->push(def);
        return true;
    }

    MComputeThis *thisObj = MComputeThis::New(def);
    current->add(thisObj);
    current->push(thisObj);

    current->setSlot(info().thisSlot(), thisObj);

    return resumeAfter(thisObj);
}

bool
IonBuilder::jsop_typeof()
{
    MDefinition *input = current->pop();
    MTypeOf *ins = MTypeOf::New(input, input->type());

    ins->infer();

    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_toid()
{
    // No-op if the index is an integer.
    if (current->peek(-1)->type() == MIRType_Int32)
        return true;

    MDefinition *index = current->pop();
    MToId *ins = MToId::New(current->peek(-1), index);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_iter(uint8_t flags)
{
    if (flags != JSITER_ENUMERATE)
        nonStringIteration_ = true;

    MDefinition *obj = current->pop();
    MInstruction *ins = MIteratorStart::New(obj, flags);

    if (!iterators_.append(ins))
        return false;

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_iternext()
{
    MDefinition *iter = current->peek(-1);
    MInstruction *ins = MIteratorNext::New(iter);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    if (!nonStringIteration_ && !inspector->hasSeenNonStringIterNext(pc)) {
        ins = MUnbox::New(ins, MIRType_String, MUnbox::Fallible, Bailout_BaselineInfo);
        current->add(ins);
        current->rewriteAtDepth(-1, ins);
    }

    return true;
}

bool
IonBuilder::jsop_itermore()
{
    MDefinition *iter = current->peek(-1);
    MInstruction *ins = MIteratorMore::New(iter);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_iterend()
{
    MDefinition *iter = current->pop();
    MInstruction *ins = MIteratorEnd::New(iter);

    current->add(ins);

    return resumeAfter(ins);
}

MDefinition *
IonBuilder::walkScopeChain(unsigned hops)
{
    MDefinition *scope = current->getSlot(info().scopeChainSlot());

    for (unsigned i = 0; i < hops; i++) {
        MInstruction *ins = MEnclosingScope::New(scope);
        current->add(ins);
        scope = ins;
    }

    return scope;
}

bool
IonBuilder::hasStaticScopeObject(ScopeCoordinate sc, JSObject **pcall)
{
    JSScript *outerScript = ScopeCoordinateFunctionScript(cx, script(), pc);
    if (!outerScript || !outerScript->treatAsRunOnce)
        return false;

    types::TypeObjectKey *funType = types::TypeObjectKey::get(outerScript->function());
    if (funType->hasFlags(constraints(), types::OBJECT_FLAG_RUNONCE_INVALIDATED))
        return false;

    // The script this aliased var operation is accessing will run only once,
    // so there will be only one call object and the aliased var access can be
    // compiled in the same manner as a global access. We still need to find
    // the call object though.

    // Look for the call object on the current script's function's scope chain.
    // If the current script is inner to the outer script and the function has
    // singleton type then it should show up here.

    MDefinition *scope = current->getSlot(info().scopeChainSlot());
    scope->setFoldedUnchecked();

    JSObject *environment = script()->function()->environment();
    while (environment && !environment->is<GlobalObject>()) {
        if (environment->is<CallObject>() &&
            !environment->as<CallObject>().isForEval() &&
            environment->as<CallObject>().callee().nonLazyScript() == outerScript)
        {
            JS_ASSERT(environment->hasSingletonType());
            *pcall = environment;
            return true;
        }
        environment = environment->enclosingScope();
    }

    // Look for the call object on the current frame, if we are compiling the
    // outer script itself. Don't do this if we are at entry to the outer
    // script, as the call object we see will not be the real one --- after
    // entering the Ion code a different call object will be created.

    if (script() == outerScript && baselineFrame_ && info().osrPc()) {
        JSObject *scope = baselineFrame_->scopeChain();
        if (scope->is<CallObject>() &&
            scope->as<CallObject>().callee().nonLazyScript() == outerScript)
        {
            JS_ASSERT(scope->hasSingletonType());
            *pcall = scope;
            return true;
        }
    }

    return true;
}

bool
IonBuilder::jsop_getaliasedvar(ScopeCoordinate sc)
{
    JSObject *call = nullptr;
    if (hasStaticScopeObject(sc, &call) && call) {
        PropertyName *name = ScopeCoordinateName(cx, script(), pc);
        bool succeeded;
        if (!getStaticName(call, name, &succeeded))
            return false;
        if (succeeded)
            return true;
    }

    MDefinition *obj = walkScopeChain(sc.hops);

    Shape *shape = ScopeCoordinateToStaticScopeShape(cx, script(), pc);

    MInstruction *load;
    if (shape->numFixedSlots() <= sc.slot) {
        MInstruction *slots = MSlots::New(obj);
        current->add(slots);

        load = MLoadSlot::New(slots, sc.slot - shape->numFixedSlots());
    } else {
        load = MLoadFixedSlot::New(obj, sc.slot);
    }

    current->add(load);
    current->push(load);

    types::TemporaryTypeSet *types = bytecodeTypes(pc);
    return pushTypeBarrier(load, types, true);
}

bool
IonBuilder::jsop_setaliasedvar(ScopeCoordinate sc)
{
    JSObject *call = nullptr;
    if (hasStaticScopeObject(sc, &call)) {
        uint32_t depth = current->stackDepth() + 1;
        if (depth > current->nslots()) {
            if (!current->increaseSlots(depth - current->nslots()))
                return false;
        }
        MDefinition *value = current->pop();
        PropertyName *name = ScopeCoordinateName(cx, script(), pc);

        if (call) {
            // Push the object on the stack to match the bound object expected in
            // the global and property set cases.
            MInstruction *constant = MConstant::New(ObjectValue(*call));
            current->add(constant);
            current->push(constant);
            current->push(value);
            return setStaticName(call, name);
        }

        // The call object has type information we need to respect but we
        // couldn't find it. Just do a normal property assign.
        MDefinition *obj = walkScopeChain(sc.hops);
        current->push(obj);
        current->push(value);
        return jsop_setprop(name);
    }

    MDefinition *rval = current->peek(-1);
    MDefinition *obj = walkScopeChain(sc.hops);

    Shape *shape = ScopeCoordinateToStaticScopeShape(cx, script(), pc);

    if (NeedsPostBarrier(info(), rval))
        current->add(MPostWriteBarrier::New(obj, rval));

    MInstruction *store;
    if (shape->numFixedSlots() <= sc.slot) {
        MInstruction *slots = MSlots::New(obj);
        current->add(slots);

        store = MStoreSlot::NewBarriered(slots, sc.slot - shape->numFixedSlots(), rval);
    } else {
        store = MStoreFixedSlot::NewBarriered(obj, sc.slot, rval);
    }

    current->add(store);
    return resumeAfter(store);
}

bool
IonBuilder::jsop_in()
{
    MDefinition *obj = current->peek(-1);
    MDefinition *id = current->peek(-2);

    if (ElementAccessIsDenseNative(obj, id) &&
        !ElementAccessHasExtraIndexedProperty(constraints(), obj))
    {
        return jsop_in_dense();
    }

    current->pop();
    current->pop();
    MIn *ins = new MIn(id, obj);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_in_dense()
{
    MDefinition *obj = current->pop();
    MDefinition *id = current->pop();

    bool needsHoleCheck = !ElementAccessIsPacked(constraints(), obj);

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    MInitializedLength *initLength = MInitializedLength::New(elements);
    current->add(initLength);

    // Check if id < initLength and elem[id] not a hole.
    MInArray *ins = MInArray::New(elements, id, initLength, obj, needsHoleCheck);

    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_instanceof()
{
    MDefinition *rhs = current->pop();
    MDefinition *obj = current->pop();

    // If this is an 'x instanceof function' operation and we can determine the
    // exact function and prototype object being tested for, use a typed path.
    do {
        types::TemporaryTypeSet *rhsTypes = rhs->resultTypeSet();
        JSObject *rhsObject = rhsTypes ? rhsTypes->getSingleton() : nullptr;
        if (!rhsObject || !rhsObject->is<JSFunction>() || rhsObject->isBoundFunction())
            break;

        types::TypeObjectKey *rhsType = types::TypeObjectKey::get(rhsObject);
        if (rhsType->unknownProperties())
            break;

        types::HeapTypeSetKey protoProperty =
            rhsType->property(NameToId(names().prototype));
        JSObject *protoObject = protoProperty.singleton(constraints());
        if (!protoObject)
            break;

        rhs->setFoldedUnchecked();

        MInstanceOf *ins = new MInstanceOf(obj, protoObject);

        current->add(ins);
        current->push(ins);

        return resumeAfter(ins);
    } while (false);

    MCallInstanceOf *ins = new MCallInstanceOf(obj, rhs);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

MInstruction *
IonBuilder::addConvertElementsToDoubles(MDefinition *elements)
{
    MInstruction *convert = MConvertElementsToDoubles::New(elements);
    current->add(convert);
    return convert;
}

MInstruction *
IonBuilder::addBoundsCheck(MDefinition *index, MDefinition *length)
{
    MInstruction *check = MBoundsCheck::New(index, length);
    current->add(check);

    // If a bounds check failed in the past, don't optimize bounds checks.
    if (failedBoundsCheck_)
        check->setNotMovable();

    return check;
}

MInstruction *
IonBuilder::addShapeGuard(MDefinition *obj, Shape *const shape, BailoutKind bailoutKind)
{
    MGuardShape *guard = MGuardShape::New(obj, shape, bailoutKind);
    current->add(guard);

    // If a shape guard failed in the past, don't optimize shape guard.
    if (failedShapeGuard_)
        guard->setNotMovable();

    return guard;
}

types::TemporaryTypeSet *
IonBuilder::bytecodeTypes(jsbytecode *pc)
{
    return types::TypeScript::BytecodeTypes(script(), pc, &typeArrayHint, typeArray);
}

TypeRepresentationSetHash *
IonBuilder::getOrCreateReprSetHash()
{
    if (!reprSetHash_) {
        TypeRepresentationSetHash* hash =
            cx->new_<TypeRepresentationSetHash>();
        if (!hash || !hash->init()) {
            js_delete(hash);
            return nullptr;
        }

        reprSetHash_ = hash;
    }
    return reprSetHash_.get();
}

bool
IonBuilder::lookupTypeRepresentationSet(MDefinition *typedObj,
                                        TypeRepresentationSet *out)
{
    *out = TypeRepresentationSet(); // default to unknown

    // Extract TypeRepresentationSet directly if we can
    if (typedObj->isNewDerivedTypedObject()) {
        *out = typedObj->toNewDerivedTypedObject()->set();
        return true;
    }

    // Extract TypeRepresentationSet directly if we can
    types::TemporaryTypeSet *types = typedObj->resultTypeSet();
    if (!types || types->getKnownTypeTag() != JSVAL_TYPE_OBJECT)
        return true;

    // And only known objects.
    if (types->unknownObject())
        return true;

    TypeRepresentationSetBuilder set;
    for (uint32_t i = 0; i < types->getObjectCount(); i++) {
        types::TypeObject *type = types->getTypeObject(i);
        if (!type || type->unknownProperties())
            return true;

        if (!type->hasTypedObject())
            return true;

        TypeRepresentation *typeRepr = type->typedObject()->typeRepr;
        if (!set.insert(typeRepr))
            return false;
    }

    return set.build(*this, out);
}

MDefinition *
IonBuilder::loadTypedObjectType(MDefinition *typedObj)
{
    // Shortcircuit derived type objects, meaning the intermediate
    // objects created to represent `a.b` in an expression like
    // `a.b.c`. In that case, the type object can be simply pulled
    // from the operands of that instruction.
    if (typedObj->isNewDerivedTypedObject())
        return typedObj->toNewDerivedTypedObject()->type();

    MInstruction *load = MLoadFixedSlot::New(typedObj,
                                             JS_DATUM_SLOT_TYPE_OBJ);
    current->add(load);
    return load;
}

// Given a typed object `typedObj` and an offset `offset` into that
// object's data, returns another typed object and adusted offset
// where the data can be found. Often, these returned values are the
// same as the inputs, but in cases where intermediate derived type
// objects have been created, the return values will remove
// intermediate layers (often rendering those derived type objects
// into dead code).
void
IonBuilder::loadTypedObjectData(MDefinition *typedObj,
                                int32_t offset,
                                MDefinition **owner,
                                MDefinition **ownerOffset)
{
    MConstant *offsetDef = MConstant::New(Int32Value(offset));
    current->add(offsetDef);

    // Shortcircuit derived type objects, meaning the intermediate
    // objects created to represent `a.b` in an expression like
    // `a.b.c`. In that case, the owned and a base offset can be
    // pulled from the operands of the instruction and combined with
    // `offset`.
    if (typedObj->isNewDerivedTypedObject()) {
        // If we see that the
        MNewDerivedTypedObject *ins = typedObj->toNewDerivedTypedObject();

        MAdd *offsetAdd = MAdd::NewAsmJS(ins->offset(), offsetDef,
                                         MIRType_Int32);
        current->add(offsetAdd);

        *owner = ins->owner();
        *ownerOffset = offsetAdd;
        return;
    }

    *owner = typedObj;
    *ownerOffset = offsetDef;
}

// Looks up the offset/type-repr-set of the field `id`, given the type
// set `objTypes` of the field owner. Note that even when true is
// returned, `*fieldTypeReprs` might be empty if no useful type/offset
// pair could be determined.
bool
IonBuilder::lookupTypedObjectField(MDefinition *typedObj,
                                   PropertyName *name,
                                   int32_t *fieldOffset,
                                   TypeRepresentationSet *fieldTypeReprs,
                                   size_t *fieldIndex)
{
    TypeRepresentationSet objTypeReprs;
    if (!lookupTypeRepresentationSet(typedObj, &objTypeReprs))
        return false;

    // Must be accessing a struct.
    if (!objTypeReprs.allOfKind(TypeRepresentation::Struct))
        return true;

    // Determine the type/offset of the field `name`, if any.
    size_t offset;
    if (!objTypeReprs.fieldNamed(*this, NameToId(name), &offset,
                                 fieldTypeReprs, fieldIndex))
        return false;
    if (fieldTypeReprs->empty())
        return false;

    // Field offset must be representable as signed integer.
    if (offset >= size_t(INT_MAX)) {
        *fieldTypeReprs = TypeRepresentationSet();
        return true;
    }

    *fieldOffset = int32_t(offset);
    JS_ASSERT(*fieldOffset >= 0);

    return true;
}

MDefinition *
IonBuilder::typeObjectForFieldFromStructType(MDefinition *typeObj,
                                             size_t fieldIndex)
{
    // Load list of field type objects.

    MInstruction *fieldTypes = MLoadFixedSlot::New(typeObj, JS_TYPEOBJ_SLOT_STRUCT_FIELD_TYPES);
    current->add(fieldTypes);

    // Index into list with index of field.

    MInstruction *fieldTypesElements = MElements::New(fieldTypes);
    current->add(fieldTypesElements);

    MConstant *fieldIndexDef = MConstant::New(Int32Value(fieldIndex));
    current->add(fieldIndexDef);

    MInstruction *fieldType = MLoadElement::New(fieldTypesElements, fieldIndexDef, false, false);
    current->add(fieldType);

    return fieldType;
}
