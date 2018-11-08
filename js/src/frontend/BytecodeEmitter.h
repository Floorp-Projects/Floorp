/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS bytecode generation. */

#ifndef frontend_BytecodeEmitter_h
#define frontend_BytecodeEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Span.h"

#include "ds/InlineTable.h"
#include "frontend/BCEParserHandle.h"
#include "frontend/EitherParser.h"
#include "frontend/JumpList.h"
#include "frontend/NameFunctions.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "frontend/SourceNotes.h"
#include "frontend/ValueUsage.h"
#include "vm/BytecodeUtil.h"
#include "vm/Interpreter.h"
#include "vm/Iteration.h"

namespace js {
namespace frontend {

class CGNumberList {
    Vector<double> list;

  public:
    explicit CGNumberList(JSContext* cx) : list(cx) {}
    MOZ_MUST_USE bool append(double v) {
        return list.append(v);
    }
    size_t length() const { return list.length(); }
    void finish(mozilla::Span<GCPtrValue> array);
};

struct CGObjectList {
    uint32_t            length;     /* number of emitted so far objects */
    ObjectBox*          lastbox;   /* last emitted object */

    CGObjectList() : length(0), lastbox(nullptr) {}

    unsigned add(ObjectBox* objbox);
    void finish(mozilla::Span<GCPtrObject> array);
};

struct MOZ_STACK_CLASS CGScopeList {
    Rooted<GCVector<Scope*>> vector;

    explicit CGScopeList(JSContext* cx)
      : vector(cx, GCVector<Scope*>(cx))
    { }

    bool append(Scope* scope) { return vector.append(scope); }
    uint32_t length() const { return vector.length(); }
    void finish(mozilla::Span<GCPtrScope> array);
};

struct CGTryNoteList {
    Vector<JSTryNote> list;
    explicit CGTryNoteList(JSContext* cx) : list(cx) {}

    // Start/end offset are relative to main section and will be patch in
    // finish().

    MOZ_MUST_USE bool append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end);
    size_t length() const { return list.length(); }
    void finish(mozilla::Span<JSTryNote> array, uint32_t prologueLength);
};

struct CGScopeNote : public ScopeNote
{
    // The end offset. Used to compute the length; may need adjusting first if
    // in the prologue.
    uint32_t end;

    // Is the start offset in the prologue?
    bool startInPrologue;

    // Is the end offset in the prologue?
    bool endInPrologue;
};

struct CGScopeNoteList {
    Vector<CGScopeNote> list;
    explicit CGScopeNoteList(JSContext* cx) : list(cx) {}

    MOZ_MUST_USE bool append(uint32_t scopeIndex, uint32_t offset, bool inPrologue,
                             uint32_t parent);
    void recordEnd(uint32_t index, uint32_t offset, bool inPrologue);
    size_t length() const { return list.length(); }
    void finish(mozilla::Span<ScopeNote> array, uint32_t prologueLength);
};

struct CGResumeOffsetList {
    Vector<uint32_t> list;
    explicit CGResumeOffsetList(JSContext* cx) : list(cx) {}

    MOZ_MUST_USE bool append(uint32_t offset) { return list.append(offset); }
    size_t length() const { return list.length(); }
    void finish(mozilla::Span<uint32_t> array, uint32_t prologueLength);
};

// Have a few inline elements, so as to avoid heap allocation for tiny
// sequences.  See bug 1390526.
typedef Vector<jsbytecode, 64> BytecodeVector;
typedef Vector<jssrcnote, 64> SrcNotesVector;

class CallOrNewEmitter;
class ElemOpEmitter;
class EmitterScope;
class NestableControl;
class TDZCheckCache;

struct MOZ_STACK_CLASS BytecodeEmitter
{
    SharedContext* const sc;      /* context shared between parsing and bytecode generation */

    JSContext* const cx;

    BytecodeEmitter* const parent;  /* enclosing function or global context */

    Rooted<JSScript*> script;       /* the JSScript we're ultimately producing */

    Rooted<LazyScript*> lazyScript; /* the lazy script if mode is LazyFunction,
                                        nullptr otherwise. */

    struct EmitSection {
        BytecodeVector code;        /* bytecode */
        SrcNotesVector notes;       /* source notes, see below */
        ptrdiff_t   lastNoteOffset; /* code offset for last source note */

        // Line number for srcnotes.
        //
        // WARNING: If this becomes out of sync with already-emitted srcnotes,
        // we can get undefined behavior.
        uint32_t    currentLine;

