/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_bytecode_analyzer_h__
#define jsion_bytecode_analyzer_h__

// This file declares the data structures for building a MIRGraph from a
// JSScript.

#include "MIR.h"
#include "MIRGraph.h"

namespace js {
namespace ion {

class LIRGraph;

class IonBuilder : public MIRGenerator
{
    enum ControlStatus {
        ControlStatus_Error,
        ControlStatus_Ended,        // There is no continuation/join point.
        ControlStatus_Joined,       // Created a join node.
        ControlStatus_Jumped,       // Parsing another branch at the same level.
        ControlStatus_None          // No control flow.
    };

    struct DeferredEdge : public TempObject
    {
        MBasicBlock *block;
        DeferredEdge *next;

        DeferredEdge(MBasicBlock *block, DeferredEdge *next)
          : block(block), next(next)
        { }
    };

    struct ControlFlowInfo {
        // Entry in the cfgStack.
        uint32 cfgEntry;

        // Label that continues go to.
        jsbytecode *continuepc;

        ControlFlowInfo(uint32 cfgEntry, jsbytecode *continuepc)
          : cfgEntry(cfgEntry),
            continuepc(continuepc)
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
            DO_WHILE_LOOP_BODY, // do { x } while ()
            DO_WHILE_LOOP_COND, // do { } while (x)
            WHILE_LOOP_COND,    // while (x) { }
            WHILE_LOOP_BODY,    // while () { x }
            FOR_LOOP_COND,      // for (; x;) { }
            FOR_LOOP_BODY,      // for (; ;) { x }
            FOR_LOOP_UPDATE,    // for (; ; x) { }
            TABLE_SWITCH,       // switch() { x }
            LOOKUP_SWITCH,      // switch() { x }
            AND_OR              // && x, || x
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

                // Common exit point. Created lazily, so it may be NULL.
                MBasicBlock *successor;

                // Deferred break and continue targets.
                DeferredEdge *breaks;
                DeferredEdge *continues;

                // For-loops only.
                jsbytecode *condpc;
                jsbytecode *updatepc;
                jsbytecode *updateEnd;
            } loop;
            struct {
                // pc immediately after the switch.
                jsbytecode *exitpc;

                // Deferred break and continue targets.
                DeferredEdge *breaks;

                // MIR instruction
                MTableSwitch *ins;

                // The number of current successor that get mapped into a block. 
                uint32 currentBlock;

            } tableswitch;
            struct {
                // pc immediately after the switch.
                jsbytecode *exitpc;

                // Deferred break and continue targets.
                DeferredEdge *breaks;

                // Vector of body blocks to process
                FixedList<MBasicBlock *> *bodies;

                // The number of current successor that get mapped into a block. 
                uint32 currentBlock;
            } lookupswitch;
        };

        inline bool isLoop() const {
            switch (state) {
              case DO_WHILE_LOOP_COND:
              case DO_WHILE_LOOP_BODY:
              case WHILE_LOOP_COND:
              case WHILE_LOOP_BODY:
              case FOR_LOOP_COND:
              case FOR_LOOP_BODY:
              case FOR_LOOP_UPDATE:
                return true;
              default:
                return false;
            }
        }

        static CFGState If(jsbytecode *join, MBasicBlock *ifFalse);
        static CFGState IfElse(jsbytecode *trueEnd, jsbytecode *falseEnd, MBasicBlock *ifFalse);
        static CFGState AndOr(jsbytecode *join, MBasicBlock *joinStart);
        static CFGState TableSwitch(jsbytecode *exitpc, MTableSwitch *ins);
        static CFGState LookupSwitch(jsbytecode *exitpc);
    };

    static int CmpSuccessors(const void *a, const void *b);

  public:
    IonBuilder(JSContext *cx, TempAllocator *temp, MIRGraph *graph,
               TypeOracle *oracle, CompileInfo *info, size_t inliningDepth = 0, uint32 loopDepth = 0);

    bool build();
    bool buildInline(IonBuilder *callerBuilder, MResumePoint *callerResumePoint,
                     MDefinition *thisDefn, MDefinitionVector &args);

  private:
    bool traverseBytecode();
    ControlStatus snoopControlFlow(JSOp op);
    void markPhiBytecodeUses(jsbytecode *pc);
    bool processIterators();
    bool inspectOpcode(JSOp op);
    uint32 readIndex(jsbytecode *pc);
    JSAtom *readAtom(jsbytecode *pc);
    bool abort(const char *message, ...);
    void spew(const char *message);

    static bool inliningEnabled() {
        return js_IonOptions.inlining;
    }

    JSFunction *getSingleCallTarget(uint32 argc, jsbytecode *pc);
    unsigned getPolyCallTargets(uint32 argc, jsbytecode *pc,
                                AutoObjectVector &targets, uint32_t maxTargets);
    bool canInlineTarget(JSFunction *target);

