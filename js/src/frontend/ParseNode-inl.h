/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParseNode_inl_h__
#define ParseNode_inl_h__

#include "frontend/Parser.h"
#include "frontend/ParseNode.h"

namespace js {
namespace frontend {

inline bool
UpvarCookie::set(JSContext *cx, unsigned newLevel, uint16_t newSlot)
{
    // This is an unsigned-to-uint16_t conversion, test for too-high values.
    // In practice, recursion in Parser and/or BytecodeEmitter will blow the
    // stack if we nest functions more than a few hundred deep, so this will
    // never trigger.  Oh well.
    if (newLevel >= FREE_LEVEL) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_DEEP, js_function_str);
        return false;
    }
    level_ = newLevel;
    slot_ = newSlot;
    return true;
}

inline PropertyName *
ParseNode::name() const
{
    JS_ASSERT(isKind(PNK_FUNCTION) || isKind(PNK_NAME));
    JSAtom *atom = isKind(PNK_FUNCTION) ? pn_funbox->function()->atom() : pn_atom;
    return atom->asPropertyName();
}

inline JSAtom *
ParseNode::atom() const
{
    JS_ASSERT(isKind(PNK_MODULE) || isKind(PNK_STRING));
    return isKind(PNK_MODULE) ? pn_modulebox->module()->atom() : pn_atom;
}

inline void
NameNode::initCommon(ParseContext<FullParseHandler> *pc)
{
    pn_expr = NULL;
    pn_cookie.makeFree();
    pn_dflags = (!pc->topStmt || pc->topStmt->type == STMT_BLOCK)
                ? PND_BLOCKCHILD
                : 0;
    pn_blockid = pc->blockid();
}

} /* namespace frontend */
} /* namespace js */

#endif /* ParseNode_inl_h__ */
