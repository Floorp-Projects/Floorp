/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "IonAnalysis.h"
#include "IonBuilder.h"
#include "Lowering.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonAnalysis.h"
#include "IonSpewer.h"
#include "frontend/BytecodeEmitter.h"

#include "jsscriptinlines.h"
#include "jstypedarrayinlines.h"
#include "ExecutionModeInlines.h"

#ifdef JS_THREADSAFE
# include "prthread.h"
#endif

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;

IonBuilder::IonBuilder(JSContext *cx, TempAllocator *temp, MIRGraph *graph,
                       TypeOracle *oracle, CompileInfo *info, size_t inliningDepth, uint32_t loopDepth)
  : MIRGenerator(cx->compartment, temp, graph, info),
    backgroundCodegen_(NULL),
    recompileInfo(cx->compartment->types.compiledInfo),
    cx(cx),
    loopDepth_(loopDepth),
    callerResumePoint_(NULL),
    callerBuilder_(NULL),
    oracle(oracle),
    inliningDepth(inliningDepth),
    failedBoundsCheck_(info->script()->failedBoundsCheck),
    failedShapeGuard_(info->script()->failedShapeGuard),
    lazyArguments_(NULL),
    callee_(NULL)
{
    script_.init(info->script());
    pc = info->startPC();
}

void
IonBuilder::clearForBackEnd()
{
    cx = NULL;
    oracle = NULL;
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
    IonSpew(IonSpew_Abort, "aborted @ %s:%d", script()->filename, PCToLineNumber(script(), pc));
#endif
    return false;
}

void
IonBuilder::spew(const char *message)
{
    // Don't call PCToLineNumber in release builds.
#ifdef DEBUG
    IonSpew(IonSpew_MIR, "%s @ %s:%d", message, script()->filename, PCToLineNumber(script(), pc));
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
    state.tableswitch.breaks = NULL;
    state.tableswitch.ins = ins;
    state.tableswitch.currentBlock = 0;
    return state;
}

IonBuilder::CFGState
IonBuilder::CFGState::LookupSwitch(jsbytecode *exitpc)
{
    CFGState state;
    state.state = LOOKUP_SWITCH;
    state.stopAt = exitpc;
    state.lookupswitch.exitpc = exitpc;
    state.lookupswitch.breaks = NULL;
    state.lookupswitch.bodies =
        (FixedList<MBasicBlock *> *)GetIonContext()->temp->allocate(sizeof(FixedList<MBasicBlock *>));
    state.lookupswitch.currentBlock = 0;
    return state;
}

JSFunction *
IonBuilder::getSingleCallTarget(uint32_t argc, jsbytecode *pc)
{
    AutoAssertNoGC nogc;

    types::StackTypeSet *calleeTypes = oracle->getCallTarget(script(), argc, pc);
    if (!calleeTypes)
        return NULL;

    RawObject obj = calleeTypes->getSingleton();
    if (!obj || !obj->isFunction())
        return NULL;

    return obj->toFunction();
}

uint32_t
IonBuilder::getPolyCallTargets(uint32_t argc, jsbytecode *pc,
                               AutoObjectVector &targets, uint32_t maxTargets)
{
    types::TypeSet *calleeTypes = oracle->getCallTarget(script(), argc, pc);
    if (!calleeTypes)
        return 0;

    if (calleeTypes->baseFlags() != 0)
        return 0;

    unsigned objCount = calleeTypes->getObjectCount();

    if (objCount == 0 || objCount > maxTargets)
        return 0;

    for(unsigned i = 0; i < objCount; i++) {
        JSObject *obj = calleeTypes->getSingleObject(i);
        if (!obj || !obj->isFunction())
            return 0;
        targets.append(obj);
    }

    return (uint32_t) objCount;
}

bool
IonBuilder::canInlineTarget(JSFunction *target)
{
    AssertCanGC();

    if (!target->isInterpreted()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-interpreted");
        return false;
    }

    if (target->getParent() != &script()->global()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to scope mismatch");
        return false;
    }

    RootedScript inlineScript(cx, target->nonLazyScript());
    ExecutionMode executionMode = info().executionMode();
    if (!CanIonCompile(inlineScript, executionMode)) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to disable Ion compilation");
        return false;
    }

    // Allow inlining of recursive calls, but only one level deep.
    IonBuilder *builder = callerBuilder_;
    while (builder) {
        if (builder->script() == inlineScript) {
            IonSpew(IonSpew_Inlining, "Not inlining recursive call");
            return false;
        }
        builder = builder->callerBuilder_;
    }

    bool canInline = oracle->canEnterInlinedFunction(target);

    if (!canInline) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to oracle veto %d", script()->lineno);
        return false;
    }

    IonSpew(IonSpew_Inlining, "Inlining good to go!");
    return true;
}

void
IonBuilder::popCfgStack()
{
    if (cfgStack_.back().isLoop())
        loops_.popBack();
    cfgStack_.popBack();
}

bool
IonBuilder::pushLoop(CFGState::State initial, jsbytecode *stopAt, MBasicBlock *entry,
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
    state.loop.entry = entry;
    state.loop.successor = NULL;
    state.loop.breaks = NULL;
    state.loop.continues = NULL;
    return cfgStack_.append(state);
}

bool
IonBuilder::build()
{
    current = newBlock(pc);
    if (!current)
        return false;

    IonSpew(IonSpew_Scripts, "Analyzing script %s:%d (%p) (usecount=%d) (maxloopcount=%d)",
            script()->filename, script()->lineno, (void *)script(), (int)script()->getUseCount(),
            (int)script()->getMaxLoopCount());

    if (!graph().addScript(script()))
        return false;

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
    {
        MInstruction *scope = MConstant::New(UndefinedValue());
        current->add(scope);
        current->initSlot(info().scopeChainSlot(), scope);
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
    for (uint32_t i = 0; i < CountArgSlots(info().fun()); i++) {
        MInstruction *ins = current->getEntrySlot(i)->toInstruction();
        if (ins->type() == MIRType_Value)
            ins->setResumePoint(current->entryResumePoint());
    }

    // Recompile to inline calls if this function is hot.
    insertRecompileCheck();

    if (script()->argumentsHasVarBinding()) {
        lazyArguments_ = MConstant::New(MagicValue(JS_OPTIMIZED_ARGUMENTS));
        current->add(lazyArguments_);
    }

    if (!traverseBytecode())
        return false;

    if (!processIterators())
        return false;

    JS_ASSERT(loopDepth_ == 0);
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
                        MDefinition *thisDefn, MDefinitionVector &argv)
{
    IonSpew(IonSpew_Scripts, "Inlining script %s:%d (%p)",
            script()->filename, script()->lineno, (void *)script());

    if (!graph().addScript(script()))
        return false;

    callerBuilder_ = callerBuilder;
    callerResumePoint_ = callerResumePoint;

    if (callerBuilder->failedBoundsCheck_)
        failedBoundsCheck_ = true;

    if (callerBuilder->failedShapeGuard_)
        failedShapeGuard_ = true;

    // Generate single entrance block.
    current = newBlock(pc);
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
                                                inliningDepth));

    predecessor->end(MGoto::New(current));
    if (!current->addPredecessorWithoutPhis(predecessor))
        return false;

    // Explicitly pass Undefined for missing arguments.
    const size_t numActualArgs = argv.length() - 1;
    const size_t nargs = info().nargs();

    if (numActualArgs < nargs) {
        const size_t missing = nargs - numActualArgs;

        for (size_t i = 0; i < missing; i++) {
            MConstant *undef = MConstant::New(UndefinedValue());
            current->add(undef);
            if (!argv.append(undef))
                return false;
        }
    }

    // The Oracle ensures that the inlined script does not use the scope chain.
    JS_ASSERT(!script()->analysis()->usesScopeChain());
    MInstruction *scope = MConstant::New(UndefinedValue());
    current->add(scope);
    current->initSlot(info().scopeChainSlot(), scope);

    current->initSlot(info().thisSlot(), thisDefn);

    IonSpew(IonSpew_Inlining, "Initializing %u arg slots", nargs);

    // Initialize argument references.
    MDefinitionVector::Range args = argv.all();
    args.popFront();
    JS_ASSERT(args.remain() >= nargs);
    for (size_t i = 0; i < nargs; ++i) {
        MDefinition *arg = args.popCopyFront();
        current->initSlot(info().argSlot(i), arg);
    }

    IonSpew(IonSpew_Inlining, "Initializing %u local slots", info().nlocals());

    // Initialize local variables.
    for (uint32_t i = 0; i < info().nlocals(); i++) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        current->initSlot(info().localSlot(i), undef);
    }

    IonSpew(IonSpew_Inlining, "Inline entry block MResumePoint %p, %u operands",
            (void *) current->entryResumePoint(), current->entryResumePoint()->numOperands());

    // +2 for the scope chain and |this|.
    JS_ASSERT(current->entryResumePoint()->numOperands() == nargs + info().nlocals() + 2);

    return traverseBytecode();
}

// Apply Type Inference information to parameters early on, unboxing them if
// they have a definitive type. The actual guards will be emitted by the code
// generator, explicitly, as part of the function prologue.
void
IonBuilder::rewriteParameters()
{
    JS_ASSERT(info().scopeChainSlot() == 0);
    static const uint32_t START_SLOT = 1;

    for (uint32_t i = START_SLOT; i < CountArgSlots(info().fun()); i++) {
        MParameter *param = current->getSlot(i)->toParameter();

        // Find the original (not cloned) type set for the MParameter, as we
        // will be adding constraints to it.
        types::StackTypeSet *types;
        if (param->index() == MParameter::THIS_SLOT)
            types = oracle->thisTypeSet(script());
        else
            types = oracle->parameterTypeSet(script(), param->index());
        if (!types)
            continue;

        JSValueType definiteType = types->getKnownTypeTag();
        if (definiteType == JSVAL_TYPE_UNKNOWN)
            continue;

        MInstruction *actual = NULL;
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
        current->rewriteSlot(i, actual);
    }
}

bool
IonBuilder::initParameters()
{
    if (!info().fun())
        return true;

    MParameter *param = MParameter::New(MParameter::THIS_SLOT,
                                        cloneTypeSet(oracle->thisTypeSet(script())));
    current->add(param);
    current->initSlot(info().thisSlot(), param);

    for (uint32_t i = 0; i < info().nargs(); i++) {
        param = MParameter::New(i, cloneTypeSet(oracle->parameterTypeSet(script(), i)));
        current->add(param);
        current->initSlot(info().argSlot(i), param);
    }

    return true;
}

bool
IonBuilder::initScopeChain()
{
    MInstruction *scope = NULL;

    // Add callee, it will be removed if it is not used by neither the scope
    // chain nor the function body.
    JSFunction *fun = info().fun();
    if (fun) {
        JS_ASSERT(!callee_);
        callee_ = MCallee::New();
        current->add(callee_);
    }

    // If the script doesn't use the scopechain, then it's already initialized
    // from earlier.
    if (!script()->analysis()->usesScopeChain())
        return true;

    // The scope chain is only tracked in scripts that have NAME opcodes which
    // will try to access the scope. For other scripts, the scope instructions
    // will be held live by resume points and code will still be generated for
    // them, so just use a constant undefined value.
    if (!script()->compileAndGo)
        return abort("non-CNG global scripts are not supported");

    if (fun) {
        scope = MFunctionEnvironment::New(callee_);
        current->add(scope);

        // This reproduce what is done in CallObject::createForFunction
        if (fun->isHeavyweight()) {
            if (fun->isNamedLambda()) {
                scope = createDeclEnvObject(callee_, scope);
                if (!scope)
                    return false;
            }

            scope = createCallObject(callee_, scope);
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
                if (!current)
                    return true;
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
            if (!current)
                return true;
        }

        // Nothing in inspectOpcode() is allowed to advance the pc.
        JSOp op = JSOp(*pc);
        if (!inspectOpcode(op))
            return false;

        pc += js_CodeSpec[op].length;
#ifdef TRACK_SNAPSHOTS
        current->updateTrackedPc(pc);
#endif
    }

    return true;
}

IonBuilder::ControlStatus
IonBuilder::snoopControlFlow(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return maybeLoop(op, info().getNote(cx, pc));

      case JSOP_POP:
        return maybeLoop(op, info().getNote(cx, pc));

      case JSOP_RETURN:
      case JSOP_STOP:
        return processReturn(op);

      case JSOP_THROW:
        return processThrow();

      case JSOP_GOTO:
      {
        jssrcnote *sn = info().getNote(cx, pc);
        switch (sn ? SN_TYPE(sn) : SRC_NULL) {
          case SRC_BREAK:
          case SRC_BREAK2LABEL:
            return processBreak(op, sn);

          case SRC_CONTINUE:
          case SRC_CONT2LABEL:
            return processContinue(op, sn);

          case SRC_SWITCHBREAK:
            return processSwitchBreak(op, sn);

          case SRC_WHILE:
          case SRC_FOR_IN:
            // while (cond) { }
            return whileOrForInLoop(op, sn);

          default:
            // Hard assert for now - make an error later.
            JS_NOT_REACHED("unknown goto case");
            break;
        }
        break;
      }

      case JSOP_TABLESWITCH:
        return tableSwitch(op, info().getNote(cx, pc));

      case JSOP_LOOKUPSWITCH:
        return lookupSwitch(op, info().getNote(cx, pc));

      case JSOP_IFNE:
        // We should never reach an IFNE, it's a stopAt point, which will
        // trigger closing the loop.
        JS_NOT_REACHED("we should never reach an ifne!");
        return ControlStatus_Error;

      default:
        break;
    }
    return ControlStatus_None;
}

bool
IonBuilder::inspectOpcode(JSOp op)
{
    AssertCanGC();

    // Don't compile fat opcodes, run the decomposed version instead.
    if (js_CodeSpec[op].format & JOF_DECOMPOSE)
        return true;

    switch (op) {
      case JSOP_LOOPENTRY:
        insertRecompileCheck();
        return true;

      case JSOP_NOP:
        return true;

      case JSOP_LABEL:
        return true;

      case JSOP_UNDEFINED:
        return pushConstant(UndefinedValue());

      case JSOP_IFEQ:
        return jsop_ifeq(JSOP_IFEQ);

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
        return pushConstant(MagicValue(JS_ARRAY_HOLE));

      case JSOP_FALSE:
        return pushConstant(BooleanValue(false));

      case JSOP_TRUE:
        return pushConstant(BooleanValue(true));

      case JSOP_ARGUMENTS:
        return jsop_arguments();

      case JSOP_NOTEARG:
        return jsop_notearg();

      case JSOP_GETARG:
      case JSOP_CALLARG:
        current->pushArg(GET_SLOTNO(pc));
        return true;

      case JSOP_SETARG:
        // To handle this case, we should spill the arguments to the space where
        // actual arguments are stored. The tricky part is that if we add a MIR
        // to wrap the spilling action, we don't want the spilling to be
        // captured by the GETARG and by the resume point, only by
        // MGetArgument.
        if (info().hasArguments())
            return abort("NYI: arguments & setarg.");
        current->setArg(GET_SLOTNO(pc));
        return true;

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

      case JSOP_NEWINIT:
      {
        if (GET_UINT8(pc) == JSProto_Array)
            return jsop_newarray(0);
        RootedObject baseObj(cx, NULL);
        return jsop_newobject(baseObj);
      }

      case JSOP_NEWARRAY:
        return jsop_newarray(GET_UINT24(pc));

      case JSOP_NEWOBJECT:
      {
        RootedObject baseObj(cx, info().getObject(pc));
        return jsop_newobject(baseObj);
      }

      case JSOP_INITELEM_ARRAY:
        return jsop_initelem_array();

      case JSOP_INITPROP:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_initprop(name);
      }

      case JSOP_ENDINIT:
        return true;

      case JSOP_FUNCALL:
        return jsop_funcall(GET_ARGC(pc));

      case JSOP_FUNAPPLY:
        return jsop_funapply(GET_ARGC(pc));

      case JSOP_CALL:
      case JSOP_NEW:
        return jsop_call(GET_ARGC(pc), (JSOp)*pc == JSOP_NEW);

      case JSOP_INT8:
        return pushConstant(Int32Value(GET_INT8(pc)));

      case JSOP_UINT16:
        return pushConstant(Int32Value(GET_UINT16(pc)));

      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_getgname(name);
      }

      case JSOP_BINDGNAME:
        return pushConstant(ObjectValue(script()->global()));

      case JSOP_SETGNAME:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_setgname(name);
      }

      case JSOP_NAME:
      case JSOP_CALLNAME:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_getname(name);
      }

      case JSOP_GETINTRINSIC:
      case JSOP_CALLINTRINSIC:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
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
        JS_NOT_REACHED("JSOP_LOOPHEAD outside loop");
        return true;

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
        JS_ASSERT(callee_);
        current->push(callee_);
        return true;

      case JSOP_GETPROP:
      case JSOP_CALLPROP:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_getprop(name);
      }

      case JSOP_SETPROP:
      case JSOP_SETNAME:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_setprop(name);
      }

      case JSOP_DELPROP:
      {
        RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
        return jsop_delprop(name);
      }

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

      case JSOP_INSTANCEOF:
        return jsop_instanceof();

      default:
#ifdef DEBUG
        return abort("Unsupported opcode: %s (line %d)", js_CodeName[op], info().lineno(cx, pc));
