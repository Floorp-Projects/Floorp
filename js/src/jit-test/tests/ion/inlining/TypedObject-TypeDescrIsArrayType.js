/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Testing TypeDescrIsArrayType() is somewhat straightforward: it's
 * used early in the map() method on TypedObject arrays.
 *
 * This basically asserts that map() works, and thus that the code is
 * at least not broken.
 *
 * To verify that inlining happens:
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsArrayType func*pass00*.pdf", this
 * might net a function that is a likely candidate for manual inspection.
 *
 * (It is sometimes useful to neuter the assert() macro in the
 * self-hosted code.)
 */

if (!this.TypedObject) {
    print("No TypedObject, skipping");
    quit();
}

var T = TypedObject;
var AT = new T.ArrayType(T.int32);

function check(v) {
    return v.map(x => x+1);
}

function test() {
    var w = new AT(100);
    for ( var i=0 ; i < 1000 ; i++ )
	w = check(w);
    return w;
}

var w = test();
assertEq(w.length, 100);
assertEq(w[99], 1000);
print("Done");

