/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_Parser_h
#define frontend_Parser_h

/*
 * JS parser definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseMaps.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "frontend/SyntaxParseHandler.h"

namespace js {
namespace frontend {

struct StmtInfoPC : public StmtInfoBase {
    StmtInfoPC      *down;          /* info for enclosing statement */
    StmtInfoPC      *downScope;     /* next enclosing lexical scope */

    uint32_t        blockid;        /* for simplified dominance computation */

    StmtInfoPC(ExclusiveContext *cx) : StmtInfoBase(cx) {}
};

typedef HashSet<JSAtom *> FuncStmtSet;
class SharedContext;

typedef Vector<Definition *, 16> DeclVector;

struct GenericParseContext
{
    // Enclosing function or global context.
    GenericParseContext *parent;

    // Context shared between parsing and bytecode generation.
    SharedContext *sc;

    // The following flags are set when a particular code feature is detected
    // in a function.

    // Function has 'return <expr>;'
    bool funHasReturnExpr:1;

    // Function has 'return;'
    bool funHasReturnVoid:1;

    // The following flags are set when parsing enters a particular region of
    // source code, and cleared when that region is exited.

    // true while parsing init expr of for; exclude 'in'
    bool parsingForInit:1;

    // true while we are within a with-statement in the current ParseContext
    // chain (which stops at the top-level or an eval()
    bool parsingWith:1;

    GenericParseContext(GenericParseContext *parent, SharedContext *sc)
      : parent(parent),
        sc(sc),
        funHasReturnExpr(false),
        funHasReturnVoid(false),
        parsingForInit(false),
        parsingWith(parent ? parent->parsingWith : false)
    {}
};

template <typename ParseHandler>
bool
GenerateBlockId(ParseContext<ParseHandler> *pc, uint32_t &blockid);

/*
 * The struct ParseContext stores information about the current parsing context,
 * which is part of the parser state (see the field Parser::pc). The current
 * parsing context is either the global context, or the function currently being
 * parsed. When the parser encounters a function definition, it creates a new
 * ParseContext, makes it the new current context, and sets its parent to the
 * context in which it encountered the definition.
 */
template <typename ParseHandler>
struct ParseContext : public GenericParseContext
{
    typedef StmtInfoPC StmtInfo;
    typedef typename ParseHandler::Node Node;
    typedef typename ParseHandler::DefinitionNode DefinitionNode;

    uint32_t        bodyid;         /* block number of program/function body */
    uint32_t        blockidGen;     /* preincremented block number generator */

    StmtInfoPC      *topStmt;       /* top of statement info stack */
    StmtInfoPC      *topScopeStmt;  /* top lexical scope statement */
    Rooted<StaticBlockObject *> blockChain;
                                    /* compile time block scope chain */

    const unsigned  staticLevel;    /* static compilation unit nesting level */

    uint32_t        parenDepth;     /* nesting depth of parens that might turn out
                                       to be generator expressions */
    uint32_t        yieldCount;     /* number of |yield| tokens encountered at
                                       non-zero depth in current paren tree */
    Node            blockNode;      /* parse node for a block with let declarations
                                       (block with its own lexical scope)  */
  private:
    AtomDecls<ParseHandler> decls_; /* function, const, and var declarations */
    DeclVector      args_;          /* argument definitions */
    DeclVector      vars_;          /* var/const definitions */

  public:
    const AtomDecls<ParseHandler> &decls() const {
        return decls_;
    }

    uint32_t numArgs() const {
        JS_ASSERT(sc->isFunctionBox());
        return args_.length();
    }

    uint32_t numVars() const {
        JS_ASSERT(sc->isFunctionBox());
        return vars_.length();
    }

