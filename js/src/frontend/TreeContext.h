/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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

JS_ENUM_HEADER(TreeContextFlags, uint32_t)
{
    // parsing inside function body
    TCF_IN_FUNCTION =                          0x1,

    // function has 'return expr;'
    TCF_RETURN_EXPR =                          0x2,

    // function has 'return;'
    TCF_RETURN_VOID =                          0x4,

    // parsing init expr of for; exclude 'in'
    TCF_IN_FOR_INIT =                          0x8,

    // function needs Call object per call
    TCF_FUN_HEAVYWEIGHT =                     0x10,

    // parsed yield statement in function
    TCF_FUN_IS_GENERATOR =                    0x20,

    // block contains a function statement
    TCF_HAS_FUNCTION_STMT =                   0x40,

    // flag lambda from generator expression
    TCF_GENEXP_LAMBDA =                       0x80,

    // script can optimize name references based on scope chain
    TCF_COMPILE_N_GO =                       0x100,

    // API caller does not want result value from global script
    TCF_NO_SCRIPT_RVAL =                     0x200,

    // Set when parsing a declaration-like destructuring pattern.  This flag
    // causes PrimaryExpr to create PN_NAME parse nodes for variable references
    // which are not hooked into any definition's use chain, added to any tree
    // context's AtomList, etc. etc.  CheckDestructuring will do that work
    // later.
    //
    // The comments atop CheckDestructuring explain the distinction between
    // assignment-like and declaration-like destructuring patterns, and why
    // they need to be treated differently.
    //
    TCF_DECL_DESTRUCTURING =                 0x400,

    // This function/global/eval code body contained a Use Strict Directive.
    // Treat certain strict warnings as errors, and forbid the use of 'with'.
    // See also TSF_STRICT_MODE_CODE, JSScript::strictModeCode, and
    // JSREPORT_STRICT_ERROR.
    //
    TCF_STRICT_MODE_CODE =                   0x800,

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
    TCF_BINDINGS_ACCESSED_DYNAMICALLY =     0x1000,

    // Compiling an eval() script.
    TCF_COMPILE_FOR_EVAL =                  0x2000,

    // The function or a function that encloses it may define new local names
    // at runtime through means other than calling eval.
    TCF_FUN_MIGHT_ALIAS_LOCALS =            0x4000,

    // The script contains singleton initialiser JSOP_OBJECT.
    TCF_HAS_SINGLETONS =                    0x8000,

    // Some enclosing scope is a with-statement or E4X filter-expression.
    TCF_IN_WITH =                          0x10000,

    // This function does something that can extend the set of bindings in its
    // call objects --- it does a direct eval in non-strict code, or includes a
    // function statement (as opposed to a function definition).
    //
    // This flag is *not* inherited by enclosed or enclosing functions; it
    // applies only to the function in whose flags it appears.
    //
    TCF_FUN_EXTENSIBLE_SCOPE =             0x20000,

    // The caller is JS_Compile*Script*.
    TCF_NEED_SCRIPT_GLOBAL =               0x40000,

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
    TCF_ARGUMENTS_HAS_LOCAL_BINDING =      0x80000,

    // In many cases where 'arguments' has a local binding (as described above)
    // we do not need to actually create an arguments object in the function
    // prologue: instead we can analyze how 'arguments' is used (using the
    // simple dataflow analysis in analyzeSSA) to determine that uses of
    // 'arguments' can just read from the stack frame directly. However, the
    // dataflow analysis only looks at how JSOP_ARGUMENTS is used, so it will
    // be unsound in several cases. The frontend filters out such cases by
    // setting this flag which eagerly sets script->needsArgsObj to true.
    //
    TCF_DEFINITELY_NEEDS_ARGS_OBJ =       0x100000

} JS_ENUM_FOOTER(TreeContextFlags);

