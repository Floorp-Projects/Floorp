/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS bytecode generation. */

#ifndef frontend_BytecodeEmitter_h
#define frontend_BytecodeEmitter_h

#include "jscntxt.h"
#include "jsopcode.h"
#include "jsscript.h"

#include "ds/InlineTable.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"
#include "frontend/SourceNotes.h"
#include "vm/Interpreter.h"

namespace js {
namespace frontend {

class FullParseHandler;
class ObjectBox;
class ParseNode;
template <typename ParseHandler> class Parser;
class SharedContext;
class TokenStream;

class CGConstList {
    Vector<Value> list;
  public:
    explicit CGConstList(ExclusiveContext* cx) : list(cx) {}
    MOZ_MUST_USE bool append(const Value& v) {
        MOZ_ASSERT_IF(v.isString(), v.toString()->isAtom());
        return list.append(v);
    }
    size_t length() const { return list.length(); }
    void finish(ConstArray* array);
};

struct CGObjectList {
    uint32_t            length;     /* number of emitted so far objects */
    ObjectBox*          lastbox;   /* last emitted object */

    CGObjectList() : length(0), lastbox(nullptr) {}

    unsigned add(ObjectBox* objbox);
    unsigned indexOf(JSObject* obj);
    void finish(ObjectArray* array);
    ObjectBox* find(uint32_t index);
};

struct MOZ_STACK_CLASS CGScopeList {
    Rooted<GCVector<Scope*>> vector;

    explicit CGScopeList(ExclusiveContext* cx)
      : vector(cx, GCVector<Scope*>(cx))
    { }

    bool append(Scope* scope) { return vector.append(scope); }
    uint32_t length() const { return vector.length(); }
    void finish(ScopeArray* array);
};

struct CGTryNoteList {
    Vector<JSTryNote> list;
    explicit CGTryNoteList(ExclusiveContext* cx) : list(cx) {}

    MOZ_MUST_USE bool append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end);
    size_t length() const { return list.length(); }
    void finish(TryNoteArray* array);
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
    explicit CGScopeNoteList(ExclusiveContext* cx) : list(cx) {}

    MOZ_MUST_USE bool append(uint32_t scopeIndex, uint32_t offset, bool inPrologue,
                             uint32_t parent);
    void recordEnd(uint32_t index, uint32_t offset, bool inPrologue);
    size_t length() const { return list.length(); }
    void finish(ScopeNoteArray* array, uint32_t prologueLength);
};

struct CGYieldOffsetList {
    Vector<uint32_t> list;
    explicit CGYieldOffsetList(ExclusiveContext* cx) : list(cx) {}

    MOZ_MUST_USE bool append(uint32_t offset) { return list.append(offset); }
    size_t length() const { return list.length(); }
    void finish(YieldOffsetArray& array, uint32_t prologueLength);
};

// Use zero inline elements because these go on the stack and affect how many
// nested functions are possible.
typedef Vector<jsbytecode, 0> BytecodeVector;
typedef Vector<jssrcnote, 0> SrcNotesVector;

// Linked list of jump instructions that need to be patched. The linked list is
// stored in the bytes of the incomplete bytecode that will be patched, so no
// extra memory is needed, and patching the instructions destroys the list.
//
// Example:
//
//     JumpList brList;
//     if (!emitJump(JSOP_IFEQ, &brList))
//         return false;
//     ...
//     JumpTarget label;
//     if (!emitJumpTarget(&label))
//         return false;
//     ...
//     if (!emitJump(JSOP_GOTO, &brList))
//         return false;
//     ...
//     patchJumpsToTarget(brList, label);
//
//                 +-> -1
//                 |
//                 |
//    ifeq ..   <+ +                +-+   ifeq ..
//    ..         |                  |     ..
//  label:       |                  +-> label:
//    jumptarget |                  |     jumptarget
//    ..         |                  |     ..
//    goto .. <+ +                  +-+   goto .. <+
//             |                                   |
//             |                                   |
//             +                                   +
//           brList                              brList
//
//       |                                  ^
//       +------- patchJumpsToTarget -------+
//

// Offset of a jump target instruction, used for patching jump instructions.
struct JumpTarget {
    ptrdiff_t offset;
};

struct JumpList {
    // -1 is used to mark the end of jump lists.
    JumpList() : offset(-1) {}
    ptrdiff_t offset;

