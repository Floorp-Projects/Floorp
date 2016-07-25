/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that everything in the heap graph gets serialized once, and only once.

#include "DevTools.h"

DEF_TEST(SerializesEverythingInHeapGraphOnce, {
    FakeNode nodeA;
    FakeNode nodeB;
    FakeNode nodeC;
    FakeNode nodeD;

    AddEdge(nodeA, nodeB);
    AddEdge(nodeB, nodeC);
    AddEdge(nodeC, nodeD);
    AddEdge(nodeD, nodeA);

    ::testing::NiceMock<MockWriter> writer;

    // Should serialize each node once.
    ExpectWriteNode(writer, nodeA);
    ExpectWriteNode(writer, nodeB);
    ExpectWriteNode(writer, nodeC);
    ExpectWriteNode(writer, nodeD);

    JS::AutoCheckCannotGC noGC(cx);

    ASSERT_TRUE(WriteHeapGraph(cx,
                               JS::ubi::Node(&nodeA),
                               writer,
                               /* wantNames = */ false,
                               /* zones = */ nullptr,
                               noGC));
  });