#else
        return abort("Unsupported opcode: %d (line %d)", op, info().lineno(cx, pc));
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
// If |current| is NULL when this function returns, then there is no more
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

      case CFGState::LOOKUP_SWITCH:
        return processNextLookupSwitchCase(state);

      case CFGState::COND_SWITCH_CASE:
        return processCondSwitchCase(state);

      case CFGState::COND_SWITCH_BODY:
        return processCondSwitchBody(state);

      case CFGState::AND_OR:
        return processAndOrEnd(state);

      default:
        JS_NOT_REACHED("unknown cfgstate");
    }
    return ControlStatus_Error;
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

    current = state.branch.ifFalse;
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
    current = state.branch.ifFalse;
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
    current = join;
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
    current = state.loop.successor;
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

        current = block;
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
    if (!state.loop.entry->setBackedge(current))
        return ControlStatus_Error;
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

    current = successor;

    // An infinite loop (for (;;) { }) will not have a successor.
    if (!current)
        return ControlStatus_Ended;

    pc = current->pc();
    return ControlStatus_Joined;
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
    current = header;
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

    state.state = CFGState::WHILE_LOOP_BODY;
    state.stopAt = state.loop.bodyEnd;
    pc = state.loop.bodyStart;
    current = body;
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
    current = body;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processForBodyEnd(CFGState &state)
{
    if (!processDeferredContinues(state))
        return ControlStatus_Error;

    // If there is no updatepc, just go right to processing what would be the
    // end of the update clause. Otherwise, |current| might be NULL; if this is
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

bool
IonBuilder::processDeferredContinues(CFGState &state)
{
    // If there are any continues for this loop, and there is an update block,
    // then we need to create a new basic block to house the update.
    if (state.loop.continues) {
        DeferredEdge *edge = state.loop.continues;

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
        state.loop.continues = NULL;

        current = update;
    }

    return true;
}

MBasicBlock *
IonBuilder::createBreakCatchBlock(DeferredEdge *edge, jsbytecode *pc)
{
    // Create block, using the first break statement as predecessor
    MBasicBlock *successor = newBlock(edge->block, pc);
    if (!successor)
        return NULL;

    // No need to use addPredecessor for first edge,
    // because it is already predecessor.
    edge->block->end(MGoto::New(successor));
    edge = edge->next;

    // Finish up remaining breaks.
    while (edge) {
        edge->block->end(MGoto::New(successor));
        if (!successor->addPredecessor(edge->block))
            return NULL;
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

        // Insert successor after the current block, to maintain RPO.
        graph().moveBlockToEnd(successor);
    }

    // If this is the last successor the block should stop at the end of the tableswitch
    // Else it should stop at the start of the next successor
    if (state.tableswitch.currentBlock+1 < state.tableswitch.ins->numBlocks())
        state.stopAt = state.tableswitch.ins->getBlock(state.tableswitch.currentBlock+1)->pc();
    else
        state.stopAt = state.tableswitch.exitpc;

    current = successor;
    pc = current->pc();
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processNextLookupSwitchCase(CFGState &state)
{
    JS_ASSERT(state.state == CFGState::LOOKUP_SWITCH);

    size_t curBlock = state.lookupswitch.currentBlock;
    IonSpew(IonSpew_MIR, "processNextLookupSwitchCase curBlock=%d", curBlock);

    state.lookupswitch.currentBlock = ++curBlock;

    // Test if there are still unprocessed successors (cases/default)
    if (curBlock >= state.lookupswitch.bodies->length())
        return processSwitchEnd(state.lookupswitch.breaks, state.lookupswitch.exitpc);

    // Get the next successor
    MBasicBlock *successor = (*state.lookupswitch.bodies)[curBlock];

    // Add current block as predecessor if available.
    // This means the previous case didn't have a break statement.
    // So flow will continue in this block.
    if (current) {
        current->end(MGoto::New(successor));
        successor->addPredecessor(current);
    }

    // Move next body block to end to maintain RPO.
    graph().moveBlockToEnd(successor);

    // If this is the last successor the block should stop at the end of the lookupswitch
    // Else it should stop at the start of the next successor
    if (curBlock + 1 < state.lookupswitch.bodies->length())
        state.stopAt = (*state.lookupswitch.bodies)[curBlock + 1]->pc();
    else
        state.stopAt = state.lookupswitch.exitpc;

    current = successor;
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

    current = state.branch.ifFalse;
    graph().moveBlockToEnd(current);
    pc = current->pc();
    return ControlStatus_Joined;
}

IonBuilder::ControlStatus
IonBuilder::processBreak(JSOp op, jssrcnote *sn)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target loop.
    CFGState *found = NULL;
    jsbytecode *target = pc + GetJumpOffset(pc);
    for (size_t i = loops_.length() - 1; i < loops_.length(); i--) {
        CFGState &cfg = cfgStack_[loops_[i].cfgEntry];
        if (cfg.loop.exitpc == target) {
            found = &cfg;
            break;
        }
    }

    if (!found) {
        // Sometimes, we can't determine the structure of a labeled break. For
        // example:
        //
        // 0:    label: {
        // 1:        for (;;) {
        // 2:            break label;
        // 3:        }
        // 4:        stuff;
        // 5:    }
        //
        // In this case, the successor of the block is 4, but the target of the
        // single-level break is actually 5. To recognize this case we'd need
        // to know about the label structure at 0,5 ahead of time - and lacking
        // those source notes for now, we just abort instead.
        abort("could not find the target of a break");
        return ControlStatus_Error;
    }

    // There must always be a valid target loop structure. If not, there's
    // probably an off-by-something error in which pc we track.
    CFGState &state = *found;

    state.loop.breaks = new DeferredEdge(current, state.loop.breaks);

    current = NULL;
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
IonBuilder::processContinue(JSOp op, jssrcnote *sn)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target loop.
    CFGState *found = NULL;
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

    current = NULL;
    pc += js_CodeSpec[op].length;
    return processControlEnd();
}

IonBuilder::ControlStatus
IonBuilder::processSwitchBreak(JSOp op, jssrcnote *sn)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target switch.
    CFGState *found = NULL;
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

    DeferredEdge **breaks = NULL;
    switch (state.state) {
      case CFGState::TABLE_SWITCH:
        breaks = &state.tableswitch.breaks;
        break;
      case CFGState::LOOKUP_SWITCH:
        breaks = &state.lookupswitch.breaks;
        break;
      case CFGState::COND_SWITCH_BODY:
        breaks = &state.condswitch.breaks;
        break;
      default:
        JS_NOT_REACHED("Unexpected switch state.");
        return ControlStatus_Error;
    }

    *breaks = new DeferredEdge(current, *breaks);

    current = NULL;
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
    MBasicBlock *successor = NULL;
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
    current = successor;
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
        JS_NOT_REACHED("unexpected opcode");
        return ControlStatus_Error;
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
    jssrcnote *sn = info().getNote(cx, pc);
    if (sn) {
        jsbytecode *ifne = pc + js_GetSrcNoteOffset(sn, 0);

        jsbytecode *expected_ifne;
        switch (state.state) {
          case CFGState::DO_WHILE_LOOP_BODY:
            expected_ifne = state.loop.updateEnd;
            break;

          default:
            JS_NOT_REACHED("JSOP_LOOPHEAD unexpected source note");
            return;
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

    jssrcnote *sn2 = info().getNote(cx, pc+1);
    int offset = js_GetSrcNoteOffset(sn2, 0);
    jsbytecode *ifne = pc + offset + 1;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a loophead op.
    jsbytecode *loopHead = GetNextPc(pc);
    JS_ASSERT(JSOp(*loopHead) == JSOP_LOOPHEAD);
    JS_ASSERT(loopHead == ifne + GetJumpOffset(ifne));

    jsbytecode *loopEntry = GetNextPc(loopHead);
    if (info().hasOsrAt(loopEntry)) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        current = preheader;
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    jsbytecode *bodyStart = GetNextPc(GetNextPc(pc));
    jsbytecode *bodyEnd = conditionpc;
    jsbytecode *exitpc = GetNextPc(ifne);
    if (!pushLoop(CFGState::DO_WHILE_LOOP_BODY, conditionpc, header,
                  bodyStart, bodyEnd, exitpc, conditionpc))
    {
        return ControlStatus_Error;
    }

    CFGState &state = cfgStack_.back();
    state.loop.updatepc = conditionpc;
    state.loop.updateEnd = ifne;

    current = header;
    if (!jsop_loophead(GetNextPc(pc)))
        return ControlStatus_Error;

    pc = bodyStart;
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::whileOrForInLoop(JSOp op, jssrcnote *sn)
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
    size_t which = (SN_TYPE(sn) == SRC_FOR_IN) ? 1 : 0;
    int ifneOffset = js_GetSrcNoteOffset(sn, which);
    jsbytecode *ifne = pc + ifneOffset;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a loophead op.
    JS_ASSERT(JSOp(*GetNextPc(pc)) == JSOP_LOOPHEAD);
    JS_ASSERT(GetNextPc(pc) == ifne + GetJumpOffset(ifne));

    jsbytecode *loopEntry = pc + GetJumpOffset(pc);
    if (info().hasOsrAt(loopEntry)) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        current = preheader;
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc);
    if (!header)
        return ControlStatus_Error;
    current->end(MGoto::New(header));

    // Skip past the JSOP_LOOPHEAD for the body start.
    jsbytecode *bodyStart = GetNextPc(GetNextPc(pc));
    jsbytecode *bodyEnd = pc + GetJumpOffset(pc);
    jsbytecode *exitpc = GetNextPc(ifne);
    if (!pushLoop(CFGState::WHILE_LOOP_COND, ifne, header, bodyStart, bodyEnd, exitpc))
        return ControlStatus_Error;

    // Parse the condition first.
    current = header;
    if (!jsop_loophead(GetNextPc(pc)))
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

    if (info().hasOsrAt(loopEntry)) {
        MBasicBlock *preheader = newOsrPreheader(current, loopEntry);
        if (!preheader)
            return ControlStatus_Error;
        current->end(MGoto::New(preheader));
        current = preheader;
    }

    MBasicBlock *header = newPendingLoopHeader(current, pc);
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

    if (!pushLoop(initial, stopAt, header, bodyStart, bodyEnd, exitpc, updatepc))
        return ControlStatus_Error;

    CFGState &state = cfgStack_.back();
    state.loop.condpc = (condpc != ifne) ? condpc : NULL;
    state.loop.updatepc = (updatepc != condpc) ? updatepc : NULL;
    if (state.loop.updatepc)
        state.loop.updateEnd = condpc;

    current = header;
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
    jsbytecode *casepc = NULL;
    for (int i = 0; i < high-low+1; i++) {
        casepc = pc + GET_JUMP_OFFSET(pc2);

        JS_ASSERT(casepc >= pc && casepc <= exitpc);

        MBasicBlock *caseblock = newBlock(current, casepc);
        if (!caseblock)
            return ControlStatus_Error;

        // If the casepc equals the current pc, it is not a written case,
        // but a filled gap. That way we can use a tableswitch instead of
        // lookupswitch, even if not all numbers are consecutive.
        // In that case this block goes to the default case
        if (casepc == pc) {
            caseblock->end(MGoto::New(defaultcase));
            defaultcase->addPredecessor(caseblock);
        }

        tableswitch->addCase(caseblock);

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
    current = tableswitch->getBlock(0);

    if (!cfgStack_.append(state))
        return ControlStatus_Error;

    pc = current->pc();
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::lookupSwitch(JSOp op, jssrcnote *sn)
{
    // LookupSwitch op looks as follows:
    // DEFAULT  : JUMP_OFFSET           # jump offset (exitpc if no default block)
    // NCASES   : UINT16                # number of cases
    // CONST_1  : UINT32_INDEX          # case 1 constant index
    // OFFSET_1 : JUMP_OFFSET           # case 1 offset
    // ...
    // CONST_N  : UINT32_INDEX          # case N constant index
    // OFFSET_N : JUMP_OFFSET           # case N offset

    // A sketch of some of the design decisions on this code.
    //
    // 1. The bodies of case expressions may be shared, e.g.:
    //   case FOO:
    //   case BAR:
    //     /* code */
    //   case BAZ:
    //     /* code */
    //  In this cases we want to build a single codeblock for the conditionals (e.g. for FOO and BAR).
    //
    // 2. The ending MTest can only be added to a conditional block once the next conditional
    //    block has been created, and ending MTest on the final conditional block can only be
    //    added after the default body block has been created.
    //
    //    For the above two reasons, the loop keeps track of the previous iteration's major
    //    components (cond block, body block, cmp instruction, body start pc, whether the
    //    previous case had a shared body, etc.) and uses them in the next iteration.
    //
    // 3. The default body block may be shared with the body of a 'case'.  This is tested for
    //    within the iteration loop in IonBuilder::lookupSwitch.  Also, the default body block
    //    may not occur at the end of the switch statements, and instead may occur in between.
    //
    //    For this reason, the default body may be created within the loop (when a regular body
    //    block is created, because the default body IS the regular body), or it will be created
    //    after the loop.  It must then still be inserted into the right location into the list
    //    of body blocks to process, which is done later in lookupSwitch.

    JS_ASSERT(op == JSOP_LOOKUPSWITCH);

    // Pop input.
    MDefinition *ins = current->pop();

    // Get the default and exit pc
    jsbytecode *exitpc = pc + js_GetSrcNoteOffset(sn, 0);
    jsbytecode *defaultpc = pc + GET_JUMP_OFFSET(pc);

    JS_ASSERT(defaultpc > pc && defaultpc <= exitpc);

    // Get ncases, which will be >= 1, since a zero-case switch
    // will get byte-compiled into a TABLESWITCH.
    jsbytecode *pc2 = pc;
    pc2 += JUMP_OFFSET_LEN;
    unsigned int ncases = GET_UINT16(pc2);
    pc2 += UINT16_LEN;
    JS_ASSERT(ncases >= 1);

    // Vector of body blocks.
    Vector<MBasicBlock*, 0, IonAllocPolicy> bodyBlocks;

    MBasicBlock *defaultBody = NULL;
    unsigned int defaultIdx = UINT_MAX;
    bool defaultShared = false;

    MBasicBlock *prevCond = NULL;
    MCompare *prevCmpIns = NULL;
    MBasicBlock *prevBody = NULL;
    bool prevShared = false;
    jsbytecode *prevpc = NULL;
    for (unsigned int i = 0; i < ncases; i++) {
        Value rval = script()->getConst(GET_UINT32_INDEX(pc2));
        pc2 += UINT32_INDEX_LEN;
        jsbytecode *casepc = pc + GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        JS_ASSERT(casepc > pc && casepc <= exitpc);
        JS_ASSERT_IF(i > 0, prevpc <= casepc);

        // Create case block
        MBasicBlock *cond = newBlock(((i == 0) ? current : prevCond), casepc);
        if (!cond)
            return ControlStatus_Error;

        MConstant *rvalIns = MConstant::New(rval);
        cond->add(rvalIns);

        MCompare *cmpIns = MCompare::New(ins, rvalIns, JSOP_STRICTEQ);
        cond->add(cmpIns);
        if (cmpIns->isEffectful() && !resumeAfter(cmpIns))
            return ControlStatus_Error;

        // Create or pull forward body block
        MBasicBlock *body;
        if (prevpc == casepc) {
            body = prevBody;
        } else {
            body = newBlock(cond, casepc);
            if (!body)
                return ControlStatus_Error;
            bodyBlocks.append(body);
        }

        // Check for default body
        if (defaultpc <= casepc && defaultIdx == UINT_MAX) {
            defaultIdx = bodyBlocks.length() - 1;
            if (defaultpc == casepc) {
                defaultBody = body;
                defaultShared = true;
            }
        }

        // Go back and fill in the MTest for the previous case block, or add the MGoto
        // to the current block
        if (i == 0) {
            // prevCond is definitely NULL, end 'current' with MGoto to this case.
            current->end(MGoto::New(cond));
        } else {
            // End previous conditional block with an MTest.
            MTest *test = MTest::New(prevCmpIns, prevBody, cond);
            prevCond->end(test);

            // If the previous cond shared its body with a prior cond, then
            // add the previous cond as a predecessor to its body (since it's
            // now finished).
            if (prevShared)
                prevBody->addPredecessor(prevCond);
        }

        // Save the current cond block, compare ins, and body block for next iteration
        prevCond = cond;
        prevCmpIns = cmpIns;
        prevBody = body;
        prevShared = (prevpc == casepc);
        prevpc = casepc;
    }

    // Create a new default body block if one was not already created.
    if (!defaultBody) {
        JS_ASSERT(!defaultShared);
        defaultBody = newBlock(prevCond, defaultpc);
        if (!defaultBody)
            return ControlStatus_Error;

        if (defaultIdx >= bodyBlocks.length())
            bodyBlocks.append(defaultBody);
        else
            bodyBlocks.insert(&bodyBlocks[defaultIdx], defaultBody);
    }

    // Add edge from last conditional block to the default block
    if (defaultBody == prevBody) {
        // Last conditional block goes to default body on both comparison
        // success and comparison failure.
        prevCond->end(MGoto::New(defaultBody));
    } else {
        // Last conditional block has body that is distinct from
        // the default block.
        MTest *test = MTest::New(prevCmpIns, prevBody, defaultBody);
        prevCond->end(test);

        // Add the cond as a predecessor as a default, but only if
        // the default is shared with another block, because otherwise
        // the default block would have been constructed with the final
        // cond as its predecessor anyway.
        if (defaultShared)
            defaultBody->addPredecessor(prevCond);
    }

    // If the last cond shared its body with a prior cond, then
    // it needs to be explicitly added as a predecessor now that it's finished.
    if (prevShared)
        prevBody->addPredecessor(prevCond);

    // Create CFGState
    CFGState state = CFGState::LookupSwitch(exitpc);
    if (!state.lookupswitch.bodies || !state.lookupswitch.bodies->init(bodyBlocks.length()))
        return ControlStatus_Error;

    // Fill bodies in CFGState using bodies in bodyBlocks, move them to
    // end in order in order to maintain RPO
    for (size_t i = 0; i < bodyBlocks.length(); i++) {
        (*state.lookupswitch.bodies)[i] = bodyBlocks[i];
    }
    graph().moveBlockToEnd(bodyBlocks[0]);

    // Create control flow info
    ControlFlowInfo switchinfo(cfgStack_.length(), exitpc);
    if (!switches_.append(switchinfo))
        return ControlStatus_Error;

    // If there is more than one block, next stopAt is at beginning of second block.
    if (state.lookupswitch.bodies->length() > 1)
        state.stopAt = (*state.lookupswitch.bodies)[1]->pc();
    if (!cfgStack_.append(state))
        return ControlStatus_Error;

    current = (*state.lookupswitch.bodies)[0];
    pc = current->pc();
    return ControlStatus_Jumped;
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
    jssrcnote *sn = info().getNote(cx, pc);

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
        jssrcnote *caseSn = info().getNote(cx, curCase);
        JS_ASSERT(caseSn && SN_TYPE(caseSn) == SRC_PCDELTA);
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
    CFGState state = CFGState::CondSwitch(exitpc, defaultTarget);
    if (!state.condswitch.bodies || !state.condswitch.bodies->init(nbBodies))
        return ControlStatus_Error;

    // We loop on case conditions with processCondSwitchCase.
    JS_ASSERT(JSOp(*firstCase) == JSOP_CASE);
    state.stopAt = firstCase;
    state.state = CFGState::COND_SWITCH_CASE;

    return cfgStack_.append(state);
}

IonBuilder::CFGState
IonBuilder::CFGState::CondSwitch(jsbytecode *exitpc, jsbytecode *defaultTarget)
{
    CFGState state;
    state.state = COND_SWITCH_CASE;
    state.stopAt = NULL;
    state.condswitch.bodies = (FixedList<MBasicBlock *> *)GetIonContext()->temp->allocate(
        sizeof(FixedList<MBasicBlock *>));
    state.condswitch.currentIdx = 0;
    state.condswitch.defaultTarget = defaultTarget;
    state.condswitch.defaultIdx = uint32_t(-1);
    state.condswitch.exitpc = exitpc;
    state.condswitch.breaks = NULL;
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
    jsbytecode *lastTarget = currentIdx ? bodies[currentIdx - 1]->pc() : NULL;

    // Fetch the following case in which we will continue.
    jssrcnote *sn = info().getNote(cx, pc);
    ptrdiff_t off = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *casePc = off ? pc + off : GetNextPc(pc);
    bool caseIsDefault = JSOp(*casePc) == JSOP_DEFAULT;
    JS_ASSERT(JSOp(*casePc) == JSOP_CASE || caseIsDefault);

    // Allocate the block of the matching case.
    bool bodyIsNew = false;
    MBasicBlock *bodyBlock = NULL;
    jsbytecode *bodyTarget = pc + GetJumpOffset(pc);
    if (lastTarget < bodyTarget) {
        // If the default body is in the middle or aliasing the current target.
        if (lastTarget < defaultTarget && defaultTarget <= bodyTarget) {
            JS_ASSERT(state.condswitch.defaultIdx == uint32_t(-1));
            state.condswitch.defaultIdx = currentIdx;
            bodies[currentIdx] = NULL;
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
    MBasicBlock *caseBlock = NULL;
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
        } else if (bodies[state.condswitch.defaultIdx] == NULL) {
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
        current = NULL;
        state.state = CFGState::COND_SWITCH_BODY;
        return processCondSwitchBody(state);
    }

    // Continue until the case condition.
    current = caseBlock;
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
    current = nextBody;
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
    TypeOracle::UnaryTypes types = oracle->unaryTypes(script(), pc);
    test->infer(types, cx);
    current->end(test);

    if (!cfgStack_.append(CFGState::AndOr(joinStart, join)))
        return false;

    current = evalRhs;
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
    jssrcnote *sn = info().getNote(cx, pc);
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
        JS_ASSERT(!info().getNote(cx, trueEnd));

        jsbytecode *falseEnd = trueEnd + GetJumpOffset(trueEnd);
        JS_ASSERT(falseEnd > trueEnd);
        JS_ASSERT(falseEnd >= falseStart);

        if (!cfgStack_.append(CFGState::IfElse(trueEnd, falseEnd, ifFalse)))
            return false;
        break;
      }

      default:
        JS_NOT_REACHED("unexpected source note type");
        break;
    }

    // Switch to parsing the true branch. Note that no PC update is needed,
    // it's the next instruction.
    current = ifTrue;

    return true;
}

IonBuilder::ControlStatus
IonBuilder::processReturn(JSOp op)
{
    MDefinition *def;
    switch (op) {
      case JSOP_RETURN:
        def = current->pop();
        break;

      case JSOP_STOP:
      {
        MInstruction *ins = MConstant::New(UndefinedValue());
        current->add(ins);
        def = ins;
        break;
      }

      default:
        def = NULL;
        JS_NOT_REACHED("unknown return op");
        break;
    }

    if (instrumentedProfiling())
        current->add(MFunctionBoundary::New(script(), MFunctionBoundary::Exit));
    MReturn *ret = MReturn::New(def);
    current->end(ret);

    if (!graph().addExit(current))
        return ControlStatus_Error;

    // Make sure no one tries to use this block now.
    current = NULL;
    return processControlEnd();
}

IonBuilder::ControlStatus
IonBuilder::processThrow()
{
    MDefinition *def = current->pop();

    MThrow *ins = MThrow::New(def);
    current->end(ins);

    if (!graph().addExit(current))
        return ControlStatus_Error;

    // Make sure no one tries to use this block now.
    current = NULL;
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
    ins->infer(oracle->unaryTypes(script(), pc));

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
        JS_NOT_REACHED("unexpected bitop");
        return false;
    }

    current->add(ins);
    TypeOracle::BinaryTypes types = oracle->binaryTypes(script(), pc);
    ins->infer(types);

    current->push(ins);
    if (ins->isEffectful() && !resumeAfter(ins))
        return false;

    return true;
}

bool
IonBuilder::jsop_binary(JSOp op, MDefinition *left, MDefinition *right)
{
    TypeOracle::Binary b = oracle->binaryOp(script(), pc);

    if (op == JSOP_ADD && b.rval == MIRType_String &&
        (b.lhs == MIRType_String || b.lhs == MIRType_Int32) &&
        (b.rhs == MIRType_String || b.rhs == MIRType_Int32))
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
        JS_NOT_REACHED("unexpected binary opcode");
        return false;
    }

    TypeOracle::BinaryTypes types = oracle->binaryTypes(script(), pc);
    current->add(ins);
    ins->infer(types, cx);
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
    TypeOracle::Unary types = oracle->unaryOp(script(), pc);
    if (IsNumberType(types.ival)) {
        // Already int32 or double.
        JS_ASSERT(IsNumberType(types.rval));
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
    AutoAccumulateExits(MIRGraph &graph, MIRGraphExits &exits) : graph_(graph) {
        prev_ = graph_.exitAccumulator();
        graph_.setExitAccumulator(&exits);
    }
    ~AutoAccumulateExits() {
        graph_.setExitAccumulator(prev_);
    }
};


bool
IonBuilder::jsop_call_inline(HandleFunction callee, uint32_t argc, bool constructing,
                             MConstant *constFun, MBasicBlock *bottom,
                             Vector<MDefinition *, 8, IonAllocPolicy> &retvalDefns)
{
    AssertCanGC();

    int calleePos = -((int) argc + 2);
    current->peek(calleePos)->setFoldedUnchecked();

    // Rewrite the stack position containing the function with the constant
    // function definition, before we take the inlineResumePoint
    current->rewriteAtDepth(calleePos, constFun);

    // This resume point collects outer variables only.  It is used to recover
    // the stack state before the current bytecode.
    MResumePoint *inlineResumePoint =
        MResumePoint::New(current, pc, callerResumePoint_, MResumePoint::Outer);
    if (!inlineResumePoint)
        return false;

    // We do not inline JSOP_FUNCALL for now.
    JS_ASSERT(argc == GET_ARGC(inlineResumePoint->pc()));

    // Gather up  the arguments and |this| to the inline function.
    // Note that we leave the callee on the simulated stack for the
    // duration of the call.
    MDefinitionVector argv;
    if (!argv.resizeUninitialized(argc + 1))
        return false;
    for (int32_t i = argc; i >= 0; i--)
        argv[i] = current->pop();

    // Compilation information is allocated for the duration of the current tempLifoAlloc
    // lifetime.
    RootedScript calleeScript(cx, callee->nonLazyScript());
    CompileInfo *info = cx->tempLifoAlloc().new_<CompileInfo>(calleeScript.get(), callee,
                                                              (jsbytecode *)NULL, constructing,
                                                              SequentialExecution);
    if (!info)
        return false;

    MIRGraphExits saveExits;
    AutoAccumulateExits aae(graph(), saveExits);

    TypeInferenceOracle oracle;
    if (!oracle.init(cx, calleeScript))
        return false;

    IonBuilder inlineBuilder(cx, &temp(), &graph(), &oracle,
                             info, inliningDepth + 1, loopDepth_);

    // Create |this| on the caller-side for inlined constructors.
    MDefinition *thisDefn = NULL;
    if (constructing) {
        thisDefn = createThis(callee, constFun);
        if (!thisDefn)
            return false;
    } else {
        thisDefn = argv[0];
    }

    // Build the graph.
    if (!inlineBuilder.buildInline(this, inlineResumePoint, thisDefn, argv))
        return false;

    MIRGraphExits &exits = *inlineBuilder.graph().exitAccumulator();

    // Replace all MReturns with MGotos, and remember the MDefinition that
    // would have been returned.
    for (MBasicBlock **it = exits.begin(), **end = exits.end(); it != end; ++it) {
        MBasicBlock *exitBlock = *it;

        MDefinition *rval = exitBlock->lastIns()->toReturn()->getOperand(0);
        exitBlock->discardLastIns();

        // Inlined constructors return |this| unless overridden by another Object.
        if (constructing) {
            if (rval->type() == MIRType_Value) {
                MReturnFromCtor *filter = MReturnFromCtor::New(rval, thisDefn);
                exitBlock->add(filter);
                rval = filter;
            } else if (rval->type() != MIRType_Object) {
                rval = thisDefn;
            }
        }

        if (!retvalDefns.append(rval))
            return false;

        MGoto *replacement = MGoto::New(bottom);
        exitBlock->end(replacement);
        if (!bottom->addPredecessorWithoutPhis(exitBlock))
            return false;
    }
    JS_ASSERT(!retvalDefns.empty());
    return true;
}

bool
IonBuilder::makeInliningDecision(AutoObjectVector &targets, uint32_t argc)
{
    AssertCanGC();

    if (inliningDepth >= js_IonOptions.maxInlineDepth)
        return false;

    // For "small" functions, we should be more aggressive about inlining.
    // This is based on the following intuition:
    //  1. The call overhead for a small function will likely be a much
    //     higher proportion of the runtime of the function than for larger
    //     functions.
    //  2. The cost of inlining (in terms of size expansion of the SSA graph),
    //     and size expansion of the ultimately generated code, will be
    //     less significant.
    //  3. Do not inline functions which are not called as frequently as their
    //     callers.

    uint32_t callerUses = script()->getUseCount();

    uint32_t totalSize = 0;
    uint32_t checkUses = js_IonOptions.usesBeforeInlining;
    bool allFunctionsAreSmall = true;
    RootedFunction target(cx);
    RootedScript targetScript(cx);
    for (size_t i = 0; i < targets.length(); i++) {
        target = targets[i]->toFunction();
        if (!target->isInterpreted())
            return false;

        targetScript = target->nonLazyScript();
        uint32_t calleeUses = targetScript->getUseCount();

        if (target->nargs < argc) {
            IonSpew(IonSpew_Inlining, "Not inlining, overflow of arguments.");
            return false;
        }

        totalSize += targetScript->length;
        if (totalSize > js_IonOptions.inlineMaxTotalBytecodeLength)
            return false;

        if (targetScript->length > js_IonOptions.smallFunctionMaxBytecodeLength)
            allFunctionsAreSmall = false;

        if (calleeUses * js_IonOptions.inlineUseCountRatio < callerUses) {
            IonSpew(IonSpew_Inlining, "Not inlining, callee is not hot");
            return false;
        }
    }
    if (allFunctionsAreSmall)
        checkUses = js_IonOptions.smallFunctionUsesBeforeInlining;

    if (script()->getUseCount() < checkUses) {
        IonSpew(IonSpew_Inlining, "Not inlining, caller is not hot");
        return false;
    }

    RootedScript scriptRoot(cx, script());
    if (!oracle->canInlineCall(scriptRoot, pc)) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to uninlineable call site");
        return false;
    }

    for (size_t i = 0; i < targets.length(); i++) {
        if (!canInlineTarget(targets[i]->toFunction())) {
            IonSpew(IonSpew_Inlining, "Decided not to inline");
            return false;
        }
    }

    return true;
}

