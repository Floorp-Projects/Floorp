/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TreeContext_h__
#define TreeContext_h__

#include "jstypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#include "frontend/ParseMaps.h"

#include "vm/ScopeObject.h"

typedef struct BindData BindData;

namespace js {

struct StmtInfo;

class ContextFlags {

    // This class's data is all private and so only visible to these friends.
    friend struct SharedContext;
    friend struct FunctionBox;

    // This function/global/eval code body contained a Use Strict Directive.
    // Treat certain strict warnings as errors, and forbid the use of 'with'.
    // See also TSF_STRICT_MODE_CODE, JSScript::strictModeCode, and
    // JSREPORT_STRICT_ERROR.
    //
    bool            inStrictMode:1;

    // The (static) bindings of this script need to support dynamic name
    // read/write access. Here, 'dynamic' means dynamic dictionary lookup on
    // the scope chain for a dynamic set of keys. The primary examples are:
    //  - direct eval
    //  - function::
    //  - with
    // since both effectively allow any name to be accessed. Non-exmaples are:
    //  - upvars of nested functions
    //  - function statement
    // since the set of assigned name is known dynamically. 'with' could be in
    // the non-example category, provided the set of all free variables within
    // the with block was noted. However, we do not optimize 'with' so, for
    // simplicity, 'with' is treated like eval.
    //
    // Note: access through the arguments object is not considered dynamic
    // binding access since it does not go through the normal name lookup
    // mechanism. This is debatable and could be changed (although care must be
    // taken not to turn off the whole 'arguments' optimization). To answer the
    // more general "is this argument aliased" question, script->needsArgsObj
    // should be tested (see JSScript::argIsAlised).
    //
    bool            bindingsAccessedDynamically:1;

    // The |fun*| flags are only relevant if |inFunction| is true.  Due to
    // sloppiness, however, some are set in cases where |inFunction| is
    // false.

    // The function needs Call object per call.
    bool            funIsHeavyweight:1;

    // We parsed a yield statement in the function.
    bool            funIsGenerator:1;

    // The function or a function that encloses it may define new local names
    // at runtime through means other than calling eval.
    bool            funMightAliasLocals:1;

    // This function does something that can extend the set of bindings in its
    // call objects --- it does a direct eval in non-strict code, or includes a
    // function statement (as opposed to a function definition).
    //
    // This flag is *not* inherited by enclosed or enclosing functions; it
    // applies only to the function in whose flags it appears.
    //
    bool            funHasExtensibleScope:1;

    // Technically, every function has a binding named 'arguments'. Internally,
    // this binding is only added when 'arguments' is mentioned by the function
    // body. This flag indicates whether 'arguments' has been bound either
    // through implicit use:
    //   function f() { return arguments }
    // or explicit redeclaration:
    //   function f() { var arguments; return arguments }
    //
    // Note 1: overwritten arguments (function() { arguments = 3 }) will cause
    // this flag to be set but otherwise require no special handling:
    // 'arguments' is just a local variable and uses of 'arguments' will just
    // read the local's current slot which may have been assigned. The only
    // special semantics is that the initial value of 'arguments' is the
    // arguments object (not undefined, like normal locals).
    //
    // Note 2: if 'arguments' is bound as a formal parameter, there will be an
    // 'arguments' in Bindings, but, as the "LOCAL" in the name indicates, this
    // flag will not be set. This is because, as a formal, 'arguments' will
    // have no special semantics: the initial value is unconditionally the
    // actual argument (or undefined if nactual < nformal).
    //
    bool            funArgumentsHasLocalBinding:1;

    // In many cases where 'arguments' has a local binding (as described above)
    // we do not need to actually create an arguments object in the function
    // prologue: instead we can analyze how 'arguments' is used (using the
    // simple dataflow analysis in analyzeSSA) to determine that uses of
    // 'arguments' can just read from the stack frame directly. However, the
    // dataflow analysis only looks at how JSOP_ARGUMENTS is used, so it will
    // be unsound in several cases. The frontend filters out such cases by
    // setting this flag which eagerly sets script->needsArgsObj to true.
    //
    bool            funDefinitelyNeedsArgsObj:1;

