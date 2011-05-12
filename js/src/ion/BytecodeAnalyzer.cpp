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
#include "Ion.h"
#include "jsemit.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

bool
ion::Go(JSContext *cx, JSScript *script, StackFrame *fp)
{
    TempAllocator temp(&cx->tempPool);

    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;
    BytecodeAnalyzer analyzer(cx, script, fun, temp);

    if (!analyzer.analyze())
        return false;

    return false;
}

BytecodeAnalyzer::BytecodeAnalyzer(JSContext *cx, JSScript *script, JSFunction *fun, TempAllocator &temp)
  : MIRGenerator(cx, temp, script, fun),
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

    c1spew(stdout, "Built SSA");

    return false;
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
        JS_ASSERT(pc < script()->code + script()->length);

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
        jssrcnote *sn = js_GetSrcNote(script(), pc);
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
    return (atoms - script()->atomMap.vector) + GET_INDEX(pc);
}

bool
BytecodeAnalyzer::inspectOpcode(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return maybeLoop(op, js_GetSrcNote(script(), pc));

      case JSOP_PUSH:
        return pushConstant(UndefinedValue());

      case JSOP_IFEQ:
        return jsop_ifeq(JSOP_IFEQ);

      case JSOP_BITAND:
        return jsop_binary(op);

      case JSOP_DOUBLE:
        return pushConstant(script()->getConst(readIndex(pc)));

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
        return maybeLoop(op, js_GetSrcNote(script(), pc));

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
    jssrcnote *sn = js_GetSrcNote(script(), pc);
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
    jssrcnote *sn = js_GetSrcNote(script(), pc);
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
        JS_ASSERT(!js_GetSrcNote(script(), trueEnd));

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

bool
BytecodeAnalyzer::addBlock(MBasicBlock *block)
{
    if (!block)
        return false;
    block->setId(blocks_.length());
    return blocks_.append(block);
}

MBasicBlock *
BytecodeAnalyzer::newBlock(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::New(this, predecessor, pc);
    if (!addBlock(block))
        return NULL;
    return block;
}

MBasicBlock *
BytecodeAnalyzer::newLoopHeader(MBasicBlock *predecessor, jsbytecode *pc)
{
    MBasicBlock *block = MBasicBlock::NewLoopHeader(this, predecessor, pc);
    if (!addBlock(block))
        return NULL;
    return block;
}

MBasicBlock *
MBasicBlock::New(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc)
{
    MBasicBlock *block = new (gen->temp()) MBasicBlock(gen, entryPc);
    if (!block || !block->init())
        return NULL;

    if (pred && !block->inherit(pred))
        return NULL;

    return block;
}

MBasicBlock *
MBasicBlock::NewLoopHeader(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc)
{
    MBasicBlock *block = MBasicBlock::New(gen, pred, entryPc);
    if (!block || !block->initLoopHeader())
        return NULL;
    return block;
}

MBasicBlock::MBasicBlock(MIRGenerator *gen, jsbytecode *pc)
  : gen(gen),
    instructions_(TempAllocPolicy(gen->cx)),
    predecessors_(TempAllocPolicy(gen->cx)),
    phis_(TempAllocPolicy(gen->cx)),
    stackPosition_(gen->firstStackSlot()),
    lastIns_(NULL),
    pc_(pc),
    header_(NULL)
{
}

bool
MBasicBlock::init()
{
    slots_ = gen->allocate<StackSlot>(gen->nslots());
    if (!slots_)
        return false;
    return true;
}

bool
MBasicBlock::initLoopHeader()
{
    // Copy the initial definitions so we can place phi nodes at the back edge.
    header_ = gen->allocate<StackSlot>(stackPosition_);
    if (!header_)
        return false;
    for (uint32 i = 0; i < stackPosition_; i++)
        header_[i] = slots_[i];
    return true;
}

bool
MBasicBlock::inherit(MBasicBlock *pred)
{
    stackPosition_ = pred->stackPosition_;

    for (uint32 i = 0; i < stackPosition_; i++)
        slots_[i] = pred->slots_[i];

    if (!predecessors_.append(pred))
        return false;

    return true;
}

MInstruction *
MBasicBlock::getSlot(uint32 index)
{
    JS_ASSERT(index < stackPosition_);
    return slots_[index].ins;
}

void
MBasicBlock::initSlot(uint32 slot, MInstruction *ins)
{
    slots_[slot].set(ins);
}

void
MBasicBlock::setSlot(uint32 slot, MInstruction *ins)
{
    StackSlot &var = slots_[slot];

    // If |var| is copied, we must fix any of its copies so that they point to
    // a usable value.
    if (var.isCopied()) {
        // Find the lowest copy on the stack, to preserve the invariant that
        // the copy list starts near the top of the stack and proceeds
        // toward the bottom.
        uint32 lowest = var.firstCopy;
        uint32 prev = NotACopy;
        do {
            uint32 next = slots_[lowest].nextCopy;
            if (next == NotACopy)
                break;
            JS_ASSERT(next < lowest);
            prev = lowest;
            lowest = next;
        } while (true);

        // Rewrite every copy.
        for (uint32 copy = var.firstCopy; copy != lowest; copy = slots_[copy].nextCopy)
            slots_[copy].copyOf = lowest;

        // Make the lowest the new store.
        slots_[lowest].copyOf = NotACopy;
        slots_[lowest].firstCopy = prev;
    }

    var.set(ins);
}

// We must avoid losing copies when inserting phis. For example, the code
// on the left must not be naively rewritten as shown.
//   t = 1           ||   0: const(#1)   ||   0: const(#1)
//   i = t           \|   do {           \|   do {
//   do {             ->    2: add(0, 0)  ->   1: phi(0, 1)
//      i = i + t    /|                  /|    2: add(1, 1)
//   } ...           ||   } ...          ||   } ...
//
// Which is not correct. To solve this, we create a new SSA name at the point
// where |t| is assigned to |i|, like so:
//   t = 1          ||   0: const(#1)    ||   0: const(#1)
//   t' = copy(t)   ||   1: copy(0)      ||   1: copy(0)
//   i = t'         \|   do {            \|   do {
//   do {            ->    3: add(0, 1)   ->    2: phi(1, 3)
//      i = i + t   /|   } ...           /|     3: add(0, 2)
//   } ...          ||                   ||   } ...
//                  ||                   ||
// 
// We assume that the only way such copies can be created is via simple
// assignment, like (x = y), which will be reflected in the bytecode via
// a GET[LOCAL,ARG] that propagates into a SET[LOCAL,ARG]. Normal calls
// to push() will be compiler-created temporaries. So to minimize creation of
// new SSA names, we lazily create them when applying a setVariable() whose
// stack top was pushed by a pushVariable(). That also means we do not create
// "copies" for calls to push().
bool
MBasicBlock::setVariable(uint32 index)
{
    JS_ASSERT(stackPosition_ > gen->firstStackSlot());
    StackSlot &top = slots_[stackPosition_ - 1];

    MInstruction *ins = top.ins;
    if (top.isCopy()) {
        // Set the local variable to be a copy of |ins|. Note that unlike
        // JaegerMonkey, no complicated logic is needed to figure out how to
        // make |top| a copy of |var|. There is no need, because we only care
        // about (1) popping being fast, thus the backwarding ordering of
        // copies, and (2) knowing when a GET flows into a SET.
        ins = MCopy::New(gen, ins);
        if (!add(ins))
            return false;
    }

    setSlot(index, ins);

    if (!top.isCopy()) {
        // If the top is not a copy, we make it one anyway, in case the
        // bytecode ever emits something like:
        //    GETELEM
        //    SETLOCAL 0
        //    SETLOCAL 1
        //
        // In this case, we want the second assignment to act as though there
        // was an intervening POP; GETLOCAL. Note that |ins| is already
        // correct, because we only created a new instruction if |top.isCopy()|
        // was true.
        top.copyOf = index;
        top.nextCopy = slots_[index].firstCopy;
        slots_[index].firstCopy = stackPosition_ - 1;
    }

    return true;
}

bool
MBasicBlock::setArg(uint32 arg)
{
    // :TODO:  assert not closed
    return setVariable(gen->argSlot(arg));
}

bool
MBasicBlock::setLocal(uint32 local)
{
    // :TODO:  assert not closed
    return setVariable(gen->localSlot(local));
}

void
MBasicBlock::push(MInstruction *ins)
{
    JS_ASSERT(stackPosition_ < gen->nslots());
    slots_[stackPosition_].set(ins);
    stackPosition_++;
}

void
MBasicBlock::pushVariable(uint32 slot)
{
    if (slots_[slot].isCopy())
        slot = slots_[slot].copyOf;

    JS_ASSERT(stackPosition_ < gen->nslots());
    StackSlot &to = slots_[stackPosition_];
    StackSlot &from = slots_[slot];

    to.ins = from.ins;
    to.copyOf = slot;
    to.nextCopy = from.firstCopy;
    from.firstCopy = stackPosition_;

    stackPosition_++;
}

void
MBasicBlock::pushArg(uint32 arg)
{
    // :TODO:  assert not closed
    pushVariable(gen->argSlot(arg));
}

void
MBasicBlock::pushLocal(uint32 local)
{
    // :TODO:  assert not closed
    pushVariable(gen->localSlot(local));
}

MInstruction *
MBasicBlock::pop()
{
    JS_ASSERT(stackPosition_ > gen->firstStackSlot());

    StackSlot &slot = slots_[--stackPosition_];
    if (slot.isCopy()) {
        // The latest copy is at the top of the stack, and is the first copy
        // in the linked list. We only remove copies from the head of the list.
        StackSlot &backing = slots_[slot.copyOf];
        JS_ASSERT(backing.isCopied());
        JS_ASSERT(backing.firstCopy == stackPosition_);

        backing.firstCopy = slot.nextCopy;
    }

    // The slot cannot have live copies if it is being removed.
    JS_ASSERT(!slot.isCopied());

    return slot.ins;
}

MInstruction *
MBasicBlock::peek(int32 depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= gen->firstStackSlot());
    return getSlot(stackPosition_ + depth);
}