static bool
ValidateInlineableGetPropertyCache(MGetPropertyCache *getPropCache, MDefinition *thisDefn,
                                   size_t maxUseCount)
{
    JS_ASSERT(getPropCache->object()->type() == MIRType_Object);

    if (getPropCache->useCount() > maxUseCount)
        return false;

    // Ensure that the input to the GetPropertyCache is the thisDefn for this function.
    if (getPropCache->object() != thisDefn)
        return false;

    InlinePropertyTable *propTable = getPropCache->inlinePropertyTable();
    if (!propTable || propTable->numEntries() == 0)
        return false;

    return true;
}

MGetPropertyCache *
IonBuilder::checkInlineableGetPropertyCache(uint32_t argc)
{
    // Stack state:
    // ..., Func, This, Arg1, ..., ArgC
    // Note: PassArgs have already been eliminated.

    JS_ASSERT(current->stackDepth() >= argc + 2);

    // Ensure that This is object-typed.
    int thisDefnDepth = -((int) argc + 1);
    MDefinition *thisDefn = current->peek(thisDefnDepth);
    if (thisDefn->type() != MIRType_Object)
        return NULL;

    // Ensure that Func is defined by a GetPropertyCache that is then TypeBarriered and then
    // infallibly Unboxed to an object.
    int funcDefnDepth = -((int) argc + 2);
    MDefinition *funcDefn = current->peek(funcDefnDepth);
    if (funcDefn->type() != MIRType_Object)
        return NULL;

    // If it's a constant, then ignore it since there's nothing to optimize: any potential
    // GetProp that led to the funcDefn has already been optimized away.
    if (funcDefn->isConstant())
        return NULL;

    // Match patterns:
    // 1. MGetPropertyCache
    // 2. MUnbox[MIRType_Object, Infallible] <- MTypeBarrier <- MGetPropertyCache

    // If it's a GetPropertyCache, return it immediately, but make sure its not used anywhere
    // else (because otherwise we wouldn't be able to move it).
    if (funcDefn->isGetPropertyCache()) {
        MGetPropertyCache *getPropCache = funcDefn->toGetPropertyCache();
        if (!ValidateInlineableGetPropertyCache(getPropCache, thisDefn, 0))
            return NULL;

        return getPropCache;
    }

    // Check for MUnbox[MIRType_Object, Infallible] <- MTypeBarrier <- MGetPropertyCache
    if (!funcDefn->isUnbox() || funcDefn->toUnbox()->useCount() > 0)
        return NULL;

    MUnbox *unbox = current->peek(funcDefnDepth)->toUnbox();
    if (unbox->mode() != MUnbox::Infallible || !unbox->input()->isTypeBarrier())
        return NULL;

    MTypeBarrier *typeBarrier = unbox->input()->toTypeBarrier();
    if (typeBarrier->useCount() != 1 || !typeBarrier->input()->isGetPropertyCache())
        return NULL;

    MGetPropertyCache *getPropCache = typeBarrier->input()->toGetPropertyCache();
    JS_ASSERT(getPropCache->object()->type() == MIRType_Object);

    if (!ValidateInlineableGetPropertyCache(getPropCache, thisDefn, 1))
        return NULL;

    return getPropCache;
}

