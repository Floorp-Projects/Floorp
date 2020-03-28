/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonBuilder_h
#define jit_IonBuilder_h

// This file declares the data structures for building a MIRGraph from a
// JSScript.

#include "mozilla/Maybe.h"

#include "jit/BaselineInspector.h"
#include "jit/BytecodeAnalysis.h"
#include "jit/IonAnalysis.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/MIR.h"
#include "jit/MIRBuilderShared.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/TIOracle.h"

namespace js {

class TypedArrayObject;

namespace jit {

class CodeGenerator;
class CallInfo;
class BaselineFrameInspector;

enum class InlinableNative : uint16_t;

// Records information about a baseline frame for compilation that is stable
// when later used off thread.
BaselineFrameInspector* NewBaselineFrameInspector(TempAllocator* temp,
                                                  BaselineFrame* frame,
                                                  uint32_t frameSize);

using CallTargets = Vector<JSFunction*, 6, JitAllocPolicy>;

// [SMDOC] Control Flow handling in IonBuilder
//
// IonBuilder traverses the script's bytecode and compiles each instruction to
// corresponding MIR instructions. Handling control flow bytecode ops requires
// some special machinery:
//
// Forward branches
// ----------------
// Most branches in the bytecode are forward branches to a JSOp::JumpTarget
// instruction that we have not inspected yet. We compile them in two phases:
//
// 1) When compiling the source instruction: the MBasicBlock is terminated
//    with a control instruction that has a nullptr successor block. We also add
//    a PendingEdge instance to the PendingEdges list for the target bytecode
//    location.
//
// 2) When finally compiling the JSOp::JumpTarget: IonBuilder::visitJumpTarget
//    creates the target block and uses the list of PendingEdges to 'link' the
//    blocks.
//
// Loops
// -----
// Loops complicate this a bit:
//
// * Because of IonBuilder's single pass design, we sometimes have to 'restart'
//   a loop when we find new types for locals, arguments, or stack slots while
//   compiling the loop body. When this happens the loop has to be recompiled
//   from the beginning.
//
// * Loops may be nested within other loops, so we track loop states in a stack
//   per IonBuilder.
//
// Unreachable/dead code
// ---------------------
// Some bytecode instructions never fall through to the next instruction, for
// example JSOp::Return, JSOp::Goto, or JSOp::Throw. Code after such
// instructions is guaranteed to be dead so IonBuilder skips it until it gets to
// a jump target instruction with pending edges.
//
// Note: The frontend may generate unnecessary JSOp::JumpTarget instructions we
// can ignore when they have no incoming pending edges.
//
// Try-catch
// ---------
// IonBuilder supports scripts with try-catch by only compiling the try-block
// and bailing out (to the Baseline Interpreter) from the exception handler
// whenever we need to execute the catch-block.
//
// Because we don't compile the catch-block and the code after the try-catch may
// only be reachable via the catch-block, MGotoWithFake is used to ensure the
// code after the try-catch is always compiled and is part of the graph.
// See IonBuilder::visitTry for more information.
//
// Finally-blocks are currently not supported by Ion.

class MOZ_STACK_CLASS IonBuilder {
 public:
  IonBuilder(JSContext* analysisContext, MIRGenerator& mirGen,
             CompileInfo* info, CompilerConstraintList* constraints,
             BaselineInspector* inspector,
             BaselineFrameInspector* baselineFrame, size_t inliningDepth = 0,
             uint32_t loopDepth = 0);

  // Callers of build() and buildInline() should always check whether the
  // call overrecursed, if false is returned.  Overrecursion is not
  // signaled as OOM and will not in general be caught by OOM paths.
  AbortReasonOr<Ok> build();
  AbortReasonOr<Ok> buildInline(IonBuilder* callerBuilder,
                                MResumePoint* callerResumePoint,
                                CallInfo& callInfo);

  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r);
  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r,
                                                 const char* message, ...)
      MOZ_FORMAT_PRINTF(3, 4);

 private:
  AbortReasonOr<Ok> traverseBytecode();
  AbortReasonOr<Ok> startTraversingBlock(MBasicBlock* block);
  AbortReasonOr<Ok> processIterators();
  AbortReasonOr<Ok> inspectOpcode(JSOp op, bool* restarted);
  uint32_t readIndex(jsbytecode* pc);
  JSAtom* readAtom(jsbytecode* pc);

  void spew(const char* message);

  JSFunction* getSingleCallTarget(TemporaryTypeSet* calleeTypes);
  AbortReasonOr<Ok> getPolyCallTargets(TemporaryTypeSet* calleeTypes,
                                       bool constructing,
                                       InliningTargets& targets,
                                       uint32_t maxTargets);

  AbortReasonOr<Ok> analyzeNewLoopTypes(MBasicBlock* entry);
  AbortReasonOr<Ok> analyzeNewLoopTypesForLocation(
      MBasicBlock* entry, const BytecodeLocation loc,
      const mozilla::Maybe<BytecodeLocation>& last_,
      const mozilla::Maybe<BytecodeLocation>& earlier);

  AbortReasonOr<MBasicBlock*> newBlock(size_t stackDepth, jsbytecode* pc,
                                       MBasicBlock* maybePredecessor = nullptr);
  AbortReasonOr<MBasicBlock*> newBlock(MBasicBlock* predecessor, jsbytecode* pc,
                                       MResumePoint* priorResumePoint);
  AbortReasonOr<MBasicBlock*> newBlockPopN(MBasicBlock* predecessor,
                                           jsbytecode* pc, uint32_t popped);
  AbortReasonOr<MBasicBlock*> newBlockAfter(
      MBasicBlock* at, size_t stackDepth, jsbytecode* pc,
      MBasicBlock* maybePredecessor = nullptr);
  AbortReasonOr<MBasicBlock*> newOsrPreheader(MBasicBlock* header,
                                              jsbytecode* loopHead);
  AbortReasonOr<MBasicBlock*> newPendingLoopHeader(MBasicBlock* predecessor,
                                                   jsbytecode* pc, bool osr);

  AbortReasonOr<MBasicBlock*> newBlock(MBasicBlock* predecessor,
                                       jsbytecode* pc) {
    return newBlock(predecessor->stackDepth(), pc, predecessor);
  }

  AbortReasonOr<Ok> addPendingEdge(const PendingEdge& edge, jsbytecode* target);

  AbortReasonOr<Ok> jsop_loophead();

  AbortReasonOr<Ok> visitJumpTarget(JSOp op);
  AbortReasonOr<Ok> visitTest(JSOp op, bool* restarted);
  AbortReasonOr<Ok> visitTestBackedge(JSOp op, bool* restarted);
  AbortReasonOr<Ok> visitReturn(JSOp op);
  AbortReasonOr<Ok> visitGoto(jsbytecode* target);
  AbortReasonOr<Ok> visitBackEdge(bool* restarted);
  AbortReasonOr<Ok> visitTry();
  AbortReasonOr<Ok> visitThrow();
  AbortReasonOr<Ok> visitTableSwitch();