    /*
     * This function adds a definition to the lexical scope represented by this
     * ParseContext.
     *
     * Pre-conditions:
     *  + The caller must have already taken care of name collisions:
     *    - For non-let definitions, this means 'name' isn't in 'decls'.
     *    - For let definitions, this means 'name' isn't already a name in the
     *      current block.
     *  + The given 'pn' is either a placeholder (created by a previous unbound
     *    use) or an un-bound un-linked name node.
     *  + The given 'kind' is one of ARG, CONST, VAR, or LET. In particular,
     *    NAMED_LAMBDA is handled in an ad hoc special case manner (see
     *    LeaveFunction) that we should consider rewriting.
     *
     * Post-conditions:
     *  + pc->decls().lookupFirst(name) == pn
     *  + The given name 'pn' has been converted in-place into a
     *    non-placeholder definition.
     *  + If this is a function scope (sc->inFunction), 'pn' is bound to a
     *    particular local/argument slot.
     *  + PND_CONST is set for Definition::COSNT
     *  + Pre-existing uses of pre-existing placeholders have been linked to
     *    'pn' if they are in the scope of 'pn'.
     *  + Pre-existing placeholders in the scope of 'pn' have been removed.
     */
    bool define(TokenStream &ts, PropertyName *name, Node pn, Definition::Kind);

    /*
     * Let definitions may shadow same-named definitions in enclosing scopes.
     * To represesent this, 'decls' is not a plain map, but actually:
     *   decls :: name -> stack of definitions
     * New bindings are pushed onto the stack, name lookup always refers to the
     * top of the stack, and leaving a block scope calls popLetDecl for each
     * name in the block's scope.
     */
    void popLetDecl(JSAtom *atom);

    /* See the sad story in defineArg. */
    void prepareToAddDuplicateArg(HandlePropertyName name, DefinitionNode prevDecl);

    /* See the sad story in MakeDefIntoUse. */
    void updateDecl(JSAtom *atom, Node newDecl);

    /*
     * After a function body has been parsed, the parser generates the
     * function's "bindings". Bindings are a data-structure, ultimately stored
     * in the compiled JSScript, that serve three purposes:
     *  - After parsing, the ParseContext is destroyed and 'decls' along with
     *    it. Mostly, the emitter just uses the binding information stored in
     *    the use/def nodes, but the emitter occasionally needs 'bindings' for
     *    various scope-related queries.
     *  - Bindings provide the initial js::Shape to use when creating a dynamic
     *    scope object (js::CallObject) for the function. This shape is used
     *    during dynamic name lookup.
     *  - Sometimes a script's bindings are accessed at runtime to retrieve the
     *    contents of the lexical scope (e.g., from the debugger).
     */
    bool generateFunctionBindings(ExclusiveContext *cx, LifoAlloc &alloc,
                                  InternalHandle<Bindings*> bindings) const;

  public:
    uint32_t         yieldOffset;   /* offset of a yield expression that might
                                       be an error if we turn out to be inside
                                       a generator expression.  Zero means
                                       there isn't one. */
  private:
    ParseContext    **parserPC;     /* this points to the Parser's active pc
                                       and holds either |this| or one of
                                       |this|'s descendents */

    // Value for parserPC to restore at the end. Use 'parent' instead for
    // information about the parse chain, this may be NULL if parent != NULL.
    ParseContext<ParseHandler> *oldpc;

  public:
    OwnedAtomDefnMapPtr lexdeps;    /* unresolved lexical name dependencies */

    FuncStmtSet     *funcStmts;     /* Set of (non-top-level) function statements
                                       that will alias any top-level bindings with
                                       the same name. */

    // All inner functions in this context. Only filled in when parsing syntax.
    AutoFunctionVector innerFunctions;

    // Set when parsing a declaration-like destructuring pattern.  This flag
    // causes PrimaryExpr to create PN_NAME parse nodes for variable references
    // which are not hooked into any definition's use chain, added to any tree
    // context's AtomList, etc. etc.  CheckDestructuring will do that work
    // later.
    //
    // The comments atop CheckDestructuring explain the distinction between
    // assignment-like and declaration-like destructuring patterns, and why
    // they need to be treated differently.
    bool            inDeclDestructuring:1;

    // True if we are in a function, saw a "use strict" directive, and weren't
    // strict before.
    bool            funBecameStrict:1;

    ParseContext(Parser<ParseHandler> *prs, GenericParseContext *parent,
                 SharedContext *sc, unsigned staticLevel, uint32_t bodyid)
      : GenericParseContext(parent, sc),
        bodyid(0),           // initialized in init()
        blockidGen(bodyid),  // used to set |bodyid| and subsequently incremented in init()
        topStmt(NULL),
        topScopeStmt(NULL),
        blockChain(prs->context),
        staticLevel(staticLevel),
        parenDepth(0),
        yieldCount(0),
        blockNode(ParseHandler::null()),
        decls_(prs->context, prs->alloc),
        args_(prs->context),
        vars_(prs->context),
        yieldOffset(0),
        parserPC(&prs->pc),
        oldpc(prs->pc),
        lexdeps(prs->context),
        funcStmts(NULL),
        innerFunctions(prs->context),
        inDeclDestructuring(false),
        funBecameStrict(false)
    {
        prs->pc = this;
    }

