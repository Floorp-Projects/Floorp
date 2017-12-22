/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    08 May 2003
 * SUMMARY: JS should evaluate RHS before binding LHS implicit variable
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=204919
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 204919;
var summary = 'JS should evaluate RHS before binding LHS implicit variable';
var TEST_PASSED = 'ReferenceError';
var TEST_FAILED = 'Generated an error, but NOT a ReferenceError!';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * global scope -
 */
status = inSection(1);
try
{
  x = x;
  actual = TEST_FAILED_BADLY;
}
catch(e)
{
  if (e instanceof ReferenceError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
expect = TEST_PASSED;
addThis();


/*
 * function scope -
 */
status = inSection(2);
try
{
  (function() {y = y;})();
  actual = TEST_FAILED_BADLY;
}
catch(e)
{
  if (e instanceof ReferenceError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
expect = TEST_PASSED;
addThis();


/*
 * eval scope -
 */
status = inSection(3);
try
{
  eval('z = z');
  actual = TEST_FAILED_BADLY;
}
catch(e)
{
  if (e instanceof ReferenceError)
    actual = TEST_PASSED;
  else
    actual = TEST_FAILED;
}
expect = TEST_PASSED;
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