    void popCfgStack();
    bool processDeferredContinues(CFGState &state);
    ControlStatus processControlEnd();
    ControlStatus processCfgStack();
    ControlStatus processCfgEntry(CFGState &state);
    ControlStatus processIfEnd(CFGState &state);
    ControlStatus processIfElseTrueEnd(CFGState &state);
    ControlStatus processIfElseFalseEnd(CFGState &state);
    ControlStatus processDoWhileBodyEnd(CFGState &state);
    ControlStatus processDoWhileCondEnd(CFGState &state);
    ControlStatus processWhileCondEnd(CFGState &state);
    ControlStatus processWhileBodyEnd(CFGState &state);
    ControlStatus processForCondEnd(CFGState &state);
    ControlStatus processForBodyEnd(CFGState &state);
    ControlStatus processForUpdateEnd(CFGState &state);
    ControlStatus processNextTableSwitchCase(CFGState &state);
    ControlStatus processTableSwitchEnd(CFGState &state);
    ControlStatus processNextLookupSwitchCase(CFGState &state);
    ControlStatus processLookupSwitchEnd(CFGState &state);
    ControlStatus processAndOrEnd(CFGState &state);
    ControlStatus processSwitchBreak(JSOp op, jssrcnote *sn);
    ControlStatus processReturn(JSOp op);
    ControlStatus processThrow();
    ControlStatus processContinue(JSOp op, jssrcnote *sn);
    ControlStatus processBreak(JSOp op, jssrcnote *sn);
    ControlStatus maybeLoop(JSOp op, jssrcnote *sn);
    bool pushLoop(CFGState::State state, jsbytecode *stopAt, MBasicBlock *entry,
                  jsbytecode *bodyStart, jsbytecode *bodyEnd, jsbytecode *exitpc,
                  jsbytecode *continuepc = NULL);

