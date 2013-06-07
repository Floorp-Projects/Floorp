/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Parser_inl_h__
#define Parser_inl_h__

#include "frontend/BytecodeCompiler.h"
#include "frontend/Parser.h"

#include "frontend/SharedContext-inl.h"

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

inline
GenericParseContext::GenericParseContext(GenericParseContext *parent, SharedContext *sc)
  : parent(parent),
    sc(sc),
    funHasReturnExpr(false),
    funHasReturnVoid(false),
    parsingForInit(false),
    parsingWith(parent ? parent->parsingWith : false)
{
}

template <typename ParseHandler>
inline
ParseContext<ParseHandler>::ParseContext(Parser<ParseHandler> *prs,
                                         GenericParseContext *parent, SharedContext *sc,
                                         unsigned staticLevel, uint32_t bodyid)
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
    decls_(prs->context),
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
    *parserPC = this->oldpc;
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

    if (name == cx->names().eval || name == cx->names().arguments || IsKeyword(name)) {
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