        // Zero-based column index on currentLine of last SRC_COLSPAN-annotated
        // opcode.
        //
        // WARNING: If this becomes out of sync with already-emitted srcnotes,
        // we can get undefined behavior.
        uint32_t    lastColumn;

        JumpTarget lastTarget;      // Last jump target emitted.

        EmitSection(JSContext* cx, uint32_t lineNum)
          : code(cx), notes(cx), lastNoteOffset(0), currentLine(lineNum), lastColumn(0),
            lastTarget{ -1 - ptrdiff_t(JSOP_JUMPTARGET_LENGTH) }
        {}
    };
    EmitSection prologue, main, *current;

    // Private storage for parser wrapper. DO NOT REFERENCE INTERNALLY. May not be initialized.
    // Use |parser| instead.
    mozilla::Maybe<EitherParser> ep_;
    BCEParserHandle *parser;

    PooledMapPtr<AtomIndexMap> atomIndices; /* literals indexed for mapping */
    unsigned        firstLine;      /* first line, for JSScript::initFromEmitter */

    uint32_t        maxFixedSlots;  /* maximum number of fixed frame slots so far */
    uint32_t        maxStackDepth;  /* maximum number of expression stack slots so far */

    int32_t         stackDepth;     /* current stack depth in script frame */

    unsigned        emitLevel;      /* emitTree recursion level */

    uint32_t        bodyScopeIndex; /* index into scopeList of the body scope */

    EmitterScope*    varEmitterScope;
    NestableControl* innermostNestableControl;
    EmitterScope*    innermostEmitterScope_;
    TDZCheckCache*   innermostTDZCheckCache;

#ifdef DEBUG
    bool unstableEmitterScope;

    friend class AutoCheckUnstableEmitterScope;
#endif

    EmitterScope* innermostEmitterScope() const {
        MOZ_ASSERT(!unstableEmitterScope);
        return innermostEmitterScopeNoCheck();
    }
    EmitterScope* innermostEmitterScopeNoCheck() const {
        return innermostEmitterScope_;
    }

    CGNumberList     numberList;     /* number constants to be included with the script */
    CGObjectList     objectList;     /* list of emitted objects */
    CGScopeList      scopeList;      /* list of emitted scopes */
    CGTryNoteList    tryNoteList;    /* list of emitted try notes */
    CGScopeNoteList  scopeNoteList;  /* list of emitted block scope notes */

    // Certain ops (yield, await, gosub) have an entry in the script's
    // resumeOffsets list. This can be used to map from the op's resumeIndex to
    // the bytecode offset of the next pc. This indirection makes it easy to
    // resume in the JIT (because BaselineScript stores a resumeIndex => native
    // code array).
    CGResumeOffsetList resumeOffsetList;

    // Number of yield instructions emitted. Does not include JSOP_AWAIT.
    uint32_t        numYields;

    uint16_t        typesetCount;   /* Number of JOF_TYPESET opcodes generated */

    bool            hasSingletons:1;    /* script contains singleton initializer JSOP_OBJECT */

    bool            hasTryFinally:1;    /* script contains finally block */

    bool            emittingRunOnceLambda:1; /* true while emitting a lambda which is only
                                                expected to run once. */

    bool isRunOnceLambda();

    enum EmitterMode {
        Normal,

        /*
         * Emit JSOP_GETINTRINSIC instead of JSOP_GETNAME and assert that
         * JSOP_GETNAME and JSOP_*GNAME don't ever get emitted. See the comment
         * for the field |selfHostingMode| in Parser.h for details.
         */
        SelfHosting,

        /*
         * Check the static scope chain of the root function for resolving free
         * variable accesses in the script.
         */
        LazyFunction
    };

    const EmitterMode emitterMode;

    MOZ_INIT_OUTSIDE_CTOR uint32_t scriptStartOffset;
    bool scriptStartOffsetSet;

    // The end location of a function body that is being emitted.
    MOZ_INIT_OUTSIDE_CTOR uint32_t functionBodyEndPos;
    // Whether functionBodyEndPos was set.
    bool functionBodyEndPosSet;

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destruction.
     */
  private:
    // Internal constructor, for delegation use only.
    BytecodeEmitter(BytecodeEmitter* parent, SharedContext* sc, HandleScript script,
                    Handle<LazyScript*> lazyScript, uint32_t lineNum, EmitterMode emitterMode);

    void initFromBodyPosition(TokenPos bodyPosition);

  public:

