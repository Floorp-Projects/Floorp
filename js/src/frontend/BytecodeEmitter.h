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

#ifndef BytecodeEmitter_h__
#define BytecodeEmitter_h__

/*
 * JS bytecode generation.
 */
#include "jstypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#include "frontend/Parser.h"
#include "frontend/ParseMaps.h"

namespace js {

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
    union {
        JSAtom      *label;         /* name of LABEL */
        StaticBlockObject *blockObj;/* block scope object */
    };
    StmtInfo        *down;          /* info for enclosing statement */
    StmtInfo        *downScope;     /* next enclosing lexical scope */
};

#define SIF_SCOPE        0x0001     /* statement has its own lexical scope */
#define SIF_BODY_BLOCK   0x0002     /* STMT_BLOCK type is a function body */
#define SIF_FOR_BLOCK    0x0004     /* for (let ...) induced block scope */

/*
 * To reuse space in StmtInfo, rename breaks and continues for use during
 * try/catch/finally code generation and backpatching. To match most common
 * use cases, the macro argument is a struct, not a struct pointer. Only a
 * loop, switch, or label statement info record can have breaks and continues,
 * and only a for loop has an update backpatch chain, so it's safe to overlay
 * these for the "trying" StmtTypes.
 */
#define CATCHNOTE(stmt)  ((stmt).update)
#define GOSUBS(stmt)     ((stmt).breaks)
#define GUARDJUMP(stmt)  ((stmt).continues)

#define SET_STATEMENT_TOP(stmt, top)                                          \
    ((stmt)->update = (top), (stmt)->breaks = (stmt)->continues = (-1))

#define TCF_COMPILING           0x01 /* TreeContext is BytecodeEmitter */
#define TCF_IN_FUNCTION         0x02 /* parsing inside function body */
#define TCF_RETURN_EXPR         0x04 /* function has 'return expr;' */
#define TCF_RETURN_VOID         0x08 /* function has 'return;' */
#define TCF_IN_FOR_INIT         0x10 /* parsing init expr of for; exclude 'in' */
#define TCF_FUN_SETS_OUTER_NAME 0x20 /* function set outer name (lexical or free) */
#define TCF_FUN_PARAM_ARGUMENTS 0x40 /* function has parameter named arguments */
#define TCF_FUN_LOCAL_ARGUMENTS 0x80 /* function may contain a local named arguments */
#define TCF_FUN_USES_ARGUMENTS 0x100 /* function uses arguments except as a
                                        parameter name */
#define TCF_FUN_HEAVYWEIGHT    0x200 /* function needs Call object per call */
#define TCF_FUN_IS_GENERATOR   0x400 /* parsed yield statement in function */
#define TCF_FUN_USES_OWN_NAME  0x800 /* named function expression that uses its
                                        own name */
#define TCF_HAS_FUNCTION_STMT 0x1000 /* block contains a function statement */
#define TCF_GENEXP_LAMBDA     0x2000 /* flag lambda from generator expression */
#define TCF_COMPILE_N_GO      0x4000 /* compile-and-go mode of script, can
                                        optimize name references based on scope
                                        chain */
#define TCF_NO_SCRIPT_RVAL    0x8000 /* API caller does not want result value
                                        from global script */
/*
 * Set when parsing a declaration-like destructuring pattern.  This
 * flag causes PrimaryExpr to create PN_NAME parse nodes for variable
 * references which are not hooked into any definition's use chain,
 * added to any tree context's AtomList, etc. etc.  CheckDestructuring
 * will do that work later.
 *
 * The comments atop CheckDestructuring explain the distinction
 * between assignment-like and declaration-like destructuring
 * patterns, and why they need to be treated differently.
 */
#define TCF_DECL_DESTRUCTURING  0x10000

/*
 * This function/global/eval code body contained a Use Strict Directive. Treat
 * certain strict warnings as errors, and forbid the use of 'with'. See also
 * TSF_STRICT_MODE_CODE, JSScript::strictModeCode, and JSREPORT_STRICT_ERROR.
 */
#define TCF_STRICT_MODE_CODE    0x20000

/* bits 0x40000 and 0x80000 are unused */

/*
 * "Module pattern", i.e., a lambda that is immediately applied and the whole
 * of an expression statement.
 */
#define TCF_FUN_MODULE_PATTERN 0x200000