// Flags to check for return; vs. return expr; in a function.
static const uint32_t TCF_RETURN_FLAGS = TCF_RETURN_EXPR | TCF_RETURN_VOID;

// Sticky deoptimization flags to propagate from FunctionBody.
static const uint32_t TCF_FUN_FLAGS = TCF_FUN_HEAVYWEIGHT |
                                      TCF_FUN_IS_GENERATOR |
                                      TCF_BINDINGS_ACCESSED_DYNAMICALLY |
                                      TCF_FUN_MIGHT_ALIAS_LOCALS |
                                      TCF_STRICT_MODE_CODE |
                                      TCF_FUN_EXTENSIBLE_SCOPE |
                                      TCF_ARGUMENTS_HAS_LOCAL_BINDING |
                                      TCF_DEFINITELY_NEEDS_ARGS_OBJ;

typedef HashSet<JSAtom *> FuncStmtSet;

struct Parser;
struct StmtInfo;

struct TreeContext {                /* tree context for semantic checks */
    uint32_t        flags;          /* statement state flags, see above */
    uint32_t        bodyid;         /* block number of program/function body */
    uint32_t        blockidGen;     /* preincremented block number generator */
    uint32_t        parenDepth;     /* nesting depth of parens that might turn out
                                       to be generator expressions */
    uint32_t        yieldCount;     /* number of |yield| tokens encountered at
                                       non-zero depth in current paren tree */
    StmtInfo        *topStmt;       /* top of statement info stack */
    StmtInfo        *topScopeStmt;  /* top lexical scope statement */
    RootedVar<StaticBlockObject *> blockChain;
                                    /* compile time block scope chain (NB: one
                                       deeper than the topScopeStmt/downScope
                                       chain when in head of let block/expr) */
    ParseNode       *blockNode;     /* parse node for a block with let declarations
                                       (block with its own lexical scope)  */
    AtomDecls       decls;          /* function, const, and var declarations */
    Parser          *parser;        /* ptr to common parsing and lexing data */
    ParseNode       *yieldNode;     /* parse node for a yield expression that might
                                       be an error if we turn out to be inside a
                                       generator expression */
    ParseNode       *argumentsNode; /* parse node for an arguments variable that
                                       might be an error if we turn out to be
                                       inside a generator expression */

  private:
    RootedVarFunction fun_;         /* function to store argument and variable
                                       names when flags & TCF_IN_FUNCTION */
    RootedVarObject   scopeChain_;  /* scope chain object for the script */

  public:
    JSFunction *fun() const {
        JS_ASSERT(inFunction());
        return fun_;
    }
    void setFunction(JSFunction *fun) {
        JS_ASSERT(inFunction());
        fun_ = fun;
    }
    JSObject *scopeChain() const {
        JS_ASSERT(!inFunction());
        return scopeChain_;
    }
    void setScopeChain(JSObject *scopeChain) {
        JS_ASSERT(!inFunction());
        scopeChain_ = scopeChain;
    }

    OwnedAtomDefnMapPtr lexdeps;    /* unresolved lexical name dependencies */

    TreeContext     *parent;        /* Enclosing function or global context.  */

    unsigned        staticLevel;    /* static compilation unit nesting level */

    FunctionBox     *funbox;        /* null or box for function we're compiling
                                       if (flags & TCF_IN_FUNCTION) and not in
                                       js::frontend::CompileFunctionBody */
    FunctionBox     *functionList;

    ParseNode       *innermostWith; /* innermost WITH parse node */

    Bindings        bindings;       /* bindings in this code, including
                                       arguments if we're compiling a function */
    Bindings::StackRoot bindingsRoot; /* root for stack allocated bindings. */

    FuncStmtSet *funcStmts;         /* Set of (non-top-level) function statements
                                       that will alias any top-level bindings with
                                       the same name. */

    void trace(JSTracer *trc);

    inline TreeContext(Parser *prs);
    inline ~TreeContext();

