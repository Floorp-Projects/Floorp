// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 418730;
var summary = 'export * should not halt script';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
for (var i = 0; i < 60; ++i)
  this["v" + i] = true;

expect = 'PASS';
actual = 'FAIL';

try {
  print("GO");
  export *;
  print("PASS (1)");
} catch(e) {
  print("PASS (2)");
}

actual = 'PASS';

reportCompare(expect, actual, summary);
