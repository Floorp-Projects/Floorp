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

template <typename ParseHandler>
inline unsigned
ParseContext<ParseHandler>::blockid()
{
    return topStmt ? topStmt->blockid : bodyid;
}

template <typename ParseHandler>
inline bool
ParseContext<ParseHandler>::atBodyLevel()
{
    return !topStmt;
}

template <typename ParseHandler>
inline
ParseContext<ParseHandler>::ParseContext(Parser<ParseHandler> *prs, SharedContext *sc,
                                      unsigned staticLevel, uint32_t bodyid)
  : sc(sc),
    bodyid(0),           // initialized in init()
    blockidGen(bodyid),  // used to set |bodyid| and subsequently incremented in init()
    topStmt(NULL),
    topScopeStmt(NULL),
    blockChain(prs->context),
    staticLevel(staticLevel),
    parenDepth(0),
    yieldCount(0),
    blockNode(ParseHandler::null()),
    decls_(prs->context),
    args_(prs->context),
    vars_(prs->context),
    yieldNode(ParseHandler::null()),
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

template <typename ParseHandler>
inline bool
ParseContext<ParseHandler>::init()
{
    if (!frontend::GenerateBlockId(this, this->bodyid))
        return false;

    return decls_.init() && lexdeps.ensureMap(sc->context);
}

template <typename ParseHandler>
inline
ParseContext<ParseHandler>::~ParseContext()
{
    // |*parserPC| pointed to this object.  Now that this object is about to
    // die, make |*parserPC| point to this object's parent.
    JS_ASSERT(*parserPC == this);
    *parserPC = this->parent;
    js_delete(funcStmts);
}

/*
 * Check that it is permitted to introduce a binding for atom.  Strict mode
 * forbids introducing new definitions for 'eval', 'arguments', or for any
 * strict mode reserved keyword.  Use pn for reporting error locations, or use
 * pc's token stream if pn is NULL.
 */
template <typename ParseHandler>
static bool
CheckStrictBinding(JSContext *cx, ParseHandler *handler, ParseContext<ParseHandler> *pc,
                   HandlePropertyName name, ParseNode *pn)
{
    if (!pc->sc->needStrictChecks())
        return true;

    if (name == cx->names().eval ||
        name == cx->names().arguments ||
        FindKeyword(name->charsZ(), name->length()))
    {
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, name, &bytes))
            return false;
        return handler->report(ParseStrictError, pn, JSMSG_BAD_BINDING, bytes.ptr());
    }

    return true;
}

} // namespace frontend
} // namespace js

#endif // Parser_inl_h__