    ~ParseContext() {
        // |*parserPC| pointed to this object.  Now that this object is about to
        // die, make |*parserPC| point to this object's parent.
        JS_ASSERT(*parserPC == this);
        *parserPC = this->oldpc;
        js_delete(funcStmts);
    }

    inline bool init()
{
    if (!frontend::GenerateBlockId(this, this->bodyid))
        return false;

    return decls_.init() && lexdeps.ensureMap(sc->context);
}

    InBlockBool inBlock() const { return InBlockBool(!topStmt || topStmt->type == STMT_BLOCK); }
    unsigned blockid() { return topStmt ? topStmt->blockid : bodyid; }

    // True if we are at the topmost level of a entire script or function body.
    // For example, while parsing this code we would encounter f1 and f2 at
    // body level, but we would not encounter f3 or f4 at body level:
    //
    //   function f1() { function f2() { } }
    //   if (cond) { function f3() { if (cond) { function f4() { } } } }
    //
    bool atBodyLevel() { return !topStmt; }

    inline bool useAsmOrInsideUseAsm() const {
        return sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();
    }
};

template <typename ParseHandler>
struct BindData;

class CompExprTransplanter;

template <typename ParseHandler>
class GenexpGuard;

enum LetContext { LetExpresion, LetStatement };
enum VarContext { HoistVars, DontHoistVars };
enum FunctionType { Getter, Setter, Normal };

template <typename ParseHandler>
class Parser : private AutoGCRooter, public StrictModeGetter
{
  public:
    ExclusiveContext *const context;
    LifoAlloc &alloc;

    TokenStream         tokenStream;
    LifoAlloc::Mark     tempPoolMark;

    /* list of parsed objects for GC tracing */
    ObjectBox *traceListHead;

    /* innermost parse context (stack-allocated) */
    ParseContext<ParseHandler> *pc;

    SourceCompressionToken *sct;        /* compression token for aborting */

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    const bool          foldConstants:1;

  private:
    /*
     * Not all language constructs can be handled during syntax parsing. If it
     * is not known whether the parse succeeds or fails, this bit is set and
     * the parse will return false.
     */
    bool abortedSyntaxParse;

    typedef typename ParseHandler::Node Node;
    typedef typename ParseHandler::DefinitionNode DefinitionNode;

  public:
    /* State specific to the kind of parse being performed. */
    ParseHandler handler;

  private:
    bool reportHelper(ParseReportKind kind, bool strict, uint32_t offset,
                      unsigned errorNumber, va_list args);
  public:
    bool report(ParseReportKind kind, bool strict, Node pn, unsigned errorNumber, ...);
    bool reportWithOffset(ParseReportKind kind, bool strict, uint32_t offset, unsigned errorNumber,
                          ...);

    Parser(ExclusiveContext *cx, LifoAlloc *alloc, const CompileOptions &options,
           const jschar *chars, size_t length, bool foldConstants,
           Parser<SyntaxParseHandler> *syntaxParser,
           LazyScript *lazyOuterFunction);
    ~Parser();

    friend void js::frontend::MarkParser(JSTracer *trc, AutoGCRooter *parser);

