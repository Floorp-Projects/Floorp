/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedContext_h__
#define SharedContext_h__

#include "jstypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#include "builtin/Module.h"
#include "frontend/ParseMaps.h"
#include "frontend/ParseNode.h"
#include "vm/ScopeObject.h"

namespace js {
namespace frontend {

// These flags apply to both global and function contexts.
class AnyContextFlags
{
    // This class's data is all private and so only visible to these friends.
    friend class SharedContext;

    // True if "use strict"; appears in the body instead of being inherited.
    bool            hasExplicitUseStrict:1;

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

  public:
    AnyContextFlags()
     :  hasExplicitUseStrict(false),
        bindingsAccessedDynamically(false)
    { }
};

class FunctionContextFlags
{
    // This class's data is all private and so only visible to these friends.
    friend class FunctionBox;

    // We parsed a yield statement in the function.
    bool            isGenerator:1;

    // The function or a function that encloses it may define new local names
    // at runtime through means other than calling eval.
    bool            mightAliasLocals:1;

    // This function does something that can extend the set of bindings in its
    // call objects --- it does a direct eval in non-strict code, or includes a
    // function statement (as opposed to a function definition).
    //
    // This flag is *not* inherited by enclosed or enclosing functions; it
    // applies only to the function in whose flags it appears.
    //
    bool            hasExtensibleScope:1;

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
    bool            argumentsHasLocalBinding:1;

    // In many cases where 'arguments' has a local binding (as described above)
    // we do not need to actually create an arguments object in the function
    // prologue: instead we can analyze how 'arguments' is used (using the
    // simple dataflow analysis in analyzeSSA) to determine that uses of
    // 'arguments' can just read from the stack frame directly. However, the
    // dataflow analysis only looks at how JSOP_ARGUMENTS is used, so it will
    // be unsound in several cases. The frontend filters out such cases by
    // setting this flag which eagerly sets script->needsArgsObj to true.
    //
    bool            definitelyNeedsArgsObj:1;

  public:
    FunctionContextFlags()
     :  isGenerator(false),
        mightAliasLocals(false),
        hasExtensibleScope(false),
        argumentsHasLocalBinding(false),
        definitelyNeedsArgsObj(false)
    { }
};

class GlobalSharedContext;

/*
 * The struct SharedContext is part of the current parser context (see
 * ParseContext). It stores information that is reused between the parser and
 * the bytecode emitter. Note however, that this information is not shared
 * between the two; they simply reuse the same data structure.
 */
class SharedContext
{
  public:
    JSContext *const context;
    AnyContextFlags anyCxFlags;
    bool strict;

    // If it's function code, funbox must be non-NULL and scopeChain must be NULL.
    // If it's global code, funbox must be NULL.
    inline SharedContext(JSContext *cx, bool strict);

    virtual ObjectBox *toObjectBox() = 0;
    inline bool isGlobalSharedContext() { return toObjectBox() == NULL; }
    inline bool isModuleBox() { return toObjectBox() && toObjectBox()->isModuleBox(); }
    inline bool isFunctionBox() { return toObjectBox() && toObjectBox()->isFunctionBox(); }
    inline GlobalSharedContext *asGlobalSharedContext();
    inline ModuleBox *asModuleBox();
    inline FunctionBox *asFunctionBox();

    bool hasExplicitUseStrict()        const { return anyCxFlags.hasExplicitUseStrict; }
    bool bindingsAccessedDynamically() const { return anyCxFlags.bindingsAccessedDynamically; }

    void setExplicitUseStrict()           { anyCxFlags.hasExplicitUseStrict        = true; }
    void setBindingsAccessedDynamically() { anyCxFlags.bindingsAccessedDynamically = true; }

    // JSOPTION_STRICT warnings or strict mode errors.
    inline bool needStrictChecks();
};

class GlobalSharedContext : public SharedContext
{
  private:
    const RootedObject scopeChain_; /* scope chain object for the script */

  public:
    inline GlobalSharedContext(JSContext *cx, JSObject *scopeChain, bool strict);

    ObjectBox *toObjectBox() { return NULL; }
    JSObject *scopeChain() const { return scopeChain_; }
};


class ModuleBox : public ObjectBox, public SharedContext {
public:
    size_t      bufStart;
    size_t      bufEnd;
    Bindings    bindings;

    ModuleBox(JSContext *cx, ParseContext *pc, Module *module,
              ObjectBox *traceListHead);
    ObjectBox *toObjectBox() { return this; }
    Module *module() const { return &object->asModule(); }
};

class FunctionBox : public ObjectBox, public SharedContext
{
  public:
    Bindings        bindings;               /* bindings for this function */
    size_t          bufStart;
    size_t          bufEnd;
    uint16_t        ndefaults;
    bool            inWith:1;               /* some enclosing scope is a with-statement
                                               or E4X filter-expression */
    bool            inGenexpLambda:1;       /* lambda from generator expression */

