/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS bytecode generation. */

#ifndef frontend_BytecodeEmitter_h
#define frontend_BytecodeEmitter_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_STACK_CLASS, MOZ_MUST_USE, MOZ_ALWAYS_INLINE, MOZ_NEVER_INLINE, MOZ_RAII
#include "mozilla/Maybe.h"   // mozilla::Maybe, mozilla::Some
#include "mozilla/Span.h"    // mozilla::Span
#include "mozilla/Vector.h"  // mozilla::Vector

#include <functional>  // std::function
#include <stddef.h>    // ptrdiff_t
#include <stdint.h>    // uint16_t, uint32_t

#include "jsapi.h"  // CompletionKind

#include "frontend/AbstractScopePtr.h"
#include "frontend/BCEParserHandle.h"            // BCEParserHandle
#include "frontend/BytecodeControlStructures.h"  // NestableControl
#include "frontend/BytecodeOffset.h"             // BytecodeOffset
#include "frontend/BytecodeSection.h"  // BytecodeSection, PerScriptData, CGScopeList
#include "frontend/DestructuringFlavor.h"  // DestructuringFlavor
#include "frontend/EitherParser.h"         // EitherParser
#include "frontend/ErrorReporter.h"        // ErrorReporter
#include "frontend/FullParseHandler.h"     // FullParseHandler
#include "frontend/JumpList.h"             // JumpList, JumpTarget
#include "frontend/NameAnalysisTypes.h"    // NameLocation
#include "frontend/NameCollections.h"      // AtomIndexMap
#include "frontend/ParseNode.h"            // ParseNode and subclasses
#include "frontend/Parser.h"               // Parser, PropListType
#include "frontend/SharedContext.h"        // SharedContext
#include "frontend/SourceNotes.h"          // SrcNoteType
#include "frontend/TokenStream.h"          // TokenPos
#include "frontend/ValueUsage.h"           // ValueUsage
#include "js/RootingAPI.h"                 // JS::Rooted, JS::Handle
#include "js/TypeDecls.h"                  // jsbytecode
#include "vm/BytecodeUtil.h"               // JSOp
#include "vm/Instrumentation.h"            // InstrumentationKind
#include "vm/Interpreter.h"  // CheckIsObjectKind, CheckIsCallableKind
#include "vm/Iteration.h"    // IteratorKind
#include "vm/JSFunction.h"   // JSFunction, FunctionPrefixKind
#include "vm/JSScript.h"  // JSScript, BaseScript, FieldInitializers, TryNoteKind
#include "vm/Runtime.h"   // ReportOutOfMemory
#include "vm/StringType.h"  // JSAtom

namespace js {

enum class GeneratorResumeKind;

}  // namespace js

namespace js {
namespace frontend {

class CallOrNewEmitter;
class ClassEmitter;
class ElemOpEmitter;
class EmitterScope;
class NestableControl;
class PropertyEmitter;
class PropOpEmitter;
class OptionalEmitter;
class TDZCheckCache;
class TryEmitter;

enum class ValueIsOnStack { Yes, No };

struct MOZ_STACK_CLASS BytecodeEmitter {
  // Context shared between parsing and bytecode generation.
  SharedContext* const sc = nullptr;

  JSContext* const cx = nullptr;

  // Enclosing function or global context.
  BytecodeEmitter* const parent = nullptr;

  // The JSScript we're ultimately producing.
  JS::Rooted<JSScript*> script;

  // The lazy script if mode is LazyFunction, nullptr otherwise.
  JS::Rooted<BaseScript*> lazyScript;

 private:
  BytecodeSection bytecodeSection_;

 public:
  BytecodeSection& bytecodeSection() { return bytecodeSection_; }
  const BytecodeSection& bytecodeSection() const { return bytecodeSection_; }

 private:
  PerScriptData perScriptData_;

 public:
  PerScriptData& perScriptData() { return perScriptData_; }
  const PerScriptData& perScriptData() const { return perScriptData_; }

 private:
  // switchToMain sets this to the bytecode offset of the main section.
  mozilla::Maybe<uint32_t> mainOffset_ = {};

  /* field info for enclosing class */
  const FieldInitializers fieldInitializers_;

 public:
  // Private storage for parser wrapper. DO NOT REFERENCE INTERNALLY. May not be
  // initialized. Use |parser| instead.
  mozilla::Maybe<EitherParser> ep_ = {};
  BCEParserHandle* parser = nullptr;

  CompilationInfo& compilationInfo;

  // First line and column, for JSScript::fullyInitFromStencil.
  unsigned firstLine = 0;
  unsigned firstColumn = 0;

  uint32_t maxFixedSlots = 0; /* maximum number of fixed frame slots so far */

  uint32_t bodyScopeIndex =
      UINT32_MAX; /* index into scopeList of the body scope */

  EmitterScope* varEmitterScope = nullptr;
  NestableControl* innermostNestableControl = nullptr;
  EmitterScope* innermostEmitterScope_ = nullptr;
  TDZCheckCache* innermostTDZCheckCache = nullptr;

  const FieldInitializers& getFieldInitializers() { return fieldInitializers_; }

#ifdef DEBUG
  bool unstableEmitterScope = false;

  friend class AutoCheckUnstableEmitterScope;
#endif

