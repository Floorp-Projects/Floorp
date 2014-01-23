/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonBuilder_h
#define jit_IonBuilder_h

#ifdef JS_ION

// This file declares the data structures for building a MIRGraph from a
// JSScript.

#include "jit/BytecodeAnalysis.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/TypeRepresentationSet.h"

namespace js {
namespace jit {

class CodeGenerator;
class CallInfo;
class BaselineInspector;
class BaselineFrameInspector;

// Records information about a baseline frame for compilation that is stable
// when later used off thread.
BaselineFrameInspector *
NewBaselineFrameInspector(TempAllocator *temp, BaselineFrame *frame);

class IonBuilder : public MIRGenerator
{
    enum ControlStatus {
        ControlStatus_Error,
        ControlStatus_Abort,
        ControlStatus_Ended,        // There is no continuation/join point.
        ControlStatus_Joined,       // Created a join node.
        ControlStatus_Jumped,       // Parsing another branch at the same level.
        ControlStatus_None          // No control flow.
    };

    enum SetElemSafety {
        // Normal write like a[b] = c.
        SetElem_Normal,

        // Write due to UnsafePutElements:
        // - assumed to be in bounds,
        // - not checked for data races
        SetElem_Unsafe,
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
        uint32_t cfgEntry;

        // Label that continues go to.
        jsbytecode *continuepc;

        ControlFlowInfo(uint32_t cfgEntry, jsbytecode *continuepc)
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
            COND_SWITCH_CASE,   // switch() { case X: ... }
            COND_SWITCH_BODY,   // switch() { case ...: X }
            AND_OR,             // && x, || x
            LABEL,              // label: x
            TRY                 // try { x } catch(e) { }
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

                // Whether OSR is being performed for this loop.
                bool osr;

                // Position of where the loop body starts and ends.
                jsbytecode *bodyStart;
                jsbytecode *bodyEnd;

                // pc immediately after the loop exits.
                jsbytecode *exitpc;

                // pc for 'continue' jumps.
                jsbytecode *continuepc;

                // Common exit point. Created lazily, so it may be nullptr.
                MBasicBlock *successor;

                // Deferred break and continue targets.
                DeferredEdge *breaks;
                DeferredEdge *continues;

                // Initial state, in case loop processing is restarted.
                State initialState;
                jsbytecode *initialPc;
                jsbytecode *initialStopAt;
                jsbytecode *loopHead;

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
                uint32_t currentBlock;

            } tableswitch;
            struct {
                // Vector of body blocks to process after the cases.
                FixedList<MBasicBlock *> *bodies;

                // When processing case statements, this counter points at the
                // last uninitialized body.  When processing bodies, this
                // counter targets the next body to process.
                uint32_t currentIdx;

                // Remember the block index of the default case.
                jsbytecode *defaultTarget;
                uint32_t defaultIdx;

                // Block immediately after the switch.
                jsbytecode *exitpc;
                DeferredEdge *breaks;
            } condswitch;
            struct {
                DeferredEdge *breaks;
            } label;
            struct {
                MBasicBlock *successor;
            } try_;
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
        static CFGState CondSwitch(IonBuilder *builder, jsbytecode *exitpc, jsbytecode *defaultTarget);
        static CFGState Label(jsbytecode *exitpc);
        static CFGState Try(jsbytecode *exitpc, MBasicBlock *successor);
    };

    static int CmpSuccessors(const void *a, const void *b);

  public:
    IonBuilder(JSContext *analysisContext, CompileCompartment *comp,
               const JitCompileOptions &options, TempAllocator *temp,
               MIRGraph *graph, types::CompilerConstraintList *constraints,
               BaselineInspector *inspector, CompileInfo *info,
               const OptimizationInfo *optimizationInfo, BaselineFrameInspector *baselineFrame,
               size_t inliningDepth = 0, uint32_t loopDepth = 0);

    bool build();
    bool buildInline(IonBuilder *callerBuilder, MResumePoint *callerResumePoint,
                     CallInfo &callInfo);

  private:
    bool traverseBytecode();
    ControlStatus snoopControlFlow(JSOp op);
    bool processIterators();
    bool inspectOpcode(JSOp op);
    uint32_t readIndex(jsbytecode *pc);
    JSAtom *readAtom(jsbytecode *pc);
    bool abort(const char *message, ...);
    void spew(const char *message);

