/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that edge names get serialized correctly.

#include "DevTools.h"

using testing::Field;
using testing::IsNull;
using testing::Property;
using testing::Return;

DEF_TEST(SerializesEdgeNames, {
    FakeNode node;
    FakeNode referent;

    const char16_t edgeName[] = u"edge name";
    const char16_t emptyStr[] = u"";

    AddEdge(node, referent, edgeName);
    AddEdge(node, referent, emptyStr);
    AddEdge(node, referent, nullptr);

    ::testing::NiceMock<MockWriter> writer;

    // Should get the node with edges once.
    EXPECT_CALL(
      writer,
      writeNode(AllOf(EdgesLength(cx, 3),
                      Edge(cx, 0, Field(&JS::ubi::Edge::name,
                                        UniqueUTF16StrEq(edgeName))),
                      Edge(cx, 1, Field(&JS::ubi::Edge::name,
                                        UniqueUTF16StrEq(emptyStr))),
                      Edge(cx, 2, Field(&JS::ubi::Edge::name,
                                        UniqueIsNull()))),
                _)
    )
      .Times(1)
      .WillOnce(Return(true));

    // Should get the referent node that doesn't have any edges once.
    ExpectWriteNode(writer, referent);

    JS::AutoCheckCannotGC noGC(cx);
    ASSERT_TRUE(WriteHeapGraph(cx,
                               JS::ubi::Node(&node),
                               writer,
                               /* wantNames = */ true,
                               /* zones = */ nullptr,
                               noGC));
  });
