/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 414553;
var summary = '';

printBugNumber(BUGNUMBER);
printStatus(summary);

var expected = '1,2,3,4';

let a = 1, [b,c] = [2,3], d = 4;
var actual = String([a,b,c,d]);

reportCompare(expected, actual, 'destructuring assignment in let');

function f() {
  {
    let a = 1, [b,c] = [2,3], d = 4;
    return String([a,b,c,d]);
  }
}

reportCompare(expected, f(), 'destructuring assignment in let inside func');
