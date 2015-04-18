/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeEmitter_h
#define frontend_BytecodeEmitter_h

/*
 * JS bytecode generation.
 */

#include "jscntxt.h"
#include "jsopcode.h"
#include "jsscript.h"

#include "frontend/ParseMaps.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"
#include "frontend/SourceNotes.h"

namespace js {

class StaticEvalObject;

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
    bool append(Value v) { MOZ_ASSERT_IF(v.isString(), v.toString()->isAtom()); return list.append(v); }
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

struct CGTryNoteList {
    Vector<JSTryNote> list;
    explicit CGTryNoteList(ExclusiveContext* cx) : list(cx) {}

    bool append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end);
    size_t length() const { return list.length(); }
    void finish(TryNoteArray* array);
};

struct CGBlockScopeList {
    Vector<BlockScopeNote> list;
    explicit CGBlockScopeList(ExclusiveContext* cx) : list(cx) {}

    bool append(uint32_t scopeObject, uint32_t offset, uint32_t parent);
    uint32_t findEnclosingScope(uint32_t index);
    void recordEnd(uint32_t index, uint32_t offset);
    size_t length() const { return list.length(); }
    void finish(BlockScopeArray* array);
};

struct CGYieldOffsetList {
    Vector<uint32_t> list;
    explicit CGYieldOffsetList(ExclusiveContext* cx) : list(cx) {}

    bool append(uint32_t offset) { return list.append(offset); }
    size_t length() const { return list.length(); }
    void finish(YieldOffsetArray& array, uint32_t prologueLength);
};

struct LoopStmtInfo;
struct StmtInfoBCE;

// Use zero inline elements because these go on the stack and affect how many
// nested functions are possible.
typedef Vector<jsbytecode, 0> BytecodeVector;
typedef Vector<jssrcnote, 0> SrcNotesVector;

// This enum tells EmitVariables and the destructuring functions how emit the
// given Parser::variables parse tree. In the base case, DefineVars, the caller
// only wants variables to be defined in the prologue (if necessary). For
// PushInitialValues, variable initializer expressions are evaluated and left
// on the stack. For InitializeVars, the initializer expressions values are
// assigned (to local variables) and popped.
enum VarEmitOption {
    DefineVars        = 0,
    PushInitialValues = 1,
    InitializeVars    = 2
};

struct BytecodeEmitter
{
    typedef StmtInfoBCE StmtInfo;

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

