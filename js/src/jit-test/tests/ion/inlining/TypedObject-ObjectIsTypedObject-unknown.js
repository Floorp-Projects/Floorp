/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the ObjectIsTypedObject tests
 * used in the TO.objectType() method, among other places.
 *
 * In this case the argument type is sometimes a TypedObject,
 * sometimes not, so ObjectIsTypedObject must be a run-time check and
 * sometimes it resolves to "false" and takes a more expensive path.
 * There should be no exceptions: the operation is defined also on
 * non-TypedObjects.
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
var ST1 = new T.StructType({x:T.int32});
var v1 = new ST1({x:10});

function check(v) {
    return T.objectType(v);
}

function test() {
    var v2 = { tag: "Hello, world!" };
    var a = [ v1, v2 ];
    for ( var i=0 ; i < 1000 ; i++ )
	assertEq(check(a[i%2]), (i%2) == 0 ? ST1 : T.Object);
    return check(a[i%2]);
}

print("Done");


