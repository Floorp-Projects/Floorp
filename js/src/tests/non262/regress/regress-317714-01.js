/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 317714;
var summary = 'Regression test for regression from bug 316885';
var actual = 'No Crash';
var expect = 'No Crash';

var d5="-1";
var r3=d5.split(":");
r3[0]++;

printBugNumber(BUGNUMBER);
printStatus (summary);

reportCompare(expect, actual, summary);
