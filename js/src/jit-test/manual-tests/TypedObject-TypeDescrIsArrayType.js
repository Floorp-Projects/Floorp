/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Susceptible to timeouts on some systems, see bug 1203595, if run
 * with --ion-eager --ion-offthread-compile=off.
 *
 * Probably only meaningful to run this with default flags.
 */

/* This test is intended to test Ion code generation for the internal
 * primitive TypeDescrIsArrayType(), available to self-hosted code.
 *
 * To do that, it must trigger enough uses of that primitive as well
 * as enough uses of a caller of the primitive to trigger inlining of
 * the primitive into its compiled caller.
 *
 * It turns out that TypeDescrIsArrayType() is used early in the map()
 * method on TypedObject arrays, so code that heavily uses the latter
 * method will usually be good enough.
 *
 * In practice this test just asserts that map() works, and thus that
 * the code for TypeDescrIsArrayType() is at least not completely
 * broken.
 *
 * The test is only meaningful if inlining actually happens.  Here is
 * how to verify that (manually):
 *
 * Run this test with IONFLAGS=logs, generate pdfs with iongraph, and
 * then try running "pdfgrep TypeDescrIsArrayType func*pass00*.pdf",
 * this might net a function that is a likely candidate for further
 * manual inspection.
 *
 * (It is sometimes useful to comment out the assert() macro in the
 * self-hosted code.)
 */

if (!this.TypedObject) {
    print("No TypedObject, skipping");
    quit();
}

var T = TypedObject;
var AT = new T.ArrayType(T.int32, 100);

function check(v) {
    return v.map(x => x+1);
}

function test() {
    var w = new AT();
    for ( var i=0 ; i < 1000 ; i++ )
	w = check(w);
    return w;
}

var w = test();
assertEq(w.length, 100);
assertEq(w[99], 1000);
print("Done");