/*
 * Flag to prevent a non-escaping function from being optimized into a null
 * closure (i.e., a closure that needs only its global object for free variable
 * resolution), because this function contains a closure that needs one or more
 * scope objects surrounding it (i.e., a Call object for an outer heavyweight
 * function). See bug 560234.
 */
#define TCF_FUN_ENTRAINS_SCOPES 0x400000

/* The function calls 'eval'. */
#define TCF_FUN_CALLS_EVAL       0x800000

/* The function mutates a positional (non-destructuring) parameter. */
#define TCF_FUN_MUTATES_PARAMETER 0x1000000

/*
 * Compiling an eval() script.
 */
#define TCF_COMPILE_FOR_EVAL     0x2000000

/*
 * The function or a function that encloses it may define new local names
 * at runtime through means other than calling eval.
 */
#define TCF_FUN_MIGHT_ALIAS_LOCALS  0x4000000

/*
 * The script contains singleton initialiser JSOP_OBJECT.
 */
#define TCF_HAS_SINGLETONS       0x8000000

/*
 * Some enclosing scope is a with-statement or E4X filter-expression.
 */
#define TCF_IN_WITH             0x10000000

/*
 * This function does something that can extend the set of bindings in its
 * call objects --- it does a direct eval in non-strict code, or includes a
 * function statement (as opposed to a function definition).
 *
 * This flag is *not* inherited by enclosed or enclosing functions; it
 * applies only to the function in whose flags it appears.
 */
#define TCF_FUN_EXTENSIBLE_SCOPE 0x20000000

/*
 * The caller is JS_Compile*Script*.
 */
#define TCF_NEED_SCRIPT_GLOBAL 0x40000000

/*
 * Flags to check for return; vs. return expr; in a function.
 */
#define TCF_RETURN_FLAGS        (TCF_RETURN_EXPR | TCF_RETURN_VOID)

/*
 * Sticky deoptimization flags to propagate from FunctionBody.
 */
#define TCF_FUN_FLAGS           (TCF_FUN_SETS_OUTER_NAME |                    \
                                 TCF_FUN_USES_ARGUMENTS  |                    \
                                 TCF_FUN_PARAM_ARGUMENTS |                    \
                                 TCF_FUN_LOCAL_ARGUMENTS |                    \
                                 TCF_FUN_HEAVYWEIGHT     |                    \
                                 TCF_FUN_IS_GENERATOR    |                    \
                                 TCF_FUN_USES_OWN_NAME   |                    \
                                 TCF_FUN_CALLS_EVAL      |                    \
                                 TCF_FUN_MIGHT_ALIAS_LOCALS |                 \
                                 TCF_FUN_MUTATES_PARAMETER |                  \
                                 TCF_STRICT_MODE_CODE    |                    \
                                 TCF_FUN_EXTENSIBLE_SCOPE)

struct BytecodeEmitter;

