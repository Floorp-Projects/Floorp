/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Testing TypeDescrIsSizedArrayType() is fairly straightforward:
 * it's used early in the sequential scatter() method on TypedObject
 * arrays, and is applied to the first argument (not to "this").
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsSizedArrayType func*pass00*.pdf", this
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
var IT = AT.dimension(100);
var ix = AT.build(100, x => x == 0 ? 99 : x-1);  // [99, 0, 1, ..., 98]

// This is a left-rotate by one place
function check(v) {
    return v.scatter(IT, ix);
}

function test() {
    var w = AT.build(100, x => x);
    for ( var i=0 ; i < 77 ; i++ )
	w = check(w);
    return w;
}

w = test();
assertEq(w.length, 100);
assertEq(w[0], 77);
print("Done");