bool
MBasicBlock::add(MInstruction *ins)
{
    JS_ASSERT(!lastIns_);
    if (!ins)
        return false;
    ins->setBlock(this);
    return instructions_.append(ins);
}

bool
MBasicBlock::end(MControlInstruction *ins)
{
    if (!add(ins))
        return false;
    lastIns_ = ins;
    return true;
}

bool
MBasicBlock::addPhi(MPhi *phi)
{
    if (!phi || !phis_.append(phi))
        return false;
    phi->setBlock(this);
    return true;
}

bool
MBasicBlock::addPredecessor(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackPosition_ == stackPosition_);

    for (uint32 i = 0; i < stackPosition_; i++) {
        MInstruction *mine = getSlot(i);
        MInstruction *other = pred->getSlot(i);

        if (mine != other) {
            MPhi *phi;

            // If our tracked value for this slot is the same as our initial
            // predecessor's entry for that slot, then we have not yet created
            // a phi node. Otherwise, we definitely have.
            if (predecessors_[0]->getSlot(i) != mine) {
                // We already created a phi node.
                phi = mine->toPhi();
            } else {
                // Otherwise, create a new phi node.
                phi = MPhi::New(gen);
                if (!addPhi(phi) || !phi->addInput(gen, mine))
                    return false;

#ifdef DEBUG
                // Assert that each previous predecessor has the same value
                // for this slot, so we only have to add one input.
                for (size_t j = 0; j < predecessors_.length(); j++)
                    JS_ASSERT(predecessors_[j]->getSlot(i) == mine);
#endif

                setSlot(i, phi);
            }

            if (!phi->addInput(gen, other))
                return false;
        }
    }

    return predecessors_.append(pred);
}