    // js::BytecodeEmitter derives from js::TreeContext; however, only the
    // top-level BytecodeEmitters are actually used as full-fledged tree contexts
    // (to hold decls and lexdeps). We can avoid allocation overhead by making
    // this distinction explicit.
    enum InitBehavior {
        USED_AS_TREE_CONTEXT,
        USED_AS_CODE_GENERATOR
    };

    bool init(JSContext *cx, InitBehavior ib = USED_AS_TREE_CONTEXT) {
        if (cx->hasRunOption(JSOPTION_STRICT_MODE))
            flags |= TCF_STRICT_MODE_CODE;
        if (ib == USED_AS_CODE_GENERATOR)
            return true;
        return decls.init() && lexdeps.ensureMap(cx);
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

    bool inStrictMode() const {
        return flags & TCF_STRICT_MODE_CODE;
    }

    // Return true if we need to check for conditions that elicit
    // JSOPTION_STRICT warnings or strict mode errors.
    inline bool needStrictChecks();

    bool compileAndGo() const { return flags & TCF_COMPILE_N_GO; }
    bool inFunction() const { return flags & TCF_IN_FUNCTION; }

    void noteBindingsAccessedDynamically() {
        flags |= TCF_BINDINGS_ACCESSED_DYNAMICALLY;
    }

    bool bindingsAccessedDynamically() const {
        return flags & TCF_BINDINGS_ACCESSED_DYNAMICALLY;
    }

    void noteMightAliasLocals() {
        flags |= TCF_FUN_MIGHT_ALIAS_LOCALS;
    }

    bool mightAliasLocals() const {
        return flags & TCF_FUN_MIGHT_ALIAS_LOCALS;
    }

    void noteArgumentsHasLocalBinding() {
        flags |= TCF_ARGUMENTS_HAS_LOCAL_BINDING;
    }

    bool argumentsHasLocalBinding() const {
        return flags & TCF_ARGUMENTS_HAS_LOCAL_BINDING;
    }

    unsigned argumentsLocalSlot() const;

    void noteDefinitelyNeedsArgsObj() {
        JS_ASSERT(argumentsHasLocalBinding());
        flags |= TCF_DEFINITELY_NEEDS_ARGS_OBJ;
    }

    bool definitelyNeedsArgsObj() const {
        return flags & TCF_DEFINITELY_NEEDS_ARGS_OBJ;
    }

    void noteHasExtensibleScope() {
        flags |= TCF_FUN_EXTENSIBLE_SCOPE;
    }

    bool hasExtensibleScope() const {
        return flags & TCF_FUN_EXTENSIBLE_SCOPE;
    }

    ParseNode *freeTree(ParseNode *pn);
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
SetStaticLevel(TreeContext *tc, unsigned staticLevel);

bool
GenerateBlockId(TreeContext *tc, uint32_t &blockid);

/*
 * Push the C-stack-allocated struct at stmt onto the stmtInfo stack.
 */
void
PushStatement(TreeContext *tc, StmtInfo *stmt, StmtType type, ptrdiff_t top);

/*
 * Push a block scope statement and link blockObj into tc->blockChain. To pop
 * this statement info record, use PopStatementTC as usual, or if appropriate
 * (if generating code), PopStatementBCE.
 */
void
PushBlockScope(TreeContext *tc, StmtInfo *stmt, StaticBlockObject &blockObj, ptrdiff_t top);

/*
 * Pop tc->topStmt. If the top StmtInfo struct is not stack-allocated, it
 * is up to the caller to free it.
 */
void
PopStatementTC(TreeContext *tc);

/*
 * Find a lexically scoped variable (one declared by let, catch, or an array
 * comprehension) named by atom, looking in tc's compile-time scopes.
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
LexicalLookup(TreeContext *tc, JSAtom *atom, int *slotp, StmtInfo *stmt = NULL);

} // namespace frontend

} // namespace js

#endif // TreeContext_h__