    BytecodeEmitter(BytecodeEmitter* parent, BCEParserHandle* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, uint32_t lineNum,
                    EmitterMode emitterMode = Normal);

    BytecodeEmitter(BytecodeEmitter* parent, const EitherParser& parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, uint32_t lineNum,
                    EmitterMode emitterMode = Normal);

    template<typename Unit>
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler, Unit>* parser,
                    SharedContext* sc, HandleScript script, Handle<LazyScript*> lazyScript,
                    uint32_t lineNum, EmitterMode emitterMode = Normal)
      : BytecodeEmitter(parent, EitherParser(parser), sc, script, lazyScript,
                        lineNum, emitterMode)
    {}

    // An alternate constructor that uses a TokenPos for the starting
    // line and that sets functionBodyEndPos as well.
    BytecodeEmitter(BytecodeEmitter* parent, BCEParserHandle* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, TokenPos bodyPosition,
                    EmitterMode emitterMode = Normal)
        : BytecodeEmitter(parent, parser, sc, script, lazyScript,
                          parser->errorReporter().lineAt(bodyPosition.begin),
                          emitterMode)
    {
        initFromBodyPosition(bodyPosition);
    }

    BytecodeEmitter(BytecodeEmitter* parent, const EitherParser& parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, TokenPos bodyPosition,
                    EmitterMode emitterMode = Normal)
        : BytecodeEmitter(parent, parser, sc, script, lazyScript,
                          parser.errorReporter().lineAt(bodyPosition.begin),
                          emitterMode)
    {
        initFromBodyPosition(bodyPosition);
    }

