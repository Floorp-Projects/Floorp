/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 246911;
var summary = 'switch() statement with variable as label';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
expect = 'PASS';

var a = 10;
a = 9;
var b = 10;

switch(b)
{
case a:
  actual = 'FAIL';
  break;
default:
  actual = 'PASS';
}

reportCompare(expect, actual, summary);
