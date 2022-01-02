/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 306788;
var summary = 'Do not crash sorting Arrays due to GC';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var array = new Array();

for (var i = 0; i < 5000; i++)
{
  array[i] = new Array('1', '2', '3', '4', '5');
}

array.sort();
 
reportCompare(expect, actual, summary);