  EmitterScope* innermostEmitterScope() const {
    MOZ_ASSERT(!unstableEmitterScope);
    return innermostEmitterScopeNoCheck();
  }
  EmitterScope* innermostEmitterScopeNoCheck() const {
    return innermostEmitterScope_;
  }

  // Script contains finally block.
  bool hasTryFinally = false;

  // True while emitting a lambda which is only expected to run once.
  bool emittingRunOnceLambda = false;

  bool isRunOnceLambda();

  enum EmitterMode {
    Normal,

    // Emit JSOp::GetIntrinsic instead of JSOp::GetName and assert that
    // JSOp::GetName and JSOp::*GName don't ever get emitted. See the comment
    // for the field |selfHostingMode| in Parser.h for details.
    SelfHosting,

    // Check the static scope chain of the root function for resolving free
    // variable accesses in the script.
    LazyFunction
  };

  const EmitterMode emitterMode = Normal;

  mozilla::Maybe<uint32_t> scriptStartOffset = {};

  // The end location of a function body that is being emitted.
  mozilla::Maybe<uint32_t> functionBodyEndPos = {};

  // Mask of operation kinds which need instrumentation. This is obtained from
  // the compile options and copied here for efficiency.
  uint32_t instrumentationKinds = 0;

  /*
   * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
   * space above their tempMark points. This means that you cannot alloc from
   * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
   * destruction.
   */
 private:
  // Internal constructor, for delegation use only.
  BytecodeEmitter(
      BytecodeEmitter* parent, SharedContext* sc, JS::Handle<JSScript*> script,
      JS::Handle<BaseScript*> lazyScript, uint32_t line, uint32_t column,
      CompilationInfo& compilationInfo, EmitterMode emitterMode,
      FieldInitializers fieldInitializers = FieldInitializers::Invalid());

  void initFromBodyPosition(TokenPos bodyPosition);

  /*
   * Helper for reporting that we have insufficient args.  pluralizer
   * should be "s" if requiredArgs is anything other than "1" and ""
   * if requiredArgs is "1".
   */
  void reportNeedMoreArgsError(ParseNode* pn, const char* errorName,
                               const char* requiredArgs, const char* pluralizer,
                               const ListNode* argsList);

 public:
  BytecodeEmitter(
      BytecodeEmitter* parent, BCEParserHandle* parser, SharedContext* sc,
      JS::Handle<JSScript*> script, JS::Handle<BaseScript*> lazyScript,
      uint32_t line, uint32_t column, CompilationInfo& compilationInfo,
      EmitterMode emitterMode = Normal,
      FieldInitializers fieldInitializers = FieldInitializers::Invalid());

  BytecodeEmitter(
      BytecodeEmitter* parent, const EitherParser& parser, SharedContext* sc,
      JS::Handle<JSScript*> script, JS::Handle<BaseScript*> lazyScript,
      uint32_t line, uint32_t column, CompilationInfo& compilationInfo,
      EmitterMode emitterMode = Normal,
      FieldInitializers fieldInitializers = FieldInitializers::Invalid());

  template <typename Unit>
  BytecodeEmitter(
      BytecodeEmitter* parent, Parser<FullParseHandler, Unit>* parser,
      SharedContext* sc, JS::Handle<JSScript*> script,
      JS::Handle<BaseScript*> lazyScript, uint32_t line, uint32_t column,
      CompilationInfo& compilationInfo, EmitterMode emitterMode = Normal,
      FieldInitializers fieldInitializers = FieldInitializers::Invalid())
      : BytecodeEmitter(parent, EitherParser(parser), sc, script, lazyScript,
                        line, column, compilationInfo, emitterMode,
                        fieldInitializers) {}

  MOZ_MUST_USE bool init();
  MOZ_MUST_USE bool init(TokenPos bodyPosition);

  template <typename T>
  T* findInnermostNestableControl() const;

  template <typename T, typename Predicate /* (T*) -> bool */>
  T* findInnermostNestableControl(Predicate predicate) const;

  NameLocation lookupName(JSAtom* name);

  // To implement Annex B and the formal parameter defaults scope semantics
  // requires accessing names that would otherwise be shadowed. This method
  // returns the access location of a name that is known to be bound in a
  // target scope.
  mozilla::Maybe<NameLocation> locationOfNameBoundInScope(JSAtom* name,
                                                          EmitterScope* target);

  // Get the location of a name known to be bound in the function scope,
  // starting at the source scope.
  mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(
      JSAtom* name, EmitterScope* source);

  mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(
      JSAtom* name) {
    return locationOfNameBoundInFunctionScope(name, innermostEmitterScope());
  }

  void setVarEmitterScope(EmitterScope* emitterScope) {
    MOZ_ASSERT(emitterScope);
    MOZ_ASSERT(!varEmitterScope);
    varEmitterScope = emitterScope;
  }

  AbstractScopePtr outermostScope() const {
    return perScriptData().gcThingList().firstScope();
  }
  AbstractScopePtr innermostScope() const;
  AbstractScopePtr bodyScope() const {
    return perScriptData().gcThingList().getScope(bodyScopeIndex);
  }