  // We want to make sure that our MTest instructions all check whether the
  // thing being tested might emulate undefined.  So we funnel their creation
  // through this method, to make sure that happens.  We don't want to just do
  // the check in MTest::New, because that can run on background compilation
  // threads, and we're not sure it's safe to touch that part of the typeset
  // from a background thread.
  MTest* newTest(MDefinition* ins, MBasicBlock* ifTrue, MBasicBlock* ifFalse);

  // Incorporates a type/typeSet into an OSR value for a loop, after the loop
  // body has been processed.
  AbortReasonOr<Ok> addOsrValueTypeBarrier(uint32_t slot, MInstruction** def,
                                           MIRType type,
                                           TemporaryTypeSet* typeSet);
  AbortReasonOr<Ok> maybeAddOsrTypeBarriers();

  AbortReasonOr<Ok> emitLoopHeadInstructions(jsbytecode* pc);

  // Restarts processing of a loop if the type information at its header was
  // incomplete.
  AbortReasonOr<Ok> restartLoop(MBasicBlock* header);

  // Please see the Big Honkin' Comment about how resume points work in
  // IonBuilder.cpp, near the definition for this function.
  AbortReasonOr<Ok> resume(MInstruction* ins, jsbytecode* pc,
                           MResumePoint::Mode mode);
  AbortReasonOr<Ok> resumeAt(MInstruction* ins, jsbytecode* pc);
  AbortReasonOr<Ok> resumeAfter(MInstruction* ins);
  AbortReasonOr<Ok> maybeInsertResume();

  void insertRecompileCheck(jsbytecode* pc);

  bool usesEnvironmentChain();

  AbortReasonOr<Ok> initParameters();
  void initLocals();
  void rewriteParameter(uint32_t slotIdx, MDefinition* param);
  AbortReasonOr<Ok> rewriteParameters();
  AbortReasonOr<Ok> initEnvironmentChain(MDefinition* callee = nullptr);
  void initArgumentsObject();
  void pushConstant(const Value& v);

  MConstant* constant(const Value& v);
  MConstant* constantInt(int32_t i);
  MInstruction* initializedLength(MDefinition* elements);
  MInstruction* setInitializedLength(MDefinition* obj, size_t count);

  // Improve the type information at tests
  AbortReasonOr<Ok> improveTypesAtTest(MDefinition* ins, bool trueBranch,
                                       MTest* test);
  AbortReasonOr<Ok> improveTypesAtTestSuccessor(MTest* test,
                                                MBasicBlock* successor);
  AbortReasonOr<Ok> improveTypesAtCompare(MCompare* ins, bool trueBranch,
                                          MTest* test);
  AbortReasonOr<Ok> improveTypesAtNullOrUndefinedCompare(MCompare* ins,
                                                         bool trueBranch,
                                                         MTest* test);
  AbortReasonOr<Ok> improveTypesAtTypeOfCompare(MCompare* ins, bool trueBranch,
                                                MTest* test);

  AbortReasonOr<Ok> replaceTypeSet(MDefinition* subject, TemporaryTypeSet* type,
                                   MTest* test);

  // Add a guard which ensure that the set of type which goes through this
  // generated code correspond to the observed types for the bytecode.
  MDefinition* addTypeBarrier(MDefinition* def, TemporaryTypeSet* observed,
                              BarrierKind kind,
                              MTypeBarrier** pbarrier = nullptr);
  AbortReasonOr<Ok> pushTypeBarrier(MDefinition* def,
                                    TemporaryTypeSet* observed,
                                    BarrierKind kind);

  // As pushTypeBarrier, but will compute the needBarrier boolean itself based
  // on observed and the JSFunction that we're planning to call. The
  // JSFunction must be a DOM method or getter.
  AbortReasonOr<Ok> pushDOMTypeBarrier(MInstruction* ins,
                                       TemporaryTypeSet* observed,
                                       JSFunction* func);

  // If definiteType is not known or def already has the right type, just
  // returns def.  Otherwise, returns an MInstruction that has that definite
  // type, infallibly unboxing ins as needed.  The new instruction will be
  // added to |current| in this case.
  MDefinition* ensureDefiniteType(MDefinition* def, MIRType definiteType);

  void maybeMarkEmpty(MDefinition* ins);

  JSObject* getSingletonPrototype(JSFunction* target);

  MDefinition* createThisScripted(MDefinition* callee, MDefinition* newTarget);
  MDefinition* createThisScriptedSingleton(JSFunction* target);
  MDefinition* createThisScriptedBaseline(MDefinition* callee);
  MDefinition* createThisSlow(MDefinition* callee, MDefinition* newTarget,
                              bool inlining);
  MDefinition* createThis(JSFunction* target, MDefinition* callee,
                          MDefinition* newTarget, bool inlining);
  MInstruction* createNamedLambdaObject(MDefinition* callee,
                                        MDefinition* envObj);
  AbortReasonOr<MInstruction*> createCallObject(MDefinition* callee,
                                                MDefinition* envObj);

  // Returns true if a property hasn't been overwritten and matches the given
  // predicate. Adds type constraints to ensure recompilation happens if the
  // property value ever changes.
  bool propertyIsConstantFunction(NativeObject* nobj, jsid id,
                                  bool (*test)(IonBuilder* builder,
                                               JSFunction* fun));

  bool ensureArrayPrototypeIteratorNotModified();
  bool ensureArrayIteratorPrototypeNextNotModified();

  MDefinition* walkEnvironmentChain(unsigned hops);

  MInstruction* addConvertElementsToDoubles(MDefinition* elements);
  MDefinition* addMaybeCopyElementsForWrite(MDefinition* object,
                                            bool checkNative);

  MInstruction* addBoundsCheck(MDefinition* index, MDefinition* length);

  MInstruction* addShapeGuard(MDefinition* obj, Shape* const shape,
                              BailoutKind bailoutKind);
  MInstruction* addGroupGuard(MDefinition* obj, ObjectGroup* group,
                              BailoutKind bailoutKind);
  MInstruction* addSharedTypedArrayGuard(MDefinition* obj);

  MInstruction* addGuardReceiverPolymorphic(
      MDefinition* obj, const BaselineInspector::ReceiverVector& receivers);

  bool invalidatedIdempotentCache();

  AbortReasonOr<Ok> loadSlot(MDefinition* obj, size_t slot, size_t nfixed,
                             MIRType rvalType, BarrierKind barrier,
                             TemporaryTypeSet* types);
  AbortReasonOr<Ok> loadSlot(MDefinition* obj, Shape* shape, MIRType rvalType,
                             BarrierKind barrier, TemporaryTypeSet* types);
  AbortReasonOr<Ok> storeSlot(MDefinition* obj, size_t slot, size_t nfixed,
                              MDefinition* value, bool needsBarrier,
                              MIRType slotType = MIRType::None);
  AbortReasonOr<Ok> storeSlot(MDefinition* obj, Shape* shape,
                              MDefinition* value, bool needsBarrier,
                              MIRType slotType = MIRType::None);
  bool shouldAbortOnPreliminaryGroups(MDefinition* obj);

