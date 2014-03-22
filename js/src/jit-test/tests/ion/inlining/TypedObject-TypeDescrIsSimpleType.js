/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Testing TypeDescrIsSimpleType() is tricky because it's not exposed.
 * However, the implementation of <typed-object>.build() must use it.
 *
 * This basically asserts that build() works, and thus that the code is
 * at least not broken.
 *
 * To verify that inlining happens:
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsSimpleType func*pass00*.pdf", this
 * might net a couple of functions that are likely candidates for
 * manual inspection.
 */

if (!this.TypedObject) {
    print("No TypedObject, skipping");
    quit();
}

var T = TypedObject;
var AT = new T.ArrayType(T.uint32);

function check() {
    return AT.build(100, x => x+1);
}

function test() {
    var w;
    for ( var i=0 ; i < 100 ; i++ )
	w = check();
    return w;
}

var w = test();
assertEq(w.length, 100);
assertEq(w[99], 100);
print("Done");

