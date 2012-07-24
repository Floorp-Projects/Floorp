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

static void
FlagHeavyweights(Definition *dn, FunctionBox *funbox, bool *isHeavyweight, bool topInFunction)
{
    unsigned dnLevel = dn->frameLevel();

    while ((funbox = funbox->parent) != NULL) {
        /*
         * Notice that funbox->level is the static level of the definition or
         * expression of the function parsed into funbox, not the static level
         * of its body. Therefore we must add 1 to match dn's level to find the
         * funbox whose body contains the dn definition.
         */
        if (funbox->level + 1U == dnLevel || (dnLevel == 0 && dn->isLet())) {
            funbox->setFunIsHeavyweight();
            break;
        }
    }

    if (!funbox && topInFunction)
        *isHeavyweight = true;
}

static void
SetFunctionKinds(FunctionBox *funbox, bool *isHeavyweight, bool topInFunction, bool isDirectEval)
{
    for (; funbox; funbox = funbox->siblings) {
        ParseNode *fn = funbox->node;
        if (!fn)
            continue;

        ParseNode *pn = fn->pn_body;
        if (!pn)
            continue;

        if (funbox->kids)
            SetFunctionKinds(funbox->kids, isHeavyweight, topInFunction, isDirectEval);

        JS_ASSERT(funbox->function()->isInterpreted());
        if (pn->isKind(PNK_UPVARS)) {
            /*
             * We loop again over all upvars, and for each non-free upvar,
             * ensure that its containing function has been flagged as
             * heavyweight.
             *
             * The emitter must see funIsHeavyweight() accurately before
             * generating any code for a tree of nested functions.
             */
            AtomDefnMapPtr upvars = pn->pn_names;
            JS_ASSERT(!upvars->empty());

            for (AtomDefnRange r = upvars->all(); !r.empty(); r.popFront()) {
                Definition *defn = r.front().value();
                Definition *lexdep = defn->resolve();
                if (!lexdep->isFreeVar())
                    FlagHeavyweights(lexdep, funbox, isHeavyweight, topInFunction);
            }
        }
    }
}

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
frontend::AnalyzeFunctions(Parser *parser, StackFrame *callerFrame)
{
    TreeContext *tc = parser->tc;
    SharedContext *sc = tc->sc;
    if (!tc->functionList)
        return true;
    if (!MarkExtensibleScopeDescendants(sc->context, tc->functionList, false))
        return false;
    bool isDirectEval = !!callerFrame;
    bool isHeavyweight = false;
    SetFunctionKinds(tc->functionList, &isHeavyweight, sc->inFunction(), isDirectEval);
    if (isHeavyweight)
        sc->setFunIsHeavyweight();
    return true;
}
