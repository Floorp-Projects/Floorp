/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465567;
var summary = 'TM: Weirdness with var, let, multiple assignments';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = '99999';


for (let j = 0; j < 5; ++j) {
  var e;
  e = 9;
  print(actual += '' + e);
  e = 47;
  if (e & 0) {
    let e;
  }
}


reportCompare(expect, actual, summary);
