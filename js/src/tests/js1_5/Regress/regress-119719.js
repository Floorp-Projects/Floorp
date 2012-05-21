// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 119719;
var summary = 'Rethrown errors should have line number updated.';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var err = new Error('this error was created on line 46');
try
{
  throw err; // rethrow on line 49
}
catch(e)
{
  expect = 49;
  actual = err.lineNumber;
} 
reportCompare(expect, actual, summary);
