/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TreeContext_inl_h__
#define TreeContext_inl_h__

#include "frontend/Parser.h"
#include "frontend/TreeContext.h"

#include "frontend/ParseMaps-inl.h"

namespace js {

inline
SharedContext::SharedContext(JSContext *cx, JSObject *scopeChain, JSFunction *fun,
                             FunctionBox *funbox, StrictMode::StrictModeState sms)
  : context(cx),
    fun_(cx, fun),
    funbox_(funbox),
    scopeChain_(cx, scopeChain),
    bindings(),
    bindingsRoot(cx, &bindings),
    cxFlags(cx),
    strictModeState(sms)
{
    JS_ASSERT((fun && !scopeChain_) || (!fun && !funbox));
}

inline bool
SharedContext::inStrictMode()
{
    JS_ASSERT(strictModeState != StrictMode::UNKNOWN);
    JS_ASSERT_IF(inFunction() && funbox(), funbox()->strictModeState == strictModeState);
    return strictModeState == StrictMode::STRICT;
}

inline unsigned
TreeContext::blockid()
{
    return topStmt ? topStmt->blockid : bodyid;
}

inline bool
TreeContext::atBodyLevel()
{
    return !topStmt || topStmt->isFunctionBodyBlock;
}

inline bool
SharedContext::needStrictChecks()
{
    return context->hasStrictOption() || strictModeState != StrictMode::NOTSTRICT;
}

inline
TreeContext::TreeContext(Parser *prs, SharedContext *sc, unsigned staticLevel, uint32_t bodyid)
  : sc(sc),
    bodyid(0),           // initialized in init()
    blockidGen(bodyid),  // used to set |bodyid| and subsequently incremented in init()
    topStmt(NULL),
    topScopeStmt(NULL),
    blockChain(prs->context),
    staticLevel(staticLevel),
    parenDepth(0),
    yieldCount(0),
    blockNode(NULL),
    decls(prs->context),
    yieldNode(NULL),
    functionList(NULL),
    queuedStrictModeError(NULL),
    parserTC(&prs->tc),
    lexdeps(prs->context),
    parent(prs->tc),
    innermostWith(NULL),
    funcStmts(NULL),
    hasReturnExpr(false),
    hasReturnVoid(false),
    inForInit(false),
    inDeclDestructuring(false)
{
    prs->tc = this;
}

inline bool
TreeContext::init()
{
    if (!frontend::GenerateBlockId(this, this->bodyid))
        return false;

    return decls.init() && lexdeps.ensureMap(sc->context);
}

inline void
TreeContext::setQueuedStrictModeError(CompileError *e)
{
    JS_ASSERT(!queuedStrictModeError);
    queuedStrictModeError = e;
}

inline
TreeContext::~TreeContext()
{
    // |*parserTC| pointed to this object.  Now that this object is about to
    // die, make |*parserTC| point to this object's parent.
    JS_ASSERT(*parserTC == this);
    *parserTC = this->parent;
    sc->context->delete_(funcStmts);
    if (queuedStrictModeError) {
        // If the parent context is looking for strict mode violations, pass
        // ours up. Otherwise, free it.
        if (parent && parent->sc->strictModeState == StrictMode::UNKNOWN &&
            !parent->queuedStrictModeError)
            parent->queuedStrictModeError = queuedStrictModeError;
        else
            sc->context->delete_(queuedStrictModeError);
    }
}

template <class ContextT>
void
frontend::PushStatement(ContextT *ct, typename ContextT::StmtInfo *stmt, StmtType type)
{
    stmt->type = type;
    stmt->isBlockScope = false;
    stmt->isForLetBlock = false;
    stmt->label = NULL;
    stmt->blockObj = NULL;
    stmt->down = ct->topStmt;
    ct->topStmt = stmt;
    if (stmt->linksScope()) {
        stmt->downScope = ct->topScopeStmt;
        ct->topScopeStmt = stmt;
    } else {
        stmt->downScope = NULL;
    }
}

template <class ContextT>
void
frontend::FinishPushBlockScope(ContextT *ct, typename ContextT::StmtInfo *stmt,
                               StaticBlockObject &blockObj)
{
    stmt->isBlockScope = true;
    stmt->downScope = ct->topScopeStmt;
    ct->topScopeStmt = stmt;
    ct->blockChain = &blockObj;
    stmt->blockObj = &blockObj;
}

template <class ContextT>
void
frontend::FinishPopStatement(ContextT *ct)
{
    typename ContextT::StmtInfo *stmt = ct->topStmt;
    ct->topStmt = stmt->down;
    if (stmt->linksScope()) {
        ct->topScopeStmt = stmt->downScope;
        if (stmt->isBlockScope)
            ct->blockChain = stmt->blockObj->enclosingBlock();
    }
}

/*
 * The function LexicalLookup searches a static binding for the given name in
 * the stack of statements enclosing the statement currently being parsed. Each
 * statement that introduces a new scope has a corresponding scope object, on
 * which the bindings for that scope are stored. LexicalLookup either returns
 * the innermost statement which has a scope object containing a binding with
 * the given name, or NULL.
 */
template <class ContextT>
typename ContextT::StmtInfo *
frontend::LexicalLookup(ContextT *ct, HandleAtom atom, int *slotp, typename ContextT::StmtInfo *stmt)
{
    if (!stmt)
        stmt = ct->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        /*
         * With-statements introduce dynamic bindings. Since dynamic bindings
         * can potentially override any static bindings introduced by statements
         * further up the stack, we have to abort the search.
         */
        if (stmt->type == STMT_WITH)
            break;

        // Skip statements that do not introduce a new scope
        if (!stmt->isBlockScope)
            continue;

        StaticBlockObject &blockObj = *stmt->blockObj;
        Shape *shape = blockObj.nativeLookup(ct->sc->context, AtomToId(atom));
        if (shape) {
            JS_ASSERT(shape->hasShortID());

            if (slotp)
                *slotp = blockObj.stackDepth() + shape->shortid();
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

} // namespace js

#endif // TreeContext_inl_h__