    FunctionContextFlags funCxFlags;

    FunctionBox(JSContext *cx, ObjectBox* traceListHead, JSFunction *fun, ParseContext *pc,
                bool strict);

    ObjectBox *toObjectBox() { return this; }
    JSFunction *function() const { return object->toFunction(); }

    bool isGenerator()              const { return funCxFlags.isGenerator; }
    bool mightAliasLocals()         const { return funCxFlags.mightAliasLocals; }
    bool hasExtensibleScope()       const { return funCxFlags.hasExtensibleScope; }
    bool argumentsHasLocalBinding() const { return funCxFlags.argumentsHasLocalBinding; }
    bool definitelyNeedsArgsObj()   const { return funCxFlags.definitelyNeedsArgsObj; }

    void setIsGenerator()                  { funCxFlags.isGenerator              = true; }
    void setMightAliasLocals()             { funCxFlags.mightAliasLocals         = true; }
    void setHasExtensibleScope()           { funCxFlags.hasExtensibleScope       = true; }
    void setArgumentsHasLocalBinding()     { funCxFlags.argumentsHasLocalBinding = true; }
    void setDefinitelyNeedsArgsObj()       { JS_ASSERT(funCxFlags.argumentsHasLocalBinding);
                                             funCxFlags.definitelyNeedsArgsObj   = true; }
};

/*
 * NB: If you add a new type of statement that is a scope, add it between
 * STMT_WITH and STMT_CATCH, or you will break StmtInfoBase::linksScope. If you
 * add a non-looping statement type, add it before STMT_DO_LOOP or you will
 * break StmtInfoBase::isLoop().
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

/*
 * A comment on the encoding of the js::StmtType enum and StmtInfoBase
 * type-testing methods:
 *
 * StmtInfoBase::maybeScope() tells whether a statement type is always, or may
 * become, a lexical scope. It therefore includes block and switch (the two
 * low-numbered "maybe" scope types) and excludes with (with has dynamic scope
 * pending the "reformed with" in ES4/JS2). It includes all try-catch-finally
 * types, which are high-numbered maybe-scope types.
 *
 * StmtInfoBase::linksScope() tells whether a js::StmtInfo{PC,BCE} of the given
 * type eagerly links to other scoping statement info records. It excludes the
 * two early "maybe" types, block and switch, as well as the try and both
 * finally types, since try and the other trailing maybe-scope types don't need
 * block scope unless they contain let declarations.
 *
 * We treat WITH as a static scope because it prevents lexical binding from
 * continuing further up the static scope chain. With the lost "reformed with"
 * proposal for ES4, we would be able to model it statically, too.
 */

// StmtInfoPC is used by the Parser.  StmtInfoBCE is used by the
// BytecodeEmitter.  The two types have some overlap, encapsulated by
// StmtInfoBase.  Several functions below (e.g. PushStatement) are templated to
// work with both types.

struct StmtInfoBase {
    uint16_t        type;           /* statement type */

    /*
     * True if type is STMT_BLOCK, STMT_TRY, STMT_SWITCH, or
     * STMT_FINALLY and the block contains at least one let-declaration.
     */
    bool isBlockScope:1;

    /* for (let ...) induced block scope */
    bool isForLetBlock:1;

    RootedAtom      label;          /* name of LABEL */
    Rooted<StaticBlockObject *> blockObj; /* block scope object */

    StmtInfoBase(JSContext *cx)
        : isBlockScope(false), isForLetBlock(false), label(cx), blockObj(cx)
    {}

    bool maybeScope() const {
        return STMT_BLOCK <= type && type <= STMT_SUBROUTINE && type != STMT_WITH;
    }

    bool linksScope() const {
        return (STMT_WITH <= type && type <= STMT_CATCH) || isBlockScope;
    }

    bool isLoop() const {
        return type >= STMT_DO_LOOP;
    }

    bool isTrying() const {
        return STMT_TRY <= type && type <= STMT_SUBROUTINE;
    }
};

// Push the C-stack-allocated struct at stmt onto the StmtInfoPC stack.
template <class ContextT>
void
PushStatement(ContextT *ct, typename ContextT::StmtInfo *stmt, StmtType type);

template <class ContextT>
void
FinishPushBlockScope(ContextT *ct, typename ContextT::StmtInfo *stmt, StaticBlockObject &blockObj);

// Pop pc->topStmt. If the top StmtInfoPC struct is not stack-allocated, it
// is up to the caller to free it.  The dummy argument is just to make the
// template matching work.
template <class ContextT>
void
FinishPopStatement(ContextT *ct);

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
template <class ContextT>
typename ContextT::StmtInfo *
LexicalLookup(ContextT *ct, HandleAtom atom, int *slotp, typename ContextT::StmtInfo *stmt);

} // namespace frontend

} // namespace js

#endif // SharedContext_h__