MPolyInlineDispatch *
IonBuilder::makePolyInlineDispatch(JSContext *cx, AutoObjectVector &targets, int argc,
                                   MGetPropertyCache *getPropCache,
                                   types::StackTypeSet *types, types::StackTypeSet *barrier,
                                   MBasicBlock *bottom,
                                   Vector<MDefinition *, 8, IonAllocPolicy> &retvalDefns)
{
    int funcDefnDepth = -((int) argc + 2);
    MDefinition *funcDefn = current->peek(funcDefnDepth);

    // If we're not optimizing away a GetPropertyCache, then this is pretty simple.
    if (!getPropCache)
        return MPolyInlineDispatch::New(funcDefn);

    InlinePropertyTable *inlinePropTable = getPropCache->inlinePropertyTable();

    // Take a resumepoint at this point so we can capture the state of the stack
    // immediately prior to the call operation.
    MResumePoint *preCallResumePoint = MResumePoint::New(current, pc, callerResumePoint_,
                                                         MResumePoint::ResumeAt);
    if (!preCallResumePoint)
        return NULL;
    DebugOnly<size_t> preCallFuncDefnIdx = preCallResumePoint->numOperands() - (((size_t) argc) + 2);
    JS_ASSERT(preCallResumePoint->getOperand(preCallFuncDefnIdx) == funcDefn);

    MDefinition *targetObject = getPropCache->object();

    // If we got here, then we know the following:
    //      1. The input to the CALL is a GetPropertyCache, or a GetPropertyCache
    //         followed by a TypeBarrier followed by an Unbox.
    //      2. The GetPropertyCache has inlineable cases by guarding on the Object's type
    //      3. The GetPropertyCache (and sequence of definitions) leading to the function
    //         definition is not used by anyone else.
    //      4. Notably, this means that no resume points as of yet capture the GetPropertyCache,
    //         which implies that everything from the GetPropertyCache up to the call is
    //         repeatable.

    // If we are optimizing away a getPropCache, we replace the funcDefn
    // with a constant undefined on the stack.
    MConstant *undef = MConstant::New(UndefinedValue());
    current->add(undef);
    current->rewriteAtDepth(funcDefnDepth, undef);

    // Now construct a fallbackPrepBlock that prepares the stack state for fallback.
    // Namely it pops off all the arguments and the callee.
    MBasicBlock *fallbackPrepBlock = newBlock(current, pc);
    if (!fallbackPrepBlock)
        return NULL;

    for (int i = argc + 1; i >= 0; i--)
        (void) fallbackPrepBlock->pop();

    // Generate a fallback block that'll do the call, but the PC for this fallback block
    // is the PC for the GetPropCache.
    JS_ASSERT(inlinePropTable->pc() != NULL);
    JS_ASSERT(inlinePropTable->priorResumePoint() != NULL);
    MBasicBlock *fallbackBlock = newBlock(fallbackPrepBlock, inlinePropTable->pc(),
                                          inlinePropTable->priorResumePoint());
    if (!fallbackBlock)
        return NULL;

    fallbackPrepBlock->end(MGoto::New(fallbackBlock));

    // The fallbackBlock inherits the state of the stack right before the getprop, which
    // means we have to pop off the target of the getprop before performing it.
    DebugOnly<MDefinition *> checkTargetObject = fallbackBlock->pop();
    JS_ASSERT(checkTargetObject == targetObject);

    // Remove the instructions leading to the function definition from the current
    // block and add them to the fallback block.  Also, discard the old instructions.
    if (funcDefn->isGetPropertyCache()) {
        JS_ASSERT(funcDefn->toGetPropertyCache() == getPropCache);
        fallbackBlock->addFromElsewhere(getPropCache);
        fallbackBlock->push(getPropCache);
    } else {
        JS_ASSERT(funcDefn->isUnbox());
        MUnbox *unbox = funcDefn->toUnbox();
        JS_ASSERT(unbox->input()->isTypeBarrier());
        JS_ASSERT(unbox->type() == MIRType_Object);
        JS_ASSERT(unbox->mode() == MUnbox::Infallible);

        MTypeBarrier *typeBarrier = unbox->input()->toTypeBarrier();
        JS_ASSERT(typeBarrier->input()->isGetPropertyCache());
        JS_ASSERT(typeBarrier->input()->toGetPropertyCache() == getPropCache);

        fallbackBlock->addFromElsewhere(getPropCache);
        fallbackBlock->addFromElsewhere(typeBarrier);
        fallbackBlock->addFromElsewhere(unbox);
        fallbackBlock->push(unbox);
    }

    // Finally create a fallbackEnd block to do the actual call.  The fallbackEnd block will
    // have the |pc| restored to the current PC.
    MBasicBlock *fallbackEndBlock = newBlock(fallbackBlock, pc, preCallResumePoint);
    if (!fallbackEndBlock)
        return NULL;
    fallbackBlock->end(MGoto::New(fallbackEndBlock));

    // Create Call
    MCall *call = MCall::New(NULL, argc + 1, argc, false);
    if (!call)
        return NULL;

    // Set up the MPrepCall
    MPrepareCall *prepCall = new MPrepareCall;
    fallbackEndBlock->add(prepCall);

    // Grab the arguments for the call directly from the current block's stack.
    for (int32_t i = 0; i <= argc; i++) {
        int32_t argno = argc - i;
        MDefinition *argDefn = fallbackEndBlock->pop();
        JS_ASSERT(!argDefn->isPassArg());
        MPassArg *passArg = MPassArg::New(argDefn);
        fallbackEndBlock->add(passArg);
        call->addArg(argno, passArg);
    }

    // Insert an MPrepareCall before the first argument.
    call->initPrepareCall(prepCall);

    // Add the callee function definition to the call.
    call->initFunction(fallbackEndBlock->pop());

    fallbackEndBlock->add(call);
    fallbackEndBlock->push(call);
    if (!resumeAfter(call))
        return NULL;

    MBasicBlock *top = current;
    current = fallbackEndBlock;
    if (!pushTypeBarrier(call, types, barrier))
        return NULL;
    current = top;

    // Create a new MPolyInlineDispatch containing the getprop and the fallback block
    return MPolyInlineDispatch::New(targetObject, inlinePropTable,
                                    fallbackPrepBlock, fallbackBlock,
                                    fallbackEndBlock);
}

bool
IonBuilder::inlineScriptedCall(AutoObjectVector &targets, uint32_t argc, bool constructing,
                               types::StackTypeSet *types, types::StackTypeSet *barrier)
{
#ifdef DEBUG
    uint32_t origStackDepth = current->stackDepth();
#endif

    IonSpew(IonSpew_Inlining, "Inlining %d targets", (int) targets.length());
    JS_ASSERT(targets.length() > 0);

    // |top| jumps into the callee subgraph -- save it for later use.
    MBasicBlock *top = current;

    // Unwrap all the MPassArgs and replace them with their inputs, and discard the
    // MPassArgs.
    for (int32_t i = argc; i >= 0; i--) {
        // Unwrap each MPassArg, replacing it with its contents.
        int argSlotDepth = -((int) i + 1);
        MPassArg *passArg = top->peek(argSlotDepth)->toPassArg();
        MBasicBlock *block = passArg->block();
        MDefinition *wrapped = passArg->getArgument();
        wrapped->setFoldedUnchecked();
        passArg->replaceAllUsesWith(wrapped);
        top->rewriteAtDepth(argSlotDepth, wrapped);
        block->discard(passArg);
    }

    // Check if the input is a GetPropertyCache that can be eliminated via guards on
    // the |this| object's typeguards.
    MGetPropertyCache *getPropCache = NULL;
    if (!constructing) {
        getPropCache = checkInlineableGetPropertyCache(argc);
        if(getPropCache) {
            InlinePropertyTable *inlinePropTable = getPropCache->inlinePropertyTable();
            // checkInlineableGetPropertyCache should have verified this.
            JS_ASSERT(inlinePropTable != NULL);

            int numCases = inlinePropTable->numEntries();
            IonSpew(IonSpew_Inlining, "Got inlineable property cache with %d cases", numCases);

            inlinePropTable->trimToTargets(targets);

            // Trim the cases based on those that match the targets at this call site.
            IonSpew(IonSpew_Inlining, "%d inlineable cases left after trimming to %d targets",
                                        (int) inlinePropTable->numEntries(),
                                        (int) targets.length());

            if (inlinePropTable->numEntries() == 0)
                getPropCache = NULL;
        }
    }

    // Create a |bottom| block for all the callee subgraph exits to jump to.
    JS_ASSERT(types::IsInlinableCall(pc));
    jsbytecode *postCall = GetNextPc(pc);
    MBasicBlock *bottom = newBlock(NULL, postCall);
    if (!bottom)
        return false;
    bottom->setCallerResumePoint(callerResumePoint_);

    Vector<MDefinition *, 8, IonAllocPolicy> retvalDefns;

    // Do the inline build. Return value definitions are stored in retvalDefns.
    // The monomorphic inlining only occurs if we're not handling a getPropCache guard
    // optimization.  The reasoning for this is as follows:
    //      If there was a single object type leading to a single inlineable function, then
    //      the getprop would have been optimized away to a constant load anyway.
    //
    //      If there were more than one object types where we could narrow the generated
    //      function to a single one, then we still want to guard on typeobject and save the
    //      cost of the GetPropCache.
    if (getPropCache == NULL && targets.length() == 1) {
        JSFunction *func = targets[0]->toFunction();
        MConstant *constFun = MConstant::New(ObjectValue(*func));
        current->add(constFun);

        // Monomorphic case is simple - no guards.
        RootedFunction target(cx, func);
        if (!jsop_call_inline(target, argc, constructing, constFun, bottom, retvalDefns))
            return false;

        // The Inline_Enter node is handled by buildInline, we're responsible
        // for the Inline_Exit node (mostly for the case below)
        if (instrumentedProfiling())
            bottom->add(MFunctionBoundary::New(NULL, MFunctionBoundary::Inline_Exit));

    } else {
        // In the polymorphic case, we end the current block with a MPolyInlineDispatch instruction.

        // Create a PolyInlineDispatch instruction for this call site
        MPolyInlineDispatch *disp = makePolyInlineDispatch(cx, targets, argc, getPropCache,
                                                           types, barrier, bottom, retvalDefns);
        if (!disp)
            return false;
        for (size_t i = 0; i < targets.length(); i++) {
            // Create an MConstant for the function
            JSFunction *func = targets[i]->toFunction();
            RootedFunction target(cx, func);
            MConstant *constFun = MConstant::New(ObjectValue(*func));

            // Create new entry block for the inlined callee graph.
            MBasicBlock *entryBlock = newBlock(current, pc);
            if (!entryBlock)
                return false;

            // Add case to PolyInlineDispatch
            entryBlock->add(constFun);
            disp->addCallee(constFun, entryBlock);
        }
        top->end(disp);

        // If profiling is enabled, then we need a clear-cut boundary of all of
        // the inlined functions which is distinct from the fallback path where
        // no inline functions are entered. In the case that there's a fallback
        // path and a set of inline functions, we create a new block as a join
        // point for all of the inline paths which will then go to the real end
        // block: 'bottom'. This 'inlineBottom' block is never different from
        // 'bottom' except for this one case where profiling is turned on.
        MBasicBlock *inlineBottom = bottom;
        if (instrumentedProfiling() && disp->inlinePropertyTable()) {
            inlineBottom = newBlock(NULL, pc);
            if (inlineBottom == NULL)
                return false;
        }

        for (size_t i = 0; i < disp->numCallees(); i++) {
            // Do the inline function build.
            MConstant *constFun = disp->getFunctionConstant(i);
            RootedFunction target(cx, constFun->value().toObject().toFunction());
            MBasicBlock *block = disp->getSuccessor(i);
            graph().moveBlockToEnd(block);
            current = block;

            if (!jsop_call_inline(target, argc, constructing, constFun, inlineBottom, retvalDefns))
                return false;
        }

        // Regardless of whether inlineBottom != bottom, demarcate these exits
        // with an Inline_Exit instruction signifying that the inlined functions
        // on this level have all ceased running.
        if (instrumentedProfiling())
            inlineBottom->add(MFunctionBoundary::New(NULL, MFunctionBoundary::Inline_Exit));

        // In the case where we had to create a new block, all of the returns of
        // the inline functions need to be merged together with a phi node. This
        // phi node resident in the 'inlineBottom' block is then an input to the
        // phi node for this entire call sequence in the 'bottom' block.
        if (inlineBottom != bottom) {
            graph().moveBlockToEnd(inlineBottom);
            inlineBottom->inheritSlots(top);
            if (!inlineBottom->initEntrySlots())
                return false;

            // Only need to phi returns together if there's more than one
            if (retvalDefns.length() > 1) {
                // This is the same depth as the phi node of the 'bottom' block
                // after all of the 'pops' happen (see pop() sequence below)
                MPhi *phi = MPhi::New(inlineBottom->stackDepth() - argc - 2);
                inlineBottom->addPhi(phi);

                MDefinition **it = retvalDefns.begin(), **end = retvalDefns.end();
                for (; it != end; ++it) {
                    if (!phi->addInput(*it))
                        return false;
                }
                // retvalDefns should become a singleton vector of 'phi'
                retvalDefns.clear();
                if (!retvalDefns.append(phi))
                    return false;
            }

            inlineBottom->end(MGoto::New(bottom));
            if (!bottom->addPredecessorWithoutPhis(inlineBottom))
                return false;
        }

        // If inline property table is set on the dispatch instruction, then there is
        // a fallback case to consider.  Move the fallback blocks to the end of the graph
        // and link them to the bottom block.
        if (disp->inlinePropertyTable()) {
            graph().moveBlockToEnd(disp->fallbackPrepBlock());
            graph().moveBlockToEnd(disp->fallbackMidBlock());
            graph().moveBlockToEnd(disp->fallbackEndBlock());

            // Link the end fallback block to bottom.
            MBasicBlock *fallbackEndBlock = disp->fallbackEndBlock();
            MDefinition *fallbackResult = fallbackEndBlock->pop();
            if(!retvalDefns.append(fallbackResult))
                return false;
            fallbackEndBlock->end(MGoto::New(bottom));
            if (!bottom->addPredecessorWithoutPhis(fallbackEndBlock))
                return false;
        }
    }

    graph().moveBlockToEnd(bottom);

    bottom->inheritSlots(top);

    // If we were doing a polymorphic inline, then the discardCallArgs
    // happened in sub-frames, not the top frame.  Need to get rid of
    // those in the bottom.
    if (getPropCache || targets.length() > 1) {
        for (uint32_t i = 0; i < argc + 1; i++)
            bottom->pop();
    }

    // Pop the callee and push the return value.
    bottom->pop();

    MDefinition *retvalDefn;
    if (retvalDefns.length() > 1) {
        // Need to create a phi to merge the returns together.
        MPhi *phi = MPhi::New(bottom->stackDepth());
        bottom->addPhi(phi);

        for (MDefinition **it = retvalDefns.begin(), **end = retvalDefns.end(); it != end; ++it) {
            if (!phi->addInput(*it))
                return false;
        }
        retvalDefn = phi;
    } else {
        retvalDefn = retvalDefns.back();
    }

    bottom->push(retvalDefn);

    // Initialize entry slots now that the stack has been fixed up.
    if (!bottom->initEntrySlots())
        return false;

    // If this inlining was a polymorphic one, then create a new bottom block
    // to continue from.  This is because the resumePoint above would have captured
    // an incorrect stack state (with all the arguments pushed).  That's ok because
    // the Phi that is the first instruction on the bottom node can't bail out, but
    // it's not ok if some subsequent instruction bails.

    if (getPropCache || targets.length() > 1) {
        MBasicBlock *bottom2 = newBlock(bottom, postCall);
        if (!bottom2)
            return false;

        bottom->end(MGoto::New(bottom2));
        current = bottom2;
    } else {
        current = bottom;
    }

    // Check the depth change:
    //  -argc for popped args
    //  -2 for callee/this
    //  +1 for retval
    JS_ASSERT(current->stackDepth() == origStackDepth - argc - 1);

    return true;
}

MInstruction *
IonBuilder::createDeclEnvObject(MDefinition *callee, MDefinition *scope)
{
    // Create a template CallObject that we'll use to generate inline object
    // creation.

    RootedScript script(cx, script_);
    RootedFunction fun(cx, info().fun());
    RootedObject templateObj(cx, DeclEnvObject::createTemplateObject(cx, fun));
    if (!templateObj)
        return NULL;

    // Add dummy values on the slot of the template object such as we do not try
    // mark uninitialized values.
    templateObj->setFixedSlot(DeclEnvObject::enclosingScopeSlot(), MagicValue(JS_GENERIC_MAGIC));
    templateObj->setFixedSlot(DeclEnvObject::lambdaSlot(), MagicValue(JS_GENERIC_MAGIC));

    // One field is added to the function to handle its name.  This cannot be a
    // dynamic slot because there is still plenty of room on the DeclEnv object.
    JS_ASSERT(!templateObj->hasDynamicSlots());

    // Allocate the actual object. It is important that no intervening
    // instructions could potentially bailout, thus leaking the dynamic slots
    // pointer.
    MInstruction *declEnvObj = MNewDeclEnvObject::New(templateObj);
    current->add(declEnvObj);

    // Initialize the object's reserved slots.
    current->add(MStoreFixedSlot::New(declEnvObj, DeclEnvObject::enclosingScopeSlot(), scope));
    current->add(MStoreFixedSlot::New(declEnvObj, DeclEnvObject::lambdaSlot(), callee));

    return declEnvObj;
}

MInstruction *
IonBuilder::createCallObject(MDefinition *callee, MDefinition *scope)
{
    // Create a template CallObject that we'll use to generate inline object
    // creation.

    RootedScript scriptRoot(cx, script());
    RootedObject templateObj(cx, CallObject::createTemplateObject(cx, scriptRoot));
    if (!templateObj)
        return NULL;

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
    // pointer.
    MInstruction *callObj = MNewCallObject::New(templateObj, slots);
    current->add(callObj);

    // Initialize the object's reserved slots.
    current->add(MStoreFixedSlot::New(callObj, CallObject::enclosingScopeSlot(), scope));
    current->add(MStoreFixedSlot::New(callObj, CallObject::calleeSlot(), callee));

    // Initialize argument slots.
    for (AliasedFormalIter i(script()); i; i++) {
        unsigned slot = i.scopeSlot();
        unsigned formal = i.frameIndex();
        MDefinition *param = current->getSlot(info().argSlot(formal));
        if (slot >= templateObj->numFixedSlots())
            current->add(MStoreSlot::New(slots, slot - templateObj->numFixedSlots(), param));
        else
            current->add(MStoreFixedSlot::New(callObj, slot, param));
    }

    return callObj;
}

MDefinition *
IonBuilder::createThisNative()
{
    // Native constructors build the new Object themselves.
    MConstant *magic = MConstant::New(MagicValue(JS_IS_CONSTRUCTING));
    current->add(magic);
    return magic;
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
        MGetPropertyCache *getPropCache = MGetPropertyCache::New(callee, cx->names().classPrototype);
        getPropCache->setIdempotent();
        getProto = getPropCache;
    } else {
        MCallGetProperty *callGetProp = MCallGetProperty::New(callee, cx->names().classPrototype);
        callGetProp->setIdempotent();
        getProto = callGetProp;
    }
    current->add(getProto);

    // Create this from prototype
    MCreateThis *createThis = MCreateThis::New(callee, getProto);
    current->add(createThis);

    return createThis;
}

JSObject *
IonBuilder::getSingletonPrototype(JSFunction *target)
{
    if (!target->hasSingletonType())
        return NULL;
    if (target->getType(cx)->unknownProperties())
        return NULL;

    jsid protoid = NameToId(cx->names().classPrototype);
    types::HeapTypeSet *protoTypes = target->getType(cx)->getProperty(cx, protoid, false);
    if (!protoTypes)
        return NULL;

    return protoTypes->getSingleton(cx);
}

MDefinition *
IonBuilder::createThisScriptedSingleton(HandleFunction target, HandleObject proto, MDefinition *callee)
{
    // Generate an inline path to create a new |this| object with
    // the given singleton prototype.
    types::TypeObject *type = proto->getNewType(cx, target);
    if (!type)
        return NULL;
    if (!types::TypeScript::ThisTypes(target->nonLazyScript())->hasType(types::Type::ObjectType(type)))
        return NULL;

    RootedObject templateObject(cx, js_CreateThisForFunctionWithProto(cx, target, proto));
    if (!templateObject)
        return NULL;

    // Trigger recompilation if the templateObject changes.
    if (templateObject->type()->newScript)
        types::HeapTypeSet::WatchObjectStateChange(cx, templateObject->type());

    MCreateThisWithTemplate *createThis = MCreateThisWithTemplate::New(templateObject);
    current->add(createThis);

    return createThis;
}

