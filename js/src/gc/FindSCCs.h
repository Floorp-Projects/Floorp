/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_findsccs_h___
#define gc_findsccs_h___

#include "jsutil.h"

namespace js {
namespace gc {

template<class Node>
struct GraphNodeBase
{
    Node           *gcNextGraphNode;
    Node           *gcNextGraphComponent;
    unsigned       gcDiscoveryTime;
    unsigned       gcLowLink;

    GraphNodeBase()
      : gcNextGraphNode(NULL),
        gcNextGraphComponent(NULL),
        gcDiscoveryTime(0),
        gcLowLink(0) {}

    ~GraphNodeBase() {}

    Node *nextNodeInGroup() const {
        if (gcNextGraphNode && gcNextGraphNode->gcNextGraphComponent == gcNextGraphComponent)
            return gcNextGraphNode;
        return NULL;
    }

    Node *nextGroup() const {
        return gcNextGraphComponent;
    }
};

/*
 * Find the strongly connected components of a graph using Tarjan's algorithm,
 * and return them in topological order.
 *
 * Nodes derive from GraphNodeBase and implement findGraphEdges, which calls
 * finder.addEdgeTo to describe the outgoing edges from that node:
 *
 * struct MyGraphNode : public GraphNodeBase
 * {
 *     void findOutgoingEdges(ComponentFinder<MyGraphNode> &finder)
 *     {
 *         for edge in my_outgoing_edges:
 *             if is_relevant(edge):
 *                 finder.addEdgeTo(edge.destination)
 *     }
 * }
 *
 * ComponentFinder<MyGraphNode> finder;
 * finder.addNode(v);
 */
template<class Node>
class ComponentFinder
{
  public:
    ComponentFinder(uintptr_t stackLimit);
    ~ComponentFinder();

    /* Forces all nodes to be added to a single component. */
    void useOneComponent() { stackFull = true; }

    void addNode(Node *v);
    Node *getResultsList();

    static void mergeGroups(Node *first);

  public:
    /* Call from implementation of GraphNodeBase::findOutgoingEdges(). */
    void addEdgeTo(Node *w);

  private:
    /* Constant used to indicate an unprocessed vertex. */
    static const unsigned Undefined = 0;

    /* Constant used to indicate an processed vertex that is no longer on the stack. */
    static const unsigned Finished = (unsigned)-1;

    void processNode(Node *v);

  private:
    unsigned       clock;
    Node           *stack;
    Node           *firstComponent;
    Node           *cur;
    uintptr_t      stackLimit;
    bool           stackFull;
};

} /* namespace gc */
} /* namespace js */

#endif /* gc_findsccs_h___ */