void
MBasicBlock::assertUsesAreNotWithin(MOperand *use)
{
#ifdef DEBUG
    for (; use; use = use->next())
        JS_ASSERT(use->owner()->block()->id() < id());
#endif
}

bool
MBasicBlock::addBackedge(MBasicBlock *pred, MBasicBlock *successor)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(lastIns_);
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackPosition_ == stackPosition_);

    // Place minimal phi nodes by comparing the set of defintions at loop entry
    // with the loop exit. For each mismatching slot, we create a phi node, and
    // rewrite all uses of the entry definition to use the phi node instead.
    //
    // This algorithm would break in the face of copies, so we take care to
    // give every assignment its own unique SSA name. See
    // MBasicBlock::setVariable for more information.
    for (uint32 i = 0; i < stackPosition_; i++) {
        MInstruction *entryDef = header_[i].ins;
        MInstruction *exitDef = pred->slots_[i].ins;

        // If the entry definition and exit definition do not differ, then
        // no phi placement is necessary.
        if (entryDef == exitDef)
            continue;

        // Create a new phi. Do not add inputs yet, as we don't want to
        // accidentally rewrite the phi's operands.
        MPhi *phi = MPhi::New(gen);
        if (!addPhi(phi))
            return false;

        MOperand *use = entryDef->uses();
        MOperand *prev = NULL;
        while (use) {
            JS_ASSERT(use->ins() == entryDef);

            // Uses are initially sorted, with the head of the list being the
            // most recently inserted. This ordering is maintained while
            // placing phis.
            if (use->owner()->block()->id() < id()) {
                assertUsesAreNotWithin(use);
                break;
            }

            // Replace the instruction's use with the phi. Note that |prev|
            // does not change, and is really NULL since we always remove
            // from the head of the list,
            MOperand *next = use->next();
            use->owner()->replaceOperand(prev, use, phi); 
            use = next;
        }

#ifdef DEBUG
        // Assert that no slot after this one has the same entryDef. This would
        // imply that the SSA building process has accidentally allowed the
        // same SSA name to occupy two slots. Note, this is actually allowed
        // when the expression stack is non-empty, for example, (a + a) will
        // push two stack slots with the same SSA name as |a|. However, at loop
        // edges, the expression stack is empty, and thus we expect there to be
        // no copies.
        for (uint32 j = i + 1; j < stackPosition_; j++)
            JS_ASSERT(slots_[j].ins != entryDef);
#endif

        if (!phi->addInput(gen, entryDef) || !phi->addInput(gen, exitDef))
            return false;
        successor->setSlot(i, phi);
    }

    return predecessors_.append(pred);
}

