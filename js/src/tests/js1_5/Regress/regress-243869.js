/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// testcase from Alex Vincent
var BUGNUMBER = 243869;
var summary = 'Rethrown custom Errors should retain file and line number';
var actual = '';
var expect = 'Test Location:123';

printBugNumber(BUGNUMBER);
printStatus (summary);

function bar()
{
  try
  {
    var f = new Error("Test Error", "Test Location", 123);
    throw f;
  }
  catch(e)
  {
    throw e;
  }
}

try
{
  bar();
}
catch(eb)
{
  actual = eb.fileName + ':' + eb.lineNumber
    }
 
reportCompare(expect, actual, summary);