MDefinition *
IonBuilder::createThis(HandleFunction target, MDefinition *callee)
{
    // Create this for unknown target
    if (!target)
        return createThisScripted(callee);

    // Create this for native function
    if (target->isNative()) {
        if (!target->isNativeConstructor())
            return NULL;
        return createThisNative();
    }

    // Create this with known prototype.
    RootedObject proto(cx, getSingletonPrototype(target));

    // Try baking in the prototype.
    if (proto) {
        MDefinition *createThis = createThisScriptedSingleton(target, proto, callee);
        if (createThis)
            return createThis;
    }

    MDefinition *createThis = createThisScripted(callee);
    if (!createThis)
        return NULL;

    // The native function case is already handled upfront.
    // Here we can safely remove the native check for MCreateThis.
    JS_ASSERT(createThis->isCreateThis());
    createThis->toCreateThis()->removeNativeCheck();

    return createThis;
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

    // If |Function.prototype.call| may be overridden, don't optimize callsite.
    RootedFunction native(cx, getSingleCallTarget(argc, pc));
    if (!native || !native->isNative() || native->native() != &js_fun_call)
        return makeCall(native, argc, false);

    // Extract call target.
    types::StackTypeSet *funTypes = oracle->getCallArg(script(), argc, 0, pc);
    RootedObject funobj(cx, (funTypes) ? funTypes->getSingleton() : NULL);
    RootedFunction target(cx, (funobj && funobj->isFunction()) ? funobj->toFunction() : NULL);

    // Unwrap the (JSFunction *) parameter.
    int funcDepth = -((int)argc + 1);
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

    // Call without inlining.
    return makeCall(target, argc, false);
}

bool
IonBuilder::jsop_funapply(uint32_t argc)
{
    RootedFunction native(cx, getSingleCallTarget(argc, pc));
    if (argc != 2)
        return makeCall(native, argc, false);

    // Disable compilation if the second argument to |apply| cannot be guaranteed
    // to be either definitely |arguments| or definitely not |arguments|.
    types::StackTypeSet *argObjTypes = oracle->getCallArg(script(), argc, 2, pc);
    LazyArgumentsType isArgObj = oracle->isArgumentObject(argObjTypes);
    if (isArgObj == MaybeArguments)
        return abort("fun.apply with MaybeArguments");

    // Fallback to regular call if arg 2 is not definitely |arguments|.
    if (isArgObj != DefinitelyArguments)
        return makeCall(native, argc, false);

    if (!native ||
        !native->isNative() ||
        native->native() != js_fun_apply)
    {
        return abort("fun.apply speculation failed");
    }

    // Stack for JSOP_FUNAPPLY:
    // 1:      MPassArg(Vp)
    // 2:      MPassArg(This)
    // argc+1: MPassArg(JSFunction *), the 'f' in |f.call()|, in |this| position.
    // argc+2: The native 'apply' function.

    // Extract call target.
    types::StackTypeSet *funTypes = oracle->getCallArg(script(), argc, 0, pc);
    RootedObject funobj(cx, (funTypes) ? funTypes->getSingleton() : NULL);
    RootedFunction target(cx, (funobj && funobj->isFunction()) ? funobj->toFunction() : NULL);

    // Vp
    MPassArg *passVp = current->pop()->toPassArg();
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

    types::StackTypeSet *barrier;
    types::StackTypeSet *types = oracle->returnTypeSet(script(), pc, &barrier);
    return pushTypeBarrier(apply, types, barrier);
}

bool
IonBuilder::jsop_call(uint32_t argc, bool constructing)
{
    AssertCanGC();

    // Acquire known call target if existent.
    AutoObjectVector targets(cx);
    uint32_t numTargets = getPolyCallTargets(argc, pc, targets, 4);
    types::StackTypeSet *barrier;
    types::StackTypeSet *types = oracle->returnTypeSet(script(), pc, &barrier);

    // Attempt to inline native and scripted functions.
    if (inliningEnabled()) {
        // Inline a single native call if possible.
        if (numTargets == 1 && targets[0]->toFunction()->isNative()) {
            RootedFunction target(cx, targets[0]->toFunction());
            switch (inlineNativeCall(target->native(), argc, constructing)) {
              case InliningStatus_Inlined:
                return true;
              case InliningStatus_Error:
                return false;
              case InliningStatus_NotInlined:
                break;
            }
        }

        if (numTargets > 0 && makeInliningDecision(targets, argc))
            return inlineScriptedCall(targets, argc, constructing, types, barrier);
    }

    RootedFunction target(cx, NULL);
    if (numTargets == 1)
        target = targets[0]->toFunction();

    return makeCallBarrier(target, argc, constructing, types, barrier);
}

MCall *
IonBuilder::makeCallHelper(HandleFunction target, uint32_t argc, bool constructing)
{
    // This function may be called with mutated stack.
    // Querying TI for popped types is invalid.

    uint32_t targetArgs = argc;

    // Collect number of missing arguments provided that the target is
    // scripted. Native functions are passed an explicit 'argc' parameter.
    if (target && !target->isNative())
        targetArgs = Max<uint32_t>(target->nargs, argc);

    MCall *call = MCall::New(target, targetArgs + 1, argc, constructing);
    if (!call)
        return NULL;

    // Explicitly pad any missing arguments with |undefined|.
    // This permits skipping the argumentsRectifier.
    for (int i = targetArgs; i > (int)argc; i--) {
        JS_ASSERT_IF(target, !target->isNative());
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        MPassArg *pass = MPassArg::New(undef);
        current->add(pass);
        call->addArg(i, pass);
    }

    // Add explicit arguments.
    // Bytecode order: Function, This, Arg0, Arg1, ..., ArgN, Call.
    for (int32_t i = argc; i > 0; i--)
        call->addArg(i, current->pop()->toPassArg());

    // Place an MPrepareCall before the first passed argument, before we
    // potentially perform rearrangement.
    MPrepareCall *start = new MPrepareCall;
    MPassArg *firstArg = current->peek(-1)->toPassArg();
    firstArg->block()->insertBefore(firstArg, start);
    call->initPrepareCall(start);

    MPassArg *thisArg = current->pop()->toPassArg();

    // Inline the constructor on the caller-side.
    if (constructing) {
        MDefinition *callee = current->peek(-1);
        MDefinition *create = createThis(target, callee);
        if (!create) {
            abort("Failure inlining constructor for call.");
            return NULL;
        }

        MPassArg *newThis = MPassArg::New(create);

        thisArg->block()->discard(thisArg);
        current->add(newThis);
        thisArg = newThis;
    }

    // Pass |this| and function.
    call->addArg(0, thisArg);

    MDefinition *fun = current->pop();
    if (fun->isDOMFunction())
        call->setDOMFunction();
    call->initFunction(fun);

    current->add(call);
    return call;
}

bool
IonBuilder::makeCallBarrier(HandleFunction target, uint32_t argc,
                            bool constructing,
                            types::StackTypeSet *types,
                            types::StackTypeSet *barrier)
{
    MCall *call = makeCallHelper(target, argc, constructing);
    if (!call)
        return false;

    current->push(call);
    if (!resumeAfter(call))
        return false;

    return pushTypeBarrier(call, types, barrier);
}

bool
IonBuilder::makeCall(HandleFunction target, uint32_t argc, bool constructing)
{
    types::StackTypeSet *barrier;
    types::StackTypeSet *types = oracle->returnTypeSet(script(), pc, &barrier);
    return makeCallBarrier(target, argc, constructing, types, barrier);
}

bool
IonBuilder::jsop_compare(JSOp op)
{
    MDefinition *right = current->pop();
    MDefinition *left = current->pop();

    MCompare *ins = MCompare::New(left, right, op);
    current->add(ins);
    current->push(ins);

    TypeOracle::BinaryTypes b = oracle->binaryTypes(script(), pc);
    ins->infer(b, cx);

    if (ins->isEffectful() && !resumeAfter(ins))
        return false;
    return true;
}

JSObject *
IonBuilder::getNewArrayTemplateObject(uint32_t count)
{
    RootedObject templateObject(cx, NewDenseUnallocatedArray(cx, count));
    if (!templateObject)
        return NULL;

    RootedScript scriptRoot(cx, script());
    if (types::UseNewTypeForInitializer(cx, scriptRoot, pc, JSProto_Array)) {
        if (!JSObject::setSingletonType(cx, templateObject))
            return NULL;
    } else {
        types::TypeObject *type = types::TypeScript::InitObject(cx, scriptRoot, pc, JSProto_Array);
        if (!type)
            return NULL;
        templateObject->setType(type);
    }

    return templateObject;
}

