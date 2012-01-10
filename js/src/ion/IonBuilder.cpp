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

#include "IonAnalysis.h"
#include "IonBuilder.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonAnalysis.h"
#include "IonSpewer.h"
#include "frontend/BytecodeEmitter.h"
#include "jsscriptinlines.h"

#ifdef JS_THREADSAFE
# include "prthread.h"
#endif

using namespace js;
using namespace js::ion;

IonBuilder::IonBuilder(JSContext *cx, TempAllocator &temp, MIRGraph &graph, TypeOracle *oracle,
                       CompileInfo &info, size_t inliningDepth)
  : MIRGenerator(cx, temp, graph, info),
    script(info.script()),
    lastResumePoint_(NULL),
    callerResumePoint_(NULL),
    oracle(oracle),
    inliningDepth(inliningDepth)
{
    pc = info.startPC();
}

static inline int32
GetJumpOffset(jsbytecode *pc)
{
    JSOp op = JSOp(*pc);
    JS_ASSERT(js_CodeSpec[op].type() == JOF_JUMP);
    return GET_JUMP_OFFSET(pc);
}

static inline jsbytecode *
GetNextPc(jsbytecode *pc)
{
    return pc + js_CodeSpec[JSOp(*pc)].length;
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

bool
IonBuilder::getInliningTarget(uint32 argc, jsbytecode *pc, JSFunction **out)
{
    *out = NULL;

    types::TypeSet *calleeTypes = oracle->getCallTarget(script, argc, pc);
    if (!calleeTypes) {
        IonSpew(IonSpew_Inlining, "Cannot inline; no types for callee");
        return true;
    }

    if (calleeTypes->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-object");
        return true;
    }

    if (calleeTypes->unknownObject()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to unknown object");
        return true;
    }

    if (calleeTypes->getObjectCount() > 1) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to multiple objects");
        return true;
    }

    JSObject *obj = calleeTypes->getSingleObject(0);
    if (!obj) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-single object");
        return true;
    }

    if (!obj->isFunction()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-function");
        return true;
    }

    JSFunction *fun = obj->toFunction();
    if (!fun->isInterpreted()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to non-interpreted");
        return true;
    }

    if (fun->getParent() != script->global()) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to scope mismatch");
        return true;
    }

    JSScript *inlineScript = fun->script();

    bool canInline = oracle->canEnterInlinedScript(inlineScript);

    if (!canInline) {
        IonSpew(IonSpew_Inlining, "Cannot inline due to oracle veto");
        return true;
    }

    IonSpew(IonSpew_Inlining, "Inlining good to go!");
    *out = fun;
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

    IonSpew(IonSpew_MIR, "Analyzing script %s:%d (%p)",
            script->filename, script->lineno, (void *) script);

    initParameters();

    // Initialize local variables.
    for (uint32 i = 0; i < info().nlocals(); i++) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        current->initSlot(info().localSlot(i), undef);
    }

    // Guard against over-recursion.
    MCheckOverRecursed *check = new MCheckOverRecursed;
    current->add(check);
    check->setResumePoint(current->entryResumePoint());

    // Recompile to inline calls if this function is hot.
    insertRecompileCheck();

    current->makeStart(MStart::New(MStart::StartType_Default));

    // Parameters have been checked to correspond to the typeset, now we unbox
    // what we can in an infallible manner.
    rewriteParameters();

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
    for (uint32 i = 0; i < CountArgSlots(info().fun()); i++) {
        MInstruction *ins = current->getEntrySlot(i)->toInstruction();
        if (ins->type() == MIRType_Value)
            ins->setResumePoint(current->entryResumePoint());
    }

    return traverseBytecode();
}

