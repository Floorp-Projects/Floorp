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
                             FunctionBox *funbox, unsigned staticLevel)
  : context(cx),
    bodyid(0),
    blockidGen(0),
    topStmt(NULL),
    topScopeStmt(NULL),
    blockChain(cx),
    fun_(cx, fun),
    funbox_(funbox),
    scopeChain_(cx, scopeChain),
    staticLevel(staticLevel),
    bindings(cx),
    bindingsRoot(cx, &bindings),
    cxFlags(cx)
{
    JS_ASSERT((fun && !scopeChain_) || (!fun && !funbox));
}

inline unsigned
SharedContext::blockid()
{
    return topStmt ? topStmt->blockid : bodyid;
}

inline bool
SharedContext::atBodyLevel()
{
    return !topStmt || (topStmt->flags & SIF_BODY_BLOCK);
}

inline bool
SharedContext::needStrictChecks() {
    return context->hasStrictOption() || inStrictMode();
}

inline unsigned
SharedContext::argumentsLocal() const
{
    PropertyName *arguments = context->runtime->atomState.argumentsAtom;
    unsigned slot;
    DebugOnly<BindingKind> kind = bindings.lookup(context, arguments, &slot);
    JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
    return slot;
}

inline
TreeContext::TreeContext(Parser *prs, SharedContext *sc)
  : sc(sc),
    parenDepth(0),
    yieldCount(0),
    blockNode(NULL),
    decls(prs->context),
    yieldNode(NULL),
    functionList(NULL),
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
    return decls.init() && lexdeps.ensureMap(sc->context);
}

// For functions the tree context is constructed and destructed a second
// time during code generation. To avoid a redundant stats update in such
// cases, we store UINT16_MAX in maxScopeDepth.
inline
TreeContext::~TreeContext()
{
    // |*parserTC| pointed to this object.  Now that this object is about to
    // die, make |*parserTC| point to this object's parent.
    JS_ASSERT(*parserTC == this);
    *parserTC = this->parent;
    sc->context->delete_(funcStmts);
}

} // namespace js

#endif // TreeContext_inl_h__
