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

#include "BytecodeAnalyzer.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonSpew.h"
#include "jsemit.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

bool
ion::Go(JSContext *cx, JSScript *script, StackFrame *fp)
{
    TempAllocator temp(&cx->tempPool);

    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;

    MIRGraph graph(cx);
    C1Spewer spew(graph, script);
    BytecodeAnalyzer analyzer(cx, script, fun, temp, graph);
    spew.enable("/tmp/ion.cfg");

    if (!analyzer.analyze())
        return false;

    spew.spew("Building SSA");

    return false;
}

BytecodeAnalyzer::BytecodeAnalyzer(JSContext *cx, JSScript *script, JSFunction *fun, TempAllocator &temp,
                                   MIRGraph &graph)
  : MIRGenerator(cx, temp, script, fun, graph),
    cfgStack_(TempAllocPolicy(cx))
{
    pc = script->code;
    atoms = script->atomMap.vector;
}

static inline int32
GetJumpOffset(jsbytecode *pc)
{
    JSOp op = JSOp(*pc);
    JS_ASSERT(js_CodeSpec[op].type() == JOF_JUMP ||
              js_CodeSpec[op].type() == JOF_JUMPX);
    return (js_CodeSpec[op].type() == JOF_JUMP)
           ? GET_JUMP_OFFSET(pc)
           : GET_JUMPX_OFFSET(pc);
}

static inline jsbytecode *
GetNextPc(jsbytecode *pc)
{
    return pc + js_CodeSpec[JSOp(*pc)].length;
}

