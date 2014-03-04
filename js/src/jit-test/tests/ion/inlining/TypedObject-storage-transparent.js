/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the transparent/opaque type
 * tests internal to storage().
 *
 * In this case the argument type is always a transparent object, so
 * ObjectIsOpaqueTypedObject resolves to false and
 * ObjectIsTransparentTypedObject resolves to true.
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
    var v = new AT(10);
    for ( var i=0 ; i < 1000 ; i++ )
        check(v);
    return check(v);
}

test();
