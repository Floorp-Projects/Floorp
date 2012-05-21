/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 295052;
var summary = 'Do not crash when apply method is called on String.prototype.match';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  "".match.apply();
  throw new Error("should have thrown for undefined this");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "No TypeError for String.prototype.match");
}
 
reportCompare(expect, actual, summary);
