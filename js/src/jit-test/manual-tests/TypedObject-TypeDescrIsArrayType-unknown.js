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
 * This test differs from the one in TypedObject-TypeDescrIsArrayType.js
 * in that it tries to hide type information from the JIT.  Sadly,
 * it turns out to be hard to write a test that causes a run-time test
 * to be inserted at the right spot.  This is my best effort,
 * but the test was still specialized as "true" in the generated code
 * last I looked.
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
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsArrayType func*pass00*.pdf", this
 * might net a function that is a likely candidate for manual inspection.
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

function Array_build(n, f) {
  var a = new Array(n);
  for ( var i=0 ; i < n ; i++ )
    a[i] = f(i);
  return a;
}

function test() {
    var w1 = AT.build(x => x+1);
    var w2 = Array_build(100, x => x+1);
    w2.map = w1.map;
    var a = [ w1, w2 ];
    for ( var i=0 ; i < 2000 ; i++ )
	try { a[i%2] = check(a[i%2]); } catch (e) { assertEq( i%2, 1 ); }
    return a[0];
}

var w = test();
assertEq(w.length, 100);
assertEq(w[99], 1100);
print("Done");