    template<typename Unit>
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler, Unit>* parser,
                    SharedContext* sc, HandleScript script, Handle<LazyScript*> lazyScript,
                    TokenPos bodyPosition, EmitterMode emitterMode = Normal)
      : BytecodeEmitter(parent, EitherParser(parser), sc, script, lazyScript,
                        bodyPosition, emitterMode)
    {}

    MOZ_MUST_USE bool init();

    template <typename T>
    T* findInnermostNestableControl() const;

    template <typename T, typename Predicate /* (T*) -> bool */>
    T* findInnermostNestableControl(Predicate predicate) const;

    NameLocation lookupName(JSAtom* name);

    // To implement Annex B and the formal parameter defaults scope semantics
    // requires accessing names that would otherwise be shadowed. This method
    // returns the access location of a name that is known to be bound in a
    // target scope.
    mozilla::Maybe<NameLocation> locationOfNameBoundInScope(JSAtom* name, EmitterScope* target);

    // Get the location of a name known to be bound in the function scope,
    // starting at the source scope.
    mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(JSAtom* name,
                                                                    EmitterScope* source);

    mozilla::Maybe<NameLocation> locationOfNameBoundInFunctionScope(JSAtom* name) {
        return locationOfNameBoundInFunctionScope(name, innermostEmitterScope());
    }

    void setVarEmitterScope(EmitterScope* emitterScope) {
        MOZ_ASSERT(emitterScope);
        MOZ_ASSERT(!varEmitterScope);
        varEmitterScope = emitterScope;
    }

    Scope* outermostScope() const { return scopeList.vector[0]; }
    Scope* innermostScope() const;

    MOZ_ALWAYS_INLINE
    MOZ_MUST_USE bool makeAtomIndex(JSAtom* atom, uint32_t* indexp) {
        MOZ_ASSERT(atomIndices);
        AtomIndexMap::AddPtr p = atomIndices->lookupForAdd(atom);
        if (p) {
            *indexp = p->value();
            return true;
        }

        uint32_t index = atomIndices->count();
        if (!atomIndices->add(p, atom, index)) {
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

    void tellDebuggerAboutCompiledScript(JSContext* cx);

    BytecodeVector& code() const { return current->code; }
    jsbytecode* code(ptrdiff_t offset) const { return current->code.begin() + offset; }
    ptrdiff_t offset() const { return current->code.end() - current->code.begin(); }
    ptrdiff_t prologueOffset() const { return prologue.code.end() - prologue.code.begin(); }
    void switchToMain() { current = &main; }
    void switchToPrologue() { current = &prologue; }
    bool inPrologue() const { return current == &prologue; }

    SrcNotesVector& notes() const {
        // Prologue shouldn't have source notes.
        MOZ_ASSERT(!inPrologue());
        return current->notes;
    }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }

    // Check if the last emitted opcode is a jump target.
    bool lastOpcodeIsJumpTarget() const {
        return offset() - current->lastTarget.offset == ptrdiff_t(JSOP_JUMPTARGET_LENGTH);
    }

    // JumpTarget should not be part of the emitted statement, as they can be
    // aliased by multiple statements. If we included the jump target as part of
    // the statement we might have issues where the enclosing statement might
    // not contain all the opcodes of the enclosed statements.
    ptrdiff_t lastNonJumpTargetOffset() const {
        return lastOpcodeIsJumpTarget() ? current->lastTarget.offset : offset();
    }

    void setFunctionBodyEndPos(TokenPos pos) {
        functionBodyEndPos = pos.end;
        functionBodyEndPosSet = true;
    }

    void setScriptStartOffsetIfUnset(TokenPos pos) {
        if (!scriptStartOffsetSet) {
            scriptStartOffset = pos.begin;
            scriptStartOffsetSet = true;
        }
    }

    void reportError(ParseNode* pn, unsigned errorNumber, ...);
    void reportError(const mozilla::Maybe<uint32_t>& maybeOffset,
                     unsigned errorNumber, ...);
    bool reportExtraWarning(ParseNode* pn, unsigned errorNumber, ...);
    bool reportExtraWarning(const mozilla::Maybe<uint32_t>& maybeOffset,
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
    MOZ_MUST_USE bool addTryNote(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end);

    // Append a new source note of the given type (and therefore size) to the
    // notes dynamic array, updating noteCount. Return the new note's index
    // within the array pointed at by current->notes as outparam.
    MOZ_MUST_USE bool newSrcNote(SrcNoteType type, unsigned* indexp = nullptr);
    MOZ_MUST_USE bool newSrcNote2(SrcNoteType type, ptrdiff_t offset, unsigned* indexp = nullptr);
    MOZ_MUST_USE bool newSrcNote3(SrcNoteType type, ptrdiff_t offset1, ptrdiff_t offset2,
                                  unsigned* indexp = nullptr);

    void copySrcNotes(jssrcnote* destination, uint32_t nsrcnotes);
    MOZ_MUST_USE bool setSrcNoteOffset(unsigned index, unsigned which, ptrdiff_t offset);

    // NB: this function can add at most one extra extended delta note.
    MOZ_MUST_USE bool addToSrcNoteDelta(jssrcnote* sn, ptrdiff_t delta);

    // Finish taking source notes in cx's notePool. If successful, the final
    // source note count is stored in the out outparam.
    MOZ_MUST_USE bool finishTakingSrcNotes(uint32_t* out);

    // Control whether emitTree emits a line number note.
    enum EmitLineNumberNote {
        EMIT_LINENOTE,
        SUPPRESS_LINENOTE
    };

    // Emit code for the tree rooted at pn.
    MOZ_MUST_USE bool emitTree(ParseNode* pn, ValueUsage valueUsage = ValueUsage::WantValue,
                               EmitLineNumberNote emitLineNote = EMIT_LINENOTE);

    // Emit global, eval, or module code for tree rooted at body. Always
    // encompasses the entire source.
    MOZ_MUST_USE bool emitScript(ParseNode* body);

    // Emit function code for the tree rooted at body.
    enum class TopLevelFunction {
        No,
        Yes
    };
    MOZ_MUST_USE bool emitFunctionScript(CodeNode* funNode, TopLevelFunction isTopLevel);

    // If op is JOF_TYPESET (see the type barriers comment in TypeInference.h),
    // reserve a type set to store its result.
    void checkTypeSet(JSOp op);

    void updateDepth(ptrdiff_t target);
    MOZ_MUST_USE bool updateLineNumberNotes(uint32_t offset);
    MOZ_MUST_USE bool updateSourceCoordNotes(uint32_t offset);

    JSOp strictifySetNameOp(JSOp op);

    MOZ_MUST_USE bool emitCheck(ptrdiff_t delta, ptrdiff_t* offset);

    // Emit one bytecode.
    MOZ_MUST_USE bool emit1(JSOp op);

    // Emit two bytecodes, an opcode (op) with a byte of immediate operand
    // (op1).
    MOZ_MUST_USE bool emit2(JSOp op, uint8_t op1);

    // Emit three bytecodes, an opcode with two bytes of immediate operands.
    MOZ_MUST_USE bool emit3(JSOp op, jsbytecode op1, jsbytecode op2);

    // Helper to emit JSOP_DUPAT. The argument is the value's depth on the
    // JS stack, as measured from the top.
    MOZ_MUST_USE bool emitDupAt(unsigned slotFromTop);

    // Helper to emit JSOP_POP or JSOP_POPN.
    MOZ_MUST_USE bool emitPopN(unsigned n);

    // Helper to emit JSOP_CHECKISOBJ.
    MOZ_MUST_USE bool emitCheckIsObj(CheckIsObjectKind kind);

    // Helper to emit JSOP_CHECKISCALLABLE.
    MOZ_MUST_USE bool emitCheckIsCallable(CheckIsCallableKind kind);

    // Push whether the value atop of the stack is non-undefined and non-null.
    MOZ_MUST_USE bool emitPushNotUndefinedOrNull();

    // Emit a bytecode followed by an uint16 immediate operand stored in
    // big-endian order.
    MOZ_MUST_USE bool emitUint16Operand(JSOp op, uint32_t operand);

    // Emit a bytecode followed by an uint32 immediate operand.
    MOZ_MUST_USE bool emitUint32Operand(JSOp op, uint32_t operand);

    // Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
    MOZ_MUST_USE bool emitN(JSOp op, size_t extra, ptrdiff_t* offset = nullptr);

    MOZ_MUST_USE bool emitNumberOp(double dval);

    MOZ_MUST_USE bool emitThisLiteral(ThisLiteral* pn);
    MOZ_MUST_USE bool emitGetFunctionThis(NameNode* thisName);
    MOZ_MUST_USE bool emitGetFunctionThis(const mozilla::Maybe<uint32_t>& offset);
    MOZ_MUST_USE bool emitGetThisForSuperBase(UnaryNode* superBase);
    MOZ_MUST_USE bool emitSetThis(BinaryNode* setThisNode);
    MOZ_MUST_USE bool emitCheckDerivedClassConstructorReturn();

    // Handle jump opcodes and jump targets.
    MOZ_MUST_USE bool emitJumpTarget(JumpTarget* target);
    MOZ_MUST_USE bool emitJumpNoFallthrough(JSOp op, JumpList* jump);
    MOZ_MUST_USE bool emitJump(JSOp op, JumpList* jump);
    MOZ_MUST_USE bool emitBackwardJump(JSOp op, JumpTarget target, JumpList* jump,
                                       JumpTarget* fallthrough);
    void patchJumpsToTarget(JumpList jump, JumpTarget target);
    MOZ_MUST_USE bool emitJumpTargetAndPatch(JumpList jump);

    MOZ_MUST_USE bool emitCall(JSOp op, uint16_t argc,
                               const mozilla::Maybe<uint32_t>& sourceCoordOffset);
    MOZ_MUST_USE bool emitCall(JSOp op, uint16_t argc, ParseNode* pn = nullptr);
    MOZ_MUST_USE bool emitCallIncDec(UnaryNode* incDec);

    mozilla::Maybe<uint32_t> getOffsetForLoop(ParseNode* nextpn);

    MOZ_MUST_USE bool emitGoto(NestableControl* target, JumpList* jumplist,
                               SrcNoteType noteType = SRC_NULL);

    MOZ_MUST_USE bool emitIndex32(JSOp op, uint32_t index);
    MOZ_MUST_USE bool emitIndexOp(JSOp op, uint32_t index);

    MOZ_MUST_USE bool emitAtomOp(JSAtom* atom, JSOp op);
    MOZ_MUST_USE bool emitAtomOp(uint32_t atomIndex, JSOp op);

    MOZ_MUST_USE bool emitArrayLiteral(ListNode* array);
    MOZ_MUST_USE bool emitArray(ParseNode* arrayHead, uint32_t count);

    MOZ_MUST_USE bool emitInternedScopeOp(uint32_t index, JSOp op);
    MOZ_MUST_USE bool emitInternedObjectOp(uint32_t index, JSOp op);
    MOZ_MUST_USE bool emitObjectOp(ObjectBox* objbox, JSOp op);
    MOZ_MUST_USE bool emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op);
    MOZ_MUST_USE bool emitRegExp(uint32_t index);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitFunction(CodeNode* funNode, bool needsProto = false);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitObject(ListNode* objNode);

    MOZ_MUST_USE bool replaceNewInitWithNewObject(JSObject* obj, ptrdiff_t offset);

    MOZ_MUST_USE bool emitHoistedFunctionsInList(ListNode* stmtList);

    MOZ_MUST_USE bool emitPropertyList(ListNode* obj, MutableHandlePlainObject objp,
                                       PropListType type);

    // To catch accidental misuse, emitUint16Operand/emit3 assert that they are
    // not used to unconditionally emit JSOP_GETLOCAL. Variable access should
    // instead be emitted using EmitVarOp. In special cases, when the caller
    // definitely knows that a given local slot is unaliased, this function may be
    // used as a non-asserting version of emitUint16Operand.
    MOZ_MUST_USE bool emitLocalOp(JSOp op, uint32_t slot);

    MOZ_MUST_USE bool emitArgOp(JSOp op, uint16_t slot);
    MOZ_MUST_USE bool emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec);

    MOZ_MUST_USE bool emitGetNameAtLocation(JSAtom* name, const NameLocation& loc);
    MOZ_MUST_USE bool emitGetName(JSAtom* name) {
        return emitGetNameAtLocation(name, lookupName(name));
    }
    MOZ_MUST_USE bool emitGetName(NameNode* name);

    MOZ_MUST_USE bool emitTDZCheckIfNeeded(JSAtom* name, const NameLocation& loc);

    MOZ_MUST_USE bool emitNameIncDec(UnaryNode* incDec);

    MOZ_MUST_USE bool emitDeclarationList(ListNode* declList);
    MOZ_MUST_USE bool emitSingleDeclaration(ListNode* declList, NameNode* decl,
                                            ParseNode* initializer);

    MOZ_MUST_USE bool emitNewInit();
    MOZ_MUST_USE bool emitSingletonInitialiser(ListNode* objOrArray);

    MOZ_MUST_USE bool emitPrepareIteratorResult();
    MOZ_MUST_USE bool emitFinishIteratorResult(bool done);
    MOZ_MUST_USE bool iteratorResultShape(unsigned* shape);

    MOZ_MUST_USE bool emitGetDotGeneratorInInnermostScope() {
        return emitGetDotGeneratorInScope(*innermostEmitterScope());
    }
    MOZ_MUST_USE bool emitGetDotGeneratorInScope(EmitterScope& currentScope);

    MOZ_MUST_USE bool allocateResumeIndex(ptrdiff_t offset, uint32_t* resumeIndex);
    MOZ_MUST_USE bool allocateResumeIndexRange(mozilla::Span<ptrdiff_t> offsets,
                                               uint32_t* firstResumeIndex);

    MOZ_MUST_USE bool emitInitialYield(UnaryNode* yieldNode);
    MOZ_MUST_USE bool emitYield(UnaryNode* yieldNode);
    MOZ_MUST_USE bool emitYieldOp(JSOp op);
    MOZ_MUST_USE bool emitYieldStar(ParseNode* iter);
    MOZ_MUST_USE bool emitAwaitInInnermostScope() {
        return emitAwaitInScope(*innermostEmitterScope());
    }
    MOZ_MUST_USE bool emitAwaitInInnermostScope(UnaryNode* awaitNode);
    MOZ_MUST_USE bool emitAwaitInScope(EmitterScope& currentScope);

    MOZ_MUST_USE bool emitPropLHS(PropertyAccess* prop);
    MOZ_MUST_USE bool emitPropIncDec(UnaryNode* incDec);

    MOZ_MUST_USE bool emitAsyncWrapperLambda(unsigned index, bool isArrow);
    MOZ_MUST_USE bool emitAsyncWrapper(unsigned index, bool needsHomeObject, bool isArrow,
                                       bool isGenerator);

    MOZ_MUST_USE bool emitComputedPropertyName(UnaryNode* computedPropName);

    // Emit bytecode to put operands for a JSOP_GETELEM/CALLELEM/SETELEM/DELELEM
    // opcode onto the stack in the right order. In the case of SETELEM, the
    // value to be assigned must already be pushed.
    enum class EmitElemOption { Get, Call, IncDec, CompoundAssign, Ref };
    MOZ_MUST_USE bool emitElemOperands(PropertyByValue* elem, EmitElemOption opts);

    MOZ_MUST_USE bool emitElemObjAndKey(PropertyByValue* elem, bool isSuper, ElemOpEmitter& eoe);
    MOZ_MUST_USE bool emitElemOpBase(JSOp op);
    MOZ_MUST_USE bool emitElemOp(PropertyByValue* elem, JSOp op);
    MOZ_MUST_USE bool emitElemIncDec(UnaryNode* incDec);

    MOZ_MUST_USE bool emitCatch(BinaryNode* catchClause);
    MOZ_MUST_USE bool emitIf(TernaryNode* ifNode);
    MOZ_MUST_USE bool emitWith(BinaryNode* withNode);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLabeledStatement(const LabeledStatement* pn);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLexicalScope(LexicalScopeNode* lexicalScope);
    MOZ_MUST_USE bool emitLexicalScopeBody(ParseNode* body,
                                           EmitLineNumberNote emitLineNote = EMIT_LINENOTE);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitSwitch(SwitchStatement* switchStmt);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitTry(TryNode* tryNode);

    MOZ_MUST_USE bool emitGoSub(JumpList* jump);

    enum DestructuringFlavor {
        // Destructuring into a declaration.
        DestructuringDeclaration,

        // Destructuring into a formal parameter, when the formal parameters
        // contain an expression that might be evaluated, and thus require
        // this destructuring to assign not into the innermost scope that
        // contains the function body's vars, but into its enclosing scope for
        // parameter expressions.
        DestructuringFormalParameterInVarScope,

        // Destructuring as part of an AssignmentExpression.
        DestructuringAssignment
    };

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
    MOZ_MUST_USE bool emitSetOrInitializeDestructuring(ParseNode* target, DestructuringFlavor flav);

    // emitDestructuringObjRestExclusionSet emits the property exclusion set
    // for the rest-property in an object pattern.
    MOZ_MUST_USE bool emitDestructuringObjRestExclusionSet(ListNode* pattern);

    // emitDestructuringOps assumes the to-be-destructured value has been
    // pushed on the stack and emits code to destructure each part of a [] or
    // {} lhs expression.
    MOZ_MUST_USE bool emitDestructuringOps(ListNode* pattern, DestructuringFlavor flav);
    MOZ_MUST_USE bool emitDestructuringOpsArray(ListNode* pattern, DestructuringFlavor flav);
    MOZ_MUST_USE bool emitDestructuringOpsObject(ListNode* pattern, DestructuringFlavor flav);

    enum class CopyOption {
        Filtered, Unfiltered
    };

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
    MOZ_MUST_USE bool emitIteratorNext(const mozilla::Maybe<uint32_t>& callSourceCoordOffset,
                                       IteratorKind kind = IteratorKind::Sync,
                                       bool allowSelfHosted = false);
    MOZ_MUST_USE bool emitIteratorCloseInScope(EmitterScope& currentScope,
                                               IteratorKind iterKind = IteratorKind::Sync,
                                               CompletionKind completionKind = CompletionKind::Normal,
                                               bool allowSelfHosted = false);
    MOZ_MUST_USE bool emitIteratorCloseInInnermostScope(IteratorKind iterKind = IteratorKind::Sync,
                                                        CompletionKind completionKind = CompletionKind::Normal,
                                                        bool allowSelfHosted = false) {
        return emitIteratorCloseInScope(*innermostEmitterScope(), iterKind, completionKind,
                                        allowSelfHosted);
    }

    template <typename InnerEmitter>
    MOZ_MUST_USE bool wrapWithDestructuringIteratorCloseTryNote(int32_t iterDepth,
                                                                InnerEmitter emitter);

    // Check if the value on top of the stack is "undefined". If so, replace
    // that value on the stack with the value defined by |defaultExpr|.
    // |pattern| is a lhs node of the default expression.  If it's an
    // identifier and |defaultExpr| is an anonymous function, |SetFunctionName|
    // is called at compile time.
    MOZ_MUST_USE bool emitDefault(ParseNode* defaultExpr, ParseNode* pattern);

    MOZ_MUST_USE bool setOrEmitSetFunName(ParseNode* maybeFun, HandleAtom name);

    MOZ_MUST_USE bool emitInitializer(ParseNode* initializer, ParseNode* pattern);

    MOZ_MUST_USE bool emitCallSiteObject(CallSiteNode* callSiteObj);
    MOZ_MUST_USE bool emitTemplateString(ListNode* templateString);
    MOZ_MUST_USE bool emitAssignment(ParseNode* lhs, JSOp compoundOp, ParseNode* rhs);

    MOZ_MUST_USE bool emitReturn(UnaryNode* returnNode);
    MOZ_MUST_USE bool emitExpressionStatement(UnaryNode* exprStmt);
    MOZ_MUST_USE bool emitStatementList(ListNode* stmtList);

    MOZ_MUST_USE bool emitDeleteName(UnaryNode* deleteNode);
    MOZ_MUST_USE bool emitDeleteProperty(UnaryNode* deleteNode);
    MOZ_MUST_USE bool emitDeleteElement(UnaryNode* deleteNode);
    MOZ_MUST_USE bool emitDeleteExpression(UnaryNode* deleteNode);

    // |op| must be JSOP_TYPEOF or JSOP_TYPEOFEXPR.
    MOZ_MUST_USE bool emitTypeof(UnaryNode* typeofNode, JSOp op);

    MOZ_MUST_USE bool emitUnary(UnaryNode* unaryNode);
    MOZ_MUST_USE bool emitRightAssociative(ListNode* node);
    MOZ_MUST_USE bool emitLeftAssociative(ListNode* node);
    MOZ_MUST_USE bool emitLogical(ListNode* node);
    MOZ_MUST_USE bool emitSequenceExpr(ListNode* node,
                                       ValueUsage valueUsage = ValueUsage::WantValue);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitIncOrDec(UnaryNode* incDec);

    MOZ_MUST_USE bool emitConditionalExpression(ConditionalExpression& conditional,
                                                ValueUsage valueUsage = ValueUsage::WantValue);

    bool isRestParameter(ParseNode* expr);

    MOZ_MUST_USE bool emitArguments(ListNode* argsList, bool isCall, bool isSpread,
                                    CallOrNewEmitter& cone);
    MOZ_MUST_USE bool emitCallOrNew(BinaryNode* callNode,
                                    ValueUsage valueUsage = ValueUsage::WantValue);
    MOZ_MUST_USE bool emitSelfHostedCallFunction(BinaryNode* callNode);
    MOZ_MUST_USE bool emitSelfHostedResumeGenerator(BinaryNode* callNode);
    MOZ_MUST_USE bool emitSelfHostedForceInterpreter();
    MOZ_MUST_USE bool emitSelfHostedAllowContentIter(BinaryNode* callNode);
    MOZ_MUST_USE bool emitSelfHostedDefineDataProperty(BinaryNode* callNode);
    MOZ_MUST_USE bool emitSelfHostedGetPropertySuper(BinaryNode* callNode);
    MOZ_MUST_USE bool emitSelfHostedHasOwn(BinaryNode* callNode);

    MOZ_MUST_USE bool emitDo(BinaryNode* doNode);
    MOZ_MUST_USE bool emitWhile(BinaryNode* whileNode);

    MOZ_MUST_USE bool emitFor(ForNode* forNode,
                              const EmitterScope* headLexicalEmitterScope = nullptr);
    MOZ_MUST_USE bool emitCStyleFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);
    MOZ_MUST_USE bool emitForIn(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);
    MOZ_MUST_USE bool emitForOf(ForNode* forNode, const EmitterScope* headLexicalEmitterScope);

    MOZ_MUST_USE bool emitInitializeForInOrOfTarget(TernaryNode* forHead);

    MOZ_MUST_USE bool emitBreak(PropertyName* label);
    MOZ_MUST_USE bool emitContinue(PropertyName* label);

    MOZ_MUST_USE bool emitFunctionFormalParametersAndBody(ListNode* paramsBody);
    MOZ_MUST_USE bool emitFunctionFormalParameters(ListNode* paramsBody);
    MOZ_MUST_USE bool emitInitializeFunctionSpecialNames();
    MOZ_MUST_USE bool emitFunctionBody(ParseNode* funBody);
    MOZ_MUST_USE bool emitLexicalInitialization(NameNode* name);

    // Emit bytecode for the spread operator.
    //
    // emitSpread expects the current index (I) of the array, the array itself
    // and the iterator to be on the stack in that order (iterator on the bottom).
    // It will pop the iterator and I, then iterate over the iterator by calling
    // |.next()| and put the results into the I-th element of array with
    // incrementing I, then push the result I (it will be original I +
    // iteration count). The stack after iteration will look like |ARRAY INDEX|.
    MOZ_MUST_USE bool emitSpread(bool allowSelfHosted = false);

    MOZ_MUST_USE bool emitClass(ClassNode* classNode);
    MOZ_MUST_USE bool emitSuperElemOperands(PropertyByValue* elem,
                                            EmitElemOption opts = EmitElemOption::Get);
    MOZ_MUST_USE bool emitSuperGetElem(PropertyByValue* elem, bool isCall = false);

    MOZ_MUST_USE bool emitCalleeAndThis(ParseNode* callee, ParseNode* call,
                                        CallOrNewEmitter& cone);

    MOZ_MUST_USE bool emitPipeline(ListNode* node);

    MOZ_MUST_USE bool emitExportDefault(BinaryNode* exportNode);
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
