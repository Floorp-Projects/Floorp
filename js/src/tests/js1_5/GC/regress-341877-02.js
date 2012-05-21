/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 341877;
var summary = 'GC hazard with for-in loop';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var obj = { };

var prop = "xsomePropety".substr(1);

obj.first = "first"

  obj[prop] = 1;

for (var elem in obj) {
  var tmp = elem.toString();
  delete obj[prop];
  // ensure that prop is cut from all roots
  prop = "xsomePropety".substr(2);
  obj[prop] = 2;
  delete obj[prop];
  prop = null;
  if (typeof gc == 'function')
    gc();
  for (var i = 0; i != 50000; ++i) {
    var tmp = 1 / 3;
    tmp /= 10;
  }
  for (var i = 0; i != 1000; ++i) {
    // Make string with 11 characters that would take
    // (11 + 1) * 2 bytes or sizeof(JSAtom) so eventually
    // malloc will ovewrite just freed atoms.
    var tmp2 = Array(12).join(' ');
  }
}
 
reportCompare(expect, actual, summary);