    JSFunction *getSingleCallTarget(types::TemporaryTypeSet *calleeTypes);
    bool getPolyCallTargets(types::TemporaryTypeSet *calleeTypes, bool constructing,
                            ObjectVector &targets, uint32_t maxTargets, bool *gotLambda);

    void popCfgStack();
    DeferredEdge *filterDeadDeferredEdges(DeferredEdge *edge);
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
    ControlStatus processCondSwitchCase(CFGState &state);
    ControlStatus processCondSwitchBody(CFGState &state);
    ControlStatus processSwitchBreak(JSOp op);
    ControlStatus processSwitchEnd(DeferredEdge *breaks, jsbytecode *exitpc);
    ControlStatus processAndOrEnd(CFGState &state);
    ControlStatus processLabelEnd(CFGState &state);
    ControlStatus processTryEnd(CFGState &state);
    ControlStatus processReturn(JSOp op);
    ControlStatus processThrow();
    ControlStatus processContinue(JSOp op);
    ControlStatus processBreak(JSOp op, jssrcnote *sn);
    ControlStatus maybeLoop(JSOp op, jssrcnote *sn);
    bool pushLoop(CFGState::State state, jsbytecode *stopAt, MBasicBlock *entry, bool osr,
                  jsbytecode *loopHead, jsbytecode *initialPc,
                  jsbytecode *bodyStart, jsbytecode *bodyEnd, jsbytecode *exitpc,
                  jsbytecode *continuepc = nullptr);
    void analyzeNewLoopTypes(MBasicBlock *entry, jsbytecode *start, jsbytecode *end);

