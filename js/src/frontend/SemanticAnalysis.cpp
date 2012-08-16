/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SemanticAnalysis.h"

#include "jsfun.h"

#include "frontend/Parser.h"
#include "frontend/TreeContext.h"

#include "jsobjinlines.h"
#include "jsfuninlines.h"

using namespace js;
using namespace js::frontend;

/*
 * Walk the FunctionBox tree looking for functions whose call objects may
 * acquire new bindings as they execute: non-strict functions that call eval,
 * and functions that contain function statements (definitions not appearing
 * within the top statement list, which don't take effect unless they are
 * evaluated). Such call objects may acquire bindings that shadow variables
 * defined in enclosing scopes, so any enclosed functions must have their
 * bindings' extensibleParents flags set, and enclosed compiler-created blocks
 * must have their OWN_SHAPE flags set; the comments for
 * js::Bindings::extensibleParents explain why.
 */
static bool
MarkExtensibleScopeDescendants(JSContext *context, FunctionBox *funbox, bool hasExtensibleParent)
{
    for (; funbox; funbox = funbox->siblings) {
        /*
         * It would be nice to use fun->kind() here to recognize functions
         * that will never consult their parent chains, and thus don't need
         * their 'extensible parents' flag set. Filed as bug 619750.
         */

        JS_ASSERT(!funbox->bindings.extensibleParents());
        if (hasExtensibleParent) {
            if (!funbox->bindings.setExtensibleParents(context))
                return false;
        }

        if (funbox->kids) {
            if (!MarkExtensibleScopeDescendants(context, funbox->kids,
                                                hasExtensibleParent ||
                                                funbox->funHasExtensibleScope()))
            {
                return false;
            }
        }
    }

    return true;
}

bool
frontend::AnalyzeFunctions(Parser *parser)
{
    TreeContext *tc = parser->tc;
    if (!tc->functionList)
        return true;
    if (!MarkExtensibleScopeDescendants(tc->sc->context, tc->functionList, false))
        return false;
    return true;
}
