/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 203278;
var summary = 'Don\'t crash in recursive js_MarkGCThing';
var actual = 'FAIL';
var expect = 'PASS';

printBugNumber(BUGNUMBER);
printStatus (summary);

// Prepare  array to test DeutschSchorrWaite implementation
// and its reverse pointer scanning performance

var a = new Array(1000 * 1000);

var i = a.length;
while (i-- != 0) {
  switch (i % 11) {
  case 0:
    a[i] = { };
    break;
  case 1:
    a[i] = { a: true, b: false, c: 0 };
    break;
  case 2:
    a[i] = { 0: true, 1: {}, 2: false };
    break;
  case 3:
    a[i] = { a: 1.2, b: "", c: [] };
    break;
  case 4:
    a[i] = [ false ];
    break;
  case 6:
    a[i] = [];
    break;
  case 7:
    a[i] = false;
    break;
  case 8:
    a[i] = "x";
    break;
  case 9:
    a[i] = new String("x");
    break;
  case 10:
    a[i] = 1.1;
    break;
  case 10:
    a[i] = new Boolean();
    break;
  }	   
}

printStatus("DSF is prepared");

// Prepare linked list that causes recursion during GC with
// depth O(list size)
// Note: pass "-S 500000" option to the shell to limit stack quota
// available for recursion

for (i = 0; i != 50*1000; ++i) {
  a = [a, a, {}];
  a = [a,  {}, a];

}

printStatus("Linked list is prepared");

gc();

actual = 'PASS';

reportCompare(expect, actual, summary);