struct TreeContext {                /* tree context for semantic checks */
    uint32_t        flags;          /* statement state flags, see above */
    uint32_t        bodyid;         /* block number of program/function body */
    uint32_t        blockidGen;     /* preincremented block number generator */
    uint32_t        parenDepth;     /* nesting depth of parens that might turn out
                                       to be generator expressions */
    uint32_t        yieldCount;     /* number of |yield| tokens encountered at
                                       non-zero depth in current paren tree */
    uint32_t        argumentsCount; /* number of |arguments| references encountered
                                       at non-zero depth in current paren tree */
    StmtInfo        *topStmt;       /* top of statement info stack */
    StmtInfo        *topScopeStmt;  /* top lexical scope statement */
    StaticBlockObject *blockChain;  /* compile block scope chain (NB: one
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
    union {
        JSFunction  *fun_;          /* function to store argument and variable
                                       names when flags & TCF_IN_FUNCTION */
        JSObject    *scopeChain_;   /* scope chain object for the script */
    };

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
    TreeContext     *parent;        /* enclosing function or global context */
    unsigned           staticLevel;    /* static compilation unit nesting level */

    FunctionBox     *funbox;        /* null or box for function we're compiling
                                       if (flags & TCF_IN_FUNCTION) and not in
                                       js::frontend::CompileFunctionBody */
    FunctionBox     *functionList;

    ParseNode       *innermostWith; /* innermost WITH parse node */

    Bindings        bindings;       /* bindings in this code, including
                                       arguments if we're compiling a function */
    Bindings::StackRoot bindingsRoot; /* root for stack allocated bindings. */

    void trace(JSTracer *trc);

    inline TreeContext(Parser *prs);
    inline ~TreeContext();

    /*
     * js::BytecodeEmitter derives from js::TreeContext; however, only the
     * top-level BytecodeEmitters are actually used as full-fledged tree contexts
     * (to hold decls and lexdeps). We can avoid allocation overhead by making
     * this distinction explicit.
     */
    enum InitBehavior {
        USED_AS_TREE_CONTEXT,
        USED_AS_CODE_GENERATOR
    };

    bool init(JSContext *cx, InitBehavior ib = USED_AS_TREE_CONTEXT) {
        if (ib == USED_AS_CODE_GENERATOR)
            return true;
        return decls.init() && lexdeps.ensureMap(cx);
    }

    unsigned blockid() { return topStmt ? topStmt->blockid : bodyid; }

    /*
     * True if we are at the topmost level of a entire script or function body.
     * For example, while parsing this code we would encounter f1 and f2 at
     * body level, but we would not encounter f3 or f4 at body level:
     *
     *   function f1() { function f2() { } }
     *   if (cond) { function f3() { if (cond) { function f4() { } } } }
     */
    bool atBodyLevel() { return !topStmt || (topStmt->flags & SIF_BODY_BLOCK); }

    /* Test whether we're in a statement of given type. */
    bool inStatement(StmtType type);

    bool inStrictMode() const {
        return flags & TCF_STRICT_MODE_CODE;
    }

    inline bool needStrictChecks();

    // Return true there is a generator function within |skip| lexical scopes
    // (going upward) from this context's lexical scope. Always return true if
    // this context is itself a generator.
    bool skipSpansGenerator(unsigned skip);

    bool compileAndGo() const { return flags & TCF_COMPILE_N_GO; }
    bool inFunction() const { return flags & TCF_IN_FUNCTION; }

    bool compiling() const { return flags & TCF_COMPILING; }
    inline BytecodeEmitter *asBytecodeEmitter();

    bool usesArguments() const {
        return flags & TCF_FUN_USES_ARGUMENTS;
    }

    void noteCallsEval() {
        flags |= TCF_FUN_CALLS_EVAL;
    }

    bool callsEval() const {
        return flags & TCF_FUN_CALLS_EVAL;
    }

    void noteMightAliasLocals() {
        flags |= TCF_FUN_MIGHT_ALIAS_LOCALS;
    }

    bool mightAliasLocals() const {
        return flags & TCF_FUN_MIGHT_ALIAS_LOCALS;
    }

    void noteParameterMutation() {
        JS_ASSERT(inFunction());
        flags |= TCF_FUN_MUTATES_PARAMETER;
    }

    bool mutatesParameter() const {
        JS_ASSERT(inFunction());
        return flags & TCF_FUN_MUTATES_PARAMETER;
    }

    bool mayOverwriteArguments() const {
        JS_ASSERT(inFunction());
        JS_ASSERT_IF(inStrictMode(),
                     !(flags & (TCF_FUN_PARAM_ARGUMENTS | TCF_FUN_LOCAL_ARGUMENTS)));
        return !inStrictMode() &&
               (callsEval() ||
                flags & (TCF_FUN_PARAM_ARGUMENTS | TCF_FUN_LOCAL_ARGUMENTS));
    }

    void noteArgumentsNameUse(ParseNode *node) {
        JS_ASSERT(inFunction());
        JS_ASSERT(node->isKind(PNK_NAME));
        JS_ASSERT(node->pn_atom == parser->context->runtime->atomState.argumentsAtom);
        countArgumentsUse(node);
        flags |= TCF_FUN_USES_ARGUMENTS;
        if (funbox)
            funbox->node->pn_dflags |= PND_FUNARG;
    }

    /*
     * Uses of |arguments| must be noted so that such uses can be forbidden if
     * they occur inside generator expressions (which desugar to functions and
     * yields, in which |arguments| would have an entirely different meaning).
     */
    void countArgumentsUse(ParseNode *node) {
        JS_ASSERT(node->isKind(PNK_NAME));
        JS_ASSERT(node->pn_atom == parser->context->runtime->atomState.argumentsAtom);
        argumentsCount++;
        argumentsNode = node;
    }

    bool needsEagerArguments() const {
        return inStrictMode() && ((usesArguments() && mutatesParameter()) || callsEval());
    }

    void noteHasExtensibleScope() {
        flags |= TCF_FUN_EXTENSIBLE_SCOPE;
    }

    bool hasExtensibleScope() const {
        return flags & TCF_FUN_EXTENSIBLE_SCOPE;
    }

    ParseNode *freeTree(ParseNode *pn) { return parser->freeTree(pn); }
};