  MOZ_ALWAYS_INLINE
  MOZ_MUST_USE bool makeAtomIndex(JSAtom* atom, uint32_t* indexp) {
    MOZ_ASSERT(perScriptData().atomIndices());
    AtomIndexMap::AddPtr p = perScriptData().atomIndices()->lookupForAdd(atom);
    if (p) {
      *indexp = p->value();
      return true;
    }

    uint32_t index = perScriptData().atomIndices()->count();
    if (!perScriptData().atomIndices()->add(p, atom, index)) {
      ReportOutOfMemory(cx);
      return false;
    }

    *indexp = index;
    return true;
  }

  bool isInLoop();
  MOZ_MUST_USE bool checkSingletonContext();

  // Check whether our function is in a run-once context (a toplevel
  // run-one script or a run-once lambda).
  MOZ_MUST_USE bool checkRunOnceContext();

  bool needsImplicitThis();

  MOZ_MUST_USE bool emitThisEnvironmentCallee();
  MOZ_MUST_USE bool emitSuperBase();

  uint32_t mainOffset() const { return *mainOffset_; }

  bool inPrologue() const { return mainOffset_.isNothing(); }

  MOZ_MUST_USE bool switchToMain() {
    MOZ_ASSERT(inPrologue());
    mainOffset_.emplace(bytecodeSection().code().length());

    return emitInstrumentation(InstrumentationKind::Main);
  }

  void setFunctionBodyEndPos(uint32_t pos) {
    functionBodyEndPos = mozilla::Some(pos);
  }

  void setScriptStartOffsetIfUnset(uint32_t pos) {
    if (scriptStartOffset.isNothing()) {
      scriptStartOffset = mozilla::Some(pos);
    }
  }

  void reportError(ParseNode* pn, unsigned errorNumber, ...);
  void reportError(const mozilla::Maybe<uint32_t>& maybeOffset,
                   unsigned errorNumber, ...);

  // If pn contains a useful expression, return true with *answer set to true.
  // If pn contains a useless expression, return true with *answer set to
  // false. Return false on error.
  //
  // The caller should initialize *answer to false and invoke this function on
  // an expression statement or similar subtree to decide whether the tree
  // could produce code that has any side effects.  For an expression
  // statement, we define useless code as code with no side effects, because
  // the main effect, the value left on the stack after the code executes,
  // will be discarded by a pop bytecode.
  MOZ_MUST_USE bool checkSideEffects(ParseNode* pn, bool* answer);

#ifdef DEBUG
  MOZ_MUST_USE bool checkStrictOrSloppy(JSOp op);
#endif

  // Add TryNote to the tryNoteList array. The start and end offset are
  // relative to current section.
  MOZ_MUST_USE bool addTryNote(TryNoteKind kind, uint32_t stackDepth,
                               BytecodeOffset start, BytecodeOffset end);

  // Append a new source note of the given type (and therefore size) to the
  // notes dynamic array, updating noteCount. Return the new note's index
  // within the array pointed at by current->notes as outparam.
  MOZ_MUST_USE bool newSrcNote(SrcNoteType type, unsigned* indexp = nullptr);
  MOZ_MUST_USE bool newSrcNote2(SrcNoteType type, ptrdiff_t offset,
                                unsigned* indexp = nullptr);
  MOZ_MUST_USE bool newSrcNote3(SrcNoteType type, ptrdiff_t offset1,
                                ptrdiff_t offset2, unsigned* indexp = nullptr);

  MOZ_MUST_USE bool setSrcNoteOffset(unsigned index, unsigned which,
                                     BytecodeOffsetDiff offset);

  // Control whether emitTree emits a line number note.
  enum EmitLineNumberNote { EMIT_LINENOTE, SUPPRESS_LINENOTE };

  // Emit code for the tree rooted at pn.
  MOZ_MUST_USE bool emitTree(ParseNode* pn,
                             ValueUsage valueUsage = ValueUsage::WantValue,
                             EmitLineNumberNote emitLineNote = EMIT_LINENOTE,
                             bool isInner = false);

  MOZ_MUST_USE bool emitOptionalTree(
      ParseNode* pn, OptionalEmitter& oe,
      ValueUsage valueUsage = ValueUsage::WantValue);

  // Emit global, eval, or module code for tree rooted at body. Always
  // encompasses the entire source.
  MOZ_MUST_USE bool emitScript(ParseNode* body);

  // Calculate the `nslots` value for BCEScriptStencil constructor parameter.
  // Fails if it overflows.
  MOZ_MUST_USE bool getNslots(uint32_t* nslots);

  // Emit function code for the tree rooted at body.
  enum class TopLevelFunction { No, Yes };
  MOZ_MUST_USE bool emitFunctionScript(FunctionNode* funNode,
                                       TopLevelFunction isTopLevel);

  MOZ_MUST_USE bool markStepBreakpoint();
  MOZ_MUST_USE bool markSimpleBreakpoint();
  MOZ_MUST_USE bool updateLineNumberNotes(uint32_t offset);
  MOZ_MUST_USE bool updateSourceCoordNotes(uint32_t offset);

  JSOp strictifySetNameOp(JSOp op);

  MOZ_MUST_USE bool emitCheck(JSOp op, ptrdiff_t delta, BytecodeOffset* offset);

  // Emit one bytecode.
  MOZ_MUST_USE bool emit1(JSOp op);

