/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 495907;
var summary = 'Read upvar from trace-entry frame when created with top-level let';
var actual = ''
var expect = '00112233';
//-----------------------------------------------------------------------------

// The test code needs to run at top level in order to expose the bug.
start_test();

var o = [];
for (let a = 0; a < 4; ++a) {
    (function () {for (var b = 0; b < 2; ++b) {o.push(a);}}());
}
actual = o.join("");

finish_test();
//-----------------------------------------------------------------------------

function start_test() {
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  jit(true);
}

function finish_test() {
  jit(false);
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