bool
BytecodeAnalyzer::analyze()
{
    current = newBlock(pc);
    if (!current)
        return false;

    // Initialize argument references if inside a function frame.
    if (fun()) {
        MParameter *param = MParameter::New(this, MParameter::CALLEE_SLOT);
        if (!current->add(param))
            return false;
        current->initSlot(calleeSlot(), param);

        param = MParameter::New(this, MParameter::THIS_SLOT);
        if (!current->add(param))
            return false;
        current->initSlot(thisSlot(), param);

        for (uint32 i = 0; i < nargs(); i++) {
            param = MParameter::New(this, int(i) - 2);
            if (!current->add(param))
                return false;
            current->initSlot(argSlot(i), param);
        }
    }

    // Initialize local variables.
    for (uint32 i = 0; i < nlocals(); i++) {
        MConstant *undef = MConstant::New(this, UndefinedValue());
        if (!undef)
            return false;
        current->initSlot(localSlot(i), undef);
    }

    if (!traverseBytecode())
        return false;

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
BytecodeAnalyzer::traverseBytecode()
{
    for (;;) {
        JS_ASSERT(pc < script->code + script->length);

        // Check if we've hit an expected join point or edge in the bytecode.
        if (!cfgStack_.empty() && cfgStack_.back().stopAt == pc) {
            ControlStatus status = processCfgStack();
            if (status == ControlStatus_Error)
                return false;
            if (!current)
                return true;
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
        ControlStatus status = snoopControlFlow(JSOp(*pc));
        if (status == ControlStatus_Error)
            return false;
        if (!current)
            return true;

        JSOp op = JSOp(*pc);
        if (!inspectOpcode(op))
            return false;

        pc += js_CodeSpec[op].length;
    }

    return true;
}

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::snoopControlFlow(JSOp op)
{
    switch (op) {
      case JSOP_RETURN:
      case JSOP_STOP:
        return processReturn(op);

      case JSOP_GOTO:
      case JSOP_GOTOX:
      {
        jssrcnote *sn = js_GetSrcNote(script, pc);
        switch (sn ? SN_TYPE(sn) : SRC_NULL) {
          case SRC_BREAK:
          case SRC_BREAK2LABEL:
            JS_NOT_REACHED("break NYI");
            return ControlStatus_Error;

          case SRC_CONTINUE:
          case SRC_CONT2LABEL:
            JS_NOT_REACHED("continue NYI");
            return ControlStatus_Error;

          case SRC_WHILE:
            // while (cond) { }
            if (!whileLoop(op, sn))
              return ControlStatus_Error;
            return ControlStatus_Jumped;

          case SRC_FOR:
            // for (init?; cond; update?) { }
            if (!forLoop(op, sn))
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

      case JSOP_IFNE:
      case JSOP_IFNEX:
        // We should never reach an IFNE, it's a stopAt point, which will
        // trigger closing the loop.
        JS_NOT_REACHED("we should never reach an ifne!");
        return ControlStatus_Error;

      default:
        break;
    }
    return ControlStatus_None;
}

BytecodeAnalyzer::CFGState
BytecodeAnalyzer::CFGState::If(jsbytecode *join, MBasicBlock *ifFalse)
{
    CFGState state;
    state.state = IF_TRUE;
    state.stopAt = join;
    state.branch.ifFalse = ifFalse;
    return state;
}

BytecodeAnalyzer::CFGState
BytecodeAnalyzer::CFGState::IfElse(jsbytecode *trueEnd, jsbytecode *falseEnd, MBasicBlock *ifFalse) 
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

BytecodeAnalyzer::CFGState
BytecodeAnalyzer::CFGState::DoWhile(jsbytecode *ifne, MBasicBlock *entry)
{
    CFGState state;
    state.state = DO_WHILE_LOOP;
    state.stopAt = ifne;
    state.loop.entry = entry;
    return state;
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
BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processControlEnd()
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
BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processCfgStack()
{
    ControlStatus status = processCfgEntry(cfgStack_.back());

    // If this terminated a CFG structure, act like processControlEnd() and
    // keep propagating upward.
    while (status == ControlStatus_Ended) {
        cfgStack_.popBack();
        if (cfgStack_.empty())
            return status;
        status = processCfgEntry(cfgStack_.back());
    }

    // If some join took place, the current structure is finished.
    if (status == ControlStatus_Joined)
        cfgStack_.popBack();

    return status;
}

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processCfgEntry(CFGState &state)
{
    switch (state.state) {
      case CFGState::IF_TRUE:
      case CFGState::IF_TRUE_EMPTY_ELSE:
        return processIfEnd(state);

      case CFGState::IF_ELSE_TRUE:
        return processIfElseTrueEnd(state);

      case CFGState::IF_ELSE_FALSE:
        return processIfElseFalseEnd(state);

      case CFGState::DO_WHILE_LOOP:
        return processDoWhileEnd(state);

      default:
        JS_NOT_REACHED("unknown cfgstate");
    }
    return ControlStatus_Error;
}

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processIfEnd(CFGState &state)
{
    if (!current)
        return ControlStatus_Ended;

    // Here, the false block is the join point. Create an edge from the
    // current block to the false block. Note that a RETURN opcode
    // could have already ended the block.
    MGoto *ins = MGoto::New(this, state.branch.ifFalse);
    if (!current->end(ins))
        return ControlStatus_Error;

    if (!current->addPredecessor(state.branch.ifFalse))
        return ControlStatus_Error;

    current = state.branch.ifFalse;
    return ControlStatus_Joined;
}

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processIfElseTrueEnd(CFGState &state)
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

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processIfElseFalseEnd(CFGState &state)
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
    MBasicBlock *join = newBlock(pred, pc);
    if (!join)
        return ControlStatus_Error;
  
    // Create edges from the true and false blocks as needed.
    MGoto *edge = MGoto::New(this, join);
    if (!pred->end(edge))
        return ControlStatus_Error;
    if (other) {
        edge = MGoto::New(this, join);
        if (!other->end(edge))
            return ControlStatus_Error;
        if (!join->addPredecessor(other))
            return ControlStatus_Error;
    }
  
    // Continue parsing at the join point.
    current = join;
    return ControlStatus_Joined;
}

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processDoWhileEnd(CFGState &state)
{
    if (!current)
        return ControlStatus_Ended;

    // First, balance the stack past the IFNE.
    MInstruction *ins = current->pop();

    // Create the successor block.
    MBasicBlock *successor = newBlock(current, pc);
    MTest *test = MTest::New(this, ins, state.loop.entry, successor);
    if (!current->end(test))
        return ControlStatus_Error;

    // Place phis at the loop header, now that the last instruction has
    // registered its operands, which might need rewriting.
    if (!state.loop.entry->addBackedge(current, successor))
        return ControlStatus_Error;

    JS_ASSERT(JSOp(*pc) == JSOP_IFNE || JSOp(*pc) == JSOP_IFNEX);
    pc += js_CodeSpec[JSOp(*pc)].length;

    current = successor;
    return ControlStatus_Joined;
}

uint32
BytecodeAnalyzer::readIndex(jsbytecode *pc)
{
    return (atoms - script->atomMap.vector) + GET_INDEX(pc);
}

bool
BytecodeAnalyzer::inspectOpcode(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return maybeLoop(op, js_GetSrcNote(script, pc));

      case JSOP_PUSH:
        return pushConstant(UndefinedValue());

      case JSOP_IFEQ:
        return jsop_ifeq(JSOP_IFEQ);

      case JSOP_BITAND:
        return jsop_binary(op);

      case JSOP_DOUBLE:
        return pushConstant(script->getConst(readIndex(pc)));

      case JSOP_STRING:
        return pushConstant(StringValue(atoms[GET_INDEX(pc)]));

      case JSOP_ZERO:
        return pushConstant(Int32Value(0));

      case JSOP_ONE:
        return pushConstant(Int32Value(1));

      case JSOP_NULL:
        return pushConstant(NullValue());

      case JSOP_FALSE:
        return pushConstant(BooleanValue(false));

      case JSOP_TRUE:
        return pushConstant(BooleanValue(true));

      case JSOP_POP:
        current->pop();
        return maybeLoop(op, js_GetSrcNote(script, pc));

      case JSOP_GETARG:
        current->pushArg(GET_SLOTNO(pc));
        return true;

      case JSOP_GETLOCAL:
        current->pushLocal(GET_SLOTNO(pc));
        return true;

      case JSOP_SETLOCAL:
        return current->setLocal(GET_SLOTNO(pc));

      case JSOP_IFEQX:
        return jsop_ifeq(JSOP_IFEQX);

      case JSOP_NULLBLOCKCHAIN:
        return true;

      case JSOP_INT8:
        return pushConstant(Int32Value(GET_INT8(pc)));

      case JSOP_TRACE:
        assertValidTraceOp(op);
        return true;

      default:
        return false;
    }
}

bool
BytecodeAnalyzer::pushConstant(const Value &v)
{
    MConstant *ins = MConstant::New(this, v);
    if (!current->add(ins))
        return false;
    current->push(ins);
    return true;
}

bool
BytecodeAnalyzer::maybeLoop(JSOp op, jssrcnote *sn)
{
    // This function looks at the opcode and source note and tries to
    // determine the structure of the loop. For some opcodes, like
    // POP/NOP which are not explicitly control flow, this source note is
    // optional. For opcodes with control flow, like GOTO, an unrecognized
    // or not-present source note is a compilation failure.
    switch (op) {
      case JSOP_POP:
        // for (init; ; update?) ...
        if (sn && SN_TYPE(sn) == SRC_FOR)
            return forLoop(op, sn);
        break;

      case JSOP_NOP:
        if (sn) {
            // do { } while (cond)
            if (SN_TYPE(sn) == SRC_WHILE)
                return doWhileLoop(op, sn);

            // for (; ; update?)
            if (SN_TYPE(sn) == SRC_FOR)
                return forLoop(op, sn);
        }
        break;

      default:
        JS_NOT_REACHED("unexpected opcode");
        return false;
    }

    return true;
}

void
BytecodeAnalyzer::assertValidTraceOp(JSOp op)
{
#ifdef DEBUG
    jssrcnote *sn = js_GetSrcNote(script, pc);
    jsbytecode *ifne = pc + js_GetSrcNoteOffset(sn, 0);
    CFGState &state = cfgStack_.back();

    // Make sure this is the next opcode after the loop header.
    JS_ASSERT(GetNextPc(state.loop.entry->pc()) == pc);

    jsbytecode *expected_ifne;
    switch (state.state) {
      case CFGState::DO_WHILE_LOOP:
        expected_ifne = state.stopAt;
        break;

      default:
        JS_NOT_REACHED("JSOP_TRACE appeared in unknown control flow construct");
        return;
    }

    // Make sure this trace op goes to the same ifne as the loop header's
    // source notes or GOTO.
    JS_ASSERT(ifne == expected_ifne);
#endif
}

bool
BytecodeAnalyzer::doWhileLoop(JSOp op, jssrcnote *sn)
{
    int offset = js_GetSrcNoteOffset(sn, 0);
    jsbytecode *ifne = pc + offset;
    JS_ASSERT(ifne > pc);

    // Verify that the IFNE goes back to a trace op.
    JS_ASSERT(JSOp(*GetNextPc(pc)) == JSOP_TRACE);
    JS_ASSERT(GetNextPc(pc) == ifne + GetJumpOffset(ifne));

    // do { } while() loops have the following structure:
    //    NOP         ; SRC_WHILE (offset to IFNE)
    //    TRACE       ; SRC_WHILE (offset to IFNE)
    //    ...
    //    IFNE ->     ; goes to TRACE
    MBasicBlock *header = newLoopHeader(current, pc);
    if (!header)
        return false;
    MGoto *ins = MGoto::New(this, header);
    if (!current->end(ins))
        return false;

    current = header;
    if (!cfgStack_.append(CFGState::DoWhile(ifne, header)))
        return false;

    return true;
}

bool
BytecodeAnalyzer::jsop_ifeq(JSOp op)
{
    // IFEQ always has a forward offset.
    jsbytecode *trueStart = pc + js_CodeSpec[op].length;
    jsbytecode *falseStart = pc + GetJumpOffset(pc);
    JS_ASSERT(falseStart > pc);

    // We only handle cases that emit source notes.
    jssrcnote *sn = js_GetSrcNote(script, pc);
    if (!sn) {
        // :FIXME: log this.
        return false;
    }

    MInstruction *ins = current->pop();

    // Create true and false branches.
    MBasicBlock *ifTrue = newBlock(current, trueStart);
    MBasicBlock *ifFalse = newBlock(current, falseStart);
    MTest *test = MTest::New(this, ins, ifTrue, ifFalse);
    if (!current->end(test))
        return false;

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
        JS_ASSERT(JSOp(*trueEnd) == JSOP_GOTO || JSOp(*trueEnd) == JSOP_GOTOX);
        JS_ASSERT(!js_GetSrcNote(script, trueEnd));

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

BytecodeAnalyzer::ControlStatus
BytecodeAnalyzer::processReturn(JSOp op)
{
    MInstruction *ins;
    switch (op) {
      case JSOP_RETURN:
        ins = current->pop();
        break;

      case JSOP_STOP:
        ins = MConstant::New(this, UndefinedValue());
        break;

      default:
        ins = NULL;
        JS_NOT_REACHED("unknown return op");
        break;
    }

    MReturn *ret = MReturn::New(this, ins);
    if (!current->end(ret))
        return ControlStatus_Error;

    // Make sure no one tries to use this block now.
    current = NULL;
    return processControlEnd();
}

bool
BytecodeAnalyzer::jsop_binary(JSOp op)
{
    MInstruction *right = current->pop();
    MInstruction *left = current->pop();

    MInstruction *ins;
    switch (op) {
      case JSOP_BITAND:
        ins = MBitAnd::New(this, left, right);
        break;

      default:
        JS_NOT_REACHED("unexpected binary opcode");
        return false;
    }

    if (!current->add(ins))
        return false;
    current->push(ins);

    return true;
}

MBasicBlock *
BytecodeAnalyzer::newBlock(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(this, predecessor, pc);
    if (!graph.addBlock(block))
        return NULL;
    return block;
}

MBasicBlock *
BytecodeAnalyzer::newLoopHeader(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::NewLoopHeader(this, predecessor, pc);
    if (!graph.addBlock(block))
        return NULL;
    return block;
}