  // Emit two bytecodes, an opcode (op) with a byte of immediate operand
  // (op1).
  MOZ_MUST_USE bool emit2(JSOp op, uint8_t op1);

  // Emit three bytecodes, an opcode with two bytes of immediate operands.
  MOZ_MUST_USE bool emit3(JSOp op, jsbytecode op1, jsbytecode op2);

  // Helper to duplicate one or more stack values. |slotFromTop| is the value's
  // depth on the JS stack, as measured from the top. |count| is the number of
  // values to duplicate, in theiro original order.
  MOZ_MUST_USE bool emitDupAt(unsigned slotFromTop, unsigned count = 1);

  // Helper to emit JSOp::Pop or JSOp::PopN.
  MOZ_MUST_USE bool emitPopN(unsigned n);

  // Helper to emit JSOp::CheckIsObj.
  MOZ_MUST_USE bool emitCheckIsObj(CheckIsObjectKind kind);

  // Helper to emit JSOp::CheckIsCallable.
  MOZ_MUST_USE bool emitCheckIsCallable(CheckIsCallableKind kind);

  // Push whether the value atop of the stack is non-undefined and non-null.
  MOZ_MUST_USE bool emitPushNotUndefinedOrNull();

  // Emit a bytecode followed by an uint16 immediate operand stored in
  // big-endian order.
  MOZ_MUST_USE bool emitUint16Operand(JSOp op, uint32_t operand);

  // Emit a bytecode followed by an uint32 immediate operand.
  MOZ_MUST_USE bool emitUint32Operand(JSOp op, uint32_t operand);

  // Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
  MOZ_MUST_USE bool emitN(JSOp op, size_t extra,
                          BytecodeOffset* offset = nullptr);

  MOZ_MUST_USE bool emitDouble(double dval);
  MOZ_MUST_USE bool emitNumberOp(double dval);

  MOZ_MUST_USE bool emitBigIntOp(BigIntLiteral* bigint);

  MOZ_MUST_USE bool emitThisLiteral(ThisLiteral* pn);
  MOZ_MUST_USE bool emitGetFunctionThis(NameNode* thisName);
  MOZ_MUST_USE bool emitGetFunctionThis(const mozilla::Maybe<uint32_t>& offset);
  MOZ_MUST_USE bool emitGetThisForSuperBase(UnaryNode* superBase);
  MOZ_MUST_USE bool emitSetThis(BinaryNode* setThisNode);
  MOZ_MUST_USE bool emitCheckDerivedClassConstructorReturn();

  // Handle jump opcodes and jump targets.
  MOZ_MUST_USE bool emitJumpTargetOp(JSOp op, BytecodeOffset* off);
  MOZ_MUST_USE bool emitJumpTarget(JumpTarget* target);
  MOZ_MUST_USE bool emitJumpNoFallthrough(JSOp op, JumpList* jump);
  MOZ_MUST_USE bool emitJump(JSOp op, JumpList* jump);
  void patchJumpsToTarget(JumpList jump, JumpTarget target);
  MOZ_MUST_USE bool emitJumpTargetAndPatch(JumpList jump);

  MOZ_MUST_USE bool emitCall(JSOp op, uint16_t argc,
                             const mozilla::Maybe<uint32_t>& sourceCoordOffset);
  MOZ_MUST_USE bool emitCall(JSOp op, uint16_t argc, ParseNode* pn = nullptr);
  MOZ_MUST_USE bool emitCallIncDec(UnaryNode* incDec);

  mozilla::Maybe<uint32_t> getOffsetForLoop(ParseNode* nextpn);

  enum class GotoKind { Break, Continue };
  MOZ_MUST_USE bool emitGoto(NestableControl* target, JumpList* jumplist,
                             GotoKind kind);

  MOZ_MUST_USE bool emitIndexOp(JSOp op, uint32_t index);

  MOZ_MUST_USE bool emitAtomOp(
      JSOp op, JSAtom* atom,
      ShouldInstrument shouldInstrument = ShouldInstrument::No);
  MOZ_MUST_USE bool emitAtomOp(
      JSOp op, uint32_t atomIndex,
      ShouldInstrument shouldInstrument = ShouldInstrument::No);

  MOZ_MUST_USE bool emitArrayLiteral(ListNode* array);
  MOZ_MUST_USE bool emitArray(ParseNode* arrayHead, uint32_t count,
                              bool isInner = false);

  MOZ_MUST_USE bool emitInternedScopeOp(uint32_t index, JSOp op);
  MOZ_MUST_USE bool emitInternedObjectOp(uint32_t index, JSOp op);
  MOZ_MUST_USE bool emitObjectPairOp(uint32_t index1, uint32_t index2, JSOp op);
  MOZ_MUST_USE bool emitRegExp(uint32_t index);

  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitFunction(
      FunctionNode* funNode, bool needsProto = false,
      ListNode* classContentsIfConstructor = nullptr);
  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitObject(ListNode* objNode,
                                                bool isInner = false);

  MOZ_MUST_USE bool emitHoistedFunctionsInList(ListNode* stmtList);

  // Can we use the object-literal writer either in singleton-object mode (with
  // values) or in template mode (field names only, no values) for the property
  // list?
  void isPropertyListObjLiteralCompatible(ListNode* obj, bool* withValues,
                                          bool* withoutValues);
  bool isArrayObjLiteralCompatible(ParseNode* arrayHead);

