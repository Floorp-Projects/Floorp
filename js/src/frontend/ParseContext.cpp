/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseNode.h"
#include "frontend/ParseContext.h"

#include "jsatominlines.h"

#include "frontend/ParseContext-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::frontend;

void
ParseContext::trace(JSTracer *trc)
{
    sc->bindings.trace(trc);
}

bool
frontend::GenerateBlockId(ParseContext *pc, uint32_t &blockid)
{
    if (pc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(pc->sc->context, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "program");
        return false;
    }
    blockid = pc->blockidGen++;
    return true;
}