  public:
    ContextFlags(JSContext *cx)
      : inStrictMode(cx->hasRunOption(JSOPTION_STRICT_MODE)),
        bindingsAccessedDynamically(false),
        funIsHeavyweight(false),
        funIsGenerator(false),
        funMightAliasLocals(false),
        funHasExtensibleScope(false),
        funArgumentsHasLocalBinding(false),
        funDefinitelyNeedsArgsObj(false)
    { }
};

struct SharedContext {
    JSContext       *context;

    uint32_t        bodyid;         /* block number of program/function body */
    uint32_t        blockidGen;     /* preincremented block number generator */

    StmtInfo        *topStmt;       /* top of statement info stack */
    StmtInfo        *topScopeStmt;  /* top lexical scope statement */
    RootedVar<StaticBlockObject *> blockChain;
                                    /* compile time block scope chain (NB: one
                                       deeper than the topScopeStmt/downScope
                                       chain when in head of let block/expr) */

  private:
    RootedVarFunction fun_;         /* function to store argument and variable
                                       names when inFunction is set */
    RootedVarObject   scopeChain_;  /* scope chain object for the script */

  public:
    unsigned        staticLevel;    /* static compilation unit nesting level */

    FunctionBox     *funbox;        /* null or box for function we're compiling
                                       if inFunction is set and not in
                                       js::frontend::CompileFunctionBody */
    FunctionBox     *functionList;

    Bindings        bindings;       /* bindings in this code, including
                                       arguments if we're compiling a function */
    Bindings::StackRoot bindingsRoot; /* root for stack allocated bindings. */

    const bool      inFunction:1;   /* parsing/emitting inside function body */

    bool            inForInit:1;    /* parsing/emitting init expr of for; exclude 'in' */

    ContextFlags    cxFlags;

    inline SharedContext(JSContext *cx, bool inFunction);

    bool inStrictMode()                const { return cxFlags.inStrictMode; }
    bool bindingsAccessedDynamically() const { return cxFlags.bindingsAccessedDynamically; }
    bool funIsHeavyweight()            const { return cxFlags.funIsHeavyweight; }
    bool funIsGenerator()              const { return cxFlags.funIsGenerator; }
    bool funMightAliasLocals()         const { return cxFlags.funMightAliasLocals; }
    bool funHasExtensibleScope()       const { return cxFlags.funHasExtensibleScope; }
    bool funArgumentsHasLocalBinding() const { return cxFlags.funArgumentsHasLocalBinding; }
    bool funDefinitelyNeedsArgsObj()   const { return cxFlags.funDefinitelyNeedsArgsObj; }

    void setInStrictMode()                  { cxFlags.inStrictMode                = true; }
    void setBindingsAccessedDynamically()   { cxFlags.bindingsAccessedDynamically = true; }
    void setFunIsHeavyweight()              { cxFlags.funIsHeavyweight            = true; }
    void setFunIsGenerator()                { cxFlags.funIsGenerator              = true; }
    void setFunMightAliasLocals()           { cxFlags.funMightAliasLocals         = true; }
    void setFunHasExtensibleScope()         { cxFlags.funHasExtensibleScope       = true; }
    void setFunArgumentsHasLocalBinding()   { cxFlags.funArgumentsHasLocalBinding = true; }
    void setFunDefinitelyNeedsArgsObj()     { JS_ASSERT(cxFlags.funArgumentsHasLocalBinding);
                                              cxFlags.funDefinitelyNeedsArgsObj   = true; }

    unsigned argumentsLocalSlot() const;

    JSFunction *fun() const {
        JS_ASSERT(inFunction);
        return fun_;
    }
    void setFunction(JSFunction *fun) {
        JS_ASSERT(inFunction);
        fun_ = fun;
    }
    JSObject *scopeChain() const {
        JS_ASSERT(!inFunction);
        return scopeChain_;
    }
    void setScopeChain(JSObject *scopeChain) {
        JS_ASSERT(!inFunction);
        scopeChain_ = scopeChain;
    }

    unsigned blockid();

    // True if we are at the topmost level of a entire script or function body.
    // For example, while parsing this code we would encounter f1 and f2 at
    // body level, but we would not encounter f3 or f4 at body level:
    //
    //   function f1() { function f2() { } }
    //   if (cond) { function f3() { if (cond) { function f4() { } } } }
    //
    bool atBodyLevel();