  MOZ_MUST_USE bool emitPropertyList(ListNode* obj, PropertyEmitter& pe,
                                     PropListType type, bool isInner = false);

  MOZ_MUST_USE bool emitPropertyListObjLiteral(ListNode* obj,
                                               ObjLiteralFlags flags);

  MOZ_MUST_USE bool emitDestructuringRestExclusionSetObjLiteral(
      ListNode* pattern);

  MOZ_MUST_USE bool emitObjLiteralArray(ParseNode* arrayHead, bool isCow);

  // Is a field value OBJLITERAL-compatible?
  MOZ_MUST_USE bool isRHSObjLiteralCompatible(ParseNode* value);

  MOZ_MUST_USE bool emitObjLiteralValue(ObjLiteralCreationData* data,
                                        ParseNode* value);

  enum class FieldPlacement { Instance, Static };
  mozilla::Maybe<FieldInitializers> setupFieldInitializers(
      ListNode* classMembers, FieldPlacement placement);
  MOZ_MUST_USE bool emitCreateFieldKeys(ListNode* obj,
                                        FieldPlacement placement);
  MOZ_MUST_USE bool emitCreateFieldInitializers(ClassEmitter& ce, ListNode* obj,
                                                FieldPlacement placement);
  const FieldInitializers& findFieldInitializersForCall();
  MOZ_MUST_USE bool emitInitializeInstanceFields();
  MOZ_MUST_USE bool emitInitializeStaticFields(ListNode* classMembers);

  // To catch accidental misuse, emitUint16Operand/emit3 assert that they are
  // not used to unconditionally emit JSOp::GetLocal. Variable access should
  // instead be emitted using EmitVarOp. In special cases, when the caller
  // definitely knows that a given local slot is unaliased, this function may be
  // used as a non-asserting version of emitUint16Operand.
  MOZ_MUST_USE bool emitLocalOp(JSOp op, uint32_t slot);

  MOZ_MUST_USE bool emitArgOp(JSOp op, uint16_t slot);
  MOZ_MUST_USE bool emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec);

  MOZ_MUST_USE bool emitGetNameAtLocation(Handle<JSAtom*> name,
                                          const NameLocation& loc);
  MOZ_MUST_USE bool emitGetNameAtLocation(ImmutablePropertyNamePtr name,
                                          const NameLocation& loc) {
    return emitGetNameAtLocation(name.toHandle(), loc);
  }
  MOZ_MUST_USE bool emitGetName(Handle<JSAtom*> name) {
    return emitGetNameAtLocation(name, lookupName(name));
  }
  MOZ_MUST_USE bool emitGetName(ImmutablePropertyNamePtr name) {
    return emitGetNameAtLocation(name, lookupName(name));
  }
  MOZ_MUST_USE bool emitGetName(NameNode* name);

  MOZ_MUST_USE bool emitTDZCheckIfNeeded(HandleAtom name,
                                         const NameLocation& loc,
                                         ValueIsOnStack isOnStack);

  MOZ_MUST_USE bool emitNameIncDec(UnaryNode* incDec);

  MOZ_MUST_USE bool emitDeclarationList(ListNode* declList);
  MOZ_MUST_USE bool emitSingleDeclaration(ListNode* declList, NameNode* decl,
                                          ParseNode* initializer);
  MOZ_MUST_USE bool emitAssignmentRhs(ParseNode* rhs,
                                      HandleAtom anonFunctionName);
  MOZ_MUST_USE bool emitAssignmentRhs(uint8_t offset);

  MOZ_MUST_USE bool emitPrepareIteratorResult();
  MOZ_MUST_USE bool emitFinishIteratorResult(bool done);
  MOZ_MUST_USE bool iteratorResultShape(uint32_t* shape);

  MOZ_MUST_USE bool emitGetDotGeneratorInInnermostScope() {
    return emitGetDotGeneratorInScope(*innermostEmitterScope());
  }
  MOZ_MUST_USE bool emitGetDotGeneratorInScope(EmitterScope& currentScope);

  MOZ_MUST_USE bool allocateResumeIndex(BytecodeOffset offset,
                                        uint32_t* resumeIndex);
  MOZ_MUST_USE bool allocateResumeIndexRange(
      mozilla::Span<BytecodeOffset> offsets, uint32_t* firstResumeIndex);

  MOZ_MUST_USE bool emitInitialYield(UnaryNode* yieldNode);
  MOZ_MUST_USE bool emitYield(UnaryNode* yieldNode);
  MOZ_MUST_USE bool emitYieldOp(JSOp op);
  MOZ_MUST_USE bool emitYieldStar(ParseNode* iter);
  MOZ_MUST_USE bool emitAwaitInInnermostScope() {
    return emitAwaitInScope(*innermostEmitterScope());
  }
  MOZ_MUST_USE bool emitAwaitInInnermostScope(UnaryNode* awaitNode);
  MOZ_MUST_USE bool emitAwaitInScope(EmitterScope& currentScope);

  MOZ_MUST_USE bool emitPushResumeKind(GeneratorResumeKind kind);

