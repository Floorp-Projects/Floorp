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

// Prepare  array a to cause O(a.length^2) behaviour in the current
// DeutschSchorrWaite implementation

var a = new Array(1000 * 100);

var i = a.length;
while (i-- != 0)
{
  a[i] = {};
}

// Prepare linked list that causes recursion during GC with
// depth O(list size)

for (i = 0; i != 50*1000; ++i)
{
  a = [a, a.concat()];
}

if (typeof gc == 'function')
{
  gc();
}

actual = 'PASS';

reportCompare(expect, actual, summary);

