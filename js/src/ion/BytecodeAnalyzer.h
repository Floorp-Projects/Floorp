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

#ifndef jsion_bytecode_analyzer_h__
#define jsion_bytecode_analyzer_h__

#include "MIR.h"

namespace js {
namespace ion {

class BytecodeAnalyzer : public MIRGenerator
{
    enum ControlStatus {
        ControlStatus_Error,
        ControlStatus_Ended,        // There is no continuation/join point.
        ControlStatus_Joined,       // Created a join node.
        ControlStatus_Jumped,       // Parsing another branch at the same level.
        ControlStatus_None          // No control flow.
    };

    struct LoopInfo {
        uint32 cfgEntry;

        LoopInfo(uint32 cfgEntry)
          : cfgEntry(cfgEntry)
        { }
    };

    // To avoid recursion, the bytecode analyzer uses a stack where each entry
    // is a small state machine. As we encounter branches or jumps in the
    // bytecode, we push information about the edges on the stack so that the
    // CFG can be built in a tree-like fashion.
    struct CFGState {
        enum State {
            IF_TRUE,            // if() { }, no else.
            IF_TRUE_EMPTY_ELSE, // if() { }, empty else
            IF_ELSE_TRUE,       // if() { X } else { }
            IF_ELSE_FALSE,      // if() { } else { X }
            DO_WHILE_LOOP,      // do (x) while (x)
            WHILE_LOOP_COND,    // while (x) { }
            WHILE_LOOP_BODY     // while () { x }
        };

        State state;            // Current state of this control structure.
        jsbytecode *stopAt;     // Bytecode at which to stop the processing loop.
        
        // For if structures, this contains branch information.
        union {
            struct {
                MBasicBlock *ifFalse;
                jsbytecode *falseEnd;
                MBasicBlock *ifTrue;    // Set when the end of the true path is reached.
            } branch;
            struct {
                // Common entry point.
                MBasicBlock *entry;

                // Position of where the loop body starts and ends.
                jsbytecode *bodyStart;
                jsbytecode *bodyEnd;

                // pc immediately after the loop exits.
                jsbytecode *exitpc;

                // Common continue point. Created lazily, so it may be NULL.
                MBasicBlock *repeat;

                // Common break point. Created lazily, so it may be NULL.
                MBasicBlock *exit;

                // Common exit point. Created lazily, so it may be NULL.
                MBasicBlock *successor;
            } loop;
        };

        inline bool isLoop() const {
            switch (state) {
              case DO_WHILE_LOOP:
              case WHILE_LOOP_COND:
              case WHILE_LOOP_BODY:
                return true;
              default:
                return false;
            }
        }

        static CFGState If(jsbytecode *join, MBasicBlock *ifFalse);
        static CFGState IfElse(jsbytecode *trueEnd, jsbytecode *falseEnd, MBasicBlock *ifFalse);
    };

  public:
    BytecodeAnalyzer(JSContext *cx, JSScript *script, JSFunction *fun, TempAllocator &temp,
                     MIRGraph &graph);

  public:
    bool analyze();

  private:
    bool traverseBytecode();
    ControlStatus snoopControlFlow(JSOp op);
    bool inspectOpcode(JSOp op);
    uint32 readIndex(jsbytecode *pc);

    void popCfgStack();
    CFGState &findInnermostLoop();
    ControlStatus processControlEnd();
    ControlStatus processCfgStack();
    ControlStatus processCfgEntry(CFGState &state);
    ControlStatus processIfEnd(CFGState &state);
    ControlStatus processIfElseTrueEnd(CFGState &state);
    ControlStatus processIfElseFalseEnd(CFGState &state);
    ControlStatus processDoWhileEnd(CFGState &state);
    ControlStatus processWhileCondEnd(CFGState &state);
    ControlStatus processWhileBodyEnd(CFGState &state);
    ControlStatus processReturn(JSOp op);
    ControlStatus simpleContinue(JSOp op, jssrcnote *sn);
    ControlStatus simpleBreak(JSOp op, jssrcnote *sn);
    bool pushLoop(CFGState::State state, jsbytecode *stopAt, MBasicBlock *entry,
                  jsbytecode *bodyStart, jsbytecode *bodyEnd, jsbytecode *exitpc);

    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newLoopHeader(MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newBlock(jsbytecode *pc) {
        return newBlock(NULL, pc);
    }

    void assertValidTraceOp(JSOp op);
    bool maybeLoop(JSOp op, jssrcnote *sn);
    bool jsop_ifeq(JSOp op);

    bool forLoop(JSOp op, jssrcnote *sn) {
        return false;
    }
    bool forInLoop(JSOp op, jssrcnote *sn) {
        return false;
    }
    bool whileLoop(JSOp op, jssrcnote *sn);
    bool doWhileLoop(JSOp op, jssrcnote *sn);

    bool pushConstant(const Value &v);
    bool jsop_binary(JSOp op);

  private:
    JSAtom **atoms;
    MBasicBlock *current;
    Vector<CFGState, 8, TempAllocPolicy> cfgStack_;
    Vector<LoopInfo, 4, TempAllocPolicy> loops_;
};

} // namespace ion
} // namespace js

#endif // jsion_bytecode_analyzer_h__