bool
IonBuilder::buildInline(MResumePoint *callerResumePoint, MDefinition *thisDefn,
                        MDefinitionVector &args)
{
    current = newBlock(pc);
    if (!current)
        return false;

    IonSpew(IonSpew_MIR, "Inlining script %s:%d (%p)",
            script->filename, script->lineno, (void *) script);

    callerResumePoint_ = callerResumePoint;
    MBasicBlock *predecessor = callerResumePoint->block();
    predecessor->end(MGoto::New(current));
    if (!current->addPredecessorWithoutPhis(predecessor))
        return false;
    JS_ASSERT(predecessor->numSuccessors() == 1);
    JS_ASSERT(current->numPredecessors() == 1);
    current->setCallerResumePoint(callerResumePoint);

    // Fill in any missing arguments with undefined.
    const size_t nargs = info().nargs();
    if (args.length() < nargs) {
        for (size_t i = 0, missing = nargs - args.length(); i < missing; ++i) {
            MConstant *undef = MConstant::New(UndefinedValue());
            current->add(undef);
            if (!args.append(undef))
                return false;
        }
    }

    // The oracle ensures that the inlined script does not use the scope chain, so we
    // just initialize its slot to |undefined|.
    JS_ASSERT(!script->analysis()->usesScopeChain());
    MInstruction *scope = MConstant::New(UndefinedValue());
    current->add(scope);
    current->initSlot(info().scopeChainSlot(), scope);

    current->initSlot(info().thisSlot(), thisDefn);

    IonSpew(IonSpew_Inlining, "Initializing %u arg slots", nargs);

    // Initialize argument references.
    JS_ASSERT(args.length() >= nargs);
    for (size_t i = 0; i < nargs; ++i) {
        MDefinition *arg = args[i];
        current->initSlot(info().argSlot(i), arg);
    }

    IonSpew(IonSpew_Inlining, "Initializing %u local slots", info().nlocals());

    // Initialize local variables.
    for (uint32 i = 0; i < info().nlocals(); i++) {
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        current->initSlot(info().localSlot(i), undef);
    }

    IonSpew(IonSpew_Inlining, "Inline entry block MResumePoint %p, %u operands",
            (void *) current->entryResumePoint(), current->entryResumePoint()->numOperands());

    // Note: +2 for the scope chain and |this|.
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
    static const uint32 START_SLOT = 1;

    for (uint32 i = START_SLOT; i < CountArgSlots(info().fun()); i++) {
        MParameter *param = current->getSlot(i)->toParameter();
        types::TypeSet *types = param->typeSet();
        if (!types)
            continue;

        JSValueType definiteType = types->getKnownTypeTag(cx);
        if (definiteType == JSVAL_TYPE_UNKNOWN)
            continue;

        MInstruction *actual = NULL;
        switch (definiteType) {
          case JSVAL_TYPE_UNDEFINED:
            actual = MConstant::New(UndefinedValue());
            break;

          case JSVAL_TYPE_NULL:
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

void
IonBuilder::initParameters()
{
    if (!info().fun())
        return;

    MParameter *param = MParameter::New(MParameter::THIS_SLOT,
                                        oracle->thisTypeSet(script));
    current->add(param);
    current->initSlot(info().thisSlot(), param);

    for (uint32 i = 0; i < info().nargs(); i++) {
        param = MParameter::New(i, oracle->parameterTypeSet(script, i));
        current->add(param);
        current->initSlot(info().argSlot(i), param);
    }

    // The scope chain is only tracked in scripts that have NAME opcodes which
    // will try to access the scope. For other scripts, the scope instructions
    // will be held live by resume points and code will still be generated for
    // them, so just use a constant undefined value.
    MInstruction *scope;
    if (script->analysis()->usesScopeChain()) {
        MCallee *callee = MCallee::New();
        current->add(callee);

        scope = MFunctionEnvironment::New(callee);
    } else {
        scope = MConstant::New(UndefinedValue());
    }

    current->add(scope);
    current->initSlot(info().scopeChainSlot(), scope);
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
            // while (cond) { }
            if (!whileLoop(op, sn))
              return ControlStatus_Error;
            return ControlStatus_Jumped;

          case SRC_FOR_IN:
            // for (x in y) { }
            if (!forInLoop(op, sn))
              return ControlStatus_Error;
            return ControlStatus_Jumped;

          default:
            // Hard assert for now - make an error later.
            JS_NOT_REACHED("unknown goto case");
            break;
        }
        break;
      }

      case JSOP_TABLESWITCH:
        return tableSwitch(op, info().getNote(cx, pc));

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
    // Don't compile fat opcodes, run the decomposed version instead.
    if (js_CodeSpec[op].format & JOF_DECOMPOSE)
        return true;

    switch (op) {
      case JSOP_NOP:
        return true;

      case JSOP_UNDEFINED:
        return pushConstant(UndefinedValue());

      case JSOP_IFEQ:
        return jsop_ifeq(JSOP_IFEQ);

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

      case JSOP_LOCALINC:
      case JSOP_INCLOCAL:
      case JSOP_LOCALDEC:
      case JSOP_DECLOCAL:
        return jsop_localinc(op);

      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
        return jsop_compare(op);

      case JSOP_ARGINC:
      case JSOP_INCARG:
      case JSOP_ARGDEC:
      case JSOP_DECARG:
        return jsop_arginc(op);

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

      case JSOP_FALSE:
        return pushConstant(BooleanValue(false));

      case JSOP_TRUE:
        return pushConstant(BooleanValue(true));

      case JSOP_NOTEARG:
        return jsop_notearg();

      case JSOP_GETARG:
      case JSOP_CALLARG:
        current->pushArg(GET_SLOTNO(pc));
        return true;

      case JSOP_SETARG:
        current->setArg(GET_SLOTNO(pc));
        return true;

      case JSOP_GETLOCAL:
        current->pushLocal(GET_SLOTNO(pc));
        return true;

      case JSOP_SETLOCAL:
        current->setLocal(GET_SLOTNO(pc));
        return true;

      case JSOP_POP:
        current->pop();
        return true;

      case JSOP_NEWARRAY:
        return jsop_newarray(GET_UINT24(pc));

      case JSOP_ENDINIT:
        return true;

      case JSOP_CALL:
        return jsop_call(GET_ARGC(pc));

      case JSOP_INT8:
        return pushConstant(Int32Value(GET_INT8(pc)));

      case JSOP_UINT16:
        return pushConstant(Int32Value(GET_UINT16(pc)));

      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
        return jsop_getgname(info().getAtom(pc));

      case JSOP_BINDGNAME:
        return pushConstant(ObjectValue(*script->global()));

      case JSOP_SETGNAME:
        return jsop_setgname(info().getAtom(pc));

      case JSOP_NAME:
        return jsop_getname(info().getAtom(pc));

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

      case JSOP_UINT24:
        return pushConstant(Int32Value(GET_UINT24(pc)));

      case JSOP_INT32:
        return pushConstant(Int32Value(GET_INT32(pc)));

      case JSOP_LOOPHEAD:
        // JSOP_LOOPHEAD is handled when processing the loop header.
        JS_NOT_REACHED("JSOP_LOOPHEAD outside loop");
        return true;

      case JSOP_GETELEM:
        return jsop_getelem();

      case JSOP_SETELEM:
        return jsop_setelem();

      case JSOP_LENGTH:
        return jsop_length();

      case JSOP_THIS:
        return jsop_this();

      case JSOP_GETPROP:
        return jsop_getprop(info().getAtom(pc));

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

    // If the loop started with a condition (while/for) then even if the
    // structure never actually loops, the condition itself can still fail and
    // thus we must resume at the successor, if one exists.
    current = state.loop.successor;

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

    // Compute phis in the loop header and propagate them throughout the loop,
    // including the successor.
    if (!state.loop.entry->setBackedge(current))
        return ControlStatus_Error;
    if (successor)
        successor->inheritPhis(state.loop.entry);

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
    MBasicBlock *successor = newBlock(current, GetNextPc(pc));
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
    state.loop.successor = newBlock(current, state.loop.exitpc);
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
    state.loop.successor = newBlock(current, state.loop.exitpc);
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

        MBasicBlock *update = newBlock(edge->block, pc);
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

    state.tableswitch.currentSuccessor++;

    // Test if there are still unprocessed successors (cases/default)
    if (state.tableswitch.currentSuccessor >= state.tableswitch.ins->numSuccessors())
        return processTableSwitchEnd(state);

    // Get the next successor
    MBasicBlock *successor = state.tableswitch.ins->getSuccessor(state.tableswitch.currentSuccessor);

    // Add current block as predecessor if available.
    // This means the previous case didn't have a break statement.
    // So flow will continue in this block.
    if (current) {
        current->end(MGoto::New(successor));
        successor->addPredecessor(current);
    }

    // If this is the last successor the block should stop at the end of the tableswitch
    // Else it should stop at the start of the next successor
    if (state.tableswitch.currentSuccessor+1 < state.tableswitch.ins->numSuccessors())
        state.stopAt = state.tableswitch.ins->getSuccessor(state.tableswitch.currentSuccessor+1)->pc();
    else
        state.stopAt = state.tableswitch.exitpc;

    current = successor;
    pc = current->pc();
    return ControlStatus_Jumped;
}

IonBuilder::ControlStatus
IonBuilder::processTableSwitchEnd(CFGState &state)
{
    // No break statements and no current
    // This means that control flow is cut-off from this point
    // (e.g. all cases have return statements).
    if (!state.tableswitch.breaks && !current)
        return ControlStatus_Ended;

    // Create successor block.
    // If there are breaks, create block with breaks as predecessor
    // Else create a block with current as predecessor 
    MBasicBlock *successor = NULL;
    if (state.tableswitch.breaks)
        successor = createBreakCatchBlock(state.tableswitch.breaks, state.tableswitch.exitpc);
    else
        successor = newBlock(current, state.tableswitch.exitpc);

    if (!successor)
        return ControlStatus_Ended;

    // If there is current, the current block flows into this one.
    // So current is also a predecessor to this block 
    if (current) {
        current->end(MGoto::New(successor));
        if (state.tableswitch.breaks)
            successor->addPredecessor(current);
    }

    pc = state.tableswitch.exitpc;
    current = successor;
    return ControlStatus_Joined;
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

IonBuilder::ControlStatus
IonBuilder::processContinue(JSOp op, jssrcnote *sn)
{
    JS_ASSERT(op == JSOP_GOTO);

    // Find the target loop.
    CFGState *found = NULL;
    jsbytecode *target = pc + GetJumpOffset(pc);
    for (size_t i = loops_.length() - 1; i < loops_.length(); i--) {
        if (loops_[i].continuepc == target) {
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

    state.tableswitch.breaks = new DeferredEdge(current, state.tableswitch.breaks);

    current = NULL;
    pc += js_CodeSpec[op].length;
    return processControlEnd();
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

    // Make sure this is the next opcode after the loop header.
    CFGState &state = cfgStack_.back();
    JS_ASSERT(GetNextPc(state.loop.entry->pc()) == pc);

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
    JS_ASSERT(JSOp(*GetNextPc(pc)) == JSOP_LOOPHEAD);
    JS_ASSERT(GetNextPc(pc) == ifne + GetJumpOffset(ifne));

    if (GetNextPc(pc) == info().osrPc()) {
        MBasicBlock *preheader = newOsrPreheader(current, GetNextPc(pc));
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
    if (!pushLoop(CFGState::DO_WHILE_LOOP_BODY, conditionpc, header, bodyStart, bodyEnd, exitpc, conditionpc))
        return ControlStatus_Error;

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
IonBuilder::whileLoop(JSOp op, jssrcnote *sn)
{
    // while (cond) { } loops have the following structure:
    //    GOTO cond   ; SRC_WHILE (offset to IFNE)
    //    LOOPHEAD
    //    ...
    //  cond:
    //    ...
    //    IFNE        ; goes to LOOPHEAD
    int ifneOffset = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *ifne = pc + ifneOffset;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a loophead op.
    JS_ASSERT(JSOp(*GetNextPc(pc)) == JSOP_LOOPHEAD);
    JS_ASSERT(GetNextPc(pc) == ifne + GetJumpOffset(ifne));

    if (GetNextPc(pc) == info().osrPc()) {
        MBasicBlock *preheader = newOsrPreheader(current, GetNextPc(pc));
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
    //   [GOTO cond]
    //   LOOPHEAD
    // body:
    //    ; [body]
    // [increment:]
    //    ; [increment]
    // [cond:]
    //   GOTO body
    //
    // If there is a condition (condpc != ifne), this acts similar to a while
    // loop otherwise, it acts like a do-while loop.
    jsbytecode *bodyStart = pc;
    jsbytecode *bodyEnd = updatepc;
    if (condpc != ifne) {
        JS_ASSERT(JSOp(*bodyStart) == JSOP_GOTO);
        JS_ASSERT(bodyStart + GetJumpOffset(bodyStart) == condpc);
        bodyStart = GetNextPc(bodyStart);
    }
    jsbytecode *loopHead = bodyStart;
    JS_ASSERT(JSOp(*bodyStart) == JSOP_LOOPHEAD);
    JS_ASSERT(ifne + GetJumpOffset(ifne) == bodyStart);
    bodyStart = GetNextPc(bodyStart);

    if (GetNextPc(pc) == info().osrPc()) {
        MBasicBlock *preheader = newOsrPreheader(current, GetNextPc(pc));
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
    jsint low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;

    // Create MIR instruction
    MTableSwitch *tableswitch = MTableSwitch::New(ins, low, high);

    // Create default case
    MBasicBlock *defaultcase = newBlock(current, defaultpc);
    if (!defaultcase)
        return ControlStatus_Error;
    tableswitch->addDefault(defaultcase);

    // Create cases
    jsbytecode *casepc = NULL;
    for (jsint i = 0; i < high-low+1; i++) {
        casepc = pc + GET_JUMP_OFFSET(pc2);
        
        JS_ASSERT(casepc >= pc && casepc <= exitpc);

        // If the casepc equals the current pc, it is not a written case,
        // but a filled gap. That way we can use a tableswitch instead of 
        // lookupswitch, even if not all numbers are consecutive.
        if (casepc == pc) {
            tableswitch->addCase(defaultcase, true);
        } else {
            MBasicBlock *caseblock = newBlock(current, casepc);
            if (!caseblock)
                return ControlStatus_Error;
            tableswitch->addCase(caseblock);
        }

        pc2 += JUMP_OFFSET_LEN;
    }

    JS_ASSERT(tableswitch->numCases() == (uint32)(high - low + 1));
    JS_ASSERT(tableswitch->numSuccessors() > 0);

    // Sort the successors
    qsort(tableswitch->successors(), tableswitch->numSuccessors(),
          sizeof(MBasicBlock*), CmpSuccessors);

    // Create info 
    ControlFlowInfo switchinfo(cfgStack_.length(), exitpc);
    if (!switches_.append(switchinfo))
        return ControlStatus_Error;

    // Use a state to retrieve some information
    CFGState state;
    state.state = CFGState::TABLE_SWITCH;
    state.tableswitch.exitpc = exitpc;
    state.tableswitch.breaks = NULL;
    state.tableswitch.ins = tableswitch;
    state.tableswitch.currentSuccessor = 0;

    // Save the MIR instruction as last instruction of this block.
    current->end(tableswitch);

    // If there is only one successor the block should stop at the end of the switch
    // Else it should stop at the start of the next successor
    if (tableswitch->numSuccessors() == 1)
        state.stopAt = state.tableswitch.exitpc;
    else
        state.stopAt = tableswitch->getSuccessor(1)->pc();
    current = tableswitch->getSuccessor(0);

    if (!cfgStack_.append(state))
        return ControlStatus_Error;

    pc = current->pc();
    return ControlStatus_Jumped;
}

bool
IonBuilder::jsop_andor(JSOp op)
{
    jsbytecode *rhsStart = pc + js_CodeSpec[op].length;
    jsbytecode *joinStart = pc + GetJumpOffset(pc);
    JS_ASSERT(joinStart > pc);

    // We have to leave the LHS on the stack.
    MDefinition *lhs = current->peek(-1);

    MBasicBlock *evalRhs = newBlock(current, rhsStart);
    MBasicBlock *join = newBlock(current, joinStart);
    if (!evalRhs || !join)
        return false;

    if (op == JSOP_AND) {
        current->end(MTest::New(lhs, evalRhs, join));
    } else {
        JS_ASSERT(op == JSOP_OR);
        current->end(MTest::New(lhs, join, evalRhs));
    }

    if (!cfgStack_.append(CFGState::AndOr(joinStart, join)))
        return false;

    current = evalRhs;
    return true;
}

bool
IonBuilder::jsop_dup2()
{
    uint32 lhsSlot = current->stackDepth() - 2;
    uint32 rhsSlot = current->stackDepth() - 1;
    current->pushSlot(lhsSlot);
    current->pushSlot(rhsSlot);
    return true;
}

bool
IonBuilder::jsop_loophead(jsbytecode *pc)
{
    assertValidLoopHeadOp(pc);
    insertRecompileCheck();
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

    current->end(MTest::New(ins, ifTrue, ifFalse));

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

    MReturn *ret = MReturn::New(def);
    current->end(ret);

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
    ins->infer(oracle->unaryOp(script, pc));

    current->push(ins);
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
    ins->infer(oracle->binaryOp(script, pc));

    current->push(ins);
    return true;
}

bool
IonBuilder::jsop_binary(JSOp op, MDefinition *left, MDefinition *right)
{
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

    current->add(ins);
    ins->infer(oracle->binaryOp(script, pc));

    if (ins->type() == MIRType_Value || (op == JSOP_MOD && ins->type() != MIRType_Int32))
        return abort("unspecialized add not yet supported");

    current->push(ins);
    return true;
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
    TypeOracle::Unary types = oracle->unaryOp(script, pc);
    MDefinition *value = current->pop();
    MInstruction *ins;
    if (types.rval == MIRType_Int32)
        ins = MToInt32::New(value);
    else
        ins = MToDouble::New(value);
    current->add(ins);
    current->push(ins);
    return true;
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

bool
IonBuilder::jsop_call_inline(uint32 argc, IonBuilder &inlineBuilder, InliningData *data)
{
#ifdef DEBUG
    uint32 origStackDepth = current->stackDepth();
#endif

    // |top| jumps into the callee subgraph -- save it for later use.
    MBasicBlock *top = current;

    MResumePoint *inlineResumePoint = MResumePoint::New(top, pc, callerResumePoint_);
    if (!inlineResumePoint)
        return false;

    // Gather up the arguments and |this| to the inline function.
    // Note that we leave the callee on the simulated stack for the
    // duration of the call.
    MDefinitionVector args;
    if (!args.growByUninitialized(argc))
        return false;

    // Arguments are popped right-to-left so we have to fill |args| backwards.
    for (uintN i = argc - 1; i < argc; i--) {
        MPassArg *passArg = top->pop()->toPassArg();
        JS_ASSERT(passArg->useCount() == 0);
        passArg->block()->discard(passArg);
        MDefinition *wrapped = passArg->getArgument();
        args[i] = wrapped;
    }

    MPassArg *thisArg = top->pop()->toPassArg();
    MDefinition *thisDefn = thisArg->getArgument();
    JS_ASSERT(thisArg->useCount() == 0);
    thisArg->block()->discard(thisArg);

    // Build the graph.
    if (!inlineBuilder.buildInline(inlineResumePoint, thisDefn, args))
        return false;

    MIRGraphExits &exits = inlineBuilder.graph().getExitAccumulator();

    // Create a |bottom| block for all the callee subgraph exits to jump to.
    JS_ASSERT(*pc == JSOP_CALL);
    jsbytecode *postCall = GetNextPc(pc);
    MBasicBlock *bottom = newBlock(NULL, postCall);

    // Link graph exits to |bottom| via MGotos, replacing MReturns.
    Vector<MDefinition *, 8, IonAllocPolicy> retvalDefns;
    for (MBasicBlock **it = exits.begin(), **end = exits.end(); it != end; ++it) {
        MBasicBlock *exitBlock = *it;
        JS_ASSERT(exitBlock->lastIns()->op() == MDefinition::Op_Return);

        if (!retvalDefns.append(exitBlock->lastIns()->toReturn()->getOperand(0)))
            return false;

        exitBlock->discardLastIns();
        MGoto *replacement = MGoto::New(bottom);
        exitBlock->end(replacement);
        if (!bottom->addPredecessorWithoutPhis(exitBlock))
            return false;
    }
    JS_ASSERT(!retvalDefns.empty());

    if (!bottom->inheritNonPredecessor(top))
        return false;

    // Pop the callee and push the return value.
    (void) bottom->pop();

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

    // Replace the callee with the return value in the resumepoint.
    uint32 retvalSlot = bottom->stackDepth() - 1;
    bottom->entryResumePoint()->replaceOperand(retvalSlot, retvalDefn);

    // Check the depth change:
    //  -argc for popped args
    //  -2 for callee/this
    //  +1 for retval
    JS_ASSERT(bottom->stackDepth() == origStackDepth - argc - 1);

    current = bottom;
    return true;
}

class AutoAccumulateExits
{
    MIRGraph &graph;

  public:
    AutoAccumulateExits(MIRGraph &graph, MIRGraphExits &exits) : graph(graph) {
        graph.setExitAccumulator(&exits);
    }
    ~AutoAccumulateExits() {
        graph.setExitAccumulator(NULL);
    }
};

bool
IonBuilder::makeInliningDecision(uint32 argc, InliningData *data)
{
    JS_ASSERT(data->shouldInline == false);

    if (script->getUseCount() < js_IonOptions.usesBeforeInlining) {
        IonSpew(IonSpew_Inlining, "Not inlining, caller is not hot");
        return true;
    }

    JSFunction *inlineFunc = NULL;
    if (!getInliningTarget(argc, pc, &inlineFunc))
        return false;

    if (!inlineFunc) {
        IonSpew(IonSpew_Inlining, "Decided not to inline");
        return true;
    }

    data->shouldInline = true;
    data->callee = inlineFunc;
    return true;
}

IonBuilder::InliningStatus
IonBuilder::maybeInline(uint32 argc)
{
    InliningData data;
    if (!makeInliningDecision(argc, &data))
        return InliningStatus_Error;

    if (!data.shouldInline || inliningDepth >= 1)
        return InliningStatus_NotInlined;

    IonSpew(IonSpew_Inlining, "Recursively building");
    // Compilation information is allocated for the duration of the current tempLifoAlloc
    // lifetime.
    CompileInfo *info = cx->tempLifoAlloc().new_<CompileInfo>(data.callee->script().get(),
                                                              data.callee, (jsbytecode *)NULL);
    if (!info)
        return InliningStatus_Error;

    MIRGraphExits exits;
    AutoAccumulateExits aae(graph(), exits);

    if (cx->typeInferenceEnabled()) {
        TypeInferenceOracle oracle;
        if (!oracle.init(cx, data.callee->script()))
            return InliningStatus_Error;
        IonBuilder inlineBuilder(cx, temp(), graph(), &oracle, *info, inliningDepth + 1);
        return jsop_call_inline(argc, inlineBuilder, &data)
             ? InliningStatus_Inlined
             : InliningStatus_Error;
    }

    DummyOracle oracle;
    IonBuilder inlineBuilder(cx, temp(), graph(), &oracle, *info, inliningDepth + 1);
    return jsop_call_inline(argc, inlineBuilder, &data)
         ? InliningStatus_Inlined
         : InliningStatus_Error;
}

bool
IonBuilder::jsop_call(uint32 argc)
{
    if (inliningEnabled()) {
        InliningStatus status = maybeInline(argc);
        switch (status) {
          case InliningStatus_Error:
            return false;
          case InliningStatus_Inlined:
            return true;
          case InliningStatus_NotInlined:
            IonSpew(IonSpew_Inlining, "Building out-of-line call");
            break;
        }

    }

    MCall *ins = MCall::New(argc + 1); // +1 for implicit this.
    if (!ins)
        return false;

    // Bytecode order: Function, This, Arg0, Arg1, ..., ArgN, Call.
    for (int32 i = argc; i >= 0; i--)
        ins->addArg(i, current->pop()->toPassArg());
    ins->initFunction(current->pop());

    // Insert an MPrepareCall immediately before the first argument is pushed.
    MPrepareCall *start = new MPrepareCall;
    MPassArg *arg = ins->getArg(0)->toPassArg();
    current->insertBefore(arg, start);

    ins->initPrepareCall(start);

    current->add(ins);
    current->push(ins);
    if (!resumeAfter(ins))
        return false;

    types::TypeSet *barrier;
    types::TypeSet *types = oracle->returnTypeSet(script, pc, &barrier);
    return pushTypeBarrier(ins, types, barrier);
}

bool
IonBuilder::jsop_incslot(JSOp op, uint32 slot)
{
    int32 amt = (js_CodeSpec[op].format & JOF_INC) ? 1 : -1;
    bool post = !!(js_CodeSpec[op].format & JOF_POST);
    TypeOracle::Binary types = oracle->binaryOp(script, pc);

    // Grab the value at the local slot, and convert it to a number. Currently,
    // we use ToInt32 or ToNumber which are fallible but idempotent. This whole
    // operation must be idempotent because we cannot resume in the middle of
    // an INC op.
    current->pushSlot(slot);
    MDefinition *value = current->pop();
    MInstruction *lhs;
    if (types.lhs == MIRType_Int32)
        lhs = MToInt32::New(value);
    else
        lhs = MToDouble::New(value);
    current->add(lhs);

    // If this is a post operation, save the original value.
    if (post)
        current->push(lhs);

    MConstant *rhs = MConstant::New(Int32Value(amt));
    current->add(rhs);

    MAdd *result = MAdd::New(lhs, rhs);
    current->add(result);
    result->infer(types);
    current->push(result);
    current->setSlot(slot);

    if (post)
        current->pop();
    return true;
}

bool
IonBuilder::jsop_localinc(JSOp op)
{
    return jsop_incslot(op, info().localSlot(GET_SLOTNO(pc)));
}

bool
IonBuilder::jsop_arginc(JSOp op)
{
    return jsop_incslot(op, info().argSlot(GET_SLOTNO(pc)));
}

bool
IonBuilder::jsop_compare(JSOp op)
{
    // Pop inputs
    MDefinition *right = current->pop();
    MDefinition *left = current->pop();

    // Add instruction to current block
    MCompare *ins = MCompare::New(left, right, op);
    current->add(ins);

    ins->infer(oracle->binaryOp(script, pc));

    if (ins->specialization() == MIRType_None)
        return abort("unspecialized compare not yet supported");

    // Push result
    current->push(ins);
    return true;
}

bool
IonBuilder::jsop_newarray(uint32 count)
{
    using namespace types;

    // This type object will stay the same for each couple (script, pc).  We do
    // not use the type oracle here because the type oracle needs to see a few
    // runs before giving us a typeset containing only the result of the
    // InitObject function.
    TypeObject *type = TypeScript::InitObject(cx, script, pc, JSProto_Array);
    if (!type)
        return false;

    MNewArray *ins = new MNewArray(count, type);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;
    return true;
}

MBasicBlock *
IonBuilder::addBlock(MBasicBlock *block)
{
    if (!block)
        return NULL;
    graph().addBlock(block);
    block->setLoopDepth(loops_.length());
    return block;
}

MBasicBlock *
IonBuilder::newBlock(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(graph(), info(), predecessor, pc, MBasicBlock::NORMAL);
    return addBlock(block);
}

MBasicBlock *
IonBuilder::newOsrPreheader(MBasicBlock *predecessor, jsbytecode *pc)
{
    JS_ASSERT((JSOp)*pc == JSOP_LOOPHEAD);
    JS_ASSERT(pc == info().osrPc());

    // Create two blocks: one for the OSR entry with no predecessors, one for
    // the preheader, which has the OSR entry block as a predecessor.
    MBasicBlock *osrBlock  = newBlock(pc);
    MBasicBlock *preheader = newBlock(predecessor, pc);

    MOsrEntry *entry = MOsrEntry::New();
    osrBlock->add(entry);

    // Initialize |scopeChain|.
    {
        uint32 slot = info().scopeChainSlot();

        MOsrScopeChain *scopev = MOsrScopeChain::New(entry);
        osrBlock->add(scopev);
        osrBlock->initSlot(slot, scopev);
    }

    // Initialize |this| parameter.
    {
        uint32 slot = info().thisSlot();
        ptrdiff_t offset = StackFrame::offsetOfThis(info().fun());

        MOsrValue *thisv = MOsrValue::New(entry, offset);
        osrBlock->add(thisv);
        osrBlock->initSlot(slot, thisv);
    }

    // Initialize arguments.
    for (uint32 i = 0; i < info().nargs(); i++) {
        uint32 slot = info().argSlot(i);
        ptrdiff_t offset = StackFrame::offsetOfFormalArg(info().fun(), i);

        MOsrValue *osrv = MOsrValue::New(entry, offset);
        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
    }

    // Initialize locals.
    for (uint32 i = 0; i < info().nlocals(); i++) {
        uint32 slot = info().localSlot(i);
        ptrdiff_t offset = StackFrame::offsetOfFixed(i);

        MOsrValue *osrv = MOsrValue::New(entry, offset);
        osrBlock->add(osrv);
        osrBlock->initSlot(slot, osrv);
    }

    // Initialize stack.
    uint32 numSlots = preheader->stackDepth() - CountArgSlots(info().fun()) - info().nlocals();
    for (uint32 i = 0; i < numSlots; i++) {
        uint32 slot = info().stackSlot(i);
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
    if (!resumeAt(start, pc))
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
    JS_ASSERT(osrBlock->getSlot(info().scopeChainSlot())->type() == MIRType_Object);
    for (uint32 i = 1; i < osrBlock->stackDepth(); i++) {
        MIRType guessType = predecessor->getSlot(i)->type();
        if (guessType != MIRType_Value) {
            MDefinition *def = osrBlock->getSlot(i);
            JS_ASSERT(def->type() == MIRType_Value);
            MInstruction *actual = MUnbox::New(def, guessType, MUnbox::Fallible);
            osrBlock->add(actual);
            osrBlock->rewriteSlot(i, actual);
        }
    }

    // Finish the osrBlock.
    osrBlock->end(MGoto::New(preheader));
    preheader->addPredecessor(osrBlock);
    graph().setOsrBlock(osrBlock);

    return preheader;
}

MBasicBlock *
IonBuilder::newPendingLoopHeader(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::NewPendingLoopHeader(graph(), info(), predecessor, pc);
    return addBlock(block);
}

// A resume point is a mapping of stack slots to MDefinitions. It is used to
// capture the environment such that if a guard fails, and IonMonkey needs
// to exit back to the interpreter, the interpreter state can be
// reconstructed.
//
// The resume model differs from TraceMonkey in that we do not need to
// take snapshots for every guard. Instead, we capture stack state at
// critical points:
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
// resumeAfter() must be used. The state is attached directly to the effectful
// instruction to ensure that no intermediate instructions could be injected
// in between by a future analysis pass.
//
// During LIR construction, if an instruction can bail back to the interpreter,
// we create an LSnapshot, which uses the last known resume point to request
// register/stack assignments for every live value.
bool
IonBuilder::resumeAfter(MInstruction *ins)
{
    return resumeAt(ins, GetNextPc(pc));
}

bool
IonBuilder::resumeAt(MInstruction *ins, jsbytecode *pc)
{
    JS_ASSERT(ins->isEffectful());

    MResumePoint *resumePoint = MResumePoint::New(ins->block(), pc, callerResumePoint_);
    if (!resumePoint)
        return false;
    lastResumePoint_ = resumePoint;
    ins->setResumePoint(resumePoint);
    return true;
}

void
IonBuilder::insertRecompileCheck()
{
    if (!inliningEnabled())
        return;

    // Don't recompile if we are already inlining.
    if (script->getUseCount() >= js_IonOptions.usesBeforeInlining)
        return;

    // Don't recompile if the oracle cannot provide inlining information
    // or if the script has no calls.
    if (!oracle->canInlineCalls())
        return;

    MRecompileCheck *check = MRecompileCheck::New();
    current->add(check);
}

static inline bool
TestSingletonProperty(JSContext *cx, JSObject *obj, jsid id, bool *isKnownConstant)
{
    // We would like to completely no-op property/global accesses which can
    // produce only a particular JSObject or undefined, provided we can
    // determine the pushed value must not be undefined (or, if it could be
    // undefined, a recompilation will be triggered).
    // 
    // If the access definitely goes through obj, either directly or on the
    // prototype chain, then if obj has a defined property now, and the
    // property has a default or method shape, the only way it can produce
    // undefined in the future is if it is deleted. Deletion causes type
    // properties to be explicitly marked with undefined.
    *isKnownConstant = false;

    JSObject *pobj = obj;
    while (pobj) {
        if (!pobj->isNative())
            return true;
        if (pobj->getClass()->ops.lookupProperty)
            return true;
        pobj = pobj->getProto();
    }

    JSObject *holder;
    JSProperty *prop = NULL;
    if (!obj->lookupGeneric(cx, id, &holder, &prop))
        return false;
    if (!prop)
        return true;

    Shape *shape = (Shape *)prop;
    if (shape->hasDefaultGetter()) {
        if (!shape->hasSlot())
            return true;
        if (holder->getSlot(shape->slot()).isUndefined())
            return true;
    } else if (!shape->isMethod()) {
        return true;
    }

    *isKnownConstant = true;
    return true;
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
IonBuilder::pushTypeBarrier(MInstruction *ins, types::TypeSet *actual, types::TypeSet *observed)
{
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
        JSValueType type = actual->getKnownTypeTag(cx);
        MInstruction *replace = NULL;
        switch (type) {
          case JSVAL_TYPE_UNDEFINED:
            replace = MConstant::New(UndefinedValue());
            break;
          case JSVAL_TYPE_NULL:
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
    observed->addFreeze(cx);

    MInstruction *barrier;
    JSValueType type = observed->getKnownTypeTag(cx);

    // An unbox instruction isn't enough to capture JSVAL_TYPE_OBJECT.
    if (type == JSVAL_TYPE_OBJECT && !observed->hasType(types::Type::AnyObjectType()))
        type = JSVAL_TYPE_UNKNOWN;

    switch (type) {
      case JSVAL_TYPE_UNKNOWN:
      case JSVAL_TYPE_UNDEFINED:
      case JSVAL_TYPE_NULL:
        barrier = MTypeBarrier::New(ins, observed);
        current->add(barrier);

        if (type == JSVAL_TYPE_UNDEFINED)
            return pushConstant(UndefinedValue());
        if (type == JSVAL_TYPE_NULL)
            return pushConstant(NullValue());
        break;
      default:
        MUnbox::Mode mode = ins->isEffectful() ? MUnbox::TypeBarrier : MUnbox::Fallible;
        barrier = MUnbox::New(ins, MIRTypeFromValueType(type), mode);
        current->add(barrier);
    }
    current->push(barrier);
    return true;
}

bool
IonBuilder::jsop_getgname(JSAtom *atom)
{
    // Optimize undefined, NaN, and Infinity.
    if (atom == cx->runtime->atomState.typeAtoms[JSTYPE_VOID])
        return pushConstant(UndefinedValue());
    if (atom == cx->runtime->atomState.NaNAtom)
        return pushConstant(cx->runtime->NaNValue);
    if (atom == cx->runtime->atomState.InfinityAtom)
        return pushConstant(cx->runtime->positiveInfinityValue);

    jsid id = ATOM_TO_JSID(atom);
    JSObject *globalObj = script->global();
    JS_ASSERT(globalObj->isNative());

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    const js::Shape *shape = globalObj->nativeLookup(cx, id);
    if (!shape)
        return abort("GETGNAME property not found on global");
    if (!shape->hasDefaultGetterOrIsMethod())
        return abort("GETGNAME found a getter");
    if (!shape->hasSlot())
        return abort("GETGNAME property has no slot");

    // If the property is permanent, a shape guard isn't necessary.
    JSValueType knownType = JSVAL_TYPE_UNKNOWN;

    types::TypeSet *barrier = oracle->propertyReadBarrier(script, pc);
    types::TypeSet *types = oracle->propertyRead(script, pc);
    if (types) {
        JSObject *singleton = types->getSingleton(cx);

        knownType = types->getKnownTypeTag(cx);
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

    types::TypeSet *propertyTypes = oracle->globalPropertyTypeSet(script, pc, id);
    if (propertyTypes && propertyTypes->isOwnProperty(cx, globalObj->getType(cx), true)) {
        return abort("GETGNAME property reconfigured as non-configurable, non-enumerable "
                     "or non-writable");
    }

    // If we have a property typeset, the isOwnProperty call will trigger recompilation if
    // the property is deleted or reconfigured.
    if (!propertyTypes && shape->configurable()) {
        MGuardShape *guard = MGuardShape::New(global, globalObj->lastProperty());
        current->add(guard);
    }

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
IonBuilder::jsop_setgname(JSAtom *atom)
{
    jsid id = ATOM_TO_JSID(atom);

    bool canSpecialize;
    types::TypeSet *propertyTypes = oracle->globalPropertyWrite(script, pc, id, &canSpecialize);

    // This should only happen for a few names like __proto__.
    if (!canSpecialize)
        return abort("SETGNAME unable to specialize property access");

    JSObject *globalObj = script->global();
    JS_ASSERT(globalObj->isNative());

    // For the fastest path, the property must be found, and it must be found
    // as a normal data property on exactly the global object.
    const js::Shape *shape = globalObj->nativeLookup(cx, id);
    if (!shape)
        return abort("SETGNAME property not found on global");
    if (shape->isMethod())
        return abort("SETGNAME found a method");
    if (!shape->hasDefaultSetter())
        return abort("SETGNAME found a setter");
    if (!shape->writable())
        return abort("SETGNAME non-writable property");
    if (!shape->hasSlot())
        return abort("SETGNAME property has no slot");

    if (propertyTypes && propertyTypes->isOwnProperty(cx, globalObj->getType(cx), true))
        return abort("SETGNAME property is non-configurable, non-enumerable or non-writable");

    MInstruction *global = MConstant::New(ObjectValue(*globalObj));
    current->add(global);

    // If we have a property type set, the isOwnProperty call will trigger recompilation
    // if the property is deleted or reconfigured. Without TI, we always need a shape guard
    // to guard against the property being reconfigured as non-writable.
    if (!propertyTypes) {
        MGuardShape *guard = MGuardShape::New(global, globalObj->lastProperty());
        current->add(guard);
    }

    JS_ASSERT(shape->slot() >= globalObj->numFixedSlots());

    MSlots *slots = MSlots::New(global);
    current->add(slots);

    MDefinition *value = current->pop();
    MStoreSlot *store = MStoreSlot::New(slots, shape->slot() - globalObj->numFixedSlots(), value);
    current->add(store);

#ifdef JSGC_INCREMENTAL
    // Determine whether write barrier is required.
    if (cx->compartment->needsBarrier() &&
        (!propertyTypes || propertyTypes->needsBarrier(cx)))
    {
        store->setNeedsBarrier(true);
    }
#endif

    if (!resumeAfter(store))
        return false;

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

    current->push(value);
    return true;
}

bool
IonBuilder::jsop_getname(JSAtom *atom)
{
    current->pushSlot(info().scopeChainSlot());
    MDefinition *scopeChain = current->pop();

    MCallGetPropertyOrName *ins;

    JSOp op2 = JSOp(pc[JSOP_NAME_LENGTH]);
    if (op2 == JSOP_TYPEOF)
        ins = MCallGetNameTypeOf::New(scopeChain, atom);
    else
        ins = MCallGetName::New(scopeChain, atom);

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    types::TypeSet *barrier = oracle->propertyReadBarrier(script, pc);
    types::TypeSet *types = oracle->propertyRead(script, pc);
    return pushTypeBarrier(ins, types, barrier);
}

bool
IonBuilder::jsop_getelem()
{
    if (oracle->elementReadIsDense(script, pc))
        return jsop_getelem_dense();

    return abort("GETELEM Not a dense array");
}

bool
IonBuilder::jsop_getelem_dense()
{
    if (oracle->arrayPrototypeHasIndexedProperty())
        return abort("GETELEM Array proto has indexed properties");

    types::TypeSet *barrier = oracle->propertyReadBarrier(script, pc);
    types::TypeSet *types = oracle->propertyRead(script, pc);
    bool needsHoleCheck = !oracle->elementReadIsPacked(script, pc);

    MDefinition *id = current->pop();
    MDefinition *obj = current->pop();

    JSValueType knownType = JSVAL_TYPE_UNKNOWN;
    if (!needsHoleCheck && !barrier) {
        knownType = types->getKnownTypeTag(cx);

        if (knownType == JSVAL_TYPE_UNDEFINED)
            return pushConstant(UndefinedValue());
        if (knownType == JSVAL_TYPE_NULL)
            return pushConstant(NullValue());
    }

    // Ensure id is an integer.
    MInstruction *idInt32 = MToInt32::New(id);
    current->add(idInt32);
    id = idInt32;

    // Get the elements vector.
    MElements *elements = MElements::New(obj);
    current->add(elements);

    // Read and check length.
    MInitializedLength *initLength = MInitializedLength::New(elements);
    current->add(initLength);

    MBoundsCheck *check = MBoundsCheck::New(id, initLength);
    current->add(check);

    // Load the value.
    MLoadElement *load = MLoadElement::New(elements, id, needsHoleCheck);
    current->add(load);

    if (knownType != JSVAL_TYPE_UNKNOWN)
        load->setResultType(MIRTypeFromValueType(knownType));

    current->push(load);
    return pushTypeBarrier(load, types, barrier);
}

bool
IonBuilder::jsop_setelem()
{
    if (!oracle->propertyWriteCanSpecialize(script, pc))
        return abort("SETELEM cannot specialize");

    if (oracle->elementWriteIsDense(script, pc))
        return jsop_setelem_dense();

    return abort("SETELEM Not a dense array");
}

bool
IonBuilder::jsop_setelem_dense()
{
    if (oracle->arrayPrototypeHasIndexedProperty())
        return abort("SETELEM Array proto has indexed properties");

    MIRType elementType = oracle->elementWrite(script, pc);
    bool packed = oracle->elementWriteIsPacked(script, pc);

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

    // Read and check length.

    MInitializedLength *initLength = MInitializedLength::New(elements);
    current->add(initLength);

    MBoundsCheck *check = MBoundsCheck::New(id, initLength);
    current->add(check);

    // Store the value.
    MStoreElement *store = MStoreElement::New(elements, id, value);
    current->add(store);
    current->push(value);

#ifdef JSGC_INCREMENTAL
    // Determine whether a write barrier is required.
    if (cx->compartment->needsBarrier() &&
        oracle->propertyWriteNeedsBarrier(script, pc, JSID_VOID))
    {
        store->setNeedsBarrier(true);
    }
#endif

    if (elementType != MIRType_None && packed)
        store->setElementType(elementType);

    return resumeAfter(store);
}

bool
IonBuilder::jsop_length()
{
    if (jsop_length_fastPath())
        return true;
    return jsop_getprop(info().getAtom(pc));
}

bool
IonBuilder::jsop_length_fastPath()
{
    TypeOracle::UnaryTypes sig = oracle->unaryTypes(script, pc);
    if (!sig.inTypes || !sig.outTypes)
        return false;

    if (sig.outTypes->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
        return false;

    switch (sig.inTypes->getKnownTypeTag(cx)) {
      case JSVAL_TYPE_STRING: {
        MDefinition *obj = current->pop();
        MStringLength *ins = new MStringLength(obj);
        current->add(ins);
        current->push(ins);
        return true;
      }

      case JSVAL_TYPE_OBJECT: {
        if (sig.inTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY))
            return false;

        MDefinition *obj = current->pop();
        MElements *elements = MElements::New(obj);
        current->add(elements);

        // Read length.
        MArrayLength *length = new MArrayLength(elements);
        current->add(length);
        current->push(length);

        return true;
      }

      default:
        break;
    }

    return false;
}

bool
IonBuilder::jsop_getprop(JSAtom *atom)
{
    MDefinition *obj = current->pop();
    MInstruction *ins;

    types::TypeSet *barrier = oracle->propertyReadBarrier(script, pc);
    types::TypeSet *types = oracle->propertyRead(script, pc);

    TypeOracle::Unary unary = oracle->unaryOp(script, pc);
    if (unary.ival == MIRType_Object) {
        MGetPropertyCache *load = MGetPropertyCache::New(obj, atom, script, pc);
        if (!barrier)
            load->setResultType(unary.rval);
        ins = load;
    } else {
        ins = MCallGetProperty::New(obj, atom);
    }

    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return false;

    return pushTypeBarrier(ins, types, barrier);
}

bool
IonBuilder::jsop_this()
{
    if (!info().fun())
        return abort("JSOP_THIS outside of a JSFunction.");

    // Do not re-pushed, use pushSlot instead.
    MDefinition *thisParam = current->getSlot(info().thisSlot());

    if (thisParam->type() != MIRType_Object)
        return abort("Cannot compile JSOP_THIS, not an object.");

    current->pushSlot(info().thisSlot());
    return true;
}
