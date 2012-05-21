/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338804;
var summary = 'GC hazards in constructor functions';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof Script != 'undefined')
{
  Script({ toString: fillHeap });
}
RegExp({ toString: fillHeap });

function fillHeap() {
  if (typeof gc == 'function') gc();
  var x = 1, tmp;
  for (var i = 0; i != 50000; ++i) {
    tmp = x / 3;
  }
}
 
reportCompare(expect, actual, summary);
