/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>
#include <string.h>

#include "jscntxt.h"
#include "jsgc.h"

#include "gc/FindSCCs.h"
#include "jsapi-tests/tests.h"

static const unsigned MaxVertices = 10;

using js::gc::GraphNodeBase;
using js::gc::ComponentFinder;

struct TestNode : public GraphNodeBase<TestNode>
{
    unsigned   index;
    bool       hasEdge[MaxVertices];

    void findOutgoingEdges(ComponentFinder<TestNode> &finder);
};

static TestNode Vertex[MaxVertices];

void
TestNode::findOutgoingEdges(ComponentFinder<TestNode> &finder)
{
    for (unsigned i = 0; i < MaxVertices; ++i) {
        if (hasEdge[i])
            finder.addEdgeTo(&Vertex[i]);
    }
}

BEGIN_TEST(testFindSCCs)
{
    // no vertices

    setup(0);
    run();
    CHECK(end());

    // no edges

    setup(1);
    run();
    CHECK(group(0, -1));
    CHECK(end());

    setup(3);
    run();
    CHECK(group(2, -1));
    CHECK(group(1, -1));
    CHECK(group(0, -1));
    CHECK(end());

    // linear

    setup(3);
    edge(0, 1);
    edge(1, 2);
    run();
    CHECK(group(0, -1));
    CHECK(group(1, -1));
    CHECK(group(2, -1));
    CHECK(end());

    // tree

    setup(3);
    edge(0, 1);
    edge(0, 2);
    run();
    CHECK(group(0, -1));
    CHECK(group(2, -1));
    CHECK(group(1, -1));
    CHECK(end());

    // cycles

    setup(3);
    edge(0, 1);
    edge(1, 2);
    edge(2, 0);
    run();
    CHECK(group(0, 1, 2, -1));
    CHECK(end());

    setup(4);
    edge(0, 1);
    edge(1, 2);
    edge(2, 1);
    edge(2, 3);
    run();
    CHECK(group(0, -1));
    CHECK(group(1, 2, -1));
    CHECK(group(3, -1));
    CHECK(end());

    // remaining

    setup(2);
    edge(0, 1);
    run();
    CHECK(remaining(0, 1, -1));
    CHECK(end());

    setup(2);
    edge(0, 1);
    run();
    CHECK(group(0, -1));
    CHECK(remaining(1, -1));
    CHECK(end());

    setup(2);
    edge(0, 1);
    run();
    CHECK(group(0, -1));
    CHECK(group(1, -1));
    CHECK(remaining(-1));
    CHECK(end());

    return true;
}

unsigned vertex_count;
ComponentFinder<TestNode> *finder;
TestNode *resultsList;

void setup(unsigned count)
{
    vertex_count = count;
    for (unsigned i = 0; i < MaxVertices; ++i) {
        TestNode &v = Vertex[i];
        v.gcNextGraphNode = NULL;
        v.index = i;
        memset(&v.hasEdge, 0, sizeof(v.hasEdge));
    }
}

void edge(unsigned src_index, unsigned dest_index)
{
    Vertex[src_index].hasEdge[dest_index] = true;
}

void run()
{
    finder = new ComponentFinder<TestNode>(rt->mainThread.nativeStackLimit);
    for (unsigned i = 0; i < vertex_count; ++i)
        finder->addNode(&Vertex[i]);
    resultsList = finder->getResultsList();
}

bool group(int vertex, ...)
{
    TestNode *v = resultsList;

    va_list ap;
    va_start(ap, vertex);
    while (vertex != -1) {
        CHECK(v != NULL);
        CHECK(v->index == unsigned(vertex));
        v = v->nextNodeInGroup();
        vertex = va_arg(ap, int);
    }
    va_end(ap);

    CHECK(v == NULL);
    resultsList = resultsList->nextGroup();
    return true;
}

bool remaining(int vertex, ...)
{
    TestNode *v = resultsList;

    va_list ap;
    va_start(ap, vertex);
    while (vertex != -1) {
        CHECK(v != NULL);
        CHECK(v->index == unsigned(vertex));
        v = (TestNode *)v->gcNextGraphNode;
        vertex = va_arg(ap, int);
    }
    va_end(ap);

    CHECK(v == NULL);
    resultsList = NULL;
    return true;
}

bool end()
{
    CHECK(resultsList == NULL);

    delete finder;
    finder = NULL;
    return true;
}
END_TEST(testFindSCCs)

struct TestNode2 : public GraphNodeBase<TestNode2>
{
    TestNode2 *edge;

    TestNode2() :
        edge(NULL)
    {
    }

    void
    findOutgoingEdges(ComponentFinder<TestNode2> &finder) {
        if (edge)
            finder.addEdgeTo(edge);
    }
};

BEGIN_TEST(testFindSCCsStackLimit)
{
    /*
     * Test what happens if recusion causes the stack to become full while
     * traversing the graph.
     *
     * The test case is a large number of vertices, almost all of which are
     * arranged in a linear chain.  The last few are left unlinked to exercise
     * adding vertices after the stack full condition has already been detected.
     *
     * Such an arrangement with no cycles would normally result in one group for
     * each vertex, but since the stack is exhasted in processing a single group
     * is returned containing all the vertices.
     */
    const unsigned max = 1000000;
    const unsigned initial = 10;

    TestNode2 *vertices = new TestNode2[max]();
    for (unsigned i = initial; i < (max - 10); ++i)
        vertices[i].edge = &vertices[i + 1];

    ComponentFinder<TestNode2> finder(rt->mainThread.nativeStackLimit);
    for (unsigned i = 0; i < max; ++i)
        finder.addNode(&vertices[i]);

    TestNode2 *r = finder.getResultsList();
    CHECK(r);
    TestNode2 *v = r;

    unsigned count = 0;
    while (v) {
        ++count;
        v = v->nextNodeInGroup();
    }
    CHECK(count == max - initial);

    count = 0;
    v = r->nextGroup();
    while (v) {
        ++count;
        CHECK(!v->nextNodeInGroup());
        v = v->nextGroup();
    }

    delete [] vertices;
    return true;
}
END_TEST(testFindSCCsStackLimit)