  MOZ_MUST_USE bool emitPropLHS(PropertyAccess* prop);
  MOZ_MUST_USE bool emitPropIncDec(UnaryNode* incDec);

  MOZ_MUST_USE bool emitComputedPropertyName(UnaryNode* computedPropName);

  // Emit bytecode to put operands for a JSOp::GetElem/CallElem/SetElem/DelElem
  // opcode onto the stack in the right order. In the case of SetElem, the
  // value to be assigned must already be pushed.
  enum class EmitElemOption { Get, Call, IncDec, CompoundAssign, Ref };
  MOZ_MUST_USE bool emitElemOperands(PropertyByValue* elem,
                                     EmitElemOption opts);

  MOZ_MUST_USE bool emitElemObjAndKey(PropertyByValue* elem, bool isSuper,
                                      ElemOpEmitter& eoe);
  MOZ_MUST_USE bool emitElemOpBase(
      JSOp op, ShouldInstrument shouldInstrument = ShouldInstrument::No);
  MOZ_MUST_USE bool emitElemOp(PropertyByValue* elem, JSOp op);
  MOZ_MUST_USE bool emitElemIncDec(UnaryNode* incDec);

  MOZ_MUST_USE bool emitCatch(BinaryNode* catchClause);
  MOZ_MUST_USE bool emitIf(TernaryNode* ifNode);
  MOZ_MUST_USE bool emitWith(BinaryNode* withNode);

  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLabeledStatement(
      const LabeledStatement* labeledStmt);
  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLexicalScope(
      LexicalScopeNode* lexicalScope);
  MOZ_MUST_USE bool emitLexicalScopeBody(
      ParseNode* body, EmitLineNumberNote emitLineNote = EMIT_LINENOTE);
  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitSwitch(SwitchStatement* switchStmt);
  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitTry(TryNode* tryNode);

  MOZ_MUST_USE bool emitGoSub(JumpList* jump);

  // emitDestructuringLHSRef emits the lhs expression's reference.
  // If the lhs expression is object property |OBJ.prop|, it emits |OBJ|.
  // If it's object element |OBJ[ELEM]|, it emits |OBJ| and |ELEM|.
  // If there's nothing to evaluate for the reference, it emits nothing.
  // |emitted| parameter receives the number of values pushed onto the stack.
  MOZ_MUST_USE bool emitDestructuringLHSRef(ParseNode* target, size_t* emitted);

  // emitSetOrInitializeDestructuring assumes the lhs expression's reference
  // and the to-be-destructured value has been pushed on the stack.  It emits
  // code to destructure a single lhs expression (either a name or a compound
  // []/{} expression).
  MOZ_MUST_USE bool emitSetOrInitializeDestructuring(ParseNode* target,
                                                     DestructuringFlavor flav);

  // emitDestructuringObjRestExclusionSet emits the property exclusion set
  // for the rest-property in an object pattern.
  MOZ_MUST_USE bool emitDestructuringObjRestExclusionSet(ListNode* pattern);

  // emitDestructuringOps assumes the to-be-destructured value has been
  // pushed on the stack and emits code to destructure each part of a [] or
  // {} lhs expression.
  MOZ_MUST_USE bool emitDestructuringOps(ListNode* pattern,
                                         DestructuringFlavor flav);
  MOZ_MUST_USE bool emitDestructuringOpsArray(ListNode* pattern,
                                              DestructuringFlavor flav);
  MOZ_MUST_USE bool emitDestructuringOpsObject(ListNode* pattern,
                                               DestructuringFlavor flav);

  enum class CopyOption { Filtered, Unfiltered };

  // Calls either the |CopyDataProperties| or the
  // |CopyDataPropertiesUnfiltered| intrinsic function, consumes three (or
  // two in the latter case) elements from the stack.
  MOZ_MUST_USE bool emitCopyDataProperties(CopyOption option);

  // emitIterator expects the iterable to already be on the stack.
  // It will replace that stack value with the corresponding iterator
  MOZ_MUST_USE bool emitIterator();

  MOZ_MUST_USE bool emitAsyncIterator();

  // Pops iterator from the top of the stack. Pushes the result of |.next()|
  // onto the stack.
  MOZ_MUST_USE bool emitIteratorNext(
      const mozilla::Maybe<uint32_t>& callSourceCoordOffset,
      IteratorKind kind = IteratorKind::Sync, bool allowSelfHosted = false);
  MOZ_MUST_USE bool emitIteratorCloseInScope(
      EmitterScope& currentScope, IteratorKind iterKind = IteratorKind::Sync,
      CompletionKind completionKind = CompletionKind::Normal,
      bool allowSelfHosted = false);
  MOZ_MUST_USE bool emitIteratorCloseInInnermostScope(
      IteratorKind iterKind = IteratorKind::Sync,
      CompletionKind completionKind = CompletionKind::Normal,
      bool allowSelfHosted = false) {
    return emitIteratorCloseInScope(*innermostEmitterScope(), iterKind,
                                    completionKind, allowSelfHosted);
  }

  template <typename InnerEmitter>
  MOZ_MUST_USE bool wrapWithDestructuringTryNote(int32_t iterDepth,
                                                 InnerEmitter emitter);

  MOZ_MUST_USE bool defineHoistedTopLevelFunctions(ParseNode* body);