/*
 * Return true if we need to check for conditions that elicit
 * JSOPTION_STRICT warnings or strict mode errors.
 */
inline bool TreeContext::needStrictChecks() {
    return parser->context->hasStrictOption() || inStrictMode();
}

namespace frontend {

bool
SetStaticLevel(TreeContext *tc, unsigned staticLevel);

bool
GenerateBlockId(TreeContext *tc, uint32_t &blockid);

} /* namespace frontend */

struct TryNode {
    JSTryNote       note;
    TryNode       *prev;
};

struct CGObjectList {
    uint32_t            length;     /* number of emitted so far objects */
    ObjectBox           *lastbox;   /* last emitted object */

    CGObjectList() : length(0), lastbox(NULL) {}

    unsigned index(ObjectBox *objbox);
    void finish(JSObjectArray *array);
};

class GCConstList {
    Vector<Value> list;
  public:
    GCConstList(JSContext *cx) : list(cx) {}
    bool append(Value v) { return list.append(v); }
    size_t length() const { return list.length(); }
    void finish(JSConstArray *array);
};

struct GlobalScope {
    GlobalScope(JSContext *cx, JSObject *globalObj, BytecodeEmitter *bce)
      : globalObj(globalObj), bce(bce), defs(cx), names(cx)
    { }

    struct GlobalDef {
        JSAtom        *atom;        // If non-NULL, specifies the property name to add.
        FunctionBox   *funbox;      // If non-NULL, function value for the property.
                                    // This value is only set/used if atom is non-NULL.
        uint32_t      knownSlot;    // If atom is NULL, this is the known shape slot.

        GlobalDef() { }
        GlobalDef(uint32_t knownSlot) : atom(NULL), knownSlot(knownSlot) { }
        GlobalDef(JSAtom *atom, FunctionBox *box) : atom(atom), funbox(box) { }
    };

    JSObject        *globalObj;
    BytecodeEmitter *bce;

    /*
     * This is the table of global names encountered during parsing. Each
     * global name appears in the list only once, and the |names| table
     * maps back into |defs| for fast lookup.
     *
     * A definition may either specify an existing global property, or a new
     * one that must be added after compilation succeeds.
     */
    Vector<GlobalDef, 16> defs;
    AtomIndexMap      names;
};

struct BytecodeEmitter : public TreeContext
{
    struct {
        jsbytecode  *base;          /* base of JS bytecode vector */
        jsbytecode  *limit;         /* one byte beyond end of bytecode */
        jsbytecode  *next;          /* pointer to next free bytecode */
        jssrcnote   *notes;         /* source notes, see below */
        unsigned       noteCount;      /* number of source notes so far */
        unsigned       noteLimit;      /* limit number for source notes in notePool */
        ptrdiff_t   lastNoteOffset; /* code offset for last source note */
        unsigned       currentLine;    /* line number for tree-based srcnote gen */
    } prolog, main, *current;

    OwnedAtomIndexMapPtr atomIndices; /* literals indexed for mapping */
    AtomDefnMapPtr  roLexdeps;
    unsigned           firstLine;      /* first line, for JSScript::NewScriptFromEmitter */

    int            stackDepth;     /* current stack depth in script frame */
    unsigned           maxStackDepth;  /* maximum stack depth so far */

    unsigned           ntrynotes;      /* number of allocated so far try notes */
    TryNode         *lastTryNode;   /* the last allocated try node */

    unsigned           arrayCompDepth; /* stack depth of array in comprehension */

    unsigned           emitLevel;      /* js::frontend::EmitTree recursion level */

    typedef HashMap<JSAtom *, Value> ConstMap;
    ConstMap        constMap;       /* compile time constants */

    GCConstList     constList;      /* constants to be included with the script */

    CGObjectList    objectList;     /* list of emitted objects */
    CGObjectList    regexpList;     /* list of emitted regexp that will be
                                       cloned during execution */

    GlobalScope     *globalScope;   /* frontend::CompileScript global scope, or null */

    typedef Vector<GlobalSlotArray::Entry, 16> GlobalUseVector;

