/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "frontend/SemanticAnalysis.h"

#include "jsfun.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"

#include "jsobjinlines.h"
#include "jsfuninlines.h"

using namespace js;
using namespace js::frontend;

/*
 * Walk the function box list at |*funboxHead|, removing boxes for deleted
 * functions and cleaning up method lists. We do this once, before
 * performing function analysis, to avoid traversing possibly long function
 * lists repeatedly when recycling nodes.
 *
 * There are actually three possible states for function boxes and their
 * nodes:
 *
 * - Live: funbox->node points to the node, and funbox->node->pn_funbox
 *   points back to the funbox.
 *
 * - Recycled: funbox->node points to the node, but funbox->node->pn_funbox
 *   is NULL. When a function node is part of a tree that gets recycled, we
 *   must avoid corrupting any method list the node is on, so we leave the
 *   function node unrecycled until we call CleanFunctionList. At recycle
 *   time, we clear such nodes' pn_funbox pointers to indicate that they
 *   are deleted and should be recycled once we get here.
 *
 * - Mutated: funbox->node is NULL; the contents of the node itself could
 *   be anything. When we mutate a function node into some other kind of
 *   node, we lose all indication that the node was ever part of the
 *   function box tree; it could later be recycled, reallocated, and turned
 *   into anything at all. (Fortunately, method list members never get
 *   mutated, so we don't have to worry about that case.)
 *   ParseNodeAllocator::prepareNodeForMutation clears the node's function
 *   box's node pointer, disconnecting it entirely from the function box tree,
 *   and marking the function box to be trimmed out.
 */
static void
CleanFunctionList(ParseNodeAllocator *allocator, FunctionBox **funboxHead)
{
    FunctionBox **link = funboxHead;
    while (FunctionBox *box = *link) {
        if (!box->node) {
            /*
             * This funbox's parse node was mutated into something else. Drop the box,
             * and stay at the same link.
             */
            *link = box->siblings;
        } else if (!box->node->pn_funbox) {
            /*
             * This funbox's parse node is ready to be recycled. Drop the box, recycle
             * the node, and stay at the same link.
             */
            *link = box->siblings;
            allocator->freeNode(box->node);
        } else {
            /* The function is still live. */

            /* First, remove nodes for deleted functions from our methods list. */
            {
                ParseNode **methodLink = &box->methods;
                while (ParseNode *method = *methodLink) {
                    /* Method nodes are never rewritten in place to be other kinds of nodes. */
                    JS_ASSERT(method->isArity(PN_FUNC));
                    if (!method->pn_funbox) {
                        /* Deleted: drop the node, and stay on this link. */
                        *methodLink = method->pn_link;
                    } else {
                        /* Live: keep the node, and move to the next link. */
                        methodLink = &method->pn_link;
                    }
                }
            }

            /* Second, remove boxes for deleted functions from our kids list. */
            CleanFunctionList(allocator, &box->kids);

            /* Keep the box on the list, and move to the next link. */
            link = &box->siblings;
        }
    }
}

static void
FlagHeavyweights(Definition *dn, FunctionBox *funbox, uint32_t *tcflags)
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
            funbox->tcflags |= TCF_FUN_HEAVYWEIGHT;
            break;
        }
    }

    if (!funbox && (*tcflags & TCF_IN_FUNCTION))
        *tcflags |= TCF_FUN_HEAVYWEIGHT;
}

static void
SetFunctionKinds(FunctionBox *funbox, uint32_t *tcflags, bool isDirectEval)
{
    for (; funbox; funbox = funbox->siblings) {
        ParseNode *fn = funbox->node;
        ParseNode *pn = fn->pn_body;

        if (funbox->kids)
            SetFunctionKinds(funbox->kids, tcflags, isDirectEval);

        JSFunction *fun = funbox->function();

        JS_ASSERT(fun->kind() == JSFUN_INTERPRETED);

        if (funbox->tcflags & TCF_FUN_HEAVYWEIGHT) {
            /* nothing to do */
        } else if (isDirectEval || funbox->inAnyDynamicScope()) {
            /*
             * Either we are in a with-block or a function scope that is
             * subject to direct eval; or we are compiling strict direct eval
             * code.
             *
             * In either case, fun may reference names that are not bound but
             * are not necessarily global either. (In the strict direct eval
             * case, we could bind them, but currently do not bother; see
             * the comment about strict mode code in BindTopLevelVar.)
             */
            JS_ASSERT(!fun->isNullClosure());
        } else {
            bool hasUpvars = false;

            if (pn->isKind(PNK_UPVARS)) {
                AtomDefnMapPtr upvars = pn->pn_names;
                JS_ASSERT(!upvars->empty());

                /* Determine whether the this function contains upvars. */
                for (AtomDefnRange r = upvars->all(); !r.empty(); r.popFront()) {
                    if (!r.front().value()->resolve()->isFreeVar()) {
                        hasUpvars = true;
                        break;
                    }
                }
            }

            if (!hasUpvars) {
                /* No lexical dependencies => null closure, for best performance. */
                fun->setKind(JSFUN_NULL_CLOSURE);
            }
        }

        if (fun->kind() == JSFUN_INTERPRETED && pn->isKind(PNK_UPVARS)) {
            /*
             * We loop again over all upvars, and for each non-free upvar,
             * ensure that its containing function has been flagged as
             * heavyweight.
             *
             * The emitter must see TCF_FUN_HEAVYWEIGHT accurately before
             * generating any code for a tree of nested functions.
             */
            AtomDefnMapPtr upvars = pn->pn_names;
            JS_ASSERT(!upvars->empty());

            for (AtomDefnRange r = upvars->all(); !r.empty(); r.popFront()) {
                Definition *defn = r.front().value();
                Definition *lexdep = defn->resolve();
                if (!lexdep->isFreeVar())
                    FlagHeavyweights(lexdep, funbox, tcflags);
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
                                                hasExtensibleParent || funbox->scopeIsExtensible())) {
                return false;
            }
        }
    }

    return true;
}

bool
frontend::AnalyzeFunctions(TreeContext *tc)
{
    CleanFunctionList(&tc->parser->allocator, &tc->functionList);
    if (!tc->functionList)
        return true;
    if (!MarkExtensibleScopeDescendants(tc->parser->context, tc->functionList, false))
        return false;
    bool isDirectEval = !!tc->parser->callerFrame;
    SetFunctionKinds(tc->functionList, &tc->flags, isDirectEval);
    return true;
}