    // Add a jump instruction to the list.
    void push(jsbytecode* code, ptrdiff_t jumpOffset);

    // Patch all jump instructions in this list to jump to `target`.  This
    // clobbers the list.
    void patchAll(jsbytecode* code, JumpTarget target);
};

struct MOZ_STACK_CLASS BytecodeEmitter
{
    class TDZCheckCache;
    class NestableControl;
    class EmitterScope;

    SharedContext* const sc;      /* context shared between parsing and bytecode generation */

    ExclusiveContext* const cx;

    BytecodeEmitter* const parent;  /* enclosing function or global context */

    Rooted<JSScript*> script;       /* the JSScript we're ultimately producing */

    Rooted<LazyScript*> lazyScript; /* the lazy script if mode is LazyFunction,
                                        nullptr otherwise. */

    struct EmitSection {
        BytecodeVector code;        /* bytecode */
        SrcNotesVector notes;       /* source notes, see below */
        ptrdiff_t   lastNoteOffset; /* code offset for last source note */
        uint32_t    currentLine;    /* line number for tree-based srcnote gen */
        uint32_t    lastColumn;     /* zero-based column index on currentLine of
                                       last SRC_COLSPAN-annotated opcode */
        JumpTarget lastTarget;      // Last jump target emitted.

        EmitSection(ExclusiveContext* cx, uint32_t lineNum)
          : code(cx), notes(cx), lastNoteOffset(0), currentLine(lineNum), lastColumn(0),
            lastTarget{ -1 - ptrdiff_t(JSOP_JUMPTARGET_LENGTH) }
        {}
    };
    EmitSection prologue, main, *current;

    Parser<FullParseHandler>* const parser;

    PooledMapPtr<AtomIndexMap> atomIndices; /* literals indexed for mapping */
    unsigned        firstLine;      /* first line, for JSScript::initFromEmitter */

    uint32_t        maxFixedSlots;  /* maximum number of fixed frame slots so far */
    uint32_t        maxStackDepth;  /* maximum number of expression stack slots so far */

    int32_t         stackDepth;     /* current stack depth in script frame */

    uint32_t        arrayCompDepth; /* stack depth of array in comprehension */

    unsigned        emitLevel;      /* emitTree recursion level */

    uint32_t        bodyScopeIndex; /* index into scopeList of the body scope */

    EmitterScope*    varEmitterScope;
    NestableControl* innermostNestableControl;
    EmitterScope*    innermostEmitterScope;
    TDZCheckCache*   innermostTDZCheckCache;

    CGConstList      constList;      /* constants to be included with the script */
    CGObjectList     objectList;     /* list of emitted objects */
    CGScopeList      scopeList;      /* list of emitted scopes */
    CGTryNoteList    tryNoteList;    /* list of emitted try notes */
    CGScopeNoteList  scopeNoteList;  /* list of emitted block scope notes */

    /*
     * For each yield op, map the yield index (stored as bytecode operand) to
     * the offset of the next op.
     */
    CGYieldOffsetList yieldOffsetList;

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

    // The end location of a function body that is being emitted.
    uint32_t functionBodyEndPos;
    // Whether functionBodyEndPos was set.
    bool functionBodyEndPosSet;

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destruction.
     */
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler>* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript, uint32_t lineNum,
                    EmitterMode emitterMode = Normal);