    GlobalUseVector globalUses;     /* per-script global uses */
    OwnedAtomIndexMapPtr globalMap; /* per-script map of global name to globalUses vector */

    /* Vectors of pn_cookie slot values. */
    typedef Vector<uint32_t, 8> SlotVector;
    SlotVector      closedArgs;
    SlotVector      closedVars;

    uint16_t        typesetCount;   /* Number of JOF_TYPESET opcodes generated */

    BytecodeEmitter(Parser *parser, unsigned lineno);
    bool init(JSContext *cx, TreeContext::InitBehavior ib = USED_AS_CODE_GENERATOR);

    JSContext *context() {
        return parser->context;
    }

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destructor call.
     */
    ~BytecodeEmitter();

    /*
     * Adds a use of a variable that is statically known to exist on the
     * global object.
     *
     * The actual slot of the variable on the global object is not known
     * until after compilation. Properties must be resolved before being
     * added, to avoid aliasing properties that should be resolved. This makes
     * slot prediction based on the global object's free slot impossible. So,
     * we use the slot to index into bce->globalScope->defs, and perform a
     * fixup of the script at the very end of compilation.
     *
     * If the global use can be cached, |cookie| will be set to |slot|.
     * Otherwise, |cookie| is set to the free cookie value.
     */
    bool addGlobalUse(JSAtom *atom, uint32_t slot, UpvarCookie *cookie);

    bool compilingForEval() const { return !!(flags & TCF_COMPILE_FOR_EVAL); }
    JSVersion version() const { return parser->versionWithFlags(); }

    bool shouldNoteClosedName(ParseNode *pn);

    JS_ALWAYS_INLINE
    bool makeAtomIndex(JSAtom *atom, jsatomid *indexp) {
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

    bool checkSingletonContext() {
        if (!compileAndGo() || inFunction())
            return false;
        for (StmtInfo *stmt = topStmt; stmt; stmt = stmt->down) {
            if (STMT_IS_LOOP(stmt))
                return false;
        }
        flags |= TCF_HAS_SINGLETONS;
        return true;
    }

    bool needsImplicitThis();

    TokenStream *tokenStream() { return &parser->tokenStream; }

    jsbytecode *base() const { return current->base; }
    jsbytecode *limit() const { return current->limit; }
    jsbytecode *next() const { return current->next; }
    jsbytecode *code(ptrdiff_t offset) const { return base() + offset; }
    ptrdiff_t offset() const { return next() - base(); }
    jsbytecode *prologBase() const { return prolog.base; }
    ptrdiff_t prologOffset() const { return prolog.next - prolog.base; }
    void switchToMain() { current = &main; }
    void switchToProlog() { current = &prolog; }

    jssrcnote *notes() const { return current->notes; }
    unsigned noteCount() const { return current->noteCount; }
    unsigned noteLimit() const { return current->noteLimit; }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }

    inline ptrdiff_t countFinalSourceNotes();
};

inline BytecodeEmitter *
TreeContext::asBytecodeEmitter()
{
    JS_ASSERT(compiling());
    return static_cast<BytecodeEmitter *>(this);
}

namespace frontend {

/*
 * Emit one bytecode.
 */
ptrdiff_t
Emit1(JSContext *cx, BytecodeEmitter *bce, JSOp op);

/*
 * Emit two bytecodes, an opcode (op) with a byte of immediate operand (op1).
 */
ptrdiff_t
Emit2(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1);

/*
 * Emit three bytecodes, an opcode with two bytes of immediate operands.
 */
ptrdiff_t
Emit3(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1, jsbytecode op2);

/*
 * Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
 */
ptrdiff_t
EmitN(JSContext *cx, BytecodeEmitter *bce, JSOp op, size_t extra);

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
 * Like PopStatementTC(bce), also patch breaks and continues unless the top
 * statement info record represents a try-catch-finally suite. May fail if a
 * jump offset overflows.
 */
JSBool
PopStatementBCE(JSContext *cx, BytecodeEmitter *bce);

/*
 * Define and lookup a primitive jsval associated with the const named by atom.
 * DefineCompileTimeConstant analyzes the constant-folded initializer at pn
 * and saves the const's value in bce->constList, if it can be used at compile
 * time. It returns true unless an error occurred.
 *
 * If the initializer's value could not be saved, DefineCompileTimeConstant
 * calls will return the undefined value. DefineCompileTimeConstant tries
 * to find a const value memorized for atom, returning true with *vp set to a
 * value other than undefined if the constant was found, true with *vp set to
 * JSVAL_VOID if not found, and false on error.
 */
JSBool
DefineCompileTimeConstant(JSContext *cx, BytecodeEmitter *bce, JSAtom *atom, ParseNode *pn);

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

/*
 * Emit code into bce for the tree rooted at pn.
 */
JSBool
EmitTree(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn);

/*
 * Emit function code using bce for the tree rooted at body.
 */
JSBool
EmitFunctionScript(JSContext *cx, BytecodeEmitter *bce, ParseNode *body);

} /* namespace frontend */