    const char *getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }

    /*
     * Parse a top-level JS script.
     */
    Node parse(JSObject *chain);

    /*
     * Allocate a new parsed object or function container from
     * cx->tempLifoAlloc.
     */
    ObjectBox *newObjectBox(JSObject *obj);
    ModuleBox *newModuleBox(Module *module, ParseContext<ParseHandler> *pc);
    FunctionBox *newFunctionBox(JSFunction *fun, ParseContext<ParseHandler> *pc, bool strict);

    /*
     * Create a new function object given parse context (pc) and a name (which
     * is optional if this is a function expression).
     */
    JSFunction *newFunction(GenericParseContext *pc, HandleAtom atom, FunctionSyntaxKind kind);

    void trace(JSTracer *trc);

    bool hadAbortedSyntaxParse() {
        return abortedSyntaxParse;
    }
    void clearAbortedSyntaxParse() {
        abortedSyntaxParse = false;
    }

  private:
    Parser *thisForCtor() { return this; }

    Node stringLiteral();
    inline Node newName(PropertyName *name);

    inline bool abortIfSyntaxParser();

  public:

    /* Public entry points for parsing. */
    Node statement(bool canHaveDirectives = false);
    bool maybeParseDirective(Node pn, bool *cont);

    // Parse a function, given only its body. Used for the Function constructor.
    Node standaloneFunctionBody(HandleFunction fun, const AutoNameVector &formals, HandleScript script,
                                Node fn, FunctionBox **funbox, bool strict,
                                bool *becameStrict = NULL);

    // Parse a function, given only its arguments and body. Used for lazily
    // parsed functions.
    Node standaloneLazyFunction(HandleFunction fun, unsigned staticLevel, bool strict);

    /*
     * Parse a function body.  Pass StatementListBody if the body is a list of
     * statements; pass ExpressionBody if the body is a single expression.
     */
    enum FunctionBodyType { StatementListBody, ExpressionBody };
    Node functionBody(FunctionSyntaxKind kind, FunctionBodyType type);

    bool functionArgsAndBodyGeneric(Node pn, HandleFunction fun,
                                    HandlePropertyName funName, FunctionType type,
                                    FunctionSyntaxKind kind, bool strict, bool *becameStrict);

    virtual bool strictMode() { return pc->sc->strict; }

    const CompileOptions &options() const {
        return tokenStream.options();
    }

  private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a ParseContext
     * object, pointed to by this->pc.
     *
     * Each returns a parse node tree or null on error.
     *
     * Parsers whose name has a '1' suffix leave the TokenStream state
     * pointing to the token one past the end of the parsed fragment.  For a
     * number of the parsers this is convenient and avoids a lot of
     * unnecessary ungetting and regetting of tokens.
     *
     * Some parsers have two versions:  an always-inlined version (with an 'i'
     * suffix) and a never-inlined version (with an 'n' suffix).
     */
    Node moduleDecl();
    Node functionStmt();
    Node functionExpr();
    Node statements();

    Node blockStatement();
    Node ifStatement();
    Node doWhileStatement();
    Node whileStatement();
    Node forStatement();
    Node switchStatement();
    Node continueStatement();
    Node breakStatement();
    Node returnStatementOrYieldExpression();
    Node withStatement();
    Node labeledStatement();
    Node throwStatement();
    Node tryStatement();
    Node debuggerStatement();

#if JS_HAS_BLOCK_SCOPE
    Node letStatement();
