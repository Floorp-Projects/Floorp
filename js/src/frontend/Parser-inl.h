/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Parser_inl_h__
#define Parser_inl_h__

#include "frontend/Parser.h"

namespace js {
namespace frontend {

inline unsigned
ParseContext::blockid()
{
    return topStmt ? topStmt->blockid : bodyid;
}

inline bool
ParseContext::atBodyLevel()
{
    return !topStmt;
}

inline
ParseContext::ParseContext(Parser *prs, SharedContext *sc, unsigned staticLevel, uint32_t bodyid)
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
    decls_(prs->context),
    args_(prs->context),
    vars_(prs->context),
    yieldNode(NULL),
    parserPC(&prs->pc),
    lexdeps(prs->context),
    parent(prs->pc),
    funcStmts(NULL),
    funHasReturnExpr(false),
    funHasReturnVoid(false),
    parsingForInit(false),
    parsingWith(prs->pc ? prs->pc->parsingWith : false), // inherit from parent context
    inDeclDestructuring(false),
    funBecameStrict(false)
{
    prs->pc = this;
}

inline bool
ParseContext::init()
{
    if (!frontend::GenerateBlockId(this, this->bodyid))
        return false;

    return decls_.init() && lexdeps.ensureMap(sc->context);
}

inline
ParseContext::~ParseContext()
{
    // |*parserPC| pointed to this object.  Now that this object is about to
    // die, make |*parserPC| point to this object's parent.
    JS_ASSERT(*parserPC == this);
    *parserPC = this->parent;
    js_delete(funcStmts);
}

} // namespace frontend
} // namespace js

#endif // Parser_inl_h__

