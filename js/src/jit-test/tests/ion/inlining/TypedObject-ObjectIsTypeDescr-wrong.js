/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the ObjectIsTypeDescr tests
 * internal to Type.toSource().
 *
 * In this case the argument type is never a type descriptor object,
 * so ObjectIsTypeDescr resolves to false (and we have to catch
 * exceptions).
 *
 * Load this into the js shell with IONFLAGS=logs, then exit and run
 * iongraph.  You're looking for a smallish function within the
 * "self-hosted" domain.  Look for a call to ObjectIsTypeDescr far
 * down in the graph for pass00, with a call to DescrToSource in a
 * subsequent block (all of this is at the mercy of the way the code
 * is currently written).
 */

if (!this.TypedObject)
  quit();

var T = TypedObject;
var ST = new T.StructType({x:T.int32});

function check(v) {
    return v.toSource();
}

function test() {
    var fake = { toSource: ST.toSource };
    for ( var i=0 ; i < 1000 ; i++ )
	try { check(fake); } catch (e) {}
    try { return check(fake); } catch (e) { return "Thrown" }
}

print(test());

