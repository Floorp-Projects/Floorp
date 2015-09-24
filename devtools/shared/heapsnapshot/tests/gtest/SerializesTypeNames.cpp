/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that a ubi::Node's typeName gets properly serialized into a core dump.

#include "DevTools.h"

using testing::Property;
using testing::Return;

DEF_TEST(SerializesTypeNames, {
    FakeNode node;

    ::testing::NiceMock<MockWriter> writer;
    EXPECT_CALL(writer, writeNode(Property(&JS::ubi::Node::typeName,
                                           UTF16StrEq(MOZ_UTF16("FakeNode"))),
                                  _))
      .Times(1)
      .WillOnce(Return(true));

    JS::AutoCheckCannotGC noGC(rt);
    ASSERT_TRUE(WriteHeapGraph(cx,
                               JS::ubi::Node(&node),
                               writer,
                               /* wantNames = */ true,
                               /* zones = */ nullptr,
                               noGC));
  });
