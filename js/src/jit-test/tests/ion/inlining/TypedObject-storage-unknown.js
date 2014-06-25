/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the transparent/opaque type
 * tests internal to storage().
 *
 * In this case the argument type is variable and thus unknown to the
 * JIT, so both ObjectIsOpaqueTypedObject and
 * ObjectIsTransparentTypedObject are resolved as uses of the
 * "HasClass" primitive.
 *
 * Load this into the js shell with IONFLAGS=logs, then exit and run
 * iongraph.  func01 will likely be the one we want (look for calls to
 * ObjectIsOpaqueTypedObject and ObjectIsTransparentTypedObject in the
 * graph for pass00).
 */

if (!this.TypedObject)
  quit();

var T = TypedObject;

function check(v) {
    return T.storage(v);
}

function test() {
    var AT = new T.ArrayType(T.int32,10);
    var v = new Object;         // Not actually a typed object
    var w = new AT(10);         // Actually a typed object
    var a = [v,w];
    for ( var i=0 ; i < 1000 ; i++ )
        try { check(a[i%2]); } catch (e) {}
    try { return check(a[1]); } catch (e) {}
}

test();
