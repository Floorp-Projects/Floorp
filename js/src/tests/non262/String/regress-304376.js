/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 304376;
var summary = 'String.prototype should be readonly and permanent';
var actual = '';
var expect = '';
printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'TypeError';

var saveString = String;
 
String = Array;

try
{
  // see if we can crash...
  "".join();
  String = saveString;
  actual = 'No Error';
}
catch(ex)
{
  String = saveString;
  actual = ex.name;
  printStatus(ex + '');
}

reportCompare(expect, actual, summary);
