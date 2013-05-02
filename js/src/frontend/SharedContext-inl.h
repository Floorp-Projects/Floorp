/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedContext_inl_h__
#define SharedContext_inl_h__

#include "frontend/Parser.h"
#include "frontend/SharedContext.h"

namespace js {
namespace frontend {

inline
SharedContext::SharedContext(JSContext *cx, bool strict)
  : context(cx),
    anyCxFlags(),
    strict(strict)
{
}

inline bool
SharedContext::needStrictChecks()
{
    return context->hasStrictOption() || strict;
}

inline GlobalSharedContext *
SharedContext::asGlobalSharedContext()
{
    JS_ASSERT(isGlobalSharedContext());
    return static_cast<GlobalSharedContext*>(this);
}

inline ModuleBox *
SharedContext::asModuleBox()
{
    JS_ASSERT(isModuleBox());
    return static_cast<ModuleBox*>(this);
}

GlobalSharedContext::GlobalSharedContext(JSContext *cx, JSObject *scopeChain, bool strict)
  : SharedContext(cx, strict),
    scopeChain_(cx, scopeChain)
{
}

} /* namespace frontend */

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
    RootedId id(ct->sc->context, AtomToId(atom));

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
        Shape *shape = blockObj.nativeLookup(ct->sc->context, id);
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

#endif // SharedContext_inl_h__