  MDefinition* tryInnerizeWindow(MDefinition* obj);
  MDefinition* maybeUnboxForPropertyAccess(MDefinition* def);

  // jsop_getprop() helpers.
  AbortReasonOr<Ok> checkIsDefinitelyOptimizedArguments(MDefinition* obj,
                                                        bool* isOptimizedArgs);
  AbortReasonOr<Ok> getPropTryInferredConstant(bool* emitted, MDefinition* obj,
                                               PropertyName* name,
                                               TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryArgumentsLength(bool* emitted, MDefinition* obj);
  AbortReasonOr<Ok> getPropTryArgumentsCallee(bool* emitted, MDefinition* obj,
                                              PropertyName* name);
  AbortReasonOr<Ok> getPropTryConstant(bool* emitted, MDefinition* obj, jsid id,
                                       TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryNotDefined(bool* emitted, MDefinition* obj,
                                         jsid id, TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryDefiniteSlot(bool* emitted, MDefinition* obj,
                                           PropertyName* name,
                                           BarrierKind barrier,
                                           TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryModuleNamespace(bool* emitted, MDefinition* obj,
                                              PropertyName* name,
                                              BarrierKind barrier,
                                              TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryCommonGetter(bool* emitted, MDefinition* obj,
                                           jsid id, TemporaryTypeSet* types,
                                           bool innerized = false);
  AbortReasonOr<Ok> getPropTryInlineAccess(bool* emitted, MDefinition* obj,
                                           PropertyName* name,
                                           BarrierKind barrier,
                                           TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryInlineProtoAccess(bool* emitted, MDefinition* obj,
                                                PropertyName* name,
                                                TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropTryInnerize(bool* emitted, MDefinition* obj,
                                       PropertyName* name,
                                       TemporaryTypeSet* types);
  AbortReasonOr<Ok> getPropAddCache(MDefinition* obj, PropertyName* name,
                                    BarrierKind barrier,
                                    TemporaryTypeSet* types);

  // jsop_setprop() helpers.
  AbortReasonOr<Ok> setPropTryCommonSetter(bool* emitted, MDefinition* obj,
                                           PropertyName* name,
                                           MDefinition* value);
  AbortReasonOr<Ok> setPropTryCommonDOMSetter(bool* emitted, MDefinition* obj,
                                              MDefinition* value,
                                              JSFunction* setter,
                                              TemporaryTypeSet* objTypes);
  AbortReasonOr<Ok> setPropTryDefiniteSlot(bool* emitted, MDefinition* obj,
                                           PropertyName* name,
                                           MDefinition* value, bool barrier);
  AbortReasonOr<Ok> setPropTryInlineAccess(bool* emitted, MDefinition* obj,
                                           PropertyName* name,
                                           MDefinition* value, bool barrier,
                                           TemporaryTypeSet* objTypes);
  AbortReasonOr<Ok> setPropTryCache(bool* emitted, MDefinition* obj,
                                    PropertyName* name, MDefinition* value,
                                    bool barrier);

  // jsop_binary_arith helpers.
  MBinaryArithInstruction* binaryArithInstruction(JSOp op, MDefinition* left,
                                                  MDefinition* right);
  MIRType binaryArithNumberSpecialization(MDefinition* left,
                                          MDefinition* right);
  AbortReasonOr<Ok> binaryArithTryConcat(bool* emitted, JSOp op,
                                         MDefinition* left, MDefinition* right);
  AbortReasonOr<MBinaryArithInstruction*> binaryArithEmitSpecialized(
      MDefinition::Opcode op, MIRType specialization, MDefinition* left,
      MDefinition* right);
  AbortReasonOr<Ok> binaryArithTrySpecialized(bool* emitted, JSOp op,
                                              MDefinition* left,
                                              MDefinition* right);
  AbortReasonOr<Ok> binaryArithTrySpecializedOnBaselineInspector(
      bool* emitted, JSOp op, MDefinition* left, MDefinition* right);
  AbortReasonOr<Ok> arithTryBinaryStub(bool* emitted, JSOp op,
                                       MDefinition* left, MDefinition* right);

  // jsop_bitop helpers.
  AbortReasonOr<MBinaryBitwiseInstruction*> binaryBitOpEmit(
      JSOp op, MIRType specialization, MDefinition* left, MDefinition* right);
  AbortReasonOr<Ok> binaryBitOpTrySpecialized(bool* emitted, JSOp op,
                                              MDefinition* left,
                                              MDefinition* right);

  // jsop_bitnot helpers.
  AbortReasonOr<Ok> bitnotTrySpecialized(bool* emitted, MDefinition* input);

  // jsop_inc_or_dec helpers.
  MDefinition* unaryArithConvertToBinary(JSOp op, MDefinition::Opcode* defOp);
  AbortReasonOr<Ok> unaryArithTrySpecialized(bool* emitted, JSOp op,
                                             MDefinition* value);
  AbortReasonOr<Ok> unaryArithTrySpecializedOnBaselineInspector(
      bool* emitted, JSOp op, MDefinition* value);

  // jsop_pow helpers.
  AbortReasonOr<Ok> powTrySpecialized(bool* emitted, MDefinition* base,
                                      MDefinition* power, MIRType outputType);

  // jsop_compare helpers.
  AbortReasonOr<Ok> compareTrySpecialized(bool* emitted, JSOp op,
                                          MDefinition* left,
                                          MDefinition* right);
  AbortReasonOr<Ok> compareTryBitwise(bool* emitted, JSOp op, MDefinition* left,
                                      MDefinition* right);
  AbortReasonOr<Ok> compareTrySpecializedOnBaselineInspector(
      bool* emitted, JSOp op, MDefinition* left, MDefinition* right);
  AbortReasonOr<Ok> compareTryBinaryStub(bool* emitted, MDefinition* left,
                                         MDefinition* right);
  AbortReasonOr<Ok> compareTryCharacter(bool* emitted, JSOp op,
                                        MDefinition* left, MDefinition* right);

  // jsop_newarray helpers.
  AbortReasonOr<Ok> newArrayTryTemplateObject(bool* emitted,
                                              JSObject* templateObject,
                                              uint32_t length);
  AbortReasonOr<Ok> newArrayTryVM(bool* emitted, JSObject* templateObject,
                                  uint32_t length);

  // jsop_newobject helpers.
  AbortReasonOr<Ok> newObjectTryTemplateObject(bool* emitted,
                                               JSObject* templateObject);
  AbortReasonOr<Ok> newObjectTryVM(bool* emitted, JSObject* templateObject);

  // jsop_in/jsop_hasown helpers.
  AbortReasonOr<Ok> inTryDense(bool* emitted, MDefinition* obj,
                               MDefinition* id);
  AbortReasonOr<Ok> hasTryNotDefined(bool* emitted, MDefinition* obj,
                                     MDefinition* id, bool ownProperty);
  AbortReasonOr<Ok> hasTryDefiniteSlotOrUnboxed(bool* emitted, MDefinition* obj,
                                                MDefinition* id);

  // jsop_setelem() helpers.
  AbortReasonOr<Ok> setElemTryTypedArray(bool* emitted, MDefinition* object,
                                         MDefinition* index,
                                         MDefinition* value);
  AbortReasonOr<Ok> initOrSetElemTryDense(bool* emitted, MDefinition* object,
                                          MDefinition* index,
                                          MDefinition* value, bool writeHole);
  AbortReasonOr<Ok> setElemTryArguments(bool* emitted, MDefinition* object);
  AbortReasonOr<Ok> initOrSetElemTryCache(bool* emitted, MDefinition* object,
                                          MDefinition* index,
                                          MDefinition* value);

  AbortReasonOr<Ok> initArrayElemTryFastPath(bool* emitted, MDefinition* obj,
                                             MDefinition* id,
                                             MDefinition* value);

  AbortReasonOr<Ok> initArrayElementFastPath(
      MNewArray* obj, MDefinition* id, MDefinition* value,
      bool addResumePointAndIncrementInitializedLength);

  AbortReasonOr<Ok> initArrayElement(MDefinition* obj, MDefinition* id,
                                     MDefinition* value);

  // jsop_getelem() helpers.
  AbortReasonOr<Ok> getElemTryDense(bool* emitted, MDefinition* obj,
                                    MDefinition* index);
  AbortReasonOr<Ok> getElemTryGetProp(bool* emitted, MDefinition* obj,
                                      MDefinition* index);
  AbortReasonOr<Ok> getElemTryTypedArray(bool* emitted, MDefinition* obj,
                                         MDefinition* index);
  AbortReasonOr<Ok> getElemTryCallSiteObject(bool* emitted, MDefinition* obj,
                                             MDefinition* index);
  AbortReasonOr<Ok> getElemTryString(bool* emitted, MDefinition* obj,
                                     MDefinition* index);
  AbortReasonOr<Ok> getElemTryArguments(bool* emitted, MDefinition* obj,
                                        MDefinition* index);
  AbortReasonOr<Ok> getElemTryArgumentsInlinedConstant(bool* emitted,
                                                       MDefinition* obj,
                                                       MDefinition* index);
  AbortReasonOr<Ok> getElemTryArgumentsInlinedIndex(bool* emitted,
                                                    MDefinition* obj,
                                                    MDefinition* index);
  AbortReasonOr<Ok> getElemAddCache(MDefinition* obj, MDefinition* index);

  TemporaryTypeSet* computeHeapType(const TemporaryTypeSet* objTypes,
                                    const jsid id);

  enum BoundsChecking { DoBoundsCheck, SkipBoundsCheck };

  MInstruction* addArrayBufferByteLength(MDefinition* obj);

  TypedArrayObject* tryTypedArrayEmbedConstantElements(MDefinition* obj);

  // Add instructions to compute a typed array's length and data.  Also
  // optionally convert |*index| into a bounds-checked definition, if
  // requested.
  //
  // If you only need the array's length, use addTypedArrayLength below.
  void addTypedArrayLengthAndData(MDefinition* obj, BoundsChecking checking,
                                  MDefinition** index, MInstruction** length,
                                  MInstruction** elements);

  // Add an instruction to compute a typed array's length to the current
  // block.  If you also need the typed array's data, use the above method
  // instead.
  MInstruction* addTypedArrayLength(MDefinition* obj) {
    MInstruction* length;
    addTypedArrayLengthAndData(obj, SkipBoundsCheck, nullptr, &length, nullptr);
    return length;
  }

  // Add an instruction to compute a typed array's byte offset to the current
  // block.
  MInstruction* addTypedArrayByteOffset(MDefinition* obj);

  AbortReasonOr<Ok> improveThisTypesForCall();

  MDefinition* getCallee();
  MDefinition* getAliasedVar(EnvironmentCoordinate ec);
  AbortReasonOr<MDefinition*> addLexicalCheck(MDefinition* input);

  MDefinition* convertToBoolean(MDefinition* input);

  AbortReasonOr<Ok> tryFoldInstanceOf(bool* emitted, MDefinition* lhs,
                                      JSObject* protoObject);
  AbortReasonOr<bool> hasOnProtoChain(TypeSet::ObjectKey* key,
                                      JSObject* protoObject, bool* onProto);

  AbortReasonOr<Ok> jsop_add(MDefinition* left, MDefinition* right);
  AbortReasonOr<Ok> jsop_bitnot();
  AbortReasonOr<Ok> jsop_bitop(JSOp op);
  AbortReasonOr<Ok> jsop_binary_arith(JSOp op);
  AbortReasonOr<Ok> jsop_binary_arith(JSOp op, MDefinition* left,
                                      MDefinition* right);
  AbortReasonOr<Ok> jsop_pow();
  AbortReasonOr<Ok> jsop_pos();
  AbortReasonOr<Ok> jsop_neg();
  AbortReasonOr<Ok> jsop_tonumeric();
  AbortReasonOr<Ok> jsop_inc_or_dec(JSOp op);
  AbortReasonOr<Ok> jsop_tostring();
  AbortReasonOr<Ok> jsop_getarg(uint32_t arg);
  AbortReasonOr<Ok> jsop_setarg(uint32_t arg);
  AbortReasonOr<Ok> jsop_defvar();
  AbortReasonOr<Ok> jsop_deflexical();
  AbortReasonOr<Ok> jsop_deffun();
  AbortReasonOr<Ok> jsop_notearg();
  AbortReasonOr<Ok> jsop_throwsetconst();
  AbortReasonOr<Ok> jsop_checklexical();
  AbortReasonOr<Ok> jsop_checkaliasedlexical(EnvironmentCoordinate ec);
  AbortReasonOr<Ok> jsop_funcall(uint32_t argc);
  AbortReasonOr<Ok> jsop_funapply(uint32_t argc);
  AbortReasonOr<Ok> jsop_funapplyarguments(uint32_t argc);
  AbortReasonOr<Ok> jsop_funapplyarray(uint32_t argc);
  AbortReasonOr<Ok> jsop_spreadcall();
  AbortReasonOr<Ok> jsop_spreadnew();
  AbortReasonOr<Ok> jsop_optimize_spreadcall();
  AbortReasonOr<Ok> jsop_call(uint32_t argc, bool constructing,
                              bool ignoresReturnValue);
  AbortReasonOr<Ok> jsop_eval(uint32_t argc);
  AbortReasonOr<Ok> jsop_label();
  AbortReasonOr<Ok> jsop_andor(JSOp op);
  AbortReasonOr<Ok> jsop_dup2();
  AbortReasonOr<Ok> jsop_goto(bool* restarted);
  AbortReasonOr<Ok> jsop_loophead(jsbytecode* pc);
  AbortReasonOr<Ok> jsop_compare(JSOp op);
  AbortReasonOr<Ok> jsop_compare(JSOp op, MDefinition* left,
                                 MDefinition* right);
  AbortReasonOr<Ok> getStaticName(bool* emitted, JSObject* staticObject,
                                  PropertyName* name,
                                  MDefinition* lexicalCheck = nullptr);
  AbortReasonOr<Ok> loadStaticSlot(JSObject* staticObject, BarrierKind barrier,
                                   TemporaryTypeSet* types, uint32_t slot);
  AbortReasonOr<Ok> setStaticName(JSObject* staticObject, PropertyName* name);
  AbortReasonOr<Ok> jsop_getgname(PropertyName* name);
  AbortReasonOr<Ok> jsop_getname(PropertyName* name);
  AbortReasonOr<Ok> jsop_intrinsic(PropertyName* name);
  AbortReasonOr<Ok> jsop_getimport(PropertyName* name);
  AbortReasonOr<Ok> jsop_bindname(PropertyName* name);
  AbortReasonOr<Ok> jsop_bindvar();
  AbortReasonOr<Ok> jsop_getelem();
  AbortReasonOr<Ok> jsop_getelem_dense(MDefinition* obj, MDefinition* index);
  AbortReasonOr<Ok> jsop_getelem_typed(MDefinition* obj, MDefinition* index,
                                       ScalarTypeDescr::Type arrayType);
  AbortReasonOr<Ok> jsop_setelem();
  AbortReasonOr<Ok> initOrSetElemDense(
      TemporaryTypeSet::DoubleConversion conversion, MDefinition* object,
      MDefinition* index, MDefinition* value, bool writeHole, bool* emitted);
  AbortReasonOr<Ok> jsop_setelem_typed(ScalarTypeDescr::Type arrayType,
                                       MDefinition* object, MDefinition* index,
                                       MDefinition* value);
  AbortReasonOr<Ok> jsop_length();
  bool jsop_length_fastPath();
  AbortReasonOr<Ok> jsop_arguments();
  AbortReasonOr<Ok> jsop_arguments_getelem();
  AbortReasonOr<Ok> jsop_rest();
  AbortReasonOr<Ok> jsop_not();
  AbortReasonOr<Ok> jsop_envcallee();
  AbortReasonOr<Ok> jsop_superbase();
  AbortReasonOr<Ok> jsop_getprop_super(PropertyName* name);
  AbortReasonOr<Ok> jsop_getelem_super();
  AbortReasonOr<Ok> jsop_getprop(PropertyName* name);
  AbortReasonOr<Ok> jsop_setprop(PropertyName* name);
  AbortReasonOr<Ok> jsop_delprop(PropertyName* name);
  AbortReasonOr<Ok> jsop_delelem();
  AbortReasonOr<Ok> jsop_newarray(uint32_t length);
  AbortReasonOr<Ok> jsop_newarray(JSObject* templateObject, uint32_t length);
  AbortReasonOr<Ok> jsop_newarray_copyonwrite();
  AbortReasonOr<Ok> jsop_newobject();
  AbortReasonOr<Ok> jsop_initelem();
  AbortReasonOr<Ok> jsop_initelem_inc();
  AbortReasonOr<Ok> jsop_initelem_array();
  AbortReasonOr<Ok> jsop_initelem_getter_setter();
  AbortReasonOr<Ok> jsop_mutateproto();
  AbortReasonOr<Ok> jsop_initprop(PropertyName* name);
  AbortReasonOr<Ok> jsop_initprop_getter_setter(PropertyName* name);
  AbortReasonOr<Ok> jsop_regexp(RegExpObject* reobj);
  AbortReasonOr<Ok> jsop_object(JSObject* obj);
  AbortReasonOr<Ok> jsop_classconstructor();
  AbortReasonOr<Ok> jsop_derivedclassconstructor();
  AbortReasonOr<Ok> jsop_lambda(JSFunction* fun);
  AbortReasonOr<Ok> jsop_lambda_arrow(JSFunction* fun);
  AbortReasonOr<Ok> jsop_funwithproto(JSFunction* fun);
  AbortReasonOr<Ok> jsop_setfunname(uint8_t prefixKind);
  AbortReasonOr<Ok> jsop_pushlexicalenv(uint32_t index);
  AbortReasonOr<Ok> jsop_copylexicalenv(bool copySlots);
  AbortReasonOr<Ok> jsop_functionthis();
  AbortReasonOr<Ok> jsop_globalthis();
  AbortReasonOr<Ok> jsop_typeof();
  AbortReasonOr<Ok> jsop_toasync();
  AbortReasonOr<Ok> jsop_toasynciter();
  AbortReasonOr<Ok> jsop_toid();
  AbortReasonOr<Ok> jsop_iter();
  AbortReasonOr<Ok> jsop_itermore();
  AbortReasonOr<Ok> jsop_isnoiter();
  AbortReasonOr<Ok> jsop_iterend();
  AbortReasonOr<Ok> jsop_iternext();
  AbortReasonOr<Ok> jsop_in();
  AbortReasonOr<Ok> jsop_hasown();
  AbortReasonOr<Ok> jsop_instanceof();
  AbortReasonOr<Ok> jsop_getaliasedvar(EnvironmentCoordinate ec);
  AbortReasonOr<Ok> jsop_setaliasedvar(EnvironmentCoordinate ec);
  AbortReasonOr<Ok> jsop_debugger();
  AbortReasonOr<Ok> jsop_newtarget();
  AbortReasonOr<Ok> jsop_checkisobj(uint8_t kind);
  AbortReasonOr<Ok> jsop_checkiscallable(uint8_t kind);
  AbortReasonOr<Ok> jsop_checkobjcoercible();
  AbortReasonOr<Ok> jsop_checkclassheritage();
  AbortReasonOr<Ok> jsop_pushcallobj();
  AbortReasonOr<Ok> jsop_implicitthis(PropertyName* name);
  AbortReasonOr<Ok> jsop_importmeta();
  AbortReasonOr<Ok> jsop_dynamic_import();
  AbortReasonOr<Ok> jsop_instrumentation_active();
  AbortReasonOr<Ok> jsop_instrumentation_callback();
  AbortReasonOr<Ok> jsop_instrumentation_scriptid();
  AbortReasonOr<Ok> jsop_coalesce();
  AbortReasonOr<Ok> jsop_objwithproto();
  AbortReasonOr<Ok> jsop_builtinproto();
  AbortReasonOr<Ok> jsop_checkreturn();
  AbortReasonOr<Ok> jsop_checkthis();
  AbortReasonOr<Ok> jsop_checkthisreinit();
  AbortReasonOr<Ok> jsop_superfun();
  AbortReasonOr<Ok> jsop_inithomeobject();

  /* Inlining. */

  enum InliningStatus {
    InliningStatus_NotInlined,
    InliningStatus_WarmUpCountTooLow,
    InliningStatus_Inlined
  };
  using InliningResult = AbortReasonOr<InliningStatus>;

  enum InliningDecision {
    InliningDecision_Error,
    InliningDecision_Inline,
    InliningDecision_DontInline,
    InliningDecision_WarmUpCountTooLow
  };

  static InliningDecision DontInline(JSScript* targetScript,
                                     const char* reason);

  // Helper function for canInlineTarget
  bool hasCommonInliningPath(const JSScript* scriptToInline);

  // Oracles.
  InliningDecision canInlineTarget(JSFunction* target, CallInfo& callInfo);
  InliningDecision makeInliningDecision(JSObject* target, CallInfo& callInfo);
  AbortReasonOr<Ok> selectInliningTargets(const InliningTargets& targets,
                                          CallInfo& callInfo,
                                          BoolVector& choiceSet,
                                          uint32_t* numInlineable);

  OptimizationLevel optimizationLevel() const {
    return optimizationInfo().level();
  }
  bool isHighestOptimizationLevel() const {
    return IonOptimizations.isLastLevel(optimizationLevel());
  }

  // Native inlining helpers.
  // The typeset for the return value of our function.  These are
  // the types it's been observed returning in the past.
  TemporaryTypeSet* getInlineReturnTypeSet();
  // The known MIR type of getInlineReturnTypeSet.
  MIRType getInlineReturnType();

  // Array natives.
  InliningResult inlineArray(CallInfo& callInfo, Realm* targetRealm);
  InliningResult inlineArrayIsArray(CallInfo& callInfo);
  InliningResult inlineArrayPopShift(CallInfo& callInfo,
                                     MArrayPopShift::Mode mode);
  InliningResult inlineArrayPush(CallInfo& callInfo);
  InliningResult inlineArraySlice(CallInfo& callInfo);
  InliningResult inlineArrayJoin(CallInfo& callInfo);

  // Boolean natives.
  InliningResult inlineBoolean(CallInfo& callInfo);

  // Iterator intrinsics.
  InliningResult inlineNewIterator(CallInfo& callInfo, MNewIterator::Type type);
  InliningResult inlineArrayIteratorPrototypeOptimizable(CallInfo& callInfo);

  // Math natives.
  InliningResult inlineMathAbs(CallInfo& callInfo);
  InliningResult inlineMathFloor(CallInfo& callInfo);
  InliningResult inlineMathCeil(CallInfo& callInfo);
  InliningResult inlineMathClz32(CallInfo& callInfo);
  InliningResult inlineMathRound(CallInfo& callInfo);
  InliningResult inlineMathSqrt(CallInfo& callInfo);
  InliningResult inlineMathAtan2(CallInfo& callInfo);
  InliningResult inlineMathHypot(CallInfo& callInfo);
  InliningResult inlineMathMinMax(CallInfo& callInfo, bool max);
  InliningResult inlineMathPow(CallInfo& callInfo);
  InliningResult inlineMathRandom(CallInfo& callInfo);
  InliningResult inlineMathImul(CallInfo& callInfo);
  InliningResult inlineMathFRound(CallInfo& callInfo);
  InliningResult inlineMathTrunc(CallInfo& callInfo);
  InliningResult inlineMathSign(CallInfo& callInfo);
  InliningResult inlineMathFunction(CallInfo& callInfo,
                                    MMathFunction::Function function);

  // String natives.
  InliningResult inlineStringObject(CallInfo& callInfo);
  InliningResult inlineStrCharCodeAt(CallInfo& callInfo);
  InliningResult inlineConstantCharCodeAt(CallInfo& callInfo);
  InliningResult inlineStrFromCharCode(CallInfo& callInfo);
  InliningResult inlineStrFromCodePoint(CallInfo& callInfo);
  InliningResult inlineStrCharAt(CallInfo& callInfo);
  InliningResult inlineStringConvertCase(CallInfo& callInfo,
                                         MStringConvertCase::Mode mode);

  // String intrinsics.
  InliningResult inlineStringReplaceString(CallInfo& callInfo);
  InliningResult inlineConstantStringSplitString(CallInfo& callInfo);
  InliningResult inlineStringSplitString(CallInfo& callInfo);

  // Reflect natives.
  InliningResult inlineReflectGetPrototypeOf(CallInfo& callInfo);

  // RegExp intrinsics.
  InliningResult inlineRegExpMatcher(CallInfo& callInfo);
  InliningResult inlineRegExpSearcher(CallInfo& callInfo);
  InliningResult inlineRegExpTester(CallInfo& callInfo);
  InliningResult inlineIsRegExpObject(CallInfo& callInfo);
  InliningResult inlineIsPossiblyWrappedRegExpObject(CallInfo& callInfo);
  InliningResult inlineRegExpPrototypeOptimizable(CallInfo& callInfo);
  InliningResult inlineRegExpInstanceOptimizable(CallInfo& callInfo);
  InliningResult inlineGetFirstDollarIndex(CallInfo& callInfo);

  // Object natives and intrinsics.
  InliningResult inlineObject(CallInfo& callInfo);
  InliningResult inlineObjectCreate(CallInfo& callInfo);
  InliningResult inlineObjectIs(CallInfo& callInfo);
  InliningResult inlineObjectToString(CallInfo& callInfo);
  InliningResult inlineDefineDataProperty(CallInfo& callInfo);

  // Atomics natives.
  InliningResult inlineAtomicsCompareExchange(CallInfo& callInfo);
  InliningResult inlineAtomicsExchange(CallInfo& callInfo);
  InliningResult inlineAtomicsLoad(CallInfo& callInfo);
  InliningResult inlineAtomicsStore(CallInfo& callInfo);
  InliningResult inlineAtomicsBinop(CallInfo& callInfo, InlinableNative target);
  InliningResult inlineAtomicsIsLockFree(CallInfo& callInfo);

  // Slot intrinsics.
  InliningResult inlineUnsafeSetReservedSlot(CallInfo& callInfo);
  InliningResult inlineUnsafeGetReservedSlot(CallInfo& callInfo,
                                             MIRType knownValueType);

  // Map and Set intrinsics.
  InliningResult inlineGetNextEntryForIterator(
      CallInfo& callInfo, MGetNextEntryForIterator::Mode mode);

  // ArrayBuffer intrinsics.
  InliningResult inlineArrayBufferByteLength(CallInfo& callInfo);
  InliningResult inlinePossiblyWrappedArrayBufferByteLength(CallInfo& callInfo);

  // TypedArray intrinsics.
  enum WrappingBehavior { AllowWrappedTypedArrays, RejectWrappedTypedArrays };
  InliningResult inlineTypedArray(CallInfo& callInfo, Native native);
  InliningResult inlineIsTypedArrayConstructor(CallInfo& callInfo);
  InliningResult inlineIsTypedArrayHelper(CallInfo& callInfo,
                                          WrappingBehavior wrappingBehavior);
  InliningResult inlineIsTypedArray(CallInfo& callInfo);
  InliningResult inlineIsPossiblyWrappedTypedArray(CallInfo& callInfo);
  InliningResult inlineTypedArrayLength(CallInfo& callInfo);
  InliningResult inlinePossiblyWrappedTypedArrayLength(CallInfo& callInfo);
  InliningResult inlineTypedArrayByteOffset(CallInfo& callInfo);
  InliningResult inlineTypedArrayElementShift(CallInfo& callInfo);

  // Utility intrinsics.
  InliningResult inlineIsCallable(CallInfo& callInfo);
  InliningResult inlineIsConstructor(CallInfo& callInfo);
  InliningResult inlineIsObject(CallInfo& callInfo);
  InliningResult inlineToObject(CallInfo& callInfo);
  InliningResult inlineIsCrossRealmArrayConstructor(CallInfo& callInfo);
  InliningResult inlineToInteger(CallInfo& callInfo);
  InliningResult inlineToString(CallInfo& callInfo);
  InliningResult inlineDump(CallInfo& callInfo);
  InliningResult inlineHasClass(CallInfo& callInfo, const JSClass* clasp,
                                const JSClass* clasp2 = nullptr,
                                const JSClass* clasp3 = nullptr,
                                const JSClass* clasp4 = nullptr);
  InliningResult inlineGuardToClass(CallInfo& callInfo, const JSClass* clasp);
  InliningResult inlineIsConstructing(CallInfo& callInfo);
  InliningResult inlineSubstringKernel(CallInfo& callInfo);
  InliningResult inlineObjectHasPrototype(CallInfo& callInfo);
  InliningResult inlineFinishBoundFunctionInit(CallInfo& callInfo);
  InliningResult inlineIsPackedArray(CallInfo& callInfo);
  InliningResult inlineWasmCall(CallInfo& callInfo, JSFunction* target);

  // Testing functions.
  InliningResult inlineBailout(CallInfo& callInfo);
  InliningResult inlineAssertFloat32(CallInfo& callInfo);
  InliningResult inlineAssertRecoveredOnBailout(CallInfo& callInfo);

  // Bind function.
  InliningResult inlineBoundFunction(CallInfo& callInfo, JSFunction* target);

  // Main inlining functions
  InliningResult inlineNativeCall(CallInfo& callInfo, JSFunction* target);
  InliningResult inlineNativeGetter(CallInfo& callInfo, JSFunction* target);
  InliningResult inlineScriptedCall(CallInfo& callInfo, JSFunction* target);
  InliningResult inlineSingleCall(CallInfo& callInfo, JSObject* target);

  // Call functions
  InliningResult inlineCallsite(const InliningTargets& targets,
                                CallInfo& callInfo);
  AbortReasonOr<Ok> inlineCalls(CallInfo& callInfo,
                                const InliningTargets& targets,
                                BoolVector& choiceSet,
                                MGetPropertyCache* maybeCache);

  // Inlining helpers.
  AbortReasonOr<Ok> inlineGenericFallback(
      const mozilla::Maybe<CallTargets>& targets, CallInfo& callInfo,
      MBasicBlock* dispatchBlock);
  AbortReasonOr<Ok> inlineObjectGroupFallback(
      const mozilla::Maybe<CallTargets>& targets, CallInfo& callInfo,
      MBasicBlock* dispatchBlock, MObjectGroupDispatch* dispatch,
      MGetPropertyCache* cache, MBasicBlock** fallbackTarget);

  enum AtomicCheckResult { DontCheckAtomicResult, DoCheckAtomicResult };

  bool atomicsMeetsPreconditions(
      CallInfo& callInfo, Scalar::Type* arrayElementType,
      bool* requiresDynamicCheck,
      AtomicCheckResult checkResult = DoCheckAtomicResult);
  void atomicsCheckBounds(CallInfo& callInfo, MInstruction** elements,
                          MDefinition** index);

  bool testNeedsArgumentCheck(JSFunction* target, CallInfo& callInfo);

  AbortReasonOr<MCall*> makeCallHelper(
      const mozilla::Maybe<CallTargets>& targets, CallInfo& callInfo);
  AbortReasonOr<Ok> makeCall(const mozilla::Maybe<CallTargets>& targets,
                             CallInfo& callInfo);
  AbortReasonOr<Ok> makeCall(JSFunction* target, CallInfo& callInfo);

  MDefinition* patchInlinedReturn(JSFunction* target, CallInfo& callInfo,
                                  MBasicBlock* exit, MBasicBlock* bottom);
  MDefinition* patchInlinedReturns(JSFunction* target, CallInfo& callInfo,
                                   MIRGraphReturns& returns,
                                   MBasicBlock* bottom);
  MDefinition* specializeInlinedReturn(MDefinition* rdef, MBasicBlock* exit);

  NativeObject* commonPrototypeWithGetterSetter(TemporaryTypeSet* types,
                                                jsid id, bool isGetter,
                                                JSFunction* getterOrSetter,
                                                bool* guardGlobal);
  AbortReasonOr<Ok> freezePropertiesForCommonPrototype(
      TemporaryTypeSet* types, jsid id, JSObject* foundProto,
      bool allowEmptyTypesForGlobal);
  /*
   * Callers must pass a non-null globalGuard if they pass a non-null
   * globalShape.
   */
  AbortReasonOr<bool> testCommonGetterSetter(
      TemporaryTypeSet* types, jsid id, bool isGetter,
      JSFunction* getterOrSetter, MDefinition** guard,
      Shape* globalShape = nullptr, MDefinition** globalGuard = nullptr);
  AbortReasonOr<bool> testShouldDOMCall(TypeSet* inTypes, JSFunction* func,
                                        JSJitInfo::OpType opType);

  MDefinition* addShapeGuardsForGetterSetter(
      MDefinition* obj, JSObject* holder, Shape* holderShape,
      const BaselineInspector::ReceiverVector& receivers, bool isOwnProperty);

  AbortReasonOr<Ok> annotateGetPropertyCache(MDefinition* obj,
                                             PropertyName* name,
                                             MGetPropertyCache* getPropCache,
                                             TemporaryTypeSet* objTypes,
                                             TemporaryTypeSet* pushedTypes);

  MGetPropertyCache* getInlineableGetPropertyCache(CallInfo& callInfo);

  JSObject* testGlobalLexicalBinding(PropertyName* name);

  JSObject* testSingletonProperty(JSObject* obj, jsid id);
  JSObject* testSingletonPropertyTypes(MDefinition* obj, jsid id);

  AbortReasonOr<bool> testNotDefinedProperty(MDefinition* obj, jsid id,
                                             bool ownProperty = false);

  uint32_t getDefiniteSlot(TemporaryTypeSet* types, jsid id, uint32_t* pnfixed);

  AbortReasonOr<Ok> checkPreliminaryGroups(MDefinition* obj);
  AbortReasonOr<Ok> freezePropTypeSets(TemporaryTypeSet* types,
                                       JSObject* foundProto,
                                       PropertyName* name);
  bool canInlinePropertyOpShapes(
      const BaselineInspector::ReceiverVector& receivers);

  TemporaryTypeSet* bytecodeTypes(jsbytecode* pc);

  // Use one of the below methods for updating the current block, rather than
  // updating |current| directly. setCurrent() should only be used in cases
  // where the block cannot have phis whose type needs to be computed.

  AbortReasonOr<Ok> setCurrentAndSpecializePhis(MBasicBlock* block) {
    MOZ_ASSERT(block);
    if (!block->specializePhis(alloc())) {
      return abort(AbortReason::Alloc);
    }
    setCurrent(block);
    return Ok();
  }

  void setCurrent(MBasicBlock* block) {
    MOZ_ASSERT(block);
    current = block;
  }

  bool hasTerminatedBlock() const { return current == nullptr; }
  void setTerminatedBlock() { current = nullptr; }

  // A builder is inextricably tied to a particular script.
  JSScript* script_;

  // Some aborts are actionable (e.g., using an unsupported bytecode). When
  // optimization tracking is enabled, the location and message of the abort
  // are recorded here so they may be propagated to the script's
  // corresponding JitcodeGlobalEntry::BaselineEntry.
  JSScript* actionableAbortScript_;
  jsbytecode* actionableAbortPc_;
  const char* actionableAbortMessage_;

 public:
  using ObjectGroupVector = Vector<ObjectGroup*, 0, JitAllocPolicy>;

  void checkNurseryCell(gc::Cell* cell);
  JSObject* checkNurseryObject(JSObject* obj);

  JSScript* script() const { return script_; }

  TIOracle& tiOracle() { return tiOracle_; }

  CompilerConstraintList* constraints() { return constraints_; }

  bool isInlineBuilder() const { return callerBuilder_ != nullptr; }

  const JSAtomState& names() { return realm->runtime()->names(); }

  bool hadActionableAbort() const {
    MOZ_ASSERT(!actionableAbortScript_ ||
               (actionableAbortPc_ && actionableAbortMessage_));
    return actionableAbortScript_ != nullptr;
  }

  void actionableAbortLocationAndMessage(JSScript** abortScript,
                                         jsbytecode** abortPc,
                                         const char** abortMessage) {
    MOZ_ASSERT(hadActionableAbort());
    *abortScript = actionableAbortScript_;
    *abortPc = actionableAbortPc_;
    *abortMessage = actionableAbortMessage_;
  }

 private:
  AbortReasonOr<Ok> init();

  JSContext* analysisContext;
  BaselineFrameInspector* baselineFrame_;

  // Constraints for recording dependencies on type information.
  CompilerConstraintList* constraints_;

  MIRGenerator& mirGen_;
  TIOracle tiOracle_;

  CompileRealm* realm;
  const CompileInfo* info_;
  const OptimizationInfo* optimizationInfo_;
  TempAllocator* alloc_;
  MIRGraph* graph_;

  TemporaryTypeSet* thisTypes;
  TemporaryTypeSet* argTypes;
  TemporaryTypeSet* typeArray;
  uint32_t typeArrayHint;
  uint32_t* bytecodeTypeMap;

  jsbytecode* pc;
  jsbytecode* nextpc = nullptr;

  // The current MIR block. This can be nullptr after a block has been
  // terminated, for example right after a 'return' or 'break' statement.
  MBasicBlock* current = nullptr;

  uint32_t loopDepth_;

  PendingEdgesMap pendingEdges_;
  LoopStateStack loopStack_;

  Vector<BytecodeSite*, 0, JitAllocPolicy> trackedOptimizationSites_;

  ObjectGroupVector abortedPreliminaryGroups_;

  BytecodeSite* bytecodeSite(jsbytecode* pc) {
    MOZ_ASSERT(info().inlineScriptTree()->script()->containsPC(pc));
    return new (alloc()) BytecodeSite(info().inlineScriptTree(), pc);
  }

  /* Information used for inline-call builders. */
  MResumePoint* callerResumePoint_;
  jsbytecode* callerPC() {
    return callerResumePoint_ ? callerResumePoint_->pc() : nullptr;
  }
  IonBuilder* callerBuilder_;

  IonBuilder* outermostBuilder();

  struct LoopHeader {
    jsbytecode* pc;
    MBasicBlock* header;

    LoopHeader(jsbytecode* pc, MBasicBlock* header) : pc(pc), header(header) {}
  };

  PhiVector iterators_;
  Vector<LoopHeader, 0, JitAllocPolicy> loopHeaders_;

  BaselineInspector* inspector;

  size_t inliningDepth_;

  // Total bytecode length of all inlined scripts. Only tracked for the
  // outermost builder.
  size_t inlinedBytecodeLength_;

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

  // True if script->failedLexicalCheck_ is set for the current script or
  // an outer script.
  bool failedLexicalCheck_;

#ifdef DEBUG
  // If this script uses the lazy arguments object.
  bool hasLazyArguments_;
#endif

  // If this is an inline builder, the call info for the builder.
  const CallInfo* inlineCallInfo_;

  // When compiling a call with multiple targets, we are first creating a
  // MGetPropertyCache.  This MGetPropertyCache is following the bytecode, and
  // is used to recover the JSFunction.  In some cases, the Type of the object
  // which own the property is enough for dispatching to the right function.
  // In such cases we do not have read the property, except when the type
  // object is unknown.
  //
  // As an optimization, we can dispatch a call based on the object group,
  // without doing the MGetPropertyCache.  This is what is achieved by
  // |IonBuilder::inlineCalls|.  As we might not know all the functions, we
  // are adding a fallback path, where this MGetPropertyCache would be moved
  // into.
  //
  // In order to build the fallback path, we have to capture a resume point
  // ahead, for the potential fallback path.  This resume point is captured
  // while building MGetPropertyCache.  It is capturing the state of Baseline
  // before the execution of the MGetPropertyCache, such as we can safely do
  // it in the fallback path.
  //
  // This field is used to discard the resume point if it is not used for
  // building a fallback path.

  // Discard the prior resume point while setting a new MGetPropertyCache.
  void replaceMaybeFallbackFunctionGetter(MGetPropertyCache* cache);

  // Discard the MGetPropertyCache if it is handled by WrapMGetPropertyCache.
  void keepFallbackFunctionGetter(MGetPropertyCache* cache) {
    if (cache == maybeFallbackFunctionGetter_) {
      maybeFallbackFunctionGetter_ = nullptr;
    }
  }

  MGetPropertyCache* maybeFallbackFunctionGetter_;

  bool needsPostBarrier(MDefinition* value);

  void addAbortedPreliminaryGroup(ObjectGroup* group);

  MIRGraph& graph() { return *graph_; }
  const CompileInfo& info() const { return *info_; }

  const OptimizationInfo& optimizationInfo() const {
    return *optimizationInfo_;
  }

  bool forceInlineCaches() {
    return MOZ_UNLIKELY(JitOptions.forceInlineCaches);
  }

 public:
  MIRGenerator& mirGen() { return mirGen_; }
  TempAllocator& alloc() { return *alloc_; }

  // When aborting with AbortReason::PreliminaryObjects, all groups with
  // preliminary objects which haven't been analyzed yet.
  const ObjectGroupVector& abortedPreliminaryGroups() const {
    return abortedPreliminaryGroups_;
  }
};

}  // namespace jit
}  // namespace js

#endif /* jit_IonBuilder_h */