    // Return true if we need to check for conditions that elicit
    // JSOPTION_STRICT warnings or strict mode errors.
    inline bool needStrictChecks();
};

typedef HashSet<JSAtom *> FuncStmtSet;
struct Parser;

struct TreeContext {                /* tree context for semantic checks */
    SharedContext   *sc;            /* context shared between parsing and bytecode generation */

    uint32_t        parenDepth;     /* nesting depth of parens that might turn out
                                       to be generator expressions */
    uint32_t        yieldCount;     /* number of |yield| tokens encountered at
                                       non-zero depth in current paren tree */
    ParseNode       *blockNode;     /* parse node for a block with let declarations
                                       (block with its own lexical scope)  */
    AtomDecls       decls;          /* function, const, and var declarations */
    ParseNode       *yieldNode;     /* parse node for a yield expression that might
                                       be an error if we turn out to be inside a
                                       generator expression */

  private:
    TreeContext     **parserTC;     /* this points to the Parser's active tc
                                       and holds either |this| or one of
                                       |this|'s descendents */

  public:
    OwnedAtomDefnMapPtr lexdeps;    /* unresolved lexical name dependencies */

    TreeContext     *parent;        /* Enclosing function or global context.  */

    ParseNode       *innermostWith; /* innermost WITH parse node */

    FuncStmtSet     *funcStmts;     /* Set of (non-top-level) function statements
                                       that will alias any top-level bindings with
                                       the same name. */

    /*
     * Flags that are set for a short time during parsing to indicate context
     * or the presence of a code feature.
     */
    bool            hasReturnExpr:1; /* function has 'return <expr>;' */
    bool            hasReturnVoid:1; /* function has 'return;' */

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

    void trace(JSTracer *trc);

    inline TreeContext(Parser *prs, SharedContext *sc);
    inline ~TreeContext();

    inline bool init();
};

/*
 * NB: If you add enumerators for scope statements, add them between STMT_WITH
 * and STMT_CATCH, or you will break the STMT_TYPE_IS_SCOPE macro. If you add
 * non-looping statement enumerators, add them before STMT_DO_LOOP or you will
 * break the STMT_TYPE_IS_LOOP macro.
 *
 * Also remember to keep the statementName array in BytecodeEmitter.cpp in
 * sync.
 */
enum StmtType {
    STMT_LABEL,                 /* labeled statement:  L: s */
    STMT_IF,                    /* if (then) statement */
    STMT_ELSE,                  /* else clause of if statement */
    STMT_SEQ,                   /* synthetic sequence of statements */
    STMT_BLOCK,                 /* compound statement: { s1[;... sN] } */
    STMT_SWITCH,                /* switch statement */
    STMT_WITH,                  /* with statement */
    STMT_CATCH,                 /* catch block */
    STMT_TRY,                   /* try block */
    STMT_FINALLY,               /* finally block */
    STMT_SUBROUTINE,            /* gosub-target subroutine body */
    STMT_DO_LOOP,               /* do/while loop statement */
    STMT_FOR_LOOP,              /* for loop statement */
    STMT_FOR_IN_LOOP,           /* for/in loop statement */
    STMT_WHILE_LOOP,            /* while loop statement */
    STMT_LIMIT
};

inline bool
STMT_TYPE_IN_RANGE(uint16_t type, StmtType begin, StmtType end)
{
    return begin <= type && type <= end;
}

/*
 * A comment on the encoding of the js::StmtType enum and type-testing macros:
 *
 * STMT_TYPE_MAYBE_SCOPE tells whether a statement type is always, or may
 * become, a lexical scope.  It therefore includes block and switch (the two
 * low-numbered "maybe" scope types) and excludes with (with has dynamic scope
 * pending the "reformed with" in ES4/JS2).  It includes all try-catch-finally
 * types, which are high-numbered maybe-scope types.
 *
 * STMT_TYPE_LINKS_SCOPE tells whether a js::StmtInfo of the given type eagerly
 * links to other scoping statement info records.  It excludes the two early
 * "maybe" types, block and switch, as well as the try and both finally types,
 * since try and the other trailing maybe-scope types don't need block scope
 * unless they contain let declarations.
 *
 * We treat WITH as a static scope because it prevents lexical binding from
 * continuing further up the static scope chain. With the lost "reformed with"
 * proposal for ES4, we would be able to model it statically, too.
 */
#define STMT_TYPE_MAYBE_SCOPE(type)                                           \
    (type != STMT_WITH &&                                                     \
     STMT_TYPE_IN_RANGE(type, STMT_BLOCK, STMT_SUBROUTINE))

#define STMT_TYPE_LINKS_SCOPE(type)                                           \
    STMT_TYPE_IN_RANGE(type, STMT_WITH, STMT_CATCH)

#define STMT_TYPE_IS_TRYING(type)                                             \
    STMT_TYPE_IN_RANGE(type, STMT_TRY, STMT_SUBROUTINE)

#define STMT_TYPE_IS_LOOP(type) ((type) >= STMT_DO_LOOP)

#define STMT_MAYBE_SCOPE(stmt)  STMT_TYPE_MAYBE_SCOPE((stmt)->type)
#define STMT_LINKS_SCOPE(stmt)  (STMT_TYPE_LINKS_SCOPE((stmt)->type) ||       \
                                 ((stmt)->flags & SIF_SCOPE))
