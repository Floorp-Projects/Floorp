/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_findsccs_h___
#define gc_findsccs_h___

#include "jsutil.h"

namespace js {
namespace gc {

class ComponentFinder;

struct GraphNodeBase {
    GraphNodeBase  *gcNextGraphNode;
    unsigned       gcDiscoveryTime;
    unsigned       gcLowLink;

    GraphNodeBase()
      : gcNextGraphNode(NULL),
        gcDiscoveryTime(0),
        gcLowLink(0) {}

    virtual ~GraphNodeBase() {}
    virtual void findOutgoingEdges(ComponentFinder& finder) = 0;
};

template <class T> static T *
NextGraphNode(const T *current)
{
    const GraphNodeBase *node = current;
    return static_cast<T *>(node->gcNextGraphNode);
}

template <class T> void
AddGraphNode(T *&listHead, T *newFirstNode)
{
    GraphNodeBase *node = newFirstNode;
    JS_ASSERT(!node->gcNextGraphNode);
    node->gcNextGraphNode = listHead;
    listHead = newFirstNode;
}

template <class T> static T *
RemoveGraphNode(T *&listHead)
{
    GraphNodeBase *node = listHead;
    if (!node)
        return NULL;

    T *result = listHead;
    listHead = static_cast<T *>(node->gcNextGraphNode);
    node->gcNextGraphNode = NULL;
    return result;
}

/*
 * Find the strongly connected components of a graph using Tarjan's algorithm,
 * and return them in topological order.
 *
 * Nodes derive from GraphNodeBase and implement findGraphEdges, which calls
 * finder.addEdgeTo to describe the outgoing edges from that node:
 *
 * struct MyGraphNode : public GraphNodeBase
 * {
 *     void findOutgoingEdges(ComponentFinder& finder)
 *     {
 *         for edge in my_outgoing_edges:
 *             if is_relevant(edge):
 *                 finder.addEdgeTo(edge.destination)
 *     }
 * }
 */
class ComponentFinder
{
  public:
    ComponentFinder(uintptr_t stackLimit);
    ~ComponentFinder();
    void addNode(GraphNodeBase *v);
    GraphNodeBase *getResultsList();

    template <class T> static T *
    getNextGroup(T *&resultsList) {
        T *group = resultsList;
        if (resultsList)
            resultsList = static_cast<T *>(removeFirstGroup(resultsList));
        return group;
    }

    template <class T> static T *
    getAllRemaining(T *&resultsList) {
        T *all = resultsList;
        removeAllRemaining(resultsList);
        resultsList = NULL;
        return all;
    }

  private:
    static GraphNodeBase *removeFirstGroup(GraphNodeBase *resultsList);
    static void removeAllRemaining(GraphNodeBase *resultsList);

  public:
    /* Call from implementation of GraphNodeBase::findOutgoingEdges(). */
    void addEdgeTo(GraphNodeBase *w);

  private:
    /* Constant used to indicate an unprocessed vertex. */
    static const unsigned Undefined = 0;

    /* Constant used to indicate an processed vertex that is no longer on the stack. */
    static const unsigned Finished = (unsigned)-1;

    void processNode(GraphNodeBase *v);
    void checkStackFull();

  private:
    unsigned       clock;
    GraphNodeBase  *stack;
    GraphNodeBase  *firstComponent;
    GraphNodeBase  *cur;
    uintptr_t      stackLimit;
    bool           stackFull;
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_findsccs_h___ */
