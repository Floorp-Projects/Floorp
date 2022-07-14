/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that heap snapshots walk the compartment boundaries correctly.

#include "DevTools.h"

DEF_TEST(DoesntCrossCompartmentBoundaries, {
  // Create a new global to get a new compartment.
  JS::RealmOptions options;
  JS::Rooted<JSObject*> newGlobal(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, options));
  ASSERT_TRUE(newGlobal);
  JS::Compartment* newCompartment = nullptr;
  {
    JSAutoRealm ar(cx, newGlobal);
    ASSERT_TRUE(JS::InitRealmStandardClasses(cx));
    newCompartment = js::GetContextCompartment(cx);
  }
  ASSERT_TRUE(newCompartment);
  ASSERT_NE(newCompartment, compartment);

  // Our set of target compartments is only the pre-existing compartment and
  // does not include the new compartment.
  JS::CompartmentSet targetCompartments;
  ASSERT_TRUE(targetCompartments.put(compartment));

  FakeNode nodeA;
  FakeNode nodeB;
  FakeNode nodeC;

  nodeA.compartment = compartment;
  nodeB.compartment = nullptr;
  nodeC.compartment = newCompartment;

  AddEdge(nodeA, nodeB);
  AddEdge(nodeB, nodeC);

  ::testing::NiceMock<MockWriter> writer;

  // Should serialize nodeA, because it is in our target compartments.
  ExpectWriteNode(writer, nodeA);

  // Should serialize nodeB, because it doesn't belong to a compartment and is
  // therefore assumed to be shared.
  ExpectWriteNode(writer, nodeB);

  // But we shouldn't ever serialize nodeC.

  JS::AutoCheckCannotGC noGC(cx);

  ASSERT_TRUE(WriteHeapGraph(cx, JS::ubi::Node(&nodeA), writer,
                             /* wantNames = */ false, &targetCompartments,
                             noGC));
});