#define STMT_IS_TRYING(stmt)    STMT_TYPE_IS_TRYING((stmt)->type)
#define STMT_IS_LOOP(stmt)      STMT_TYPE_IS_LOOP((stmt)->type)

struct StmtInfo {
    uint16_t        type;           /* statement type */
    uint16_t        flags;          /* flags, see below */
    uint32_t        blockid;        /* for simplified dominance computation */
    ptrdiff_t       update;         /* loop update offset (top if none) */
    ptrdiff_t       breaks;         /* offset of last break in loop */
    ptrdiff_t       continues;      /* offset of last continue in loop */
    RootedVarAtom   label;          /* name of LABEL */
    RootedVar<StaticBlockObject *> blockObj; /* block scope object */
    StmtInfo        *down;          /* info for enclosing statement */
    StmtInfo        *downScope;     /* next enclosing lexical scope */

    StmtInfo(JSContext *cx) : label(cx), blockObj(cx) {}
};

#define SIF_SCOPE        0x0001     /* statement has its own lexical scope */
#define SIF_BODY_BLOCK   0x0002     /* STMT_BLOCK type is a function body */
#define SIF_FOR_BLOCK    0x0004     /* for (let ...) induced block scope */

#define SET_STATEMENT_TOP(stmt, top)                                          \
    ((stmt)->update = (top), (stmt)->breaks = (stmt)->continues = (-1))

namespace frontend {

bool
SetStaticLevel(SharedContext *sc, unsigned staticLevel);

bool
GenerateBlockId(SharedContext *sc, uint32_t &blockid);

/*
 * Push the C-stack-allocated struct at stmt onto the stmtInfo stack.
 */
void
PushStatement(SharedContext *sc, StmtInfo *stmt, StmtType type, ptrdiff_t top);

/*
 * Push a block scope statement and link blockObj into sc->blockChain. To pop
 * this statement info record, use PopStatementTC as usual, or if appropriate
 * (if generating code), PopStatementBCE.
 */
void
PushBlockScope(SharedContext *sc, StmtInfo *stmt, StaticBlockObject &blockObj, ptrdiff_t top);

/*
 * Pop sc->topStmt. If the top StmtInfo struct is not stack-allocated, it
 * is up to the caller to free it.
 */
void
PopStatementSC(SharedContext *sc);

/*
 * Find a lexically scoped variable (one declared by let, catch, or an array
 * comprehension) named by atom, looking in sc's compile-time scopes.
 *
 * If a WITH statement is reached along the scope stack, return its statement
 * info record, so callers can tell that atom is ambiguous. If slotp is not
 * null, then if atom is found, set *slotp to its stack slot, otherwise to -1.
 * This means that if slotp is not null, all the block objects on the lexical
 * scope chain must have had their depth slots computed by the code generator,
 * so the caller must be under EmitTree.
 *
 * In any event, directly return the statement info record in which atom was
 * found. Otherwise return null.
 */
StmtInfo *
LexicalLookup(SharedContext *sc, JSAtom *atom, int *slotp, StmtInfo *stmt = NULL);

} // namespace frontend

} // namespace js

#endif // TreeContext_h__