    MBasicBlock *addBlock(MBasicBlock *block, uint32 loopDepth);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc, uint32 loopDepth);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc, MResumePoint *priorResumePoint);
    MBasicBlock *newBlockAfter(MBasicBlock *at, MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newOsrPreheader(MBasicBlock *header, jsbytecode *loopEntry);
    MBasicBlock *newPendingLoopHeader(MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newBlock(jsbytecode *pc) {
        return newBlock(NULL, pc);
    }
    MBasicBlock *newBlockAfter(MBasicBlock *at, jsbytecode *pc) {
        return newBlockAfter(at, NULL, pc);
    }

    // Given a list of pending breaks, creates a new block and inserts a Goto
    // linking each break to the new block.
    MBasicBlock *createBreakCatchBlock(DeferredEdge *edge, jsbytecode *pc);

    // Finishes loops that do not actually loop, containing only breaks or
    // returns.
    ControlStatus processBrokenLoop(CFGState &state);

    // Computes loop phis, places them in all successors of a loop, then
    // handles any pending breaks.
    ControlStatus finishLoop(CFGState &state, MBasicBlock *successor);

    void assertValidLoopHeadOp(jsbytecode *pc);

    ControlStatus forLoop(JSOp op, jssrcnote *sn);
    ControlStatus whileOrForInLoop(JSOp op, jssrcnote *sn);
    ControlStatus doWhileLoop(JSOp op, jssrcnote *sn);
    ControlStatus tableSwitch(JSOp op, jssrcnote *sn);
    ControlStatus lookupSwitch(JSOp op, jssrcnote *sn);

    // Please see the Big Honkin' Comment about how resume points work in
    // IonBuilder.cpp, near the definition for this function.
    bool resume(MInstruction *ins, jsbytecode *pc, MResumePoint::Mode mode);
    bool resumeAt(MInstruction *ins, jsbytecode *pc);
    bool resumeAfter(MInstruction *ins);

    void insertRecompileCheck();

    bool initParameters();
    void rewriteParameters();
    bool initScopeChain();
    bool pushConstant(const Value &v);
    bool pushTypeBarrier(MInstruction *ins, types::StackTypeSet *actual, types::StackTypeSet *observed);
    void monitorResult(MInstruction *ins, types::TypeSet *barrier, types::TypeSet *types);

    JSObject *getSingletonPrototype(JSFunction *target);

    MDefinition *createThisNative();
    MDefinition *createThisScripted(MDefinition *callee);
    MDefinition *createThisScriptedSingleton(HandleFunction target, HandleObject proto, MDefinition *callee);
    MDefinition *createThis(HandleFunction target, MDefinition *callee);
    MInstruction *createCallObject(MDefinition *callee, MDefinition *scopeObj);

    bool makeCall(HandleFunction target, uint32 argc, bool constructing);

    MDefinition *walkScopeChain(unsigned hops);

    MInstruction *addBoundsCheck(MDefinition *index, MDefinition *length);

    JSObject *getNewArrayTemplateObject(uint32 count);

    bool invalidatedIdempotentCache();

    bool loadSlot(MDefinition *obj, Shape *shape, MIRType rvalType);
    bool storeSlot(MDefinition *obj, Shape *shape, MDefinition *value, bool needsBarrier);

    // jsop_getprop() helpers.
    bool getPropTryArgumentsLength(bool *emitted);
    bool getPropTryConstant(bool *emitted, MDefinition *obj, HandleId id,
                            types::StackTypeSet *barrier, types::StackTypeSet *types,
                            TypeOracle::UnaryTypes unaryTypes);
    bool getPropTryDefiniteSlot(bool *emitted, MDefinition *obj, HandlePropertyName name,
                            types::StackTypeSet *barrier, types::StackTypeSet *types,
                            TypeOracle::Unary unary, TypeOracle::UnaryTypes unaryTypes);
    bool getPropTryCommonGetter(bool *emitted, MDefinition *obj, HandleId id,
                            types::StackTypeSet *barrier, types::StackTypeSet *types,
                            TypeOracle::UnaryTypes unaryTypes);
    bool getPropTryMonomorphic(bool *emitted, MDefinition *obj, HandleId id,
                            types::StackTypeSet *barrier, TypeOracle::Unary unary,
                            TypeOracle::UnaryTypes unaryTypes);
    bool getPropTryPolymorphic(bool *emitted, MDefinition *obj,
                               HandlePropertyName name, HandleId id,
                               types::StackTypeSet *barrier, types::StackTypeSet *types,
                               TypeOracle::Unary unary, TypeOracle::UnaryTypes unaryTypes);

    bool jsop_add(MDefinition *left, MDefinition *right);
    bool jsop_bitnot();
    bool jsop_bitop(JSOp op);
    bool jsop_binary(JSOp op);
    bool jsop_binary(JSOp op, MDefinition *left, MDefinition *right);
    bool jsop_pos();
    bool jsop_neg();
    bool jsop_defvar(uint32 index);
    bool jsop_notearg();
    bool jsop_funcall(uint32 argc);
    bool jsop_funapply(uint32 argc);
    bool jsop_call(uint32 argc, bool constructing);
    bool jsop_ifeq(JSOp op);
    bool jsop_andor(JSOp op);
    bool jsop_dup2();
    bool jsop_loophead(jsbytecode *pc);
    bool jsop_incslot(JSOp op, uint32 slot);
    bool jsop_localinc(JSOp op);
    bool jsop_arginc(JSOp op);
    bool jsop_compare(JSOp op);
    bool jsop_getgname(HandlePropertyName name);
    bool jsop_setgname(HandlePropertyName name);
    bool jsop_getname(HandlePropertyName name);
    bool jsop_bindname(PropertyName *name);
    bool jsop_getelem();
    bool jsop_getelem_dense();
    bool jsop_getelem_typed(int arrayType);
    bool jsop_getelem_string();
    bool jsop_setelem();
    bool jsop_setelem_dense();
    bool jsop_setelem_typed(int arrayType);
    bool jsop_length();
    bool jsop_length_fastPath();
    bool jsop_arguments();
    bool jsop_arguments_length();
    bool jsop_arguments_getelem();
    bool jsop_arguments_setelem();
    bool jsop_not();
    bool jsop_getprop(HandlePropertyName name);
    bool jsop_setprop(HandlePropertyName name);
    bool jsop_delprop(HandlePropertyName name);
    bool jsop_newarray(uint32 count);
    bool jsop_newobject(HandleObject baseObj);
    bool jsop_initelem();
    bool jsop_initelem_dense();
    bool jsop_initprop(HandlePropertyName name);
    bool jsop_regexp(RegExpObject *reobj);
    bool jsop_object(JSObject *obj);
    bool jsop_lambda(JSFunction *fun);
    bool jsop_deflocalfun(uint32 local, JSFunction *fun);
    bool jsop_this();
    bool jsop_typeof();
    bool jsop_toid();
    bool jsop_iter(uint8 flags);
    bool jsop_iternext();
    bool jsop_itermore();
    bool jsop_iterend();
    bool jsop_instanceof();
    bool jsop_getaliasedvar(ScopeCoordinate sc);
    bool jsop_setaliasedvar(ScopeCoordinate sc);

    /* Inlining. */

    enum InliningStatus
    {
        InliningStatus_Error,
        InliningStatus_NotInlined,
        InliningStatus_Inlined
    };

    // Inlining helpers.
    bool discardCallArgs(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb);
    bool discardCall(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb);
    types::StackTypeSet *getInlineReturnTypeSet();
    MIRType getInlineReturnType();
    types::StackTypeSet *getInlineArgTypeSet(uint32 argc, uint32 arg);
    MIRType getInlineArgType(uint32 argc, uint32 arg);

    // Array natives.
    InliningStatus inlineArray(uint32 argc, bool constructing);
    InliningStatus inlineArrayPopShift(MArrayPopShift::Mode mode, uint32 argc, bool constructing);
    InliningStatus inlineArrayPush(uint32 argc, bool constructing);
    InliningStatus inlineArrayConcat(uint32 argc, bool constructing);

    // Math natives.
    InliningStatus inlineMathAbs(uint32 argc, bool constructing);
    InliningStatus inlineMathFloor(uint32 argc, bool constructing);
    InliningStatus inlineMathRound(uint32 argc, bool constructing);
    InliningStatus inlineMathSqrt(uint32 argc, bool constructing);
    InliningStatus inlineMathMinMax(bool max, uint32 argc, bool constructing);
    InliningStatus inlineMathPow(uint32 argc, bool constructing);
    InliningStatus inlineMathRandom(uint32 argc, bool constructing);
    InliningStatus inlineMathFunction(MMathFunction::Function function, uint32 argc,
                                      bool constructing);

    // String natives.
    InliningStatus inlineStringObject(uint32 argc, bool constructing);
    InliningStatus inlineStrCharCodeAt(uint32 argc, bool constructing);
    InliningStatus inlineStrFromCharCode(uint32 argc, bool constructing);
    InliningStatus inlineStrCharAt(uint32 argc, bool constructing);

    // RegExp natives.
    InliningStatus inlineRegExpTest(uint32 argc, bool constructing);

    InliningStatus inlineNativeCall(JSNative native, uint32 argc, bool constructing);

    bool jsop_call_inline(HandleFunction callee, uint32 argc, bool constructing,
                          MConstant *constFun, MBasicBlock *bottom,
                          Vector<MDefinition *, 8, IonAllocPolicy> &retvalDefns);
    bool inlineScriptedCall(AutoObjectVector &targets, uint32 argc, bool constructing,
                            types::StackTypeSet *types, types::StackTypeSet *barrier);
    bool makeInliningDecision(AutoObjectVector &targets, uint32 argc);

    MCall *makeCallHelper(HandleFunction target, uint32 argc, bool constructing);
    bool makeCallBarrier(HandleFunction target, uint32 argc, bool constructing,
                         types::StackTypeSet *types, types::StackTypeSet *barrier);

    inline bool TestCommonPropFunc(JSContext *cx, types::StackTypeSet *types,
                                   HandleId id, JSFunction **funcp,
                                   bool isGetter, bool *isDOM);

    bool annotateGetPropertyCache(JSContext *cx, MDefinition *obj, MGetPropertyCache *getPropCache,
                                  types::StackTypeSet *objTypes, types::StackTypeSet *pushedTypes);

    MGetPropertyCache *checkInlineableGetPropertyCache(uint32_t argc);

    MPolyInlineDispatch *
    makePolyInlineDispatch(JSContext *cx, AutoObjectVector &targets, int argc,
                           MGetPropertyCache *getPropCache,
                           types::StackTypeSet *types, types::StackTypeSet *barrier,
                           MBasicBlock *bottom,
                           Vector<MDefinition *, 8, IonAllocPolicy> &retvalDefns);

    // A builder is inextricably tied to a particular script.
    HeapPtrScript script_;

  public:
    // Compilation index for this attempt.
    types::RecompileInfo const recompileInfo;

    // If off thread compilation is successful, final LIR is attached here.
    LIRGraph *lir;

    void clearForBackEnd();

    Return<JSScript*> script() const { return script_; }

  private:
    JSContext *cx;

    jsbytecode *pc;
    MBasicBlock *current;
    uint32 loopDepth_;

    /* Information used for inline-call builders. */
    MResumePoint *callerResumePoint_;
    jsbytecode *callerPC() {
        return callerResumePoint_ ? callerResumePoint_->pc() : NULL;
    }
    IonBuilder *callerBuilder_;

    Vector<CFGState, 8, IonAllocPolicy> cfgStack_;
    Vector<ControlFlowInfo, 4, IonAllocPolicy> loops_;
    Vector<ControlFlowInfo, 0, IonAllocPolicy> switches_;
    Vector<MInstruction *, 2, IonAllocPolicy> iterators_;
    TypeOracle *oracle;
    size_t inliningDepth;

    // True if script->failedBoundsCheck is set for the current script or
    // an outer script.
    bool failedBoundsCheck_;

    // If this script can use a lazy arguments object, it wil be pre-created
    // here.
    MInstruction *lazyArguments_;
};

} // namespace ion
} // namespace js

#endif // jsion_bytecode_analyzer_h__

