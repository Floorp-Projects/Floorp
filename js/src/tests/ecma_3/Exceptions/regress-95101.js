/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 13 August 2001
 *
 * SUMMARY: Invoking an undefined function should produce a ReferenceError
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=95101
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 95101;
var summary = 'Invoking an undefined function should produce a ReferenceError';
var msgERR_REF_YES = 'ReferenceError';
var msgERR_REF_NO = 'did NOT generate a ReferenceError';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


try
{
  xxxyyyzzz();
}
catch (e)
{
  status = 'Section 1 of test';
  actual = e instanceof ReferenceError;
  expect = true;
  addThis();


  /*
   * This test is more literal, and may one day be invalid.
   * Searching for literal string "ReferenceError" in e.toString()
   */
  status = 'Section 2 of test';
  var match = e.toString().search(/ReferenceError/);
  actual = (match > -1);
  expect = true;
  addThis();
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = isReferenceError(actual);
  expectedvalues[UBound] = isReferenceError(expect);
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


// converts a Boolean result into a textual result -
function isReferenceError(bResult)
{
  return bResult? msgERR_REF_YES : msgERR_REF_NO;
}