/*
 * Source notes generated along with bytecode for decompiling and debugging.
 * A source note is a uint8_t with 5 bits of type and 3 of offset from the pc
 * of the previous note. If 3 bits of offset aren't enough, extended delta
 * notes (SRC_XDELTA) consisting of 2 set high order bits followed by 6 offset
 * bits are emitted before the next note. Some notes have operand offsets
 * encoded immediately after them, in note bytes or byte-triples.
 *
 *                 Source Note               Extended Delta
 *              +7-6-5-4-3+2-1-0+           +7-6-5+4-3-2-1-0+
 *              |note-type|delta|           |1 1| ext-delta |
 *              +---------+-----+           +---+-----------+
 *
 * At most one "gettable" note (i.e., a note of type other than SRC_NEWLINE,
 * SRC_SETLINE, and SRC_XDELTA) applies to a given bytecode.
 *
 * NB: the js_SrcNoteSpec array in BytecodeEmitter.cpp is indexed by this
 * enum, so its initializers need to match the order here.
 *
 * Note on adding new source notes: every pair of bytecodes (A, B) where A and
 * B have disjoint sets of source notes that could apply to each bytecode may
 * reuse the same note type value for two notes (snA, snB) that have the same
 * arity in JSSrcNoteSpec. This is why SRC_IF and SRC_INITPROP have the same
 * value below.
 *
 * Don't forget to update JSXDR_BYTECODE_VERSION in jsxdrapi.h for all such
 * incompatible source note or other bytecode changes.
 */
enum SrcNoteType {
    SRC_NULL        = 0,        /* terminates a note vector */
    SRC_IF          = 1,        /* JSOP_IFEQ bytecode is from an if-then */
    SRC_BREAK       = 1,        /* JSOP_GOTO is a break */
    SRC_INITPROP    = 1,        /* disjoint meaning applied to JSOP_INITELEM or
                                   to an index label in a regular (structuring)
                                   or a destructuring object initialiser */
    SRC_GENEXP      = 1,        /* JSOP_LAMBDA from generator expression */
    SRC_IF_ELSE     = 2,        /* JSOP_IFEQ bytecode is from an if-then-else */
    SRC_FOR_IN      = 2,        /* JSOP_GOTO to for-in loop condition from
                                   before loop (same arity as SRC_IF_ELSE) */
    SRC_FOR         = 3,        /* JSOP_NOP or JSOP_POP in for(;;) loop head */
    SRC_WHILE       = 4,        /* JSOP_GOTO to for or while loop condition
                                   from before loop, else JSOP_NOP at top of
                                   do-while loop */
    SRC_CONTINUE    = 5,        /* JSOP_GOTO is a continue, not a break;
                                   JSOP_ENDINIT needs extra comma at end of
                                   array literal: [1,2,,];
                                   JSOP_DUP continuing destructuring pattern;
                                   JSOP_POP at end of for-in */
    SRC_DECL        = 6,        /* type of a declaration (var, const, let*) */
    SRC_DESTRUCT    = 6,        /* JSOP_DUP starting a destructuring assignment
                                   operation, with SRC_DECL_* offset operand */
    SRC_PCDELTA     = 7,        /* distance forward from comma-operator to
                                   next POP, or from CONDSWITCH to first CASE
                                   opcode, etc. -- always a forward delta */
    SRC_GROUPASSIGN = 7,        /* SRC_DESTRUCT variant for [a, b] = [c, d] */
    SRC_DESTRUCTLET = 7,        /* JSOP_DUP starting a destructuring let
                                   operation, with offset to JSOP_ENTERLET0 */
    SRC_ASSIGNOP    = 8,        /* += or another assign-op follows */
    SRC_COND        = 9,        /* JSOP_IFEQ is from conditional ?: operator */
    SRC_BRACE       = 10,       /* mandatory brace, for scope or to avoid
                                   dangling else */
    SRC_HIDDEN      = 11,       /* opcode shouldn't be decompiled */
    SRC_PCBASE      = 12,       /* distance back from annotated getprop or
                                   setprop op to left-most obj.prop.subprop
                                   bytecode -- always a backward delta */
    SRC_LABEL       = 13,       /* JSOP_LABEL for label: with atomid immediate */
    SRC_LABELBRACE  = 14,       /* JSOP_LABEL for label: {...} begin brace */
    SRC_ENDBRACE    = 15,       /* JSOP_NOP for label: {...} end brace */
    SRC_BREAK2LABEL = 16,       /* JSOP_GOTO for 'break label' with atomid */
    SRC_CONT2LABEL  = 17,       /* JSOP_GOTO for 'continue label' with atomid */
    SRC_SWITCH      = 18,       /* JSOP_*SWITCH with offset to end of switch,
                                   2nd off to first JSOP_CASE if condswitch */
    SRC_SWITCHBREAK = 18,       /* JSOP_GOTO is a break in a switch */
    SRC_FUNCDEF     = 19,       /* JSOP_NOP for function f() with atomid */
    SRC_CATCH       = 20,       /* catch block has guard */
                                /* 21 is unused */
    SRC_NEWLINE     = 22,       /* bytecode follows a source newline */
    SRC_SETLINE     = 23,       /* a file-absolute source line number note */
    SRC_XDELTA      = 24        /* 24-31 are for extended delta notes */
};