  // Check if the value on top of the stack is "undefined". If so, replace
  // that value on the stack with the value defined by |defaultExpr|.
  // |pattern| is a lhs node of the default expression.  If it's an
  // identifier and |defaultExpr| is an anonymous function, |SetFunctionName|
  // is called at compile time.
  MOZ_MUST_USE bool emitDefault(ParseNode* defaultExpr, ParseNode* pattern);

  MOZ_MUST_USE bool emitAnonymousFunctionWithName(ParseNode* node,
                                                  JS::Handle<JSAtom*> name);

  MOZ_MUST_USE bool emitAnonymousFunctionWithComputedName(
      ParseNode* node, FunctionPrefixKind prefixKind);

  MOZ_MUST_USE bool setFunName(FunctionBox* fun, JSAtom* name);
  MOZ_MUST_USE bool emitInitializer(ParseNode* initializer, ParseNode* pattern);

  MOZ_MUST_USE bool emitCallSiteObjectArray(ListNode* cookedOrRaw,
                                            uint32_t* arrayIndex);
  MOZ_MUST_USE bool emitCallSiteObject(CallSiteNode* callSiteObj);
  MOZ_MUST_USE bool emitTemplateString(ListNode* templateString);
  MOZ_MUST_USE bool emitAssignmentOrInit(ParseNodeKind kind, ParseNode* lhs,
                                         ParseNode* rhs);

  MOZ_MUST_USE bool emitReturn(UnaryNode* returnNode);
  MOZ_MUST_USE bool emitExpressionStatement(UnaryNode* exprStmt);
  MOZ_MUST_USE bool emitStatementList(ListNode* stmtList);

  MOZ_MUST_USE bool emitDeleteName(UnaryNode* deleteNode);
  MOZ_MUST_USE bool emitDeleteProperty(UnaryNode* deleteNode);
  MOZ_MUST_USE bool emitDeleteElement(UnaryNode* deleteNode);
  MOZ_MUST_USE bool emitDeleteExpression(UnaryNode* deleteNode);

  // Optional methods which emit Optional Jump Target
  MOZ_MUST_USE bool emitOptionalChain(UnaryNode* expr, ValueUsage valueUsage);
  MOZ_MUST_USE bool emitCalleeAndThisForOptionalChain(UnaryNode* expr,
                                                      CallNode* callNode,
                                                      CallOrNewEmitter& cone);
  MOZ_MUST_USE bool emitDeleteOptionalChain(UnaryNode* deleteNode);

  // Optional methods which emit a shortCircuit jump. They need to be called by
  // a method which emits an Optional Jump Target, see below.
  MOZ_MUST_USE bool emitOptionalDotExpression(PropertyAccessBase* expr,
                                              PropOpEmitter& poe, bool isSuper,
                                              OptionalEmitter& oe);
  MOZ_MUST_USE bool emitOptionalElemExpression(PropertyByValueBase* elem,
                                               ElemOpEmitter& poe, bool isSuper,
                                               OptionalEmitter& oe);
  MOZ_MUST_USE bool emitOptionalCall(CallNode* callNode, OptionalEmitter& oe,
                                     ValueUsage valueUsage);
  MOZ_MUST_USE bool emitDeletePropertyInOptChain(PropertyAccessBase* propExpr,
                                                 OptionalEmitter& oe);
  MOZ_MUST_USE bool emitDeleteElementInOptChain(PropertyByValueBase* elemExpr,
                                                OptionalEmitter& oe);

  // |op| must be JSOp::Typeof or JSOp::TypeofExpr.
  MOZ_MUST_USE bool emitTypeof(UnaryNode* typeofNode, JSOp op);

  MOZ_MUST_USE bool emitUnary(UnaryNode* unaryNode);
  MOZ_MUST_USE bool emitRightAssociative(ListNode* node);
  MOZ_MUST_USE bool emitLeftAssociative(ListNode* node);
  MOZ_MUST_USE bool emitShortCircuit(ListNode* node);
  MOZ_MUST_USE bool emitSequenceExpr(
      ListNode* node, ValueUsage valueUsage = ValueUsage::WantValue);

  MOZ_NEVER_INLINE MOZ_MUST_USE bool emitIncOrDec(UnaryNode* incDec);

  MOZ_MUST_USE bool emitConditionalExpression(
      ConditionalExpression& conditional,
      ValueUsage valueUsage = ValueUsage::WantValue);

  bool isRestParameter(ParseNode* expr);

  MOZ_MUST_USE ParseNode* getCoordNode(ParseNode* callNode,
                                       ParseNode* calleeNode, JSOp op,
                                       ListNode* argsList);

  MOZ_MUST_USE bool emitArguments(ListNode* argsList, bool isCall,
                                  bool isSpread, CallOrNewEmitter& cone);
  MOZ_MUST_USE bool emitCallOrNew(
      CallNode* callNode, ValueUsage valueUsage = ValueUsage::WantValue);
  MOZ_MUST_USE bool emitSelfHostedCallFunction(CallNode* callNode);
  MOZ_MUST_USE bool emitSelfHostedResumeGenerator(BinaryNode* callNode);
  MOZ_MUST_USE bool emitSelfHostedForceInterpreter();
  MOZ_MUST_USE bool emitSelfHostedAllowContentIter(BinaryNode* callNode);
  MOZ_MUST_USE bool emitSelfHostedDefineDataProperty(BinaryNode* callNode);
  MOZ_MUST_USE bool emitSelfHostedGetPropertySuper(BinaryNode* callNode);
  MOZ_MUST_USE bool emitSelfHostedHasOwn(BinaryNode* callNode);