bool
IonBuilder::jsop_newarray(uint32_t count)
{
    JS_ASSERT(script()->compileAndGo);

    JSObject *templateObject = getNewArrayTemplateObject(count);
    if (!templateObject)
        return false;

    MNewArray *ins = new MNewArray(count, templateObject, MNewArray::NewArray_Allocating);

    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_newobject(HandleObject baseObj)
{
    // Don't bake in the TypeObject for non-CNG scripts.
    JS_ASSERT(script()->compileAndGo);

    RootedObject templateObject(cx);

    if (baseObj) {
        templateObject = CopyInitializerObject(cx, baseObj);
    } else {
        gc::AllocKind kind = GuessObjectGCKind(0);
        templateObject = NewBuiltinClassInstance(cx, &ObjectClass, kind);
    }

    if (!templateObject)
        return false;

    RootedScript scriptRoot(cx, script());
    if (types::UseNewTypeForInitializer(cx, scriptRoot, pc, JSProto_Object)) {
        if (!JSObject::setSingletonType(cx, templateObject))
            return false;
    } else {
        types::TypeObject *type = types::TypeScript::InitObject(cx, scriptRoot, pc, JSProto_Object);
        if (!type)
            return false;
        templateObject->setType(type);
    }

    MNewObject *ins = MNewObject::New(templateObject);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_initelem_array()
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->peek(-1);

    MConstant *id = MConstant::New(Int32Value(GET_UINT24(pc)));
    current->add(id);

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    // Store the value.
    MStoreElement *store = MStoreElement::New(elements, id, value);
    current->add(store);

    // Update the length.
    MSetInitializedLength *initLength = MSetInitializedLength::New(elements, id);
    current->add(initLength);

    if (!resumeAfter(initLength))
        return false;

   return true;
}

static bool
CanEffectlesslyCallLookupGenericOnObject(JSObject *obj)
{
    while (obj) {
        if (!obj->isNative())
            return false;
        if (obj->getClass()->ops.lookupProperty)
            return false;
        obj = obj->getProto();
    }
    return true;
}

bool
IonBuilder::jsop_initprop(HandlePropertyName name)
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->peek(-1);

    RootedObject templateObject(cx, obj->toNewObject()->templateObject());

    if (!oracle->propertyWriteCanSpecialize(script(), pc)) {
        // This should only happen for a few names like __proto__.
        return abort("INITPROP Monitored initprop");
    }

    if (!CanEffectlesslyCallLookupGenericOnObject(templateObject))
        return abort("INITPROP template object is special");

    RootedObject holder(cx);
    RootedShape shape(cx);
    RootedId id(cx, NameToId(name));
    bool res = LookupPropertyWithFlags(cx, templateObject, id,
                                       JSRESOLVE_QUALIFIED, &holder, &shape);
    if (!res)
        return false;

    if (!shape || holder != templateObject) {
        // JSOP_NEWINIT becomes an MNewObject without preconfigured properties.
        MInitProp *init = MInitProp::New(obj, name, value);
        current->add(init);
        return resumeAfter(init);
    }

    bool needsBarrier = true;
    TypeOracle::BinaryTypes b = oracle->binaryTypes(script(), pc);
    if (b.lhsTypes &&
        ((jsid)id == types::MakeTypeId(cx, id)) &&
        !b.lhsTypes->propertyNeedsBarrier(cx, id))
    {
        needsBarrier = false;
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

MBasicBlock *
IonBuilder::addBlock(MBasicBlock *block, uint32_t loopDepth)
{
    if (!block)
        return NULL;
    graph().addBlock(block);
    block->setLoopDepth(loopDepth);
    return block;
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(graph(), info(), predecessor, pc, MBasicBlock::NORMAL);
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
    MBasicBlock *block = MBasicBlock::New(graph(), info(), predecessor, pc, MBasicBlock::NORMAL);
    if (!block)
        return NULL;
    graph().insertBlockAfter(at, block);
    return block;
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc, uint32_t loopDepth)
{
    MBasicBlock *block = MBasicBlock::New(graph(), info(), predecessor, pc, MBasicBlock::NORMAL);
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
        return NULL;

    MOsrEntry *entry = MOsrEntry::New();
    osrBlock->add(entry);

    // Initialize |scopeChain|.
    {
        uint32_t slot = info().scopeChainSlot();

        MOsrScopeChain *scopev = MOsrScopeChain::New(entry);
        osrBlock->add(scopev);
        osrBlock->initSlot(slot, scopev);
    }

    if (info().fun()) {
        // Initialize |this| parameter.
        uint32_t slot = info().thisSlot();
        ptrdiff_t offset = StackFrame::offsetOfThis(info().fun());

        MOsrValue *thisv = MOsrValue::New(entry, offset);
        osrBlock->add(thisv);
        osrBlock->initSlot(slot, thisv);

        // Initialize arguments.
        for (uint32_t i = 0; i < info().nargs(); i++) {
            uint32_t slot = info().argSlot(i);
            ptrdiff_t offset = StackFrame::offsetOfFormalArg(info().fun(), i);

            MOsrValue *osrv = MOsrValue::New(entry, offset);
            osrBlock->add(osrv);
            osrBlock->initSlot(slot, osrv);
        }
    }

    // Initialize locals.
    for (uint32_t i = 0; i < info().nlocals(); i++) {
        uint32_t slot = info().localSlot(i);
        ptrdiff_t offset = StackFrame::offsetOfFixed(i);

        MOsrValue *osrv = MOsrValue::New(entry, offset);
        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
    }

    // Initialize stack.
    uint32_t numSlots = preheader->stackDepth() - CountArgSlots(info().fun()) - info().nlocals();
    for (uint32_t i = 0; i < numSlots; i++) {
        uint32_t slot = info().stackSlot(i);
        ptrdiff_t offset = StackFrame::offsetOfFixed(info().nlocals() + i);

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
        return NULL;

    // Link the same MResumePoint from the MStart to each MOsrValue.
    // This causes logic in ShouldSpecializeInput() to not replace Uses with
    // Unboxes in the MResumePiont, so that the MStart always sees Values.
    osrBlock->linkOsrValues(start);

    // Clone types of the other predecessor of the pre-header to the osr block,
    // such as pre-header phi's won't discard specialized type of the
    // predecessor.
    JS_ASSERT(predecessor->stackDepth() == osrBlock->stackDepth());
    JS_ASSERT(info().scopeChainSlot() == 0);
    JS_ASSERT(osrBlock->scopeChain()->type() == MIRType_Object);

    Vector<MIRType> slotTypes(cx);
    if (!slotTypes.growByUninitialized(osrBlock->stackDepth()))
        return NULL;

    // Fill slotTypes with the types of the predecessor block.
    for (uint32_t i = 0; i < osrBlock->stackDepth(); i++)
        slotTypes[i] = MIRType_Value;

    // Update slotTypes for slots that may have a different type at this join point.
    if (!oracle->getOsrTypes(loopEntry, slotTypes))
        return NULL;

    for (uint32_t i = 1; i < osrBlock->stackDepth(); i++) {
        // Unbox the MOsrValue if it is known to be unboxable.
        switch (slotTypes[i]) {
          case MIRType_Boolean:
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_String:
          case MIRType_Object:
          {
            MDefinition *def = osrBlock->getSlot(i);
            JS_ASSERT(def->type() == MIRType_Value);

            MInstruction *actual = MUnbox::New(def, slotTypes[i], MUnbox::Infallible);
            osrBlock->add(actual);
            osrBlock->rewriteSlot(i, actual);
            break;
          }

          case MIRType_Null:
          {
            MConstant *c = MConstant::New(NullValue());
            osrBlock->add(c);
            osrBlock->rewriteSlot(i, c);
            break;
          }

          case MIRType_Undefined:
          {
            MConstant *c = MConstant::New(UndefinedValue());
            osrBlock->add(c);
            osrBlock->rewriteSlot(i, c);
            break;
          }

          case MIRType_Magic:
            JS_ASSERT(lazyArguments_);
            osrBlock->rewriteSlot(i, lazyArguments_);
            break;

          default:
            break;
        }
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
IonBuilder::newPendingLoopHeader(MBasicBlock *predecessor, jsbytecode *pc)
{
    loopDepth_++;
    MBasicBlock *block = MBasicBlock::NewPendingLoopHeader(graph(), info(), predecessor, pc);
    return addBlock(block, loopDepth_);
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

void
IonBuilder::insertRecompileCheck()
{
    if (!inliningEnabled())
        return;

    if (inliningDepth > 0)
        return;

    // Don't recompile if we are already inlining.
    if (script()->getUseCount() >= js_IonOptions.usesBeforeInlining)
        return;

    // Don't recompile if the oracle cannot provide inlining information
    // or if the script has no calls.
    if (!oracle->canInlineCalls())
        return;

    uint32_t minUses = UsesBeforeIonRecompile(script(), pc);
    MRecompileCheck *check = MRecompileCheck::New(minUses);
    current->add(check);
}

static inline bool
TestSingletonProperty(JSContext *cx, HandleObject obj, HandleId id, bool *isKnownConstant)
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

    if (!CanEffectlesslyCallLookupGenericOnObject(obj))
        return true;

    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!JSObject::lookupGeneric(cx, obj, id, &holder, &shape))
        return false;
    if (!shape)
        return true;

    if (!shape->hasDefaultGetter())
        return true;
    if (!shape->hasSlot())
        return true;
    if (holder->getSlot(shape->slot()).isUndefined())
        return true;

    *isKnownConstant = true;
    return true;
}

static inline bool
TestSingletonPropertyTypes(JSContext *cx, types::StackTypeSet *types,
                           HandleObject globalObj, HandleId id,
                           bool *isKnownConstant, bool *testObject,
                           bool *testString)
{
    // As for TestSingletonProperty, but the input is any value in a type set
    // rather than a specific object. If testObject is set then the constant
    // result can only be used after ensuring the input is an object.

    *isKnownConstant = false;
    *testObject = false;
    *testString = false;

    if (!types || types->unknownObject())
        return true;

    RootedObject singleton(cx, types->getSingleton());
    if (singleton)
        return TestSingletonProperty(cx, singleton, id, isKnownConstant);

    if (!globalObj)
        return true;

    JSProtoKey key;
    JSValueType type = types->getKnownTypeTag();
    switch (type) {
      case JSVAL_TYPE_STRING:
        key = JSProto_String;
        break;

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        key = JSProto_Number;
        break;

      case JSVAL_TYPE_BOOLEAN:
        key = JSProto_Boolean;
        break;

      case JSVAL_TYPE_OBJECT:
      case JSVAL_TYPE_UNKNOWN: {
        if (types->hasType(types::Type::StringType())) {
            // Do not optimize if the object is either a String or an Object.
            if (types->maybeObject())
                return true;
            key = JSProto_String;
            *testString = true;
            break;
        }

        // For property accesses which may be on many objects, we just need to
        // find a prototype common to all the objects; if that prototype
        // has the singleton property, the access will not be on a missing property.
        bool thoughtConstant = true;
        for (unsigned i = 0; i < types->getObjectCount(); i++) {
            types::TypeObject *object = types->getTypeObject(i);
            if (!object) {
                // Try to get it through the singleton.
                JSObject *curObj = types->getSingleObject(i);
                // As per the comment in jsinfer.h, there can be holes in
                // TypeSets, so just skip over them.
                if (!curObj)
                    continue;
                object = curObj->getType(cx);
            }

            if (object->proto) {
                // Test this type.
                RootedObject proto(cx, object->proto);
                if (!TestSingletonProperty(cx, proto, id, &thoughtConstant))
                    return false;
                // Short circuit
                if (!thoughtConstant)
                    break;
            } else {
                // Can't be on the prototype chain with no prototypes...
                thoughtConstant = false;
                break;
            }
        }
        if (thoughtConstant) {
            // If this is not a known object, a test will be needed.
            *testObject = (type != JSVAL_TYPE_OBJECT);
        }
        *isKnownConstant = thoughtConstant;
        return true;
      }
      default:
        return true;
    }

    RootedObject proto(cx);
    if (!js_GetClassPrototype(cx, key, &proto, NULL))
        return false;

    return TestSingletonProperty(cx, proto, id, isKnownConstant);
}

// Given an actual and observed type set, annotates the IR as much as possible:
// (1) If no type information is provided, the value on the top of the stack is
//     left in place.
// (2) If a single type definitely exists, and no type barrier is in place,
//     then an infallible unbox instruction replaces the value on the top of
//     the stack.
// (3) If a type barrier is in place, but has an unknown type set, leave the
//     value at the top of the stack.
// (4) If a type barrier is in place, and has a single type, an unbox
//     instruction replaces the top of the stack.
// (5) Lastly, a type barrier instruction replaces the top of the stack.
bool
IonBuilder::pushTypeBarrier(MInstruction *ins, types::StackTypeSet *actual,
                            types::StackTypeSet *observed)
{
    AutoAssertNoGC nogc;

    // If the instruction has no side effects, we'll resume the entire operation.
    // The actual type barrier will occur in the interpreter. If the
    // instruction is effectful, even if it has a singleton type, there
    // must be a resume point capturing the original def, and resuming
    // to that point will explicitly monitor the new type.

    if (!actual) {
        JS_ASSERT(!observed);
        return true;
    }

    if (!observed) {
        JSValueType type = actual->getKnownTypeTag();
        MInstruction *replace = NULL;
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
        }
        return true;
    }

    if (observed->unknown())
        return true;

    current->pop();

    MInstruction *barrier;
    JSValueType type = observed->getKnownTypeTag();

    // An unbox instruction isn't enough to capture JSVAL_TYPE_OBJECT. Use a type
    // barrier followed by an infallible unbox.
    bool isObject = false;
    if (type == JSVAL_TYPE_OBJECT && !observed->hasType(types::Type::AnyObjectType())) {
        type = JSVAL_TYPE_UNKNOWN;
        isObject = true;
    }

    switch (type) {
      case JSVAL_TYPE_UNKNOWN:
      case JSVAL_TYPE_UNDEFINED:
      case JSVAL_TYPE_NULL:
        barrier = MTypeBarrier::New(ins, cloneTypeSet(observed));
        current->add(barrier);

        if (type == JSVAL_TYPE_UNDEFINED)
            return pushConstant(UndefinedValue());
        if (type == JSVAL_TYPE_NULL)
            return pushConstant(NullValue());
        if (isObject) {
            barrier = MUnbox::New(barrier, MIRType_Object, MUnbox::Infallible);
            current->add(barrier);
        }
        break;
      default:
        MUnbox::Mode mode = ins->isEffectful() ? MUnbox::TypeBarrier : MUnbox::TypeGuard;
        barrier = MUnbox::New(ins, MIRTypeFromValueType(type), mode);
        current->add(barrier);
    }
    current->push(barrier);
    return true;
}

// Test the type of values returned by a VM call. This is an optimized version
// of calling TypeScript::Monitor inside such stubs.
void
IonBuilder::monitorResult(MInstruction *ins, types::TypeSet *barrier, types::TypeSet *types)
{
    // MonitorTypes is redundant if we will also add a type barrier.
    if (barrier)
        return;

    if (!types || types->unknown())
        return;

    MInstruction *monitor = MMonitorTypes::New(ins, cloneTypeSet(types));
    current->add(monitor);
}

bool
IonBuilder::jsop_getgname(HandlePropertyName name)
{
    // Optimize undefined, NaN, and Infinity.
    if (name == cx->names().undefined)
        return pushConstant(UndefinedValue());
    if (name == cx->names().NaN)
        return pushConstant(cx->runtime->NaNValue);
    if (name == cx->names().Infinity)
        return pushConstant(cx->runtime->positiveInfinityValue);

    RootedObject globalObj(cx, &script()->global());
    JS_ASSERT(globalObj->isNative());

    RootedId id(cx, NameToId(name));

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    RootedShape shape(cx, globalObj->nativeLookup(cx, id));
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot())
        return jsop_getname(name);

    types::HeapTypeSet *propertyTypes = oracle->globalPropertyTypeSet(script(), pc, id);
    if (propertyTypes && propertyTypes->isOwnProperty(cx, globalObj->getType(cx), true)) {
        // The property has been reconfigured as non-configurable, non-enumerable
        // or non-writable.
        return jsop_getname(name);
    }

    // If the property is permanent, a shape guard isn't necessary.
    JSValueType knownType = JSVAL_TYPE_UNKNOWN;

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);
    if (types) {
        JSObject *singleton = types->getSingleton();

        knownType = types->getKnownTypeTag();
        if (!barrier) {
            if (singleton) {
                // Try to inline a known constant value.
                bool isKnownConstant;
                if (!TestSingletonProperty(cx, globalObj, id, &isKnownConstant))
                    return false;
                if (isKnownConstant)
                    return pushConstant(ObjectValue(*singleton));
            }
            if (knownType == JSVAL_TYPE_UNDEFINED)
                return pushConstant(UndefinedValue());
            if (knownType == JSVAL_TYPE_NULL)
                return pushConstant(NullValue());
        }
    }

    MInstruction *global = MConstant::New(ObjectValue(*globalObj));
    current->add(global);

    // If we have a property typeset, the isOwnProperty call will trigger recompilation if
    // the property is deleted or reconfigured.
    if (!propertyTypes && shape->configurable())
        global = addShapeGuard(global, globalObj->lastProperty(), Bailout_ShapeGuard);

    JS_ASSERT(shape->slot() >= globalObj->numFixedSlots());

    MSlots *slots = MSlots::New(global);
    current->add(slots);
    MLoadSlot *load = MLoadSlot::New(slots, shape->slot() - globalObj->numFixedSlots());
    current->add(load);

    // Slot loads can be typed, if they have a single, known, definitive type.
    if (knownType != JSVAL_TYPE_UNKNOWN && !barrier)
        load->setResultType(MIRTypeFromValueType(knownType));

    current->push(load);
    return pushTypeBarrier(load, types, barrier);
}

bool
IonBuilder::jsop_setgname(HandlePropertyName name)
{
    RootedObject globalObj(cx, &script()->global());
    RootedId id(cx, NameToId(name));

    JS_ASSERT(globalObj->isNative());

    bool canSpecialize;
    types::HeapTypeSet *propertyTypes = oracle->globalPropertyWrite(script(), pc, id, &canSpecialize);

    // This should only happen for a few names like __proto__.
    if (!canSpecialize || globalObj->watched())
        return jsop_setprop(name);

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    RootedShape shape(cx, globalObj->nativeLookup(cx, id));
    if (!shape || !shape->hasDefaultSetter() || !shape->writable() || !shape->hasSlot())
        return jsop_setprop(name);

    if (propertyTypes && propertyTypes->isOwnProperty(cx, globalObj->getType(cx), true)) {
        // The property has been reconfigured as non-configurable, non-enumerable
        // or non-writable.
        return jsop_setprop(name);
    }

    MInstruction *global = MConstant::New(ObjectValue(*globalObj));
    current->add(global);

    // If we have a property type set, the isOwnProperty call will trigger recompilation
    // if the property is deleted or reconfigured. Without TI, we always need a shape guard
    // to guard against the property being reconfigured as non-writable.
    if (!propertyTypes)
        global = addShapeGuard(global, globalObj->lastProperty(), Bailout_ShapeGuard);

    JS_ASSERT(shape->slot() >= globalObj->numFixedSlots());

    MSlots *slots = MSlots::New(global);
    current->add(slots);

    MDefinition *value = current->pop();
    MStoreSlot *store = MStoreSlot::New(slots, shape->slot() - globalObj->numFixedSlots(), value);
    current->add(store);

    // Determine whether write barrier is required.
    if (!propertyTypes || propertyTypes->needsBarrier(cx))
        store->setNeedsBarrier();

    // Pop the global object pushed by bindgname.
    DebugOnly<MDefinition *> pushedGlobal = current->pop();
    JS_ASSERT(&pushedGlobal->toConstant()->value().toObject() == globalObj);

    // If the property has a known type, we may be able to optimize typed stores by not
    // storing the type tag. This only works if the property does not have its initial
    // |undefined| value; if |undefined| is assigned at a later point, it will be added
    // to the type set.
    if (propertyTypes && !globalObj->getSlot(shape->slot()).isUndefined()) {
        JSValueType knownType = propertyTypes->getKnownTypeTag(cx);
        if (knownType != JSVAL_TYPE_UNKNOWN)
            store->setSlotType(MIRTypeFromValueType(knownType));
    }

    JS_ASSERT_IF(store->needsBarrier(), store->slotType() != MIRType_None);

    current->push(value);
    return resumeAfter(store);
}

bool
IonBuilder::jsop_getname(HandlePropertyName name)
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

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);

    monitorResult(ins, barrier, types);
    return pushTypeBarrier(ins, types, barrier);
}

bool
IonBuilder::jsop_intrinsic(HandlePropertyName name)
{
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);
    JSValueType type = types->getKnownTypeTag();

    // If we haven't executed this opcode yet, we need to get the intrinsic
    // value and monitor the result.
    if (type == JSVAL_TYPE_UNKNOWN) {
        MCallGetIntrinsicValue *ins = MCallGetIntrinsicValue::New(name);

        current->add(ins);
        current->push(ins);

        if (!resumeAfter(ins))
            return false;

        RootedScript scriptRoot(cx, script());
        types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
        monitorResult(ins, barrier, types);
        return pushTypeBarrier(ins, types, barrier);
    }

    // Bake in the intrinsic. Make sure that TI agrees with us on the type.
    RootedValue vp(cx, UndefinedValue());
    if (!cx->global()->getIntrinsicValue(cx, name, &vp))
        return false;

    JS_ASSERT(types->hasType(types::GetValueType(cx, vp)));

    MConstant *ins = MConstant::New(vp);
    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_bindname(PropertyName *name)
{
    JS_ASSERT(script()->analysis()->usesScopeChain());

    MDefinition *scopeChain = current->scopeChain();
    MBindNameCache *ins = MBindNameCache::New(scopeChain, name, script(), pc);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_getelem()
{
    if (oracle->elementReadIsDenseArray(script(), pc))
        return jsop_getelem_dense();

    int arrayType = TypedArray::TYPE_MAX;
    if (oracle->elementReadIsTypedArray(script(), pc, &arrayType))
        return jsop_getelem_typed(arrayType);

    if (oracle->elementReadIsString(script(), pc))
        return jsop_getelem_string();

    LazyArgumentsType isArguments = oracle->elementReadMagicArguments(script(), pc);
    if (isArguments == MaybeArguments)
        return abort("Type is not definitely lazy arguments.");
    if (isArguments == DefinitelyArguments)
        return jsop_arguments_getelem();

    MDefinition *rhs = current->pop();
    MDefinition *lhs = current->pop();

    MInstruction *ins;

    // TI does not account for GETELEM with string indexes, so we have to monitor
    // the result of MGetElementCache if it's expected to access string properties.
    // If the result of MGetElementCache is not monitored, we won't generate any
    // getprop stubs.
    bool mustMonitorResult = false;
    bool cacheable = false;

    oracle->elementReadGeneric(script(), pc, &cacheable, &mustMonitorResult);

    if (cacheable)
        ins = MGetElementCache::New(lhs, rhs, mustMonitorResult);
    else
        ins = MCallGetElement::New(lhs, rhs);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);

    if (mustMonitorResult)
        monitorResult(ins, barrier, types);
    return pushTypeBarrier(ins, types, barrier);
}

bool
IonBuilder::jsop_getelem_dense()
{
    if (oracle->arrayPrototypeHasIndexedProperty())
        return abort("GETELEM Array proto has indexed properties");

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);
    bool needsHoleCheck = !oracle->elementReadIsPacked(script(), pc);
    bool maybeUndefined = types->hasType(types::Type::UndefinedType());

    MDefinition *id = current->pop();
    MDefinition *obj = current->pop();

    JSValueType knownType = JSVAL_TYPE_UNKNOWN;
    if (!barrier) {
        knownType = types->getKnownTypeTag();

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
    }

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    MInitializedLength *initLength = MInitializedLength::New(elements);
    current->add(initLength);

    MInstruction *load;

    if (!maybeUndefined) {
        // This load should not return undefined, so likely we're reading
        // in-bounds elements, and the array is packed or its holes are not
        // read. This is the best case: we can separate the bounds check for
        // hoisting.
        id = addBoundsCheck(id, initLength);

        load = MLoadElement::New(elements, id, needsHoleCheck);
        current->add(load);
    } else {
        // This load may return undefined, so assume that we *can* read holes,
        // or that we can read out-of-bounds accesses. In this case, the bounds
        // check is part of the opcode.
        load = MLoadElementHole::New(elements, id, initLength, needsHoleCheck);
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

static MInstruction *
GetTypedArrayLength(MDefinition *obj)
{
    if (obj->isConstant()) {
        JSObject *array = &obj->toConstant()->value().toObject();
        int32_t length = (int32_t) TypedArray::length(array);
        obj->setFoldedUnchecked();
        return MConstant::New(Int32Value(length));
    }
    return MTypedArrayLength::New(obj);
}

static MInstruction *
GetTypedArrayElements(MDefinition *obj)
{
    if (obj->isConstant()) {
        JSObject *array = &obj->toConstant()->value().toObject();
        void *data = TypedArray::viewData(array);
        obj->setFoldedUnchecked();
        return MConstantElements::New(data);
    }
    return MTypedArrayElements::New(obj);
}

bool
IonBuilder::jsop_getelem_typed(int arrayType)
{
    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);

    MDefinition *id = current->pop();
    MDefinition *obj = current->pop();

    bool maybeUndefined = types->hasType(types::Type::UndefinedType());

    // Reading from an Uint32Array will result in a double for values
    // that don't fit in an int32. We have to bailout if this happens
    // and the instruction is not known to return a double.
    bool allowDouble = types->hasType(types::Type::DoubleType());

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    if (!maybeUndefined) {
        // Assume the index is in range, so that we can hoist the length,
        // elements vector and bounds check.

        // If we are reading in-bounds elements, we can use knowledge about
        // the array type to determine the result type. This may be more
        // precise than the known pushed type.
        MIRType knownType;
        switch (arrayType) {
          case TypedArray::TYPE_INT8:
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED:
          case TypedArray::TYPE_INT16:
          case TypedArray::TYPE_UINT16:
          case TypedArray::TYPE_INT32:
            knownType = MIRType_Int32;
            break;
          case TypedArray::TYPE_UINT32:
            knownType = allowDouble ? MIRType_Double : MIRType_Int32;
            break;
          case TypedArray::TYPE_FLOAT32:
          case TypedArray::TYPE_FLOAT64:
            knownType = MIRType_Double;
            break;
          default:
            JS_NOT_REACHED("Unknown typed array type");
            return false;
        }

        // Get the length.
        MInstruction *length = GetTypedArrayLength(obj);
        current->add(length);

        // Bounds check.
        id = addBoundsCheck(id, length);

        // Get the elements vector.
        MInstruction *elements = GetTypedArrayElements(obj);
        current->add(elements);

        // Load the element.
        MLoadTypedArrayElement *load = MLoadTypedArrayElement::New(elements, id, arrayType);
        current->add(load);
        current->push(load);

        load->setResultType(knownType);

        // Note: we can ignore the type barrier here, we know the type must
        // be valid and unbarriered.
        JS_ASSERT_IF(knownType == MIRType_Int32, types->hasType(types::Type::Int32Type()));
        JS_ASSERT_IF(knownType == MIRType_Double, types->hasType(types::Type::DoubleType()));
        return true;
    } else {
        // Assume we will read out-of-bound values. In this case the
        // bounds check will be part of the instruction, and the instruction
        // will always return a Value.
        MLoadTypedArrayElementHole *load =
            MLoadTypedArrayElementHole::New(obj, id, arrayType, allowDouble);
        current->add(load);
        current->push(load);

        return resumeAfter(load) && pushTypeBarrier(load, types, barrier);
    }
}