    // An alternate constructor that uses a TokenPos for the starting
    // line and that sets functionBodyEndPos as well.
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler>* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript,
                    TokenPos bodyPosition, EmitterMode emitterMode = Normal);

    MOZ_MUST_USE bool init();

    template <typename Predicate /* (NestableControl*) -> bool */>
    NestableControl* findInnermostNestableControl(Predicate predicate) const;

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
        return locationOfNameBoundInFunctionScope(name, innermostEmitterScope);
    }

    void setVarEmitterScope(EmitterScope* emitterScope) {
        MOZ_ASSERT(emitterScope);
        MOZ_ASSERT(!varEmitterScope);
        varEmitterScope = emitterScope;
    }

    Scope* bodyScope() const { return scopeList.vector[bodyScopeIndex]; }
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
        if (!atomIndices->add(p, atom, index))
            return false;

        *indexp = index;
        return true;
    }

    bool isInLoop();
    MOZ_MUST_USE bool checkSingletonContext();

    // Check whether our function is in a run-once context (a toplevel
    // run-one script or a run-once lambda).
    MOZ_MUST_USE bool checkRunOnceContext();

    bool needsImplicitThis();

    MOZ_MUST_USE bool maybeSetDisplayURL();
    MOZ_MUST_USE bool maybeSetSourceMap();
    void tellDebuggerAboutCompiledScript(ExclusiveContext* cx);

    inline TokenStream* tokenStream();

    BytecodeVector& code() const { return current->code; }
    jsbytecode* code(ptrdiff_t offset) const { return current->code.begin() + offset; }
    ptrdiff_t offset() const { return current->code.end() - current->code.begin(); }
    ptrdiff_t prologueOffset() const { return prologue.code.end() - prologue.code.begin(); }
    void switchToMain() { current = &main; }
    void switchToPrologue() { current = &prologue; }
    bool inPrologue() const { return current == &prologue; }

    SrcNotesVector& notes() const { return current->notes; }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }
    unsigned lastColumn() const { return current->lastColumn; }

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

    bool reportError(ParseNode* pn, unsigned errorNumber, ...);
    bool reportExtraWarning(ParseNode* pn, unsigned errorNumber, ...);
    bool reportStrictModeError(ParseNode* pn, unsigned errorNumber, ...);

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
    MOZ_MUST_USE bool emitTree(ParseNode* pn, EmitLineNumberNote emitLineNote = EMIT_LINENOTE);

    // Emit code for the tree rooted at pn with its own TDZ cache.
    MOZ_MUST_USE bool emitTreeInBranch(ParseNode* pn);

    // Emit global, eval, or module code for tree rooted at body. Always
    // encompasses the entire source.
    MOZ_MUST_USE bool emitScript(ParseNode* body);

    // Emit function code for the tree rooted at body.
    MOZ_MUST_USE bool emitFunctionScript(ParseNode* body);

    // If op is JOF_TYPESET (see the type barriers comment in TypeInference.h),
    // reserve a type set to store its result.
    void checkTypeSet(JSOp op);

    void updateDepth(ptrdiff_t target);
    MOZ_MUST_USE bool updateLineNumberNotes(uint32_t offset);
    MOZ_MUST_USE bool updateSourceCoordNotes(uint32_t offset);

    JSOp strictifySetNameOp(JSOp op);

    MOZ_MUST_USE bool flushPops(int* npops);

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

    // Helper to emit JSOP_CHECKISOBJ.
    MOZ_MUST_USE bool emitCheckIsObj(CheckIsObjectKind kind);

    // Emit a bytecode followed by an uint16 immediate operand stored in
    // big-endian order.
    MOZ_MUST_USE bool emitUint16Operand(JSOp op, uint32_t operand);

    // Emit a bytecode followed by an uint32 immediate operand.
    MOZ_MUST_USE bool emitUint32Operand(JSOp op, uint32_t operand);

    // Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
    MOZ_MUST_USE bool emitN(JSOp op, size_t extra, ptrdiff_t* offset = nullptr);

    MOZ_MUST_USE bool emitNumberOp(double dval);

    MOZ_MUST_USE bool emitThisLiteral(ParseNode* pn);
    MOZ_MUST_USE bool emitGetFunctionThis(ParseNode* pn);
    MOZ_MUST_USE bool emitGetThisForSuperBase(ParseNode* pn);
    MOZ_MUST_USE bool emitSetThis(ParseNode* pn);
    MOZ_MUST_USE bool emitCheckDerivedClassConstructorReturn();

    // Handle jump opcodes and jump targets.
    MOZ_MUST_USE bool emitJumpTarget(JumpTarget* target);
    MOZ_MUST_USE bool emitJumpNoFallthrough(JSOp op, JumpList* jump);
    MOZ_MUST_USE bool emitJump(JSOp op, JumpList* jump);
    MOZ_MUST_USE bool emitBackwardJump(JSOp op, JumpTarget target, JumpList* jump,
                                       JumpTarget* fallthrough);
    void patchJumpsToTarget(JumpList jump, JumpTarget target);
    MOZ_MUST_USE bool emitJumpTargetAndPatch(JumpList jump);

    MOZ_MUST_USE bool emitCall(JSOp op, uint16_t argc, ParseNode* pn = nullptr);
    MOZ_MUST_USE bool emitCallIncDec(ParseNode* incDec);

    MOZ_MUST_USE bool emitLoopHead(ParseNode* nextpn, JumpTarget* top);
    MOZ_MUST_USE bool emitLoopEntry(ParseNode* nextpn, JumpList entryJump);

    MOZ_MUST_USE bool emitGoto(NestableControl* target, JumpList* jumplist,
                               SrcNoteType noteType = SRC_NULL);

    MOZ_MUST_USE bool emitIndex32(JSOp op, uint32_t index);
    MOZ_MUST_USE bool emitIndexOp(JSOp op, uint32_t index);

    MOZ_MUST_USE bool emitAtomOp(JSAtom* atom, JSOp op);
    MOZ_MUST_USE bool emitAtomOp(ParseNode* pn, JSOp op);

    MOZ_MUST_USE bool emitArrayLiteral(ParseNode* pn);
    MOZ_MUST_USE bool emitArray(ParseNode* pn, uint32_t count, JSOp op);
    MOZ_MUST_USE bool emitArrayComp(ParseNode* pn);

    MOZ_MUST_USE bool emitInternedScopeOp(uint32_t index, JSOp op);
    MOZ_MUST_USE bool emitInternedObjectOp(uint32_t index, JSOp op);
    MOZ_MUST_USE bool emitObjectOp(ObjectBox* objbox, JSOp op);
    MOZ_MUST_USE bool emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op);
    MOZ_MUST_USE bool emitRegExp(uint32_t index);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitFunction(ParseNode* pn, bool needsProto = false);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitObject(ParseNode* pn);

    MOZ_MUST_USE bool emitHoistedFunctionsInList(ParseNode* pn);

    MOZ_MUST_USE bool emitPropertyList(ParseNode* pn, MutableHandlePlainObject objp,
                                       PropListType type);

    // To catch accidental misuse, emitUint16Operand/emit3 assert that they are
    // not used to unconditionally emit JSOP_GETLOCAL. Variable access should
    // instead be emitted using EmitVarOp. In special cases, when the caller
    // definitely knows that a given local slot is unaliased, this function may be
    // used as a non-asserting version of emitUint16Operand.
    MOZ_MUST_USE bool emitLocalOp(JSOp op, uint32_t slot);

    MOZ_MUST_USE bool emitArgOp(JSOp op, uint16_t slot);
    MOZ_MUST_USE bool emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec);

    MOZ_MUST_USE bool emitGetNameAtLocation(JSAtom* name, const NameLocation& loc,
                                            bool callContext = false);
    MOZ_MUST_USE bool emitGetName(JSAtom* name, bool callContext = false) {
        return emitGetNameAtLocation(name, lookupName(name), callContext);
    }
    MOZ_MUST_USE bool emitGetName(ParseNode* pn, bool callContext = false);

    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitSetOrInitializeNameAtLocation(HandleAtom name, const NameLocation& loc,
                                                        RHSEmitter emitRhs, bool initialize);
    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitSetOrInitializeName(HandleAtom name, RHSEmitter emitRhs,
                                              bool initialize)
    {
        return emitSetOrInitializeNameAtLocation(name, lookupName(name), emitRhs, initialize);
    }
    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitSetName(ParseNode* pn, RHSEmitter emitRhs) {
        RootedAtom name(cx, pn->name());
        return emitSetName(name, emitRhs);
    }
    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitSetName(HandleAtom name, RHSEmitter emitRhs) {
        return emitSetOrInitializeName(name, emitRhs, false);
    }
    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitInitializeName(ParseNode* pn, RHSEmitter emitRhs) {
        RootedAtom name(cx, pn->name());
        return emitInitializeName(name, emitRhs);
    }
    template <typename RHSEmitter>
    MOZ_MUST_USE bool emitInitializeName(HandleAtom name, RHSEmitter emitRhs) {
        return emitSetOrInitializeName(name, emitRhs, true);
    }

    MOZ_MUST_USE bool emitTDZCheckIfNeeded(JSAtom* name, const NameLocation& loc);

    MOZ_MUST_USE bool emitNameIncDec(ParseNode* pn);

    MOZ_MUST_USE bool emitDeclarationList(ParseNode* decls);
    MOZ_MUST_USE bool emitSingleDeclaration(ParseNode* decls, ParseNode* decl,
                                            ParseNode* initializer);

    MOZ_MUST_USE bool emitNewInit(JSProtoKey key);
    MOZ_MUST_USE bool emitSingletonInitialiser(ParseNode* pn);

    MOZ_MUST_USE bool emitPrepareIteratorResult();
    MOZ_MUST_USE bool emitFinishIteratorResult(bool done);
    MOZ_MUST_USE bool iteratorResultShape(unsigned* shape);

    MOZ_MUST_USE bool emitYield(ParseNode* pn);
    MOZ_MUST_USE bool emitYieldOp(JSOp op);
    MOZ_MUST_USE bool emitYieldStar(ParseNode* iter, ParseNode* gen);

    MOZ_MUST_USE bool emitPropLHS(ParseNode* pn);
    MOZ_MUST_USE bool emitPropOp(ParseNode* pn, JSOp op);
    MOZ_MUST_USE bool emitPropIncDec(ParseNode* pn);

    MOZ_MUST_USE bool emitAsyncWrapperLambda(unsigned index, bool isArrow);
    MOZ_MUST_USE bool emitAsyncWrapper(unsigned index, bool needsHomeObject, bool isArrow);

    MOZ_MUST_USE bool emitComputedPropertyName(ParseNode* computedPropName);

    // Emit bytecode to put operands for a JSOP_GETELEM/CALLELEM/SETELEM/DELELEM
    // opcode onto the stack in the right order. In the case of SETELEM, the
    // value to be assigned must already be pushed.
    enum class EmitElemOption { Get, Set, Call, IncDec, CompoundAssign, Ref };
    MOZ_MUST_USE bool emitElemOperands(ParseNode* pn, EmitElemOption opts);

    MOZ_MUST_USE bool emitElemOpBase(JSOp op);
    MOZ_MUST_USE bool emitElemOp(ParseNode* pn, JSOp op);
    MOZ_MUST_USE bool emitElemIncDec(ParseNode* pn);

    MOZ_MUST_USE bool emitCatch(ParseNode* pn);
    MOZ_MUST_USE bool emitIf(ParseNode* pn);
    MOZ_MUST_USE bool emitWith(ParseNode* pn);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLabeledStatement(const LabeledStatement* pn);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitLexicalScope(ParseNode* pn);
    MOZ_MUST_USE bool emitLexicalScopeBody(ParseNode* body,
                                           EmitLineNumberNote emitLineNote = EMIT_LINENOTE);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitSwitch(ParseNode* pn);
    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitTry(ParseNode* pn);

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

    // emitDestructuringOps assumes the to-be-destructured value has been
    // pushed on the stack and emits code to destructure each part of a [] or
    // {} lhs expression.
    MOZ_MUST_USE bool emitDestructuringOps(ParseNode* pattern, DestructuringFlavor flav);
    MOZ_MUST_USE bool emitDestructuringOpsArray(ParseNode* pattern, DestructuringFlavor flav);
    MOZ_MUST_USE bool emitDestructuringOpsObject(ParseNode* pattern, DestructuringFlavor flav);

    typedef bool
    (*DestructuringDeclEmitter)(BytecodeEmitter* bce, ParseNode* pn);

    template <typename NameEmitter>
    MOZ_MUST_USE bool emitDestructuringDeclsWithEmitter(ParseNode* pattern, NameEmitter emitName);

    // Throw a TypeError if the value atop the stack isn't convertible to an
    // object, with no overall effect on the stack.
    MOZ_MUST_USE bool emitRequireObjectCoercible();

    // emitIterator expects the iterable to already be on the stack.
    // It will replace that stack value with the corresponding iterator
    MOZ_MUST_USE bool emitIterator();

    // Pops iterator from the top of the stack. Pushes the result of |.next()|
    // onto the stack.
    MOZ_MUST_USE bool emitIteratorNext(ParseNode* pn, bool allowSelfHosted = false);
    MOZ_MUST_USE bool emitIteratorClose(
        mozilla::Maybe<JumpTarget> yieldStarTryStart = mozilla::Nothing(),
        bool allowSelfHosted = false);

    template <typename InnerEmitter>
    MOZ_MUST_USE bool wrapWithDestructuringIteratorCloseTryNote(int32_t iterDepth,
                                                                InnerEmitter emitter);

    // Check if the value on top of the stack is "undefined". If so, replace
    // that value on the stack with the value defined by |defaultExpr|.
    // |pattern| is a lhs node of the default expression.  If it's an
    // identifier and |defaultExpr| is an anonymous function, |SetFunctionName|
    // is called at compile time.
    MOZ_MUST_USE bool emitDefault(ParseNode* defaultExpr, ParseNode* pattern);

    MOZ_MUST_USE bool setOrEmitSetFunName(ParseNode* maybeFun, HandleAtom name,
                                          FunctionPrefixKind prefixKind);

    MOZ_MUST_USE bool emitInitializer(ParseNode* initializer, ParseNode* pattern);
    MOZ_MUST_USE bool emitInitializerInBranch(ParseNode* initializer, ParseNode* pattern);

    MOZ_MUST_USE bool emitCallSiteObject(ParseNode* pn);
    MOZ_MUST_USE bool emitTemplateString(ParseNode* pn);
    MOZ_MUST_USE bool emitAssignment(ParseNode* lhs, JSOp op, ParseNode* rhs);

    MOZ_MUST_USE bool emitReturn(ParseNode* pn);
    MOZ_MUST_USE bool emitStatement(ParseNode* pn);
    MOZ_MUST_USE bool emitStatementList(ParseNode* pn);

    MOZ_MUST_USE bool emitDeleteName(ParseNode* pn);
    MOZ_MUST_USE bool emitDeleteProperty(ParseNode* pn);
    MOZ_MUST_USE bool emitDeleteElement(ParseNode* pn);
    MOZ_MUST_USE bool emitDeleteExpression(ParseNode* pn);

    // |op| must be JSOP_TYPEOF or JSOP_TYPEOFEXPR.
    MOZ_MUST_USE bool emitTypeof(ParseNode* node, JSOp op);

    MOZ_MUST_USE bool emitUnary(ParseNode* pn);
    MOZ_MUST_USE bool emitRightAssociative(ParseNode* pn);
    MOZ_MUST_USE bool emitLeftAssociative(ParseNode* pn);
    MOZ_MUST_USE bool emitLogical(ParseNode* pn);
    MOZ_MUST_USE bool emitSequenceExpr(ParseNode* pn);

    MOZ_NEVER_INLINE MOZ_MUST_USE bool emitIncOrDec(ParseNode* pn);

    MOZ_MUST_USE bool emitConditionalExpression(ConditionalExpression& conditional);

    MOZ_MUST_USE bool isRestParameter(ParseNode* pn, bool* result);
    MOZ_MUST_USE bool emitOptimizeSpread(ParseNode* arg0, JumpList* jmp, bool* emitted);

    MOZ_MUST_USE bool emitCallOrNew(ParseNode* pn);
    MOZ_MUST_USE bool emitSelfHostedCallFunction(ParseNode* pn);
    MOZ_MUST_USE bool emitSelfHostedResumeGenerator(ParseNode* pn);
    MOZ_MUST_USE bool emitSelfHostedForceInterpreter(ParseNode* pn);
    MOZ_MUST_USE bool emitSelfHostedAllowContentIter(ParseNode* pn);

    MOZ_MUST_USE bool emitComprehensionFor(ParseNode* compFor);
    MOZ_MUST_USE bool emitComprehensionForIn(ParseNode* pn);
    MOZ_MUST_USE bool emitComprehensionForInOrOfVariables(ParseNode* pn, bool* lexicalScope);
    MOZ_MUST_USE bool emitComprehensionForOf(ParseNode* pn);

    MOZ_MUST_USE bool emitDo(ParseNode* pn);
    MOZ_MUST_USE bool emitWhile(ParseNode* pn);

    MOZ_MUST_USE bool emitFor(ParseNode* pn, EmitterScope* headLexicalEmitterScope = nullptr);
    MOZ_MUST_USE bool emitCStyleFor(ParseNode* pn, EmitterScope* headLexicalEmitterScope);
    MOZ_MUST_USE bool emitForIn(ParseNode* pn, EmitterScope* headLexicalEmitterScope);
    MOZ_MUST_USE bool emitForOf(ParseNode* pn, EmitterScope* headLexicalEmitterScope);

    MOZ_MUST_USE bool emitInitializeForInOrOfTarget(ParseNode* forHead);

    MOZ_MUST_USE bool emitBreak(PropertyName* label);
    MOZ_MUST_USE bool emitContinue(PropertyName* label);

    MOZ_MUST_USE bool emitFunctionFormalParametersAndBody(ParseNode* pn);
    MOZ_MUST_USE bool emitFunctionFormalParameters(ParseNode* pn);
    MOZ_MUST_USE bool emitInitializeFunctionSpecialNames();
    MOZ_MUST_USE bool emitFunctionBody(ParseNode* pn);
    MOZ_MUST_USE bool emitLexicalInitialization(ParseNode* pn);

    // Emit bytecode for the spread operator.
    //
    // emitSpread expects the current index (I) of the array, the array itself
    // and the iterator to be on the stack in that order (iterator on the bottom).
    // It will pop the iterator and I, then iterate over the iterator by calling
    // |.next()| and put the results into the I-th element of array with
    // incrementing I, then push the result I (it will be original I +
    // iteration count). The stack after iteration will look like |ARRAY INDEX|.
    MOZ_MUST_USE bool emitSpread(bool allowSelfHosted = false);

    MOZ_MUST_USE bool emitClass(ParseNode* pn);
    MOZ_MUST_USE bool emitSuperPropLHS(ParseNode* superBase, bool isCall = false);
    MOZ_MUST_USE bool emitSuperPropOp(ParseNode* pn, JSOp op, bool isCall = false);
    MOZ_MUST_USE bool emitSuperElemOperands(ParseNode* pn,
                                            EmitElemOption opts = EmitElemOption::Get);
    MOZ_MUST_USE bool emitSuperElemOp(ParseNode* pn, JSOp op, bool isCall = false);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeEmitter_h */
