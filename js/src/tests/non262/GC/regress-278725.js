/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// testcase by James Ross <silver@warwickcompsoc.co.uk>
var BUGNUMBER = 278725;
var summary = 'Don\'t Crash during GC';
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var results = [];
for (var k = 0; k < 600000; k++) {
  if (! (k %100000)) {
    printStatus('hi');
    if (0) {
      results.length = 0;
      gc();
    }
  }
  results.push({});
}

actual = 'No Crash';
reportCompare(expect, actual, summary);
