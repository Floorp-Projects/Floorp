/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that heap snapshots cross compartment boundaries when expected.

#include "DevTools.h"

DEF_TEST(DoesCrossCompartmentBoundaries, {
  // Create a new global to get a new compartment.
  JS::RealmOptions options;
  // dummy
  options.behaviors().setReduceTimerPrecisionCallerType(
      JS::RTPCallerTypeToken{0});
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

  // Our set of target compartments is both the old and new compartments.
  JS::CompartmentSet targetCompartments;
  ASSERT_TRUE(targetCompartments.put(compartment));
  ASSERT_TRUE(targetCompartments.put(newCompartment));

  FakeNode nodeA;
  FakeNode nodeB;
  FakeNode nodeC;
  FakeNode nodeD;

  nodeA.compartment = compartment;
  nodeB.compartment = nullptr;
  nodeC.compartment = newCompartment;
  nodeD.compartment = nullptr;

  AddEdge(nodeA, nodeB);
  AddEdge(nodeA, nodeC);
  AddEdge(nodeB, nodeD);

  ::testing::NiceMock<MockWriter> writer;

  // Should serialize nodeA, because it is in one of our target compartments.
  ExpectWriteNode(writer, nodeA);

  // Should serialize nodeB, because it doesn't belong to a compartment and is
  // therefore assumed to be shared.
  ExpectWriteNode(writer, nodeB);

  // Should also serialize nodeC, which is in our target compartments, but a
  // different compartment than A.
  ExpectWriteNode(writer, nodeC);

  // Should serialize nodeD because it's reachable via B and both nodes B and D
  // don't belong to a specific compartment.
  ExpectWriteNode(writer, nodeD);

  JS::AutoCheckCannotGC noGC(cx);

  ASSERT_TRUE(WriteHeapGraph(cx, JS::ubi::Node(&nodeA), writer,
                             /* wantNames = */ false, &targetCompartments,
                             noGC));
});