bool
IonBuilder::jsop_getelem_string()
{
    MDefinition *id = current->pop();
    MDefinition *str = current->pop();

    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    MStringLength *length = MStringLength::New(str);
    current->add(length);

    // This will cause an invalidation of this script once the 'undefined' type
    // is monitored by the interpreter.
    JS_ASSERT(oracle->propertyRead(script(), pc)->getKnownTypeTag() == JSVAL_TYPE_STRING);
    id = addBoundsCheck(id, length);

    MCharCodeAt *charCode = MCharCodeAt::New(str, id);
    current->add(charCode);

    MFromCharCode *result = MFromCharCode::New(charCode);
    current->add(result);
    current->push(result);
    return true;
}

bool
IonBuilder::jsop_setelem()
{
    if (oracle->propertyWriteCanSpecialize(script(), pc)) {
        RootedScript scriptRoot(cx, script());
        if (oracle->elementWriteIsDenseArray(scriptRoot, pc))
            return jsop_setelem_dense();

        int arrayType = TypedArray::TYPE_MAX;
        if (oracle->elementWriteIsTypedArray(script(), pc, &arrayType))
            return jsop_setelem_typed(arrayType);
    }

    LazyArgumentsType isArguments = oracle->elementWriteMagicArguments(script(), pc);
    if (isArguments == MaybeArguments)
        return abort("Type is not definitely lazy arguments.");
    if (isArguments == DefinitelyArguments)
        return jsop_arguments_setelem();

    MDefinition *value = current->pop();
    MDefinition *index = current->pop();
    MDefinition *object = current->pop();

    MInstruction *ins = MCallSetElement::New(object, index, value);
    current->add(ins);
    current->push(value);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_setelem_dense()
{
    if (oracle->arrayPrototypeHasIndexedProperty())
        return abort("SETELEM Array proto has indexed properties");

    MIRType elementType = oracle->elementWrite(script(), pc);
    bool packed = oracle->elementWriteIsPacked(script(), pc);

    MDefinition *value = current->pop();
    MDefinition *id = current->pop();
    MDefinition *obj = current->pop();

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    // Use MStoreElementHole if this SETELEM has written to out-of-bounds
    // indexes in the past. Otherwise, use MStoreElement so that we can hoist
    // the initialized length and bounds check.
    MStoreElementCommon *store;
    if (oracle->setElementHasWrittenHoles(script(), pc)) {
        MStoreElementHole *ins = MStoreElementHole::New(obj, elements, id, value);
        store = ins;

        current->add(ins);
        current->push(value);

        if (!resumeAfter(ins))
            return false;
    } else {
        MInitializedLength *initLength = MInitializedLength::New(elements);
        current->add(initLength);

        id = addBoundsCheck(id, initLength);

        MStoreElement *ins = MStoreElement::New(elements, id, value);
        store = ins;

        current->add(ins);
        current->push(value);

        if (!resumeAfter(ins))
            return false;
    }

    // Determine whether a write barrier is required.
    if (oracle->elementWriteNeedsBarrier(script(), pc))
        store->setNeedsBarrier();

    if (elementType != MIRType_None && packed)
        store->setElementType(elementType);

    return true;
}

bool
IonBuilder::jsop_setelem_typed(int arrayType)
{
    MDefinition *value = current->pop();
    MDefinition *id = current->pop();
    MDefinition *obj = current->pop();

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the length.
    MInstruction *length = GetTypedArrayLength(obj);
    current->add(length);

    // Bounds check.
    id = addBoundsCheck(id, length);

    // Get the elements vector.
    MInstruction *elements = GetTypedArrayElements(obj);
    current->add(elements);

    // Clamp value to [0, 255] for Uint8ClampedArray.
    MDefinition *unclampedValue = value;
    if (arrayType == TypedArray::TYPE_UINT8_CLAMPED) {
        value = MClampToUint8::New(value);
        current->add(value->toInstruction());
    }

    // Store the value.
    MStoreTypedArrayElement *store = MStoreTypedArrayElement::New(elements, id, value, arrayType);
    current->add(store);

    current->push(unclampedValue);
    return resumeAfter(store);
}

bool
IonBuilder::jsop_length()
{
    if (jsop_length_fastPath())
        return true;

    RootedPropertyName name(cx, info().getAtom(pc)->asPropertyName());
    return jsop_getprop(name);
}

bool
IonBuilder::jsop_length_fastPath()
{
    TypeOracle::UnaryTypes sig = oracle->unaryTypes(script(), pc);
    if (!sig.inTypes || !sig.outTypes)
        return false;

    if (sig.outTypes->getKnownTypeTag() != JSVAL_TYPE_INT32)
        return false;

    switch (sig.inTypes->getKnownTypeTag()) {
      case JSVAL_TYPE_STRING: {
        MDefinition *obj = current->pop();
        MStringLength *ins = MStringLength::New(obj);
        current->add(ins);
        current->push(ins);
        return true;
      }

      case JSVAL_TYPE_OBJECT: {
        if (!sig.inTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY)) {
            MDefinition *obj = current->pop();
            MElements *elements = MElements::New(obj);
            current->add(elements);

            // Read length.
            MArrayLength *length = new MArrayLength(elements);
            current->add(length);
            current->push(length);
            return true;
        }

        if (!sig.inTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY)) {
            MDefinition *obj = current->pop();
            MInstruction *length = GetTypedArrayLength(obj);
            current->add(length);
            current->push(length);
            return true;
        }

        return false;
      }

      default:
        break;
    }

    return false;
}

bool
IonBuilder::jsop_arguments()
{
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

    MInstruction *ins = MArgumentsLength::New();
    current->add(ins);
    current->push(ins);
    return true;
}

bool
IonBuilder::jsop_arguments_getelem()
{
    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);

    MDefinition *idx = current->pop();

    // Type Inference has guaranteed this is an optimized arguments object.
    MDefinition *args = current->pop();
    args->setFoldedUnchecked();

    // To ensure that we are not looking above the number of actual arguments.
    MArgumentsLength *length = MArgumentsLength::New();
    current->add(length);

    // Ensure idx is an integer.
    MInstruction *index = MToInt32::New(idx);
    current->add(index);

    // Bailouts if we read more than the number of actual arguments.
    index = addBoundsCheck(index, length);

    // Load the argument from the actual arguments.
    MGetArgument *load = MGetArgument::New(index);
    current->add(load);
    current->push(load);

    return pushTypeBarrier(load, types, barrier);
}

bool
IonBuilder::jsop_arguments_setelem()
{
    return abort("NYI arguments[]=");
}

inline types::HeapTypeSet *
GetDefiniteSlot(JSContext *cx, types::StackTypeSet *types, JSAtom *atom)
{
    if (!types || types->unknownObject() || types->getObjectCount() != 1)
        return NULL;

    types::TypeObject *type = types->getTypeObject(0);
    if (!type || type->unknownProperties())
        return NULL;

    jsid id = AtomToId(atom);
    if (id != types::MakeTypeId(cx, id))
        return NULL;

    types::HeapTypeSet *propertyTypes = type->getProperty(cx, id, false);
    if (!propertyTypes ||
        !propertyTypes->definiteProperty() ||
        propertyTypes->isOwnProperty(cx, type, true))
    {
        return NULL;
    }

    return propertyTypes;
}

bool
IonBuilder::jsop_not()
{
    MDefinition *value = current->pop();

    MNot *ins = new MNot(value);
    current->add(ins);
    current->push(ins);
    TypeOracle::UnaryTypes types = oracle->unaryTypes(script(), pc);
    ins->infer(types, cx);
    return true;
}


inline bool
IonBuilder::TestCommonPropFunc(JSContext *cx, types::StackTypeSet *types, HandleId id,
                               JSFunction **funcp, bool isGetter, bool *isDOM,
                               MDefinition **guardOut)
{
    JSObject *found = NULL;
    JSObject *foundProto = NULL;

    *funcp = NULL;
    *isDOM = false;

    bool thinkDOM = true;

    // No sense looking if we don't know what's going on.
    if (!types || types->unknownObject())
        return true;

    // Iterate down all the types to see if they all have the same getter or
    // setter.
    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        RootedObject curObj(cx, types->getSingleObject(i));

        // Non-Singleton type
        if (!curObj) {
            types::TypeObject *typeObj = types->getTypeObject(i);

            if (!typeObj)
                continue;

            if (typeObj->unknownProperties())
                return true;

            // If the type has an own property, we can't be sure we don't shadow
            // the chain.
            jsid typeId = types::MakeTypeId(cx, id);
            types::HeapTypeSet *propSet = typeObj->getProperty(cx, typeId, false);
            if (!propSet)
                return false;
            if (propSet->ownProperty(false))
                return true;

            // Check the DOM status of the instance type
            thinkDOM = thinkDOM && !typeObj->hasAnyFlags(types::OBJECT_FLAG_NON_DOM);

            // Otherwise try using the prototype.
            curObj = typeObj->proto;
        } else {
            // We can't optimize setters on watched singleton objects. A getter
            // on an own property can be protected with the prototype
            // shapeguard, though.
            if (!isGetter && curObj->watched())
                return true;

            // Check the DOM-ness of the singleton.
            types::TypeObject *objType = curObj->getType(cx);
            thinkDOM = thinkDOM && !objType->hasAnyFlags(types::OBJECT_FLAG_NON_DOM);
        }

        // Turns out that we need to check for a property lookup op, else we
        // will end up calling it mid-compilation.
        if (!CanEffectlesslyCallLookupGenericOnObject(curObj))
            return true;

        RootedObject proto(cx);
        RootedShape shape(cx);
        if (!JSObject::lookupGeneric(cx, curObj, id, &proto, &shape))
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
            if (!curFound->isFunction())
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

        // Check here to make sure that everyone has Type Objects with known
        // properties between them and the proto we found the accessor on. We
        // need those to add freezes safely. NOTE: We do not do this above, as
        // we may be able to freeze all the types up to where we found the
        // property, even if there are unknown types higher in the prototype
        // chain.
        while (curObj != foundProto) {
            types::TypeObject *typeObj = curObj->getType(cx);

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
            jsid typeId = types::MakeTypeId(cx, id);
            types::HeapTypeSet *propSet = typeObj->getProperty(cx, typeId, false);
            if (!propSet)
                return false;
            if (propSet->ownProperty(false))
                return true;

            curObj = curObj->getProto();
        }
    }

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
    if (isGetter) {
        JS_ASSERT(wrapper->isGuardShape());
        *guardOut = wrapper;
    }

    // Now we have to freeze all the property typesets to ensure there isn't a
    // lower shadowing getter or setter installed in the future.
    types::TypeObject *curType;
    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        curType = types->getTypeObject(i);
        JSObject *obj = NULL;
        if (!curType) {
            obj = types->getSingleObject(i);
            if (!obj)
                continue;

            curType = obj->getType(cx);
        }

        // Freeze the types as being DOM objects if they are
        if (thinkDOM) {
            // Asking the question adds the freeze
            DebugOnly<bool> wasntDOM =
                types::HeapTypeSet::HasObjectFlags(cx, curType, types::OBJECT_FLAG_NON_DOM);
            JS_ASSERT(!wasntDOM);
        }

        // If we found a Singleton object's own-property, there's nothing to
        // freeze.
        if (obj != foundProto) {
            // Walk the prototype chain. Everyone has to have the property, since we
            // just checked, so propSet cannot be NULL.
            jsid typeId = types::MakeTypeId(cx, id);
            while (true) {
                types::HeapTypeSet *propSet = curType->getProperty(cx, typeId, false);
                // This assert is now assured, since we have faulted them in
                // above.
                JS_ASSERT(propSet);
                // Asking, freeze by asking.
                DebugOnly<bool> isOwn = propSet->isOwnProperty(cx, curType, false);
                JS_ASSERT(!isOwn);
                // Don't mark the proto. It will be held down by the shape
                // guard. This allows us tp use properties found on prototypes
                // with properties unknown to TI.
                if (curType->proto == foundProto)
                    break;
                curType = curType->proto->getType(cx);
            }
        }
    }

    *funcp = found->toFunction();
    *isDOM = thinkDOM;

    return true;
}

static bool
TestShouldDOMCall(JSContext *cx, types::TypeSet *inTypes, HandleFunction func,
                  JSJitInfo::OpType opType)
{
    if (!func->isNative() || !func->jitInfo())
        return false;
    // If all the DOM objects flowing through are legal with this
    // property, we can bake in a call to the bottom half of the DOM
    // accessor
    DOMInstanceClassMatchesProto instanceChecker =
        GetDOMCallbacks(cx->runtime)->instanceClassMatchesProto;

    const JSJitInfo *jinfo = func->jitInfo();
    if (jinfo->type != opType)
        return false;

    for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
        types::TypeObject *curType = inTypes->getTypeObject(i);

        if (!curType) {
            JSObject *curObj = inTypes->getSingleObject(i);

            if (!curObj)
                continue;

            curType = curObj->getType(cx);
        }

        JSObject *typeProto = curType->proto;
        RootedObject proto(cx, typeProto);
        if (!instanceChecker(proto, jinfo->protoID, jinfo->depth))
            return false;
    }

    return true;
}

static bool
TestAreKnownDOMTypes(JSContext *cx, types::TypeSet *inTypes)
{
    if (inTypes->unknown())
        return false;

    // First iterate to make sure they all are DOM objects, then freeze all of
    // them as such if they are.
    for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
        types::TypeObject *curType = inTypes->getTypeObject(i);

        if (!curType) {
            JSObject *curObj = inTypes->getSingleObject(i);

            // Skip holes in TypeSets.
            if (!curObj)
                continue;

            curType = curObj->getType(cx);
        }

        if (curType->unknownProperties())
            return false;

        // Unlike TypeSet::HasObjectFlags, TypeObject::hasAnyFlags doesn't add a
        // freeze.
        if (curType->hasAnyFlags(types::OBJECT_FLAG_NON_DOM))
            return false;
    }

    // If we didn't check anything, no reason to say yes.
    if (inTypes->getObjectCount() > 0)
        return true;

    return false;
}

static void
FreezeDOMTypes(JSContext *cx, types::StackTypeSet *inTypes)
{
    for (unsigned i = 0; i < inTypes->getObjectCount(); i++) {
        types::TypeObject *curType = inTypes->getTypeObject(i);

        if (!curType) {
            JSObject *curObj = inTypes->getSingleObject(i);

            // Skip holes in TypeSets.
            if (!curObj)
                continue;

            curType = curObj->getType(cx);
        }

        // Add freeze by asking the question.
        DebugOnly<bool> wasntDOM =
            types::HeapTypeSet::HasObjectFlags(cx, curType, types::OBJECT_FLAG_NON_DOM);
        JS_ASSERT(!wasntDOM);
    }
}

bool
IonBuilder::annotateGetPropertyCache(JSContext *cx, MDefinition *obj, MGetPropertyCache *getPropCache,
                                    types::StackTypeSet *objTypes, types::StackTypeSet *pushedTypes)
{
    RootedId id(cx, NameToId(getPropCache->name()));
    if ((jsid)id != types::MakeTypeId(cx, id))
        return true;

    // Ensure every pushed value is a singleton.
    if (pushedTypes->unknownObject() || pushedTypes->baseFlags() != 0)
        return true;

    for (unsigned i = 0; i < pushedTypes->getObjectCount(); i++) {
        if (pushedTypes->getTypeObject(i) != NULL)
            return true;
    }

    // Object's typeset should be a proper object
    if (objTypes->baseFlags() || objTypes->unknownObject())
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
        types::TypeObject *typeObj = objTypes->getTypeObject(i);
        if (!typeObj || typeObj->unknownProperties() || !typeObj->proto)
            continue;

        types::HeapTypeSet *ownTypes = typeObj->getProperty(cx, id, false);
        if (!ownTypes)
            continue;

        if (ownTypes->isOwnProperty(cx, typeObj, false))
            continue;

        bool knownConstant = false;
        Rooted<JSObject*> proto(cx, typeObj->proto);
        if (!TestSingletonProperty(cx, proto, id, &knownConstant))
            return false;

        if (!knownConstant || proto->getType(cx)->unknownProperties())
            continue;

        types::HeapTypeSet *protoTypes = proto->getType(cx)->getProperty(cx, id, false);
        if (!protoTypes)
            continue;

        JSObject *obj = protoTypes->getSingleton(cx);
        if (!obj || !obj->isFunction())
            continue;

        // Don't add cases corresponding to non-observed pushes
        if (!pushedTypes->hasType(types::Type::ObjectType(obj)))
            continue;

        if (!inlinePropTable->addEntry(typeObj, obj->toFunction()))
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
IonBuilder::loadSlot(MDefinition *obj, HandleShape shape, MIRType rvalType)
{
    JS_ASSERT(shape->hasDefaultGetter());
    JS_ASSERT(shape->hasSlot());

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);

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
IonBuilder::storeSlot(MDefinition *obj, UnrootedShape shape, MDefinition *value, bool needsBarrier)
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
    return resumeAfter(store);
}