        EmitSection(ExclusiveContext* cx, uint32_t lineNum)
          : code(cx), notes(cx), lastNoteOffset(0), currentLine(lineNum), lastColumn(0)
        {}
    };
    EmitSection prologue, main, *current;

    /* the parser */
    Parser<FullParseHandler>* const parser;

    HandleScript    evalCaller;     /* scripted caller info for eval and dbgapi */
    Handle<StaticEvalObject*> evalStaticScope;
                                   /* compile time scope for eval; does not imply stmt stack */

    StmtInfoBCE*    topStmt;       /* top of statement info stack */
    StmtInfoBCE*    topScopeStmt;  /* top lexical scope statement */
    Rooted<NestedScopeObject*> staticScope;
                                    /* compile time scope chain */

    OwnedAtomIndexMapPtr atomIndices; /* literals indexed for mapping */
    unsigned        firstLine;      /* first line, for JSScript::initFromEmitter */

    /*
     * Only unaliased locals have stack slots assigned to them. This vector is
     * used to map a local index (which includes unaliased and aliased locals)
     * to its stack slot index.
     */
    Vector<uint32_t, 16> localsToFrameSlots_;

    int32_t         stackDepth;     /* current stack depth in script frame */
    uint32_t        maxStackDepth;  /* maximum stack depth so far */

    uint32_t        arrayCompDepth; /* stack depth of array in comprehension */

    unsigned        emitLevel;      /* emitTree recursion level */

    CGConstList     constList;      /* constants to be included with the script */

    CGObjectList    objectList;     /* list of emitted objects */
    CGObjectList    regexpList;     /* list of emitted regexp that will be
                                       cloned during execution */
    CGTryNoteList   tryNoteList;    /* list of emitted try notes */
    CGBlockScopeList blockScopeList;/* list of emitted block scope notes */

    /*
     * For each yield op, map the yield index (stored as bytecode operand) to
     * the offset of the next op.
     */
    CGYieldOffsetList yieldOffsetList;

    uint16_t        typesetCount;   /* Number of JOF_TYPESET opcodes generated */

    bool            hasSingletons:1;    /* script contains singleton initializer JSOP_OBJECT */

    bool            hasTryFinally:1;    /* script contains finally block */

    bool            emittingForInit:1;  /* true while emitting init expr of for; exclude 'in' */

    bool            emittingRunOnceLambda:1; /* true while emitting a lambda which is only
                                                expected to run once. */

    bool isRunOnceLambda();

    bool            insideEval:1;       /* True if compiling an eval-expression or a function
                                           nested inside an eval. */

    const bool      insideNonGlobalEval:1;  /* True if this is a direct eval
                                               call in some non-global scope. */

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

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destruction.
     */
    BytecodeEmitter(BytecodeEmitter* parent, Parser<FullParseHandler>* parser, SharedContext* sc,
                    HandleScript script, Handle<LazyScript*> lazyScript,
                    bool insideEval, HandleScript evalCaller,
                    Handle<StaticEvalObject*> evalStaticScope, bool insideNonGlobalEval,
                    uint32_t lineNum, EmitterMode emitterMode = Normal);
    bool init();
    bool updateLocalsToFrameSlots();

    bool isAliasedName(ParseNode* pn);

    MOZ_ALWAYS_INLINE
    bool makeAtomIndex(JSAtom* atom, jsatomid* indexp) {
        AtomIndexAddPtr p = atomIndices->lookupForAdd(atom);
        if (p) {
            *indexp = p.value();
            return true;
        }

        jsatomid index = atomIndices->count();
        if (!atomIndices->add(p, atom, index))
            return false;

        *indexp = index;
        return true;
    }

    bool isInLoop();
    bool checkSingletonContext();

    // Check whether our function is in a run-once context (a toplevel
    // run-one script or a run-once lambda).
    bool checkRunOnceContext();

    bool needsImplicitThis();

    void tellDebuggerAboutCompiledScript(ExclusiveContext* cx);

    inline TokenStream* tokenStream();

    BytecodeVector& code() const { return current->code; }
    jsbytecode* code(ptrdiff_t offset) const { return current->code.begin() + offset; }
    ptrdiff_t offset() const { return current->code.end() - current->code.begin(); }
    ptrdiff_t prologueOffset() const { return prologue.code.end() - prologue.code.begin(); }
    void switchToMain() { current = &main; }
    void switchToPrologue() { current = &prologue; }

    SrcNotesVector& notes() const { return current->notes; }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }
    unsigned lastColumn() const { return current->lastColumn; }

    bool reportError(ParseNode* pn, unsigned errorNumber, ...);
    bool reportStrictWarning(ParseNode* pn, unsigned errorNumber, ...);
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
    bool checkSideEffects(ParseNode* pn, bool* answer);

    bool inTryBlockWithFinally();

#ifdef DEBUG
    bool checkStrictOrSloppy(JSOp op);
