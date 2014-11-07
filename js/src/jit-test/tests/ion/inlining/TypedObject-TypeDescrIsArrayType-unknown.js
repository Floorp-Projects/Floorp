/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 2 -*-
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
 * It turns out to be hard to write a test that causes a run-time test
 * to be inserted at the right spot, however.  This is my best effort,
 * but the test is still specialized as "true" in the generated code.
 *
 * (It is sometimes useful to neuter the assert() macro in the
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
    var w1 = AT.build(x => x+1);
    var w2 = Array.build(100, x => x+1);
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