bool
IonBuilder::jsop_getprop(HandlePropertyName name)
{
    RootedId id(cx, NameToId(name));

    RootedScript scriptRoot(cx, script());
    types::StackTypeSet *barrier = oracle->propertyReadBarrier(scriptRoot, pc);
    types::StackTypeSet *types = oracle->propertyRead(script(), pc);
    TypeOracle::Unary unary = oracle->unaryOp(script(), pc);
    TypeOracle::UnaryTypes uTypes = oracle->unaryTypes(script(), pc);

    bool emitted = false;

    // Try to optimize arguments.length.
    if (!getPropTryArgumentsLength(&emitted) || emitted)
        return emitted;

    // Try to hardcode known constants.
    if (!getPropTryConstant(&emitted, id, barrier, types, uTypes) || emitted)
        return emitted;

    // Try to emit loads from definite slots.
    if (!getPropTryDefiniteSlot(&emitted, name, barrier, types, unary, uTypes) || emitted)
        return emitted;

    // Try to inline a common property getter, or make a call.
    if (!getPropTryCommonGetter(&emitted, id, barrier, types, uTypes) || emitted)
        return emitted;

    // Try to emit a monomorphic cache based on data in JM caches.
    if (!getPropTryMonomorphic(&emitted, id, barrier, unary, uTypes) || emitted)
        return emitted;

    // Try to emit a polymorphic cache.
    if (!getPropTryPolymorphic(&emitted, name, id, barrier, types, unary, uTypes) || emitted)
        return emitted;

    // Emit a call.
    MDefinition *obj = current->pop();
    MCallGetProperty *call = MCallGetProperty::New(obj, name);
    current->add(call);
    current->push(call);
    if (!resumeAfter(call))
        return false;

    monitorResult(call, barrier, types);
    return pushTypeBarrier(call, types, barrier);
}

bool
IonBuilder::getPropTryArgumentsLength(bool *emitted)
{
    JS_ASSERT(*emitted == false);
    LazyArgumentsType isArguments = oracle->propertyReadMagicArguments(script(), pc);

    if (isArguments == MaybeArguments)
        return abort("Type is not definitely lazy arguments.");
    if (isArguments != DefinitelyArguments)
        return true;
    if (JSOp(*pc) != JSOP_LENGTH)
        return true;

    *emitted = true;
    return jsop_arguments_length();
}

bool
IonBuilder::getPropTryConstant(bool *emitted, HandleId id, types::StackTypeSet *barrier,
                               types::StackTypeSet *types, TypeOracle::UnaryTypes unaryTypes)
{
    JS_ASSERT(*emitted == false);
    JSObject *singleton = types ? types->getSingleton() : NULL;
    if (!singleton || barrier)
        return true;

    RootedObject global(cx, &script()->global());

    bool isConstant, testObject, testString;
    if (!TestSingletonPropertyTypes(cx, unaryTypes.inTypes, global, id,
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
    if (singleton->isFunction()) {
        RootedFunction singletonFunc(cx, singleton->toFunction());
        if (TestAreKnownDOMTypes(cx, unaryTypes.inTypes) &&
            TestShouldDOMCall(cx, unaryTypes.inTypes, singletonFunc, JSJitInfo::Method))
        {
            FreezeDOMTypes(cx, unaryTypes.inTypes);
            known->setDOMFunction();
        }
    }

    current->add(known);
    current->push(known);

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryDefiniteSlot(bool *emitted, HandlePropertyName name,
                                   types::StackTypeSet *barrier, types::StackTypeSet *types,
                                   TypeOracle::Unary unary, TypeOracle::UnaryTypes unaryTypes)
{
    JS_ASSERT(*emitted == false);
    types::TypeSet *propTypes = GetDefiniteSlot(cx, unaryTypes.inTypes, name);
    if (!propTypes)
        return true;

    MDefinition *obj = current->pop();
    MDefinition *useObj = obj;
    if (unaryTypes.inTypes && unaryTypes.inTypes->baseFlags()) {
        MGuardObject *guard = MGuardObject::New(obj);
        current->add(guard);
        useObj = guard;
    }

    MLoadFixedSlot *fixed = MLoadFixedSlot::New(useObj, propTypes->definiteSlot());
    if (!barrier)
        fixed->setResultType(unary.rval);

    current->add(fixed);
    current->push(fixed);

    if (!pushTypeBarrier(fixed, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryCommonGetter(bool *emitted, HandleId id, types::StackTypeSet *barrier,
                                   types::StackTypeSet *types, TypeOracle::UnaryTypes unaryTypes)
{
    JS_ASSERT(*emitted == false);
    JSFunction *commonGetter;
    bool isDOM;
    MDefinition *guard;

    if (!TestCommonPropFunc(cx, unaryTypes.inTypes, id, &commonGetter, true,
                            &isDOM, &guard))
    {
        return false;
    }
    if (!commonGetter)
        return true;

    MDefinition *obj = current->pop();
    RootedFunction getter(cx, commonGetter);

    if (isDOM && TestShouldDOMCall(cx, unaryTypes.inTypes, getter, JSJitInfo::Getter)) {
        const JSJitInfo *jitinfo = getter->jitInfo();
        MGetDOMProperty *get = MGetDOMProperty::New(jitinfo, obj, guard);
        current->add(get);
        current->push(get);

        if (get->isEffectful() && !resumeAfter(get))
            return false;
        if (!pushTypeBarrier(get, types, barrier))
            return false;

        *emitted = true;
        return true;
    }

    // Spoof stack to expected state for call.
    pushConstant(ObjectValue(*commonGetter));

    MPassArg *wrapper = MPassArg::New(obj);
    current->add(wrapper);
    current->push(wrapper);

    if (!makeCallBarrier(getter, 0, false, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryMonomorphic(bool *emitted, HandleId id, types::StackTypeSet *barrier,
                                  TypeOracle::Unary unary, TypeOracle::UnaryTypes unaryTypes)
{
    AssertCanGC();
    JS_ASSERT(*emitted == false);
    bool accessGetter = oracle->propertyReadAccessGetter(script(), pc);

    if (unary.ival != MIRType_Object)
        return true;

    RootedShape objShape(cx, mjit::GetPICSingleShape(cx, script(), pc, info().constructing()));
    if (!objShape || objShape->inDictionary()) {
        spew("GETPROP not monomorphic");
        return true;
    }

    MDefinition *obj = current->pop();

    // The JM IC was monomorphic, so we inline the property access as long as
    // the shape is not in dictionary made. We cannot be sure that the shape is
    // still a lastProperty, and calling Shape::search() on dictionary mode
    // shapes that aren't lastProperty is invalid.
    obj = addShapeGuard(obj, objShape, Bailout_CachedShapeGuard);

    spew("Inlining monomorphic GETPROP");
    RootedShape shape(cx, objShape->search(cx, id));
    JS_ASSERT(shape);

    MIRType rvalType = unary.rval;
    if (barrier || IsNullOrUndefined(unary.rval) || accessGetter)
        rvalType = MIRType_Value;

    if (!loadSlot(obj, shape, rvalType))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::getPropTryPolymorphic(bool *emitted, HandlePropertyName name, HandleId id,
                                  types::StackTypeSet *barrier, types::StackTypeSet *types,
                                  TypeOracle::Unary unary, TypeOracle::UnaryTypes unaryTypes)
{
    JS_ASSERT(*emitted == false);
    bool accessGetter = oracle->propertyReadAccessGetter(script(), pc);

    // The input value must either be an object, or we should have strong suspicions
    // that it can be safely unboxed to an object.
    if (unary.ival != MIRType_Object && !unaryTypes.inTypes->objectOrSentinel())
        return true;

    MIRType rvalType = unary.rval;
    if (barrier || IsNullOrUndefined(unary.rval) || accessGetter)
        rvalType = MIRType_Value;

    MDefinition *obj = current->pop();
    MGetPropertyCache *load = MGetPropertyCache::New(obj, name);
    load->setResultType(rvalType);

    // Try to mark the cache as idempotent. We only do this if JM is enabled
    // (its ICs are used to mark property reads as likely non-idempotent) or
    // if we are compiling eagerly (to improve test coverage).
    if (unary.ival == MIRType_Object &&
        (cx->methodJitEnabled || js_IonOptions.eagerCompilation) &&
        !invalidatedIdempotentCache())
    {
        RootedScript scriptRoot(cx, script());
        if (oracle->propertyReadIdempotent(scriptRoot, pc, id))
            load->setIdempotent();
    }

    if (JSOp(*pc) == JSOP_CALLPROP) {
        if (!annotateGetPropertyCache(cx, obj, load, unaryTypes.inTypes, types))
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
        monitorResult(load, barrier, types);

    if (!pushTypeBarrier(load, types, barrier))
        return false;

    *emitted = true;
    return true;
}

bool
IonBuilder::jsop_setprop(HandlePropertyName name)
{
    MDefinition *value = current->pop();
    MDefinition *obj = current->pop();

    bool monitored = !oracle->propertyWriteCanSpecialize(script(), pc);

    TypeOracle::BinaryTypes binaryTypes = oracle->binaryTypes(script(), pc);

    if (!monitored) {
        if (types::HeapTypeSet *propTypes = GetDefiniteSlot(cx, binaryTypes.lhsTypes, name)) {
            MStoreFixedSlot *fixed = MStoreFixedSlot::New(obj, propTypes->definiteSlot(), value);
            current->add(fixed);
            current->push(value);
            if (propTypes->needsBarrier(cx))
                fixed->setNeedsBarrier();
            return resumeAfter(fixed);
        }
    }

    RootedId id(cx, NameToId(name));
    types::StackTypeSet *types = binaryTypes.lhsTypes;

    JSFunction *commonSetter;
    bool isDOM;
    if (!TestCommonPropFunc(cx, types, id, &commonSetter, false, &isDOM, NULL))
        return false;
    if (!monitored && commonSetter) {
        RootedFunction setter(cx, commonSetter);
        if (isDOM && TestShouldDOMCall(cx, types, setter, JSJitInfo::Setter)) {
            MSetDOMProperty *set = MSetDOMProperty::New(setter->jitInfo()->op, obj, value);
            if (!set)
                return false;

            current->add(set);
            current->push(value);

            return resumeAfter(set);
        }

        // Dummy up the stack, as in getprop
        pushConstant(ObjectValue(*setter));

        MPassArg *wrapper = MPassArg::New(obj);
        current->push(wrapper);
        current->add(wrapper);

        MPassArg *arg = MPassArg::New(value);
        current->push(arg);
        current->add(arg);

        // Call the setter. Note that we have to push the original value, not
        // the setter's return value.
        MCall *call = makeCallHelper(setter, 1, false);
        if (!call)
            return false;

        current->push(value);
        return resumeAfter(call);
    }

    oracle->binaryOp(script(), pc);

    MSetPropertyInstruction *ins;
    if (monitored) {
        ins = MCallSetProperty::New(obj, value, name, script()->strict);
    } else {
        UnrootedShape objShape;
        if ((objShape = mjit::GetPICSingleShape(cx, script(), pc, info().constructing())) &&
            !objShape->inDictionary())
        {
            // The JM IC was monomorphic, so we inline the property access as
            // long as the shape is not in dictionary mode. We cannot be sure
            // that the shape is still a lastProperty, and calling Shape::search
            // on dictionary mode shapes that aren't lastProperty is invalid.
            obj = addShapeGuard(obj, objShape, Bailout_CachedShapeGuard);

            UnrootedShape shape = DropUnrooted(objShape)->search(cx, NameToId(name));
            JS_ASSERT(shape);

            spew("Inlining monomorphic SETPROP");

            jsid typeId = types::MakeTypeId(cx, id);
            bool needsBarrier = oracle->propertyWriteNeedsBarrier(script(), pc, typeId);

            return storeSlot(obj, DropUnrooted(shape), value, needsBarrier);
        }

        spew("SETPROP not monomorphic");

        ins = MSetPropertyCache::New(obj, value, name, script()->strict);

        if (!binaryTypes.lhsTypes || binaryTypes.lhsTypes->propertyNeedsBarrier(cx, id))
            ins->setNeedsBarrier();
    }

    current->add(ins);
    current->push(value);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_delprop(HandlePropertyName name)
{
    MDefinition *obj = current->pop();

    MInstruction *ins = MDeleteProperty::New(obj, name);

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

    MRegExp *ins = MRegExp::New(reobj, prototype, MRegExp::MustClone);
    current->add(ins);
    current->push(ins);

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
    JS_ASSERT(script()->analysis()->usesScopeChain());
    MLambda *ins = MLambda::New(current->scopeChain(), fun);
    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
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
    JS_ASSERT(script()->analysis()->usesScopeChain());

    // Bake the name pointer into the MDefVar.
    MDefVar *defvar = MDefVar::New(name, attrs, current->scopeChain());
    current->add(defvar);

    return resumeAfter(defvar);
}

bool
IonBuilder::jsop_this()
{
    if (!info().fun())
        return abort("JSOP_THIS outside of a JSFunction.");

    if (script()->strict) {
        current->pushSlot(info().thisSlot());
        return true;
    }

    types::StackTypeSet *types = oracle->thisTypeSet(script());
    if (types && types->getKnownTypeTag() == JSVAL_TYPE_OBJECT) {
        // This is safe, because if the entry type of |this| is an object, it
        // will necessarily be an object throughout the entire function. OSR
        // can introduce a phi, but this phi will be specialized.
        current->pushSlot(info().thisSlot());
        return true;
    }

    return abort("JSOP_THIS hard case not yet handled");
}

bool
IonBuilder::jsop_typeof()
{
    TypeOracle::Unary unary = oracle->unaryOp(script(), pc);

    MDefinition *input = current->pop();
    MTypeOf *ins = MTypeOf::New(input, unary.ival);

    current->add(ins);
    current->push(ins);

    if (ins->isEffectful() && !resumeAfter(ins))
        return false;
    return true;
}

bool
IonBuilder::jsop_toid()
{
    // No-op if the index is an integer.
    TypeOracle::Unary unary = oracle->unaryOp(script(), pc);
    if (unary.ival == MIRType_Int32)
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

    return resumeAfter(ins);
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
IonBuilder::jsop_getaliasedvar(ScopeCoordinate sc)
{
    types::StackTypeSet *barrier;
    types::StackTypeSet *actual = oracle->aliasedVarBarrier(script(), pc, &barrier);

    MDefinition *obj = walkScopeChain(sc.hops);

    RootedShape shape(cx, ScopeCoordinateToStaticScope(script(), pc).scopeShape());

    MInstruction *load;
    if (shape->numFixedSlots() <= sc.slot) {
        MInstruction *slots = MSlots::New(obj);
        current->add(slots);

        load = MLoadSlot::New(slots, sc.slot - shape->numFixedSlots());
    } else {
        load = MLoadFixedSlot::New(obj, sc.slot);
    }

    if (!barrier) {
        JSValueType type = actual->getKnownTypeTag();
        if (type != JSVAL_TYPE_UNKNOWN &&
            type != JSVAL_TYPE_UNDEFINED &&
            type != JSVAL_TYPE_NULL)
        {
            load->setResultType(MIRTypeFromValueType(type));
        }
    }

    current->add(load);
    current->push(load);

    return pushTypeBarrier(load, actual, barrier);
}

bool
IonBuilder::jsop_setaliasedvar(ScopeCoordinate sc)
{
    MDefinition *rval = current->peek(-1);
    MDefinition *obj = walkScopeChain(sc.hops);

    RootedShape shape(cx, ScopeCoordinateToStaticScope(script(), pc).scopeShape());

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
    RootedScript scriptRoot(cx, script());
    if (oracle->inObjectIsDenseArray(scriptRoot, pc))
        return jsop_in_dense();

    MDefinition *obj = current->pop();
    MDefinition *id = current->pop();
    MIn *ins = new MIn(id, obj);

    current->add(ins);
    current->push(ins);

    return resumeAfter(ins);
}

bool
IonBuilder::jsop_in_dense()
{
    if (oracle->arrayPrototypeHasIndexedProperty())
        return abort("JSOP_IN Array proto has indexed properties");

    bool needsHoleCheck = !oracle->inArrayIsPacked(script(), pc);

    MDefinition *obj = current->pop();
    MDefinition *id = current->pop();

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
    MInArray *ins = MInArray::New(elements, id, initLength, needsHoleCheck);

    current->add(ins);
    current->push(ins);

    return true;
}

bool
IonBuilder::jsop_instanceof()
{
    MDefinition *rhs = current->pop();
    MDefinition *obj = current->pop();

    TypeOracle::BinaryTypes types = oracle->binaryTypes(script(), pc);

    // If this is an 'x instanceof function' operation and we can determine the
    // exact function and prototype object being tested for, use a typed path.
    do {
        RawObject rhsObject = types.rhsTypes ? types.rhsTypes->getSingleton() : NULL;
        if (!rhsObject || !rhsObject->isFunction() || rhsObject->isBoundFunction())
            break;

        types::TypeObject *rhsType = rhsObject->getType(cx);
        if (!rhsType || rhsType->unknownProperties())
            break;

        types::HeapTypeSet *protoTypes =
            rhsType->getProperty(cx, NameToId(cx->names().classPrototype), false);
        RawObject protoObject = protoTypes ? protoTypes->getSingleton(cx) : NULL;
        if (!protoObject)
            break;

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
IonBuilder::addShapeGuard(MDefinition *obj, const UnrootedShape shape, BailoutKind bailoutKind)
{
    MGuardShape *guard = MGuardShape::New(obj, shape, bailoutKind);
    current->add(guard);

    // If a shape guard failed in the past, don't optimize shape guard.
    if (failedShapeGuard_)
        guard->setNotMovable();

    return guard;
}

const types::TypeSet *
IonBuilder::cloneTypeSet(const types::TypeSet *types)
{
    if (!js_IonOptions.parallelCompilation)
        return types;

    // Clone a type set so that it can be stored into the MIR and accessed
    // during off thread compilation. This is necessary because main thread
    // updates to type sets can race with reads in the compiler backend, and
    // after bug 804676 this code can be removed.
    return types->clone(GetIonContext()->temp->lifoAlloc());
}