  MOZ_MUST_USE bool emitDo(BinaryNode* doNode);
  MOZ_MUST_USE bool emitWhile(BinaryNode* whileNode);

  MOZ_MUST_USE bool emitFor(
      ForNode* forNode, const EmitterScope* headLexicalEmitterScope = nullptr);
  MOZ_MUST_USE bool emitCStyleFor(ForNode* forNode,
                                  const EmitterScope* headLexicalEmitterScope);
  MOZ_MUST_USE bool emitForIn(ForNode* forNode,
                              const EmitterScope* headLexicalEmitterScope);
  MOZ_MUST_USE bool emitForOf(ForNode* forNode,
                              const EmitterScope* headLexicalEmitterScope);

  MOZ_MUST_USE bool emitInitializeForInOrOfTarget(TernaryNode* forHead);

  MOZ_MUST_USE bool emitBreak(PropertyName* label);
  MOZ_MUST_USE bool emitContinue(PropertyName* label);

  MOZ_MUST_USE bool emitFunctionFormalParameters(ListNode* paramsBody);
  MOZ_MUST_USE bool emitInitializeFunctionSpecialNames();
  MOZ_MUST_USE bool emitLexicalInitialization(NameNode* name);
  MOZ_MUST_USE bool emitLexicalInitialization(Handle<JSAtom*> name);

  // Emit bytecode for the spread operator.
  //
  // emitSpread expects the current index (I) of the array, the array itself
  // and the iterator to be on the stack in that order (iterator on the bottom).
  // It will pop the iterator and I, then iterate over the iterator by calling
  // |.next()| and put the results into the I-th element of array with
  // incrementing I, then push the result I (it will be original I +
  // iteration count). The stack after iteration will look like |ARRAY INDEX|.
  MOZ_MUST_USE bool emitSpread(bool allowSelfHosted = false);

  enum class ClassNameKind {
    // The class name is defined through its BindingIdentifier, if present.
    BindingName,

    // The class is anonymous and has a statically inferred name.
    InferredName,

    // The class is anonymous and has a dynamically computed name.
    ComputedName
  };

  MOZ_MUST_USE bool emitClass(
      ClassNode* classNode, ClassNameKind nameKind = ClassNameKind::BindingName,
      JS::Handle<JSAtom*> nameForAnonymousClass = nullptr);
  MOZ_MUST_USE bool emitSuperElemOperands(
      PropertyByValue* elem, EmitElemOption opts = EmitElemOption::Get);
  MOZ_MUST_USE bool emitSuperGetElem(PropertyByValue* elem,
                                     bool isCall = false);

  MOZ_MUST_USE bool emitCalleeAndThis(ParseNode* callee, ParseNode* call,
                                      CallOrNewEmitter& cone);

  MOZ_MUST_USE bool emitOptionalCalleeAndThis(ParseNode* callee, CallNode* call,
                                              CallOrNewEmitter& cone,
                                              OptionalEmitter& oe);

  MOZ_MUST_USE bool emitPipeline(ListNode* node);

  MOZ_MUST_USE bool emitExportDefault(BinaryNode* exportNode);

  MOZ_MUST_USE bool emitReturnRval() {
    return emitInstrumentation(InstrumentationKind::Exit) &&
           emit1(JSOp::RetRval);
  }

  MOZ_MUST_USE bool emitInstrumentation(InstrumentationKind kind,
                                        uint32_t npopped = 0) {
    return MOZ_LIKELY(!instrumentationKinds) ||
           emitInstrumentationSlow(kind, std::function<bool(uint32_t)>());
  }

  MOZ_MUST_USE bool emitInstrumentationForOpcode(JSOp op, uint32_t atomIndex) {
    return MOZ_LIKELY(!instrumentationKinds) ||
           emitInstrumentationForOpcodeSlow(op, atomIndex);
  }

 private:
  MOZ_MUST_USE bool emitInstrumentationSlow(
      InstrumentationKind kind,
      const std::function<bool(uint32_t)>& pushOperandsCallback);
  MOZ_MUST_USE bool emitInstrumentationForOpcodeSlow(JSOp op,
                                                     uint32_t atomIndex);
};

class MOZ_RAII AutoCheckUnstableEmitterScope {
#ifdef DEBUG
  bool prev_;
  BytecodeEmitter* bce_;
#endif

 public:
  AutoCheckUnstableEmitterScope() = delete;
  explicit AutoCheckUnstableEmitterScope(BytecodeEmitter* bce)
#ifdef DEBUG
      : bce_(bce)
#endif
  {
#ifdef DEBUG
    prev_ = bce_->unstableEmitterScope;
    bce_->unstableEmitterScope = true;
#endif
  }
  ~AutoCheckUnstableEmitterScope() {
#ifdef DEBUG
    bce_->unstableEmitterScope = prev_;
#endif
  }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeEmitter_h */