    MBasicBlock *addBlock(MBasicBlock *block, uint32_t loopDepth);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc, uint32_t loopDepth);
    MBasicBlock *newBlock(MBasicBlock *predecessor, jsbytecode *pc, MResumePoint *priorResumePoint);
    MBasicBlock *newBlockPopN(MBasicBlock *predecessor, jsbytecode *pc, uint32_t popped);
    MBasicBlock *newBlockAfter(MBasicBlock *at, MBasicBlock *predecessor, jsbytecode *pc);
    MBasicBlock *newOsrPreheader(MBasicBlock *header, jsbytecode *loopEntry);
    MBasicBlock *newPendingLoopHeader(MBasicBlock *predecessor, jsbytecode *pc, bool osr);
    MBasicBlock *newBlock(jsbytecode *pc) {
        return newBlock(nullptr, pc);
    }
    MBasicBlock *newBlockAfter(MBasicBlock *at, jsbytecode *pc) {
        return newBlockAfter(at, nullptr, pc);
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

    // Incorporates a type/typeSet into an OSR value for a loop, after the loop
    // body has been processed.
    bool addOsrValueTypeBarrier(uint32_t slot, MInstruction **def,
                                MIRType type, types::TemporaryTypeSet *typeSet);
    bool maybeAddOsrTypeBarriers();

    // Restarts processing of a loop if the type information at its header was
    // incomplete.
    ControlStatus restartLoop(CFGState state);

    void assertValidLoopHeadOp(jsbytecode *pc);

    ControlStatus forLoop(JSOp op, jssrcnote *sn);
    ControlStatus whileOrForInLoop(jssrcnote *sn);
    ControlStatus doWhileLoop(JSOp op, jssrcnote *sn);
    ControlStatus tableSwitch(JSOp op, jssrcnote *sn);
    ControlStatus condSwitch(JSOp op, jssrcnote *sn);

    // Please see the Big Honkin' Comment about how resume points work in
    // IonBuilder.cpp, near the definition for this function.
    bool resume(MInstruction *ins, jsbytecode *pc, MResumePoint::Mode mode);
    bool resumeAt(MInstruction *ins, jsbytecode *pc);
    bool resumeAfter(MInstruction *ins);
    bool maybeInsertResume();

    void insertRecompileCheck();

    bool initParameters();
    void rewriteParameter(uint32_t slotIdx, MDefinition *param, int32_t argIndex);
    void rewriteParameters();
    bool initScopeChain(MDefinition *callee = nullptr);
    bool initArgumentsObject();
    bool pushConstant(const Value &v);

    MConstant *constant(const Value &v);
    MConstant *constantInt(int32_t i);

    // Add a guard which ensure that the set of type which goes through this
    // generated code correspond to the observed types for the bytecode.
    bool pushTypeBarrier(MDefinition *def, types::TemporaryTypeSet *observed, bool needBarrier);

    // As pushTypeBarrier, but will compute the needBarrier boolean itself based
    // on observed and the JSFunction that we're planning to call. The
    // JSFunction must be a DOM method or getter.
    bool pushDOMTypeBarrier(MInstruction *ins, types::TemporaryTypeSet *observed, JSFunction* func);

    // If definiteType is not known or def already has the right type, just
    // returns def.  Otherwise, returns an MInstruction that has that definite
    // type, infallibly unboxing ins as needed.  The new instruction will be
    // added to |current| in this case.
    MDefinition *ensureDefiniteType(MDefinition* def, JSValueType definiteType);

    JSObject *getSingletonPrototype(JSFunction *target);

    MDefinition *createThisScripted(MDefinition *callee);
    MDefinition *createThisScriptedSingleton(JSFunction *target, MDefinition *callee);
    MDefinition *createThis(JSFunction *target, MDefinition *callee);
    MInstruction *createDeclEnvObject(MDefinition *callee, MDefinition *scopeObj);
    MInstruction *createCallObject(MDefinition *callee, MDefinition *scopeObj);

    MDefinition *walkScopeChain(unsigned hops);

    MInstruction *addConvertElementsToDoubles(MDefinition *elements);
    MInstruction *addBoundsCheck(MDefinition *index, MDefinition *length);
    MInstruction *addShapeGuard(MDefinition *obj, Shape *const shape, BailoutKind bailoutKind);

    MDefinition *convertShiftToMaskForStaticTypedArray(MDefinition *id,
                                                       ArrayBufferView::ViewType viewType);

    bool invalidatedIdempotentCache();

    bool hasStaticScopeObject(ScopeCoordinate sc, JSObject **pcall);
    bool loadSlot(MDefinition *obj, size_t slot, size_t nfixed, MIRType rvalType,
                  bool barrier, types::TemporaryTypeSet *types);
    bool loadSlot(MDefinition *obj, Shape *shape, MIRType rvalType,
                  bool barrier, types::TemporaryTypeSet *types);
    bool storeSlot(MDefinition *obj, size_t slot, size_t nfixed,
                   MDefinition *value, bool needsBarrier,
                   MIRType slotType = MIRType_None);
    bool storeSlot(MDefinition *obj, Shape *shape, MDefinition *value, bool needsBarrier,
                   MIRType slotType = MIRType_None);

    // jsop_getprop() helpers.
    bool getPropTryArgumentsLength(bool *emitted);
    bool getPropTryConstant(bool *emitted, PropertyName *name,
                            types::TemporaryTypeSet *types);
    bool getPropTryDefiniteSlot(bool *emitted, PropertyName *name,
                                bool barrier, types::TemporaryTypeSet *types);
    bool getPropTryCommonGetter(bool *emitted, PropertyName *name,
                                types::TemporaryTypeSet *types);
    bool getPropTryInlineAccess(bool *emitted, PropertyName *name,
                                bool barrier, types::TemporaryTypeSet *types);
    bool getPropTryTypedObject(bool *emitted, PropertyName *name,
                               types::TemporaryTypeSet *resultTypes);
    bool getPropTryScalarPropOfTypedObject(bool *emitted,
                                           int32_t fieldOffset,
                                           TypeRepresentationSet fieldTypeReprs,
                                           types::TemporaryTypeSet *resultTypes);
    bool getPropTryComplexPropOfTypedObject(bool *emitted,
                                            int32_t fieldOffset,
                                            TypeRepresentationSet fieldTypeReprs,
                                            size_t fieldIndex,
                                            types::TemporaryTypeSet *resultTypes);
    bool getPropTryCache(bool *emitted, PropertyName *name,
                         bool barrier, types::TemporaryTypeSet *types);
    bool needsToMonitorMissingProperties(types::TemporaryTypeSet *types);

    // jsop_setprop() helpers.
    bool setPropTryCommonSetter(bool *emitted, MDefinition *obj,
                                PropertyName *name, MDefinition *value);
    bool setPropTryCommonDOMSetter(bool *emitted, MDefinition *obj,
                                   MDefinition *value, JSFunction *setter,
                                   bool isDOM);
    bool setPropTryDefiniteSlot(bool *emitted, MDefinition *obj,
                                PropertyName *name, MDefinition *value,
                                bool barrier, types::TemporaryTypeSet *objTypes);
    bool setPropTryInlineAccess(bool *emitted, MDefinition *obj,
                                PropertyName *name, MDefinition *value, bool barrier,
                                types::TemporaryTypeSet *objTypes);
    bool setPropTryTypedObject(bool *emitted, MDefinition *obj,
                               PropertyName *name, MDefinition *value);
    bool setPropTryScalarPropOfTypedObject(bool *emitted,
                                           MDefinition *obj,
                                           int32_t fieldOffset,
                                           MDefinition *value,
                                           TypeRepresentationSet fieldTypeReprs);
    bool setPropTryCache(bool *emitted, MDefinition *obj,
                         PropertyName *name, MDefinition *value,
                         bool barrier, types::TemporaryTypeSet *objTypes);

    // binary data lookup helpers.
    bool lookupTypeRepresentationSet(MDefinition *typedObj,
                                     TypeRepresentationSet *out);
    bool typeSetToTypeRepresentationSet(types::TemporaryTypeSet *types,
                                        TypeRepresentationSet *out,
                                        types::TypeTypedObject::Kind kind);
    bool lookupTypedObjectField(MDefinition *typedObj,
                                PropertyName *name,
                                int32_t *fieldOffset,
                                TypeRepresentationSet *fieldTypeReprs,
                                size_t *fieldIndex);
    MDefinition *loadTypedObjectType(MDefinition *value);
    void loadTypedObjectData(MDefinition *typedObj,
                             MDefinition *offset,
                             MDefinition **owner,
                             MDefinition **ownerOffset);
    void loadTypedObjectElements(MDefinition *typedObj,
                                 MDefinition *offset,
                                 int32_t unit,
                                 MDefinition **ownerElements,
                                 MDefinition **ownerScaledOffset);
    MDefinition *typeObjectForElementFromArrayStructType(MDefinition *typedObj);
    MDefinition *typeObjectForFieldFromStructType(MDefinition *type,
                                                  size_t fieldIndex);
    bool storeScalarTypedObjectValue(MDefinition *typedObj,
                                     MDefinition *offset,
                                     ScalarTypeRepresentation *typeRepr,
                                     MDefinition *value);
    bool checkTypedObjectIndexInBounds(size_t elemSize,
                                       MDefinition *obj,
                                       MDefinition *index,
                                       MDefinition **indexAsByteOffset,
                                       TypeRepresentationSet objTypeReprs);

    // jsop_setelem() helpers.
    bool setElemTryTyped(bool *emitted, MDefinition *object,
                         MDefinition *index, MDefinition *value);
    bool setElemTryTypedStatic(bool *emitted, MDefinition *object,
                               MDefinition *index, MDefinition *value);
    bool setElemTryDense(bool *emitted, MDefinition *object,
                         MDefinition *index, MDefinition *value);
    bool setElemTryArguments(bool *emitted, MDefinition *object,
                             MDefinition *index, MDefinition *value);
    bool setElemTryCache(bool *emitted, MDefinition *object,
                         MDefinition *index, MDefinition *value);

    // jsop_getelem() helpers.
    bool getElemTryDense(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryTypedStatic(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryTypedArray(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryTypedObject(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryString(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryArguments(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryArgumentsInlined(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryCache(bool *emitted, MDefinition *obj, MDefinition *index);
    bool getElemTryScalarElemOfTypedObject(bool *emitted,
                                           MDefinition *obj,
                                           MDefinition *index,
                                           TypeRepresentationSet objTypeReprs,
                                           TypeRepresentationSet elemTypeReprs,
                                           size_t elemSize);
    bool getElemTryComplexElemOfTypedObject(bool *emitted,
                                            MDefinition *obj,
                                            MDefinition *index,
                                            TypeRepresentationSet objTypeReprs,
                                            TypeRepresentationSet elemTypeReprs,
                                            size_t elemSize);

    // Typed array helpers.
    MInstruction *getTypedArrayLength(MDefinition *obj);
    MInstruction *getTypedArrayElements(MDefinition *obj);

    bool jsop_add(MDefinition *left, MDefinition *right);
    bool jsop_bitnot();
    bool jsop_bitop(JSOp op);
    bool jsop_binary(JSOp op);
    bool jsop_binary(JSOp op, MDefinition *left, MDefinition *right);
    bool jsop_pos();
    bool jsop_neg();
    bool jsop_setarg(uint32_t arg);
    bool jsop_defvar(uint32_t index);
    bool jsop_deffun(uint32_t index);
    bool jsop_notearg();
    bool jsop_funcall(uint32_t argc);
    bool jsop_funapply(uint32_t argc);
    bool jsop_funapplyarguments(uint32_t argc);
    bool jsop_call(uint32_t argc, bool constructing);
    bool jsop_eval(uint32_t argc);
    bool jsop_ifeq(JSOp op);
    bool jsop_try();
    bool jsop_label();
    bool jsop_condswitch();
    bool jsop_andor(JSOp op);
    bool jsop_dup2();
    bool jsop_loophead(jsbytecode *pc);
    bool jsop_compare(JSOp op);
    bool getStaticName(JSObject *staticObject, PropertyName *name, bool *psucceeded);
    bool setStaticName(JSObject *staticObject, PropertyName *name);
    bool jsop_getname(PropertyName *name);
    bool jsop_intrinsic(PropertyName *name);
    bool jsop_bindname(PropertyName *name);
    bool jsop_getelem();
    bool jsop_getelem_dense(MDefinition *obj, MDefinition *index);
    bool jsop_getelem_typed(MDefinition *obj, MDefinition *index, ScalarTypeRepresentation::Type arrayType);
    bool jsop_setelem();
    bool jsop_setelem_dense(types::TemporaryTypeSet::DoubleConversion conversion,
                            SetElemSafety safety,
                            MDefinition *object, MDefinition *index, MDefinition *value);
    bool jsop_setelem_typed(ScalarTypeRepresentation::Type arrayType,
                            SetElemSafety safety,
                            MDefinition *object, MDefinition *index, MDefinition *value);
    bool jsop_length();
    bool jsop_length_fastPath();
    bool jsop_arguments();
    bool jsop_arguments_length();
    bool jsop_arguments_getelem();
    bool jsop_runonce();
    bool jsop_rest();
    bool jsop_not();
    bool jsop_getprop(PropertyName *name);
    bool jsop_setprop(PropertyName *name);
    bool jsop_delprop(PropertyName *name);
    bool jsop_delelem();
    bool jsop_newarray(uint32_t count);
    bool jsop_newobject();
    bool jsop_initelem();
    bool jsop_initelem_array();
    bool jsop_initelem_getter_setter();
    bool jsop_initprop(PropertyName *name);
    bool jsop_initprop_getter_setter(PropertyName *name);
    bool jsop_regexp(RegExpObject *reobj);
    bool jsop_object(JSObject *obj);
    bool jsop_lambda(JSFunction *fun);
    bool jsop_this();
    bool jsop_typeof();
    bool jsop_toid();
    bool jsop_iter(uint8_t flags);
    bool jsop_iternext();
    bool jsop_itermore();
    bool jsop_iterend();
    bool jsop_in();
    bool jsop_in_dense();
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

    enum InliningDecision
    {
        InliningDecision_Error,
        InliningDecision_Inline,
        InliningDecision_DontInline
    };

    static InliningDecision DontInline(JSScript *targetScript, const char *reason);

    // Oracles.
    InliningDecision canInlineTarget(JSFunction *target, CallInfo &callInfo);
    InliningDecision makeInliningDecision(JSFunction *target, CallInfo &callInfo);
    bool selectInliningTargets(ObjectVector &targets, CallInfo &callInfo,
                               BoolVector &choiceSet, uint32_t *numInlineable);

    // Native inlining helpers.
    types::TemporaryTypeSet *getInlineReturnTypeSet();
    MIRType getInlineReturnType();

    // Array natives.
    InliningStatus inlineArray(CallInfo &callInfo);
    InliningStatus inlineArrayPopShift(CallInfo &callInfo, MArrayPopShift::Mode mode);
    InliningStatus inlineArrayPush(CallInfo &callInfo);
    InliningStatus inlineArrayConcat(CallInfo &callInfo);

    // Math natives.
    InliningStatus inlineMathAbs(CallInfo &callInfo);
    InliningStatus inlineMathFloor(CallInfo &callInfo);
    InliningStatus inlineMathCeil(CallInfo &callInfo);
    InliningStatus inlineMathRound(CallInfo &callInfo);
    InliningStatus inlineMathSqrt(CallInfo &callInfo);
    InliningStatus inlineMathAtan2(CallInfo &callInfo);
    InliningStatus inlineMathHypot(CallInfo &callInfo);
    InliningStatus inlineMathMinMax(CallInfo &callInfo, bool max);
    InliningStatus inlineMathPow(CallInfo &callInfo);
    InliningStatus inlineMathRandom(CallInfo &callInfo);
    InliningStatus inlineMathImul(CallInfo &callInfo);
    InliningStatus inlineMathFRound(CallInfo &callInfo);
    InliningStatus inlineMathFunction(CallInfo &callInfo, MMathFunction::Function function);

    // String natives.
    InliningStatus inlineStringObject(CallInfo &callInfo);
    InliningStatus inlineStringSplit(CallInfo &callInfo);
    InliningStatus inlineStrCharCodeAt(CallInfo &callInfo);
    InliningStatus inlineStrFromCharCode(CallInfo &callInfo);
    InliningStatus inlineStrCharAt(CallInfo &callInfo);
    InliningStatus inlineStrReplace(CallInfo &callInfo);

    // RegExp natives.
    InliningStatus inlineRegExpExec(CallInfo &callInfo);
    InliningStatus inlineRegExpTest(CallInfo &callInfo);

    // Array intrinsics.
    InliningStatus inlineUnsafePutElements(CallInfo &callInfo);
    bool inlineUnsafeSetDenseArrayElement(CallInfo &callInfo, uint32_t base);
    bool inlineUnsafeSetTypedArrayElement(CallInfo &callInfo, uint32_t base,
                                          ScalarTypeRepresentation::Type arrayType);
    InliningStatus inlineNewDenseArray(CallInfo &callInfo);
    InliningStatus inlineNewDenseArrayForSequentialExecution(CallInfo &callInfo);
    InliningStatus inlineNewDenseArrayForParallelExecution(CallInfo &callInfo);

    // Slot intrinsics.
    InliningStatus inlineUnsafeSetReservedSlot(CallInfo &callInfo);
    InliningStatus inlineUnsafeGetReservedSlot(CallInfo &callInfo);

    // Utility intrinsics.
    InliningStatus inlineIsCallable(CallInfo &callInfo);
    InliningStatus inlineHaveSameClass(CallInfo &callInfo);
    InliningStatus inlineToObject(CallInfo &callInfo);
    InliningStatus inlineDump(CallInfo &callInfo);

    // Testing functions.
    InliningStatus inlineForceSequentialOrInParallelSection(CallInfo &callInfo);
    InliningStatus inlineBailout(CallInfo &callInfo);
    InliningStatus inlineAssertFloat32(CallInfo &callInfo);

    // Main inlining functions
    InliningStatus inlineNativeCall(CallInfo &callInfo, JSNative native);
    bool inlineScriptedCall(CallInfo &callInfo, JSFunction *target);
    InliningStatus inlineSingleCall(CallInfo &callInfo, JSFunction *target);

    // Call functions
    InliningStatus inlineCallsite(ObjectVector &targets, ObjectVector &originals,
                                  bool lambda, CallInfo &callInfo);
    bool inlineCalls(CallInfo &callInfo, ObjectVector &targets, ObjectVector &originals,
                     BoolVector &choiceSet, MGetPropertyCache *maybeCache);

    // Inlining helpers.
    bool inlineGenericFallback(JSFunction *target, CallInfo &callInfo, MBasicBlock *dispatchBlock,
                               bool clonedAtCallsite);
    bool inlineTypeObjectFallback(CallInfo &callInfo, MBasicBlock *dispatchBlock,
                                  MTypeObjectDispatch *dispatch, MGetPropertyCache *cache,
                                  MBasicBlock **fallbackTarget);

    bool testNeedsArgumentCheck(JSFunction *target, CallInfo &callInfo);

    MDefinition *makeCallsiteClone(JSFunction *target, MDefinition *fun);
    MCall *makeCallHelper(JSFunction *target, CallInfo &callInfo, bool cloneAtCallsite);
    bool makeCall(JSFunction *target, CallInfo &callInfo, bool cloneAtCallsite);

    MDefinition *patchInlinedReturn(CallInfo &callInfo, MBasicBlock *exit, MBasicBlock *bottom);
    MDefinition *patchInlinedReturns(CallInfo &callInfo, MIRGraphReturns &returns,
                                     MBasicBlock *bottom);

    bool objectsHaveCommonPrototype(types::TemporaryTypeSet *types, PropertyName *name,
                                    bool isGetter, JSObject *foundProto);
    void freezePropertiesForCommonPrototype(types::TemporaryTypeSet *types, PropertyName *name,
                                            JSObject *foundProto);
    MDefinition *testCommonGetterSetter(types::TemporaryTypeSet *types, PropertyName *name,
                                        bool isGetter, JSObject *foundProto, Shape *lastProperty);
    bool testShouldDOMCall(types::TypeSet *inTypes,
                           JSFunction *func, JSJitInfo::OpType opType);

    bool annotateGetPropertyCache(MDefinition *obj, MGetPropertyCache *getPropCache,
                                  types::TemporaryTypeSet *objTypes,
                                  types::TemporaryTypeSet *pushedTypes);

    MGetPropertyCache *getInlineableGetPropertyCache(CallInfo &callInfo);

    JSObject *testSingletonProperty(JSObject *obj, PropertyName *name);
    bool testSingletonPropertyTypes(MDefinition *obj, JSObject *singleton, PropertyName *name,
                                    bool *testObject, bool *testString);
    bool getDefiniteSlot(types::TemporaryTypeSet *types, PropertyName *name,
                         types::HeapTypeSetKey *property);
    bool freezePropTypeSets(types::TemporaryTypeSet *types,
                            JSObject *foundProto, PropertyName *name);

    types::TemporaryTypeSet *bytecodeTypes(jsbytecode *pc);

    // Use one of the below methods for updating the current block, rather than
    // updating |current| directly. setCurrent() should only be used in cases
    // where the block cannot have phis whose type needs to be computed.

    void setCurrentAndSpecializePhis(MBasicBlock *block) {
        if (block)
            block->specializePhis();
        setCurrent(block);
    }

    void setCurrent(MBasicBlock *block) {
        current = block;
    }

    // A builder is inextricably tied to a particular script.
    JSScript *script_;

    // If off thread compilation is successful, the final code generator is
    // attached here. Code has been generated, but not linked (there is not yet
    // an IonScript). This is heap allocated, and must be explicitly destroyed,
    // performed by FinishOffThreadBuilder().
    CodeGenerator *backgroundCodegen_;

  public:
    void clearForBackEnd();

    JSScript *script() const { return script_; }

    CodeGenerator *backgroundCodegen() const { return backgroundCodegen_; }
    void setBackgroundCodegen(CodeGenerator *codegen) { backgroundCodegen_ = codegen; }

    AbortReason abortReason() { return abortReason_; }

    TypeRepresentationSetHash *getOrCreateReprSetHash(); // fallible

    types::CompilerConstraintList *constraints() {
        return constraints_;
    }

    bool isInlineBuilder() const {
        return callerBuilder_ != nullptr;
    }

    const JSAtomState &names() { return compartment->runtime()->names(); }

  private:
    bool init();

    JSContext *analysisContext;
    BaselineFrameInspector *baselineFrame_;
    AbortReason abortReason_;
    TypeRepresentationSetHash *reprSetHash_;

    // Constraints for recording dependencies on type information.
    types::CompilerConstraintList *constraints_;

    mozilla::Maybe<AutoLockForCompilation> lock_;

    void lock() {
        if (!analysisContext)
            lock_.construct(compartment);
    }
    void unlock() {
        if (!analysisContext)
            lock_.destroy();
    }

    // Basic analysis information about the script.
    BytecodeAnalysis analysis_;
    BytecodeAnalysis &analysis() {
        return analysis_;
    }

    types::TemporaryTypeSet *thisTypes, *argTypes, *typeArray;
    uint32_t typeArrayHint;

    GSNCache gsn;
    ScopeCoordinateNameCache scopeCoordinateNameCache;

    jsbytecode *pc;
    MBasicBlock *current;
    uint32_t loopDepth_;

    /* Information used for inline-call builders. */
    MResumePoint *callerResumePoint_;
    jsbytecode *callerPC() {
        return callerResumePoint_ ? callerResumePoint_->pc() : nullptr;
    }
    IonBuilder *callerBuilder_;

    struct LoopHeader {
        jsbytecode *pc;
        MBasicBlock *header;

        LoopHeader(jsbytecode *pc, MBasicBlock *header)
          : pc(pc), header(header)
        {}
    };

    Vector<CFGState, 8, IonAllocPolicy> cfgStack_;
    Vector<ControlFlowInfo, 4, IonAllocPolicy> loops_;
    Vector<ControlFlowInfo, 0, IonAllocPolicy> switches_;
    Vector<ControlFlowInfo, 2, IonAllocPolicy> labels_;
    Vector<MInstruction *, 2, IonAllocPolicy> iterators_;
    Vector<LoopHeader, 0, IonAllocPolicy> loopHeaders_;
    BaselineInspector *inspector;

    size_t inliningDepth_;

    // Cutoff to disable compilation if excessive time is spent reanalyzing
    // loop bodies to compute a fixpoint of the types for loop variables.
    static const size_t MAX_LOOP_RESTARTS = 40;
    size_t numLoopRestarts_;

    // True if script->failedBoundsCheck is set for the current script or
    // an outer script.
    bool failedBoundsCheck_;

    // True if script->failedShapeGuard is set for the current script or
    // an outer script.
    bool failedShapeGuard_;

    // Has an iterator other than 'for in'.
    bool nonStringIteration_;

    // If this script can use a lazy arguments object, it will be pre-created
    // here.
    MInstruction *lazyArguments_;

    // If this is an inline builder, the call info for the builder.
    const CallInfo *inlineCallInfo_;
};

class CallInfo
{
    MDefinition *fun_;
    MDefinition *thisArg_;
    MDefinitionVector args_;

    bool constructing_;
    bool setter_;

  public:
    CallInfo(TempAllocator &alloc, bool constructing)
      : fun_(nullptr),
        thisArg_(nullptr),
        args_(alloc),
        constructing_(constructing),
        setter_(false)
    { }

    bool init(CallInfo &callInfo) {
        JS_ASSERT(constructing_ == callInfo.constructing());

        fun_ = callInfo.fun();
        thisArg_ = callInfo.thisArg();

        if (!args_.appendAll(callInfo.argv()))
            return false;

        return true;
    }

    bool init(MBasicBlock *current, uint32_t argc) {
        JS_ASSERT(args_.empty());

        // Get the arguments in the right order
        if (!args_.reserve(argc))
            return false;
        for (int32_t i = argc; i > 0; i--)
            args_.infallibleAppend(current->peek(-i));
        current->popn(argc);

        // Get |this| and |fun|
        setThis(current->pop());
        setFun(current->pop());

        return true;
    }

    void popFormals(MBasicBlock *current) {
        current->popn(numFormals());
    }

    void pushFormals(MBasicBlock *current) {
        current->push(fun());
        current->push(thisArg());

        for (uint32_t i = 0; i < argc(); i++)
            current->push(getArg(i));
    }

    uint32_t argc() const {
        return args_.length();
    }
    uint32_t numFormals() const {
        return argc() + 2;
    }

    void setArgs(MDefinitionVector *args) {
        JS_ASSERT(args_.empty());
        args_.appendAll(*args);
    }

    MDefinitionVector &argv() {
        return args_;
    }

    const MDefinitionVector &argv() const {
        return args_;
    }

    MDefinition *getArg(uint32_t i) const {
        JS_ASSERT(i < argc());
        return args_[i];
    }

    void setArg(uint32_t i, MDefinition *def) {
        JS_ASSERT(i < argc());
        args_[i] = def;
    }

    MDefinition *thisArg() const {
        JS_ASSERT(thisArg_);
        return thisArg_;
    }

    void setThis(MDefinition *thisArg) {
        thisArg_ = thisArg;
    }

    bool constructing() const {
        return constructing_;
    }

    bool isSetter() const {
        return setter_;
    }
    void markAsSetter() {
        setter_ = true;
    }

    MDefinition *fun() const {
        JS_ASSERT(fun_);
        return fun_;
    }

    void setFun(MDefinition *fun) {
        fun_ = fun;
    }

    void setImplicitlyUsedUnchecked() {
        fun_->setImplicitlyUsedUnchecked();
        thisArg_->setImplicitlyUsedUnchecked();
        for (uint32_t i = 0; i < argc(); i++)
            getArg(i)->setImplicitlyUsedUnchecked();
    }
};

bool TypeSetIncludes(types::TypeSet *types, MIRType input, types::TypeSet *inputTypes);

bool NeedsPostBarrier(CompileInfo &info, MDefinition *value);

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_IonBuilder_h */