#endif
    Node expressionStatement();
    Node variables(ParseNodeKind kind, bool *psimple = NULL,
                   StaticBlockObject *blockObj = NULL,
                   VarContext varContext = HoistVars);
    Node expr();
    Node assignExpr();
    Node assignExprWithoutYield(unsigned err);
    Node condExpr1();
    Node orExpr1();
    Node unaryExpr();
    Node memberExpr(TokenKind tt, bool allowCallSyntax);
    Node primaryExpr(TokenKind tt);
    Node parenExpr(bool *genexp = NULL);

    /*
     * Additional JS parsers.
     */
    bool functionArguments(FunctionSyntaxKind kind, Node *list, Node funcpn, bool &hasRest);

    Node functionDef(HandlePropertyName name, const TokenStream::Position &start,
                     size_t startOffset, FunctionType type, FunctionSyntaxKind kind);
    bool functionArgsAndBody(Node pn, HandleFunction fun, HandlePropertyName funName,
                             size_t startOffset, FunctionType type, FunctionSyntaxKind kind,
                             bool strict, bool *becameStrict = NULL);

    Node unaryOpExpr(ParseNodeKind kind, JSOp op, uint32_t begin);

    Node condition();
    Node comprehensionTail(Node kid, unsigned blockid, bool isGenexp,
                           ParseContext<ParseHandler> *outerpc,
                           ParseNodeKind kind = PNK_SEMI, JSOp op = JSOP_NOP);
    bool arrayInitializerComprehensionTail(Node pn);
    Node generatorExpr(Node kid);
    bool argumentList(Node listNode);
    Node bracketedExpr();
    Node letBlock(LetContext letContext);
    Node destructuringExpr(BindData<ParseHandler> *data, TokenKind tt);

    Node identifierName();

    bool allowsForEachIn() {
#if !JS_HAS_FOR_EACH_IN
        return false;
#else
        return versionNumber() >= JSVERSION_1_6;
#endif
    }

    bool setAssignmentLhsOps(Node pn, bool isPlainAssignment);
    bool matchInOrOf(bool *isForOfp);

    bool checkFunctionArguments();
    bool makeDefIntoUse(Definition *dn, Node pn, JSAtom *atom);
    bool checkFunctionDefinition(HandlePropertyName funName, Node *pn, FunctionSyntaxKind kind,
                                 bool *pbodyProcessed);
    bool finishFunctionDefinition(Node pn, FunctionBox *funbox, Node prelude, Node body);
    bool addFreeVariablesFromLazyFunction(JSFunction *fun, ParseContext<ParseHandler> *pc);

    bool isValidForStatementLHS(Node pn1, JSVersion version,
                                bool forDecl, bool forEach, bool forOf);
    bool setLvalKid(Node pn, Node kid, const char *name);
    bool setIncOpKid(Node pn, Node kid, TokenKind tt, bool preorder);
    bool checkStrictAssignment(Node lhs);
    bool checkStrictBinding(HandlePropertyName name, Node pn);
    bool defineArg(Node funcpn, HandlePropertyName name,
                   bool disallowDuplicateArgs = false, Node *duplicatedArg = NULL);
    Node pushLexicalScope(StmtInfoPC *stmt);
    Node pushLexicalScope(Handle<StaticBlockObject*> blockObj, StmtInfoPC *stmt);
    Node pushLetScope(Handle<StaticBlockObject*> blockObj, StmtInfoPC *stmt);
    bool noteNameUse(HandlePropertyName name, Node pn);
    Node newRegExp();
    Node newBindingNode(PropertyName *name, bool functionScope, VarContext varContext = HoistVars);
    bool checkDestructuring(BindData<ParseHandler> *data, Node left, bool toplevel = true);
    bool bindDestructuringVar(BindData<ParseHandler> *data, Node pn);
    bool bindDestructuringLHS(Node pn);
    bool makeSetCall(Node pn, unsigned msg);
    Node cloneLeftHandSide(Node opn);
    Node cloneParseTree(Node opn);

    Node newNumber(const Token &tok) {
        return handler.newNumber(tok.number(), tok.decimalPoint(), tok.pos);
    }

    static bool
    bindDestructuringArg(BindData<ParseHandler> *data,
                         HandlePropertyName name, Parser<ParseHandler> *parser);

    static bool
    bindLet(BindData<ParseHandler> *data,
            HandlePropertyName name, Parser<ParseHandler> *parser);

    static bool
    bindVarOrConst(BindData<ParseHandler> *data,
                   HandlePropertyName name, Parser<ParseHandler> *parser);

    static Node null() { return ParseHandler::null(); }

    bool reportRedeclaration(Node pn, bool isConst, JSAtom *atom);
    bool reportBadReturn(Node pn, ParseReportKind kind, unsigned errnum, unsigned anonerrnum);
    bool checkFinalReturn(Node pn);
    DefinitionNode getOrCreateLexicalDependency(ParseContext<ParseHandler> *pc, JSAtom *atom);

    bool leaveFunction(Node fn, HandlePropertyName funName,
                       ParseContext<ParseHandler> *outerpc,
                       FunctionSyntaxKind kind = Expression);

    TokenPos pos() const { return tokenStream.currentToken().pos; }

    friend class CompExprTransplanter;
    friend class GenexpGuard<ParseHandler>;
    friend struct BindData<ParseHandler>;
};

/* Declare some required template specializations. */

template <>
bool
Parser<FullParseHandler>::setAssignmentLhsOps(ParseNode *pn, bool isPlainAssignment);

template <>
bool
Parser<SyntaxParseHandler>::setAssignmentLhsOps(Node pn, bool isPlainAssignment);

} /* namespace frontend */
} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

#endif /* frontend_Parser_h */