/*
 * Constants for the SRC_DECL source note.
 *
 * NB: the var_prefix array in jsopcode.c depends on these dense indexes from
 * SRC_DECL_VAR through SRC_DECL_LET.
 */
#define SRC_DECL_VAR            0
#define SRC_DECL_CONST          1
#define SRC_DECL_LET            2
#define SRC_DECL_NONE           3

#define SN_TYPE_BITS            5
#define SN_DELTA_BITS           3
#define SN_XDELTA_BITS          6
#define SN_TYPE_MASK            (JS_BITMASK(SN_TYPE_BITS) << SN_DELTA_BITS)
#define SN_DELTA_MASK           ((ptrdiff_t)JS_BITMASK(SN_DELTA_BITS))
#define SN_XDELTA_MASK          ((ptrdiff_t)JS_BITMASK(SN_XDELTA_BITS))

#define SN_MAKE_NOTE(sn,t,d)    (*(sn) = (jssrcnote)                          \
                                          (((t) << SN_DELTA_BITS)             \
                                           | ((d) & SN_DELTA_MASK)))
#define SN_MAKE_XDELTA(sn,d)    (*(sn) = (jssrcnote)                          \
                                          ((SRC_XDELTA << SN_DELTA_BITS)      \
                                           | ((d) & SN_XDELTA_MASK)))

#define SN_IS_XDELTA(sn)        ((*(sn) >> SN_DELTA_BITS) >= SRC_XDELTA)
#define SN_TYPE(sn)             ((js::SrcNoteType)(SN_IS_XDELTA(sn)           \
                                                   ? SRC_XDELTA               \
                                                   : *(sn) >> SN_DELTA_BITS))
#define SN_SET_TYPE(sn,type)    SN_MAKE_NOTE(sn, type, SN_DELTA(sn))
#define SN_IS_GETTABLE(sn)      (SN_TYPE(sn) < SRC_NEWLINE)

#define SN_DELTA(sn)            ((ptrdiff_t)(SN_IS_XDELTA(sn)                 \
                                             ? *(sn) & SN_XDELTA_MASK         \
                                             : *(sn) & SN_DELTA_MASK))
#define SN_SET_DELTA(sn,delta)  (SN_IS_XDELTA(sn)                             \
                                 ? SN_MAKE_XDELTA(sn, delta)                  \
                                 : SN_MAKE_NOTE(sn, SN_TYPE(sn), delta))

#define SN_DELTA_LIMIT          ((ptrdiff_t)JS_BIT(SN_DELTA_BITS))
#define SN_XDELTA_LIMIT         ((ptrdiff_t)JS_BIT(SN_XDELTA_BITS))

/*
 * Offset fields follow certain notes and are frequency-encoded: an offset in
 * [0,0x7f] consumes one byte, an offset in [0x80,0x7fffff] takes three, and
 * the high bit of the first byte is set.
 */
