/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the transparent/opaque type
 * tests internal to storage().
 *
 * In this case the argument type is always a non-TypedObject, so both
 * ObjectIsOpaqueTypedObject and ObjectIsTransparentTypedObject
 * resolve to false.
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
    var v = new Object;         // Not actually a typed object
    for ( var i=0 ; i < 1000 ; i++ )
        try { check(v); } catch (e) {}
    try { return check(v); } catch (e) {}
}

test();
