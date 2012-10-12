/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FindSCCs.h"

#include "jsfriendapi.h"

namespace js {
namespace gc {

ComponentFinder::ComponentFinder(uintptr_t sl)
  : clock(1),
    stack(NULL),
    firstComponent(NULL),
    cur(NULL),
    stackLimit(sl),
    stackFull(false)
{
}

ComponentFinder::~ComponentFinder()
{
    JS_ASSERT(!stack);
    JS_ASSERT(!firstComponent);
}

void
ComponentFinder::addNode(GraphNodeBase *v)
{
    if (v->gcDiscoveryTime == Undefined) {
        JS_ASSERT(v->gcLowLink == Undefined);
        JS_ASSERT(!v->gcNextGraphNode);
        processNode(v);
    }
}

void
ComponentFinder::checkStackFull()
{
    /*
     * Check for exceeding the size of the C stack.
     *
     * If this happens we give up and return all vertices in one group, by
     * pushing them onto the output list with lowLink set to 1.
     */

    if (!stackFull) {
        int stackDummy;
        if (!JS_CHECK_STACK_SIZE(stackLimit, &stackDummy))
            stackFull = true;
    }

    if (stackFull) {
        GraphNodeBase *w;
        while (stack) {
            w = stack;
            stack = w->gcNextGraphNode;

            w->gcLowLink = 1;
            w->gcNextGraphNode = firstComponent;
            firstComponent = w;
        }
    }
}

void
ComponentFinder::processNode(GraphNodeBase *v)
{
    v->gcDiscoveryTime = clock;
    v->gcLowLink = clock;
    ++clock;

    JS_ASSERT(!v->gcNextGraphNode);
    v->gcNextGraphNode = stack;
    stack = v;

    checkStackFull();
    if (stackFull)
        return;

    GraphNodeBase *old = cur;
    cur = v;
    cur->findOutgoingEdges(*this);
    cur = old;

    if (stackFull) {
        JS_ASSERT(!stack);
        return;
    }

    if (v->gcLowLink == v->gcDiscoveryTime) {
        GraphNodeBase *w;
        do {
            JS_ASSERT(stack);
            w = stack;
            stack = w->gcNextGraphNode;

            /*
             * Record the elements of a component by setting all their gcLowLink
             * fields to the same value.
             */
            w->gcLowLink = v->gcDiscoveryTime;

            /*
             * Record that the element is no longer on the stack by setting the
             * discovery time to a special value that's not Undefined.
             */
            w->gcDiscoveryTime = Finished;

            /*
             * Prepend the component to the beginning of the output list to
             * reverse the list and achieve the desired order.
             */
            w->gcNextGraphNode = firstComponent;
            firstComponent = w;
        } while (w != v);
    }
}

void
ComponentFinder::addEdgeTo(GraphNodeBase *w)
{
    if (w->gcDiscoveryTime == Undefined) {
        processNode(w);
        cur->gcLowLink = Min(cur->gcLowLink, w->gcLowLink);
    } else if (w->gcDiscoveryTime != Finished) {
        cur->gcLowLink = Min(cur->gcLowLink, w->gcDiscoveryTime);
    }
}

GraphNodeBase *
ComponentFinder::getResultsList()
{
    JS_ASSERT(!stack);
    GraphNodeBase *result = firstComponent;
    firstComponent = NULL;
    return result;
}

GraphNodeBase *
ComponentFinder::removeFirstGroup(GraphNodeBase *resultsList)
{
    /* Remove the first group from resultsList and return the new list head. */

    JS_ASSERT(resultsList);

    GraphNodeBase *v = resultsList;
    unsigned lowLink = v->gcLowLink;

    GraphNodeBase *last;
    do {
        v->gcDiscoveryTime = Undefined;
        v->gcLowLink = Undefined;
        last = v;
        v = v->gcNextGraphNode;
    }
    while (v && v->gcLowLink == lowLink);

    last->gcNextGraphNode = NULL;
    return v;
}

void
ComponentFinder::removeAllRemaining(GraphNodeBase *resultsList)
{
    for (GraphNodeBase *v = resultsList; v; v = v->gcNextGraphNode) {
        v->gcDiscoveryTime = Undefined;
        v->gcLowLink = Undefined;
    }
}

} /* namespace gc */
} /* namespace js */