#endif

    // Append a new source note of the given type (and therefore size) to the
    // notes dynamic array, updating noteCount. Return the new note's index
    // within the array pointed at by current->notes as outparam.
    bool newSrcNote(SrcNoteType type, unsigned* indexp = nullptr);
    bool newSrcNote2(SrcNoteType type, ptrdiff_t offset, unsigned* indexp = nullptr);
    bool newSrcNote3(SrcNoteType type, ptrdiff_t offset1, ptrdiff_t offset2,
                     unsigned* indexp = nullptr);

    void copySrcNotes(jssrcnote* destination, uint32_t nsrcnotes);
    bool setSrcNoteOffset(unsigned index, unsigned which, ptrdiff_t offset);

    // NB: this function can add at most one extra extended delta note.
    bool addToSrcNoteDelta(jssrcnote* sn, ptrdiff_t delta);

    // Finish taking source notes in cx's notePool. If successful, the final
    // source note count is stored in the out outparam.
    bool finishTakingSrcNotes(uint32_t* out);

    void setJumpOffsetAt(ptrdiff_t off);

    // Emit code for the tree rooted at pn.
    bool emitTree(ParseNode* pn);

    // Emit function code for the tree rooted at body.
    bool emitFunctionScript(ParseNode* body);

    // If op is JOF_TYPESET (see the type barriers comment in TypeInference.h),
    // reserve a type set to store its result.
    void checkTypeSet(JSOp op);

    void updateDepth(ptrdiff_t target);
    bool updateLineNumberNotes(uint32_t offset);
    bool updateSourceCoordNotes(uint32_t offset);

    bool bindNameToSlot(ParseNode* pn);
    bool bindNameToSlotHelper(ParseNode* pn);

    void strictifySetNameNode(ParseNode* pn);
    JSOp strictifySetNameOp(JSOp op);

    bool tryConvertFreeName(ParseNode* pn);

    void popStatement();
    void pushStatement(StmtInfoBCE* stmt, StmtType type, ptrdiff_t top);
    void pushStatementInner(StmtInfoBCE* stmt, StmtType type, ptrdiff_t top);
    void pushLoopStatement(LoopStmtInfo* stmt, StmtType type, ptrdiff_t top);

    // Return the enclosing lexical scope, which is the innermost enclosing static
    // block object or compiler created function.
    JSObject* enclosingStaticScope();

    // Compute the number of nested scope objects that will actually be on the
    // scope chain at runtime, given the current staticScope.
    unsigned dynamicNestedScopeDepth();

    bool enterNestedScope(StmtInfoBCE* stmt, ObjectBox* objbox, StmtType stmtType);
    bool leaveNestedScope(StmtInfoBCE* stmt);

    bool enterBlockScope(StmtInfoBCE* stmtInfo, ObjectBox* objbox, JSOp initialValueOp,
                         unsigned alreadyPushed = 0);

    bool computeAliasedSlots(Handle<StaticBlockObject*> blockObj);

    bool lookupAliasedName(HandleScript script, PropertyName* name, uint32_t* pslot,
                           ParseNode* pn = nullptr);
    bool lookupAliasedNameSlot(PropertyName* name, ScopeCoordinate* sc);

    // Use this function instead of assigning directly to 'hops' to guard for
    // uint8_t overflows.
    bool assignHops(ParseNode* pn, unsigned src, ScopeCoordinate* dst);

    // In a function, block-scoped locals go after the vars, and form part of the
    // fixed part of a stack frame.  Outside a function, there are no fixed vars,
    // but block-scoped locals still form part of the fixed part of a stack frame
    // and are thus addressable via GETLOCAL and friends.
    void computeLocalOffset(Handle<StaticBlockObject*> blockObj);

    bool flushPops(int* npops);

    bool emitCheck(ptrdiff_t delta, ptrdiff_t* offset);

    // Emit one bytecode.
    bool emit1(JSOp op);

    // Emit two bytecodes, an opcode (op) with a byte of immediate operand
    // (op1).
    bool emit2(JSOp op, jsbytecode op1);

    // Emit three bytecodes, an opcode with two bytes of immediate operands.
    bool emit3(JSOp op, jsbytecode op1, jsbytecode op2);

    // Dup the var in operand stack slot "slot". The first item on the operand
    // stack is one slot past the last fixed slot. The last (most recent) item is
    // slot bce->stackDepth - 1.
    //
    // The instruction that is written (JSOP_DUPAT) switches the depth around so
    // that it is addressed from the sp instead of from the fp. This is useful when
    // you don't know the size of the fixed stack segment (nfixed), as is the case
    // when compiling scripts (because each statement is parsed and compiled
    // separately, but they all together form one script with one fixed stack
    // frame).
    bool emitDupAt(unsigned slot);

    // Emit a bytecode followed by an uint16 immediate operand stored in
    // big-endian order.
    bool emitUint16Operand(JSOp op, uint32_t operand);

    // Emit a bytecode followed by an uint32 immediate operand.
    bool emitUint32Operand(JSOp op, uint32_t operand);

    // Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
    bool emitN(JSOp op, size_t extra, ptrdiff_t* offset = nullptr);

    bool emitNumberOp(double dval);

    bool emitJump(JSOp op, ptrdiff_t off, ptrdiff_t* jumpOffset = nullptr);
    bool emitCall(JSOp op, uint16_t argc, ParseNode* pn = nullptr);

    bool emitLoopHead(ParseNode* nextpn);
    bool emitLoopEntry(ParseNode* nextpn);

    // Emit a backpatch op with offset pointing to the previous jump of this
    // type, so that we can walk back up the chain fixing up the op and jump
    // offset.
    bool emitBackPatchOp(ptrdiff_t* lastp);
    void backPatch(ptrdiff_t last, jsbytecode* target, jsbytecode op);

    bool emitGoto(StmtInfoBCE* toStmt, ptrdiff_t* lastp, SrcNoteType noteType = SRC_NULL);

    bool emitIndex32(JSOp op, uint32_t index);
    bool emitIndexOp(JSOp op, uint32_t index);

    bool emitAtomOp(JSAtom* atom, JSOp op);
    bool emitAtomOp(ParseNode* pn, JSOp op);

    bool emitArray(ParseNode* pn, uint32_t count);
    bool emitArrayComp(ParseNode* pn);

    bool emitInternedObjectOp(uint32_t index, JSOp op);
    bool emitObjectOp(ObjectBox* objbox, JSOp op);
    bool emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op);
    bool emitRegExp(uint32_t index);

    MOZ_NEVER_INLINE bool emitFunction(ParseNode* pn, bool needsProto = false);
    MOZ_NEVER_INLINE bool emitObject(ParseNode* pn);

    bool emitPropertyList(ParseNode* pn, MutableHandlePlainObject objp, PropListType type);

    // To catch accidental misuse, emitUint16Operand/emit3 assert that they are
    // not used to unconditionally emit JSOP_GETLOCAL. Variable access should
    // instead be emitted using EmitVarOp. In special cases, when the caller
    // definitely knows that a given local slot is unaliased, this function may be
    // used as a non-asserting version of emitUint16Operand.
    bool emitLocalOp(JSOp op, uint32_t slot);

    bool emitScopeCoordOp(JSOp op, ScopeCoordinate sc);
    bool emitAliasedVarOp(JSOp op, ParseNode* pn);
    bool emitAliasedVarOp(JSOp op, ScopeCoordinate sc, MaybeCheckLexical checkLexical);
    bool emitUnaliasedVarOp(JSOp op, uint32_t slot, MaybeCheckLexical checkLexical);

    bool emitVarOp(ParseNode* pn, JSOp op);
    bool emitVarIncDec(ParseNode* pn);

    bool emitNameOp(ParseNode* pn, bool callContext);
    bool emitNameIncDec(ParseNode* pn);

    bool maybeEmitVarDecl(JSOp prologueOp, ParseNode* pn, jsatomid* result);
    bool emitVariables(ParseNode* pn, VarEmitOption emitOption, bool isLetExpr = false);

    bool emitNewInit(JSProtoKey key);
    bool emitSingletonInitialiser(ParseNode* pn);

    bool emitPrepareIteratorResult();
    bool emitFinishIteratorResult(bool done);
    bool iteratorResultShape(unsigned* shape);

    bool emitYield(ParseNode* pn);
    bool emitYieldOp(JSOp op);
    bool emitYieldStar(ParseNode* iter, ParseNode* gen);

    bool emitPropLHS(ParseNode* pn);
    bool emitPropOp(ParseNode* pn, JSOp op);
    bool emitPropIncDec(ParseNode* pn);

    // Emit bytecode to put operands for a JSOP_GETELEM/CALLELEM/SETELEM/DELELEM
    // opcode onto the stack in the right order. In the case of SETELEM, the
    // value to be assigned must already be pushed.
    bool emitElemOperands(ParseNode* pn, JSOp op);

    bool emitElemOpBase(JSOp op);
    bool emitElemOp(ParseNode* pn, JSOp op);
    bool emitElemIncDec(ParseNode* pn);

    bool emitCatch(ParseNode* pn);
    bool emitIf(ParseNode* pn);
    bool emitWith(ParseNode* pn);

    MOZ_NEVER_INLINE bool emitLabeledStatement(const LabeledStatement* pn);
    MOZ_NEVER_INLINE bool emitLet(ParseNode* pnLet);
    MOZ_NEVER_INLINE bool emitLexicalScope(ParseNode* pn);
    MOZ_NEVER_INLINE bool emitSwitch(ParseNode* pn);
    MOZ_NEVER_INLINE bool emitTry(ParseNode* pn);

    // EmitDestructuringLHS assumes the to-be-destructured value has been pushed on
    // the stack and emits code to destructure a single lhs expression (either a
    // name or a compound []/{} expression).
    //
    // If emitOption is InitializeVars, the to-be-destructured value is assigned to
    // locals and ultimately the initial slot is popped (-1 total depth change).
    //
    // If emitOption is PushInitialValues, the to-be-destructured value is replaced
    // with the initial values of the N (where 0 <= N) variables assigned in the
    // lhs expression. (Same post-condition as EmitDestructuringOpsHelper)
    bool emitDestructuringLHS(ParseNode* target, VarEmitOption emitOption);

    bool emitDestructuringOps(ParseNode* pattern, bool isLet = false);
    bool emitDestructuringOpsHelper(ParseNode* pattern, VarEmitOption emitOption);
    bool emitDestructuringOpsArrayHelper(ParseNode* pattern, VarEmitOption emitOption);
    bool emitDestructuringOpsObjectHelper(ParseNode* pattern, VarEmitOption emitOption);

    typedef bool
    (*DestructuringDeclEmitter)(BytecodeEmitter* bce, JSOp prologueOp, ParseNode* pn);

    template <DestructuringDeclEmitter EmitName>
    bool emitDestructuringDeclsWithEmitter(JSOp prologueOp, ParseNode* pattern);

    bool emitDestructuringDecls(JSOp prologueOp, ParseNode* pattern);

    // Emit code to initialize all destructured names to the value on the top of
    // the stack.
    bool emitInitializeDestructuringDecls(JSOp prologueOp, ParseNode* pattern);

    // Convert the value atop the stack to an object using ToObject.
    bool emitToObject();

    // emitIterator expects the iterable to already be on the stack.
    // It will replace that stack value with the corresponding iterator
    bool emitIterator();

    // Pops iterator from the top of the stack. Pushes the result of |.next()|
    // onto the stack.
    bool emitIteratorNext(ParseNode* pn);

    // Check if the value on top of the stack is "undefined". If so, replace
    // that value on the stack with the value defined by |defaultExpr|.
    bool emitDefault(ParseNode* defaultExpr);

    bool emitCallSiteObject(ParseNode* pn);
    bool emitTemplateString(ParseNode* pn);
    bool emitAssignment(ParseNode* lhs, JSOp op, ParseNode* rhs);

    bool emitReturn(ParseNode* pn);
    bool emitStatement(ParseNode* pn);
    bool emitStatementList(ParseNode* pn, ptrdiff_t top);
    bool emitSyntheticStatements(ParseNode* pn, ptrdiff_t top);

    bool emitDelete(ParseNode* pn);
    bool emitLogical(ParseNode* pn);
    bool emitUnary(ParseNode* pn);

    MOZ_NEVER_INLINE bool emitIncOrDec(ParseNode* pn);

    bool emitConditionalExpression(ConditionalExpression& conditional);

    bool emitCallOrNew(ParseNode* pn);
    bool emitSelfHostedCallFunction(ParseNode* pn);
    bool emitSelfHostedResumeGenerator(ParseNode* pn);
    bool emitSelfHostedForceInterpreter(ParseNode* pn);

    bool emitDo(ParseNode* pn);
    bool emitFor(ParseNode* pn, ptrdiff_t top);
    bool emitForIn(ParseNode* pn, ptrdiff_t top);
    bool emitForInOrOfVariables(ParseNode* pn, bool* letDecl);
    bool emitNormalFor(ParseNode* pn, ptrdiff_t top);
    bool emitWhile(ParseNode* pn, ptrdiff_t top);

    bool emitBreak(PropertyName* label);
    bool emitContinue(PropertyName* label);

    bool emitDefaults(ParseNode* pn);
    bool emitLexicalInitialization(ParseNode* pn, JSOp globalDefOp);

    bool pushInitialConstants(JSOp op, unsigned n);
    bool initializeBlockScopedLocalsFromStack(Handle<StaticBlockObject*> blockObj);

    // emitSpread expects the current index (I) of the array, the array itself
    // and the iterator to be on the stack in that order (iterator on the bottom).
    // It will pop the iterator and I, then iterate over the iterator by calling
    // |.next()| and put the results into the I-th element of array with
    // incrementing I, then push the result I (it will be original I +
    // iteration count). The stack after iteration will look like |ARRAY INDEX|.
    bool emitSpread();

    // If type is STMT_FOR_OF_LOOP, emit bytecode for a for-of loop.
    // pn should be PNK_FOR, and pn->pn_left should be PNK_FOROF.
    //
    // If type is STMT_SPREAD, emit bytecode for spread operator.
    // pn should be nullptr.
    //
    // Please refer the comment above emitSpread for additional information about
    // stack convention.
    bool emitForOf(StmtType type, ParseNode* pn, ptrdiff_t top);

    bool emitClass(ParseNode* pn);
    bool emitSuperPropLHS(bool isCall = false);
    bool emitSuperPropOp(ParseNode* pn, JSOp op, bool isCall = false);
    bool emitSuperPropIncDec(ParseNode* pn);
    enum SuperElemOptions { SuperElem_Get, SuperElem_Set, SuperElem_Call, SuperElem_IncDec };
    bool emitSuperElemOperands(ParseNode* pn, SuperElemOptions opts = SuperElem_Get);
    bool emitSuperElemOp(ParseNode* pn, JSOp op, bool isCall = false);
    bool emitSuperElemIncDec(ParseNode* pn);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeEmitter_h */
