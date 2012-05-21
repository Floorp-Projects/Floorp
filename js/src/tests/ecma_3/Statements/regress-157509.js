/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    15 July 2002
 * SUMMARY: Testing for SyntaxError on usage of '\' in identifiers
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=157509
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 157509;
var summary = "Testing for SyntaxError on usage of '\\' in identifiers";
var TEST_PASSED = 'SyntaxError';
var TEST_FAILED = 'Generated an error, but NOT a SyntaxError!';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
expect = TEST_PASSED;
actual = TEST_FAILED_BADLY;
/*
 * OK, this should generate a SyntaxError
 */
try
{
  eval('var a\\1 = 0;');
}
catch(e)
{
  if (e instanceof SyntaxError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