#define SN_3BYTE_OFFSET_FLAG    0x80
#define SN_3BYTE_OFFSET_MASK    0x7f

#define SN_MAX_OFFSET ((size_t)((ptrdiff_t)SN_3BYTE_OFFSET_FLAG << 16) - 1)

#define SN_LENGTH(sn)           ((js_SrcNoteSpec[SN_TYPE(sn)].arity == 0) ? 1 \
                                 : js_SrcNoteLength(sn))
#define SN_NEXT(sn)             ((sn) + SN_LENGTH(sn))

/* A source note array is terminated by an all-zero element. */
#define SN_MAKE_TERMINATOR(sn)  (*(sn) = SRC_NULL)
#define SN_IS_TERMINATOR(sn)    (*(sn) == SRC_NULL)

namespace frontend {

/*
 * Append a new source note of the given type (and therefore size) to bce's
 * notes dynamic array, updating bce->noteCount. Return the new note's index
 * within the array pointed at by bce->current->notes. Return -1 if out of
 * memory.
 */
int
NewSrcNote(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type);

int
NewSrcNote2(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset);

int
NewSrcNote3(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset1,
               ptrdiff_t offset2);

/*
 * NB: this function can add at most one extra extended delta note.
 */
jssrcnote *
AddToSrcNoteDelta(JSContext *cx, BytecodeEmitter *bce, jssrcnote *sn, ptrdiff_t delta);

JSBool
FinishTakingSrcNotes(JSContext *cx, BytecodeEmitter *bce, jssrcnote *notes);

void
FinishTakingTryNotes(BytecodeEmitter *bce, JSTryNoteArray *array);

} /* namespace frontend */

/*
 * Finish taking source notes in cx's notePool, copying final notes to the new
 * stable store allocated by the caller and passed in via notes. Return false
 * on malloc failure, which means this function reported an error.
 *
 * Use this to compute the number of jssrcnotes to allocate and pass in via
 * notes. This method knows a lot about details of FinishTakingSrcNotes, so
 * DON'T CHANGE js::frontend::FinishTakingSrcNotes WITHOUT CHECKING WHETHER
 * THIS METHOD NEEDS CORRESPONDING CHANGES!
 */
inline ptrdiff_t
BytecodeEmitter::countFinalSourceNotes()
{
    ptrdiff_t diff = prologOffset() - prolog.lastNoteOffset;
    ptrdiff_t cnt = prolog.noteCount + main.noteCount + 1;
    if (prolog.noteCount && prolog.currentLine != firstLine) {
        if (diff > SN_DELTA_MASK)
            cnt += JS_HOWMANY(diff - SN_DELTA_MASK, SN_XDELTA_MASK);
        cnt += 2 + ((firstLine > SN_3BYTE_OFFSET_MASK) << 1);
    } else if (diff > 0) {
        if (main.noteCount) {
            jssrcnote *sn = main.notes;
            diff -= SN_IS_XDELTA(sn)
                    ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                    : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
        }
        if (diff > 0)
            cnt += JS_HOWMANY(diff, SN_XDELTA_MASK);
    }
    return cnt;
}

/*
 * To avoid offending js_SrcNoteSpec[SRC_DECL].arity, pack the two data needed
 * to decompile let into one ptrdiff_t:
 *   offset: offset to the LEAVEBLOCK(EXPR) op (not including ENTER/LEAVE)
 *   groupAssign: whether this was an optimized group assign ([x,y] = [a,b])
 */
inline ptrdiff_t PackLetData(size_t offset, bool groupAssign)
{
    JS_ASSERT(offset <= (size_t(-1) >> 1));
    return ptrdiff_t(offset << 1) | ptrdiff_t(groupAssign);
}

inline size_t LetDataToOffset(ptrdiff_t w)
{
    return size_t(w) >> 1;
}

inline bool LetDataToGroupAssign(ptrdiff_t w)
{
    return size_t(w) & 1;
}

} /* namespace js */

struct JSSrcNoteSpec {
    const char      *name;      /* name for disassembly/debugging output */
    int8_t          arity;      /* number of offset operands */
};

extern JS_FRIEND_DATA(JSSrcNoteSpec)  js_SrcNoteSpec[];
extern JS_FRIEND_API(unsigned)         js_SrcNoteLength(jssrcnote *sn);

/*
 * Get and set the offset operand identified by which (0 for the first, etc.).
 */
extern JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, unsigned which);

#endif /* BytecodeEmitter_h__ */
