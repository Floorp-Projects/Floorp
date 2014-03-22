/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the ObjectIsTypedObject tests
 * used in the TO.objectType() method, among other places.
 *
 * In this case the argument type is always a TypedObject, so
 * ObjectIsTypedObject resolves to true and there should be no
 * exceptions.
 *
 * Load this into the js shell with IONFLAGS=logs, then exit and run
 * iongraph.  You're looking for a smallish function within the
 * "self-hosted" domain.  Look for a call to ObjectIsTypedObject far
 * down in the graph for pass00, with a subgraph before it that looks
 * like it's comparing something to a string and to null (this is the
 * inlining of IsObject).  (All of this is at the mercy of the way the
 * code is currently written.)
 */

if (!this.TypedObject) {
    print("No TypedObject, skipping");
    quit();
}

var T = TypedObject;
var ST = new T.StructType({x:T.int32});
var v = new ST({x:10});

function check(v) {
  return T.objectType(v);
}

function test() {
  for ( var i=0 ; i < 1000 ; i++ )
    assertEq(check(v), ST);
  return check(v);
}

print("Done");


