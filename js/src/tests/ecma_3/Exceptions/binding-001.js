/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-08-27
 *
 * SUMMARY:  Testing binding of function names
 *
 * Brendan:
 *
 * "... the question is, does Rhino bind 'sum' in the global object
 * for the following test? If it does, it's buggy.
 *
 *   var f = function sum(){};
 *   print(sum);  // should fail with 'sum is not defined' "
 *
 */

//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing binding of function names';
var ERR_REF_YES = 'ReferenceError';
var ERR_REF_NO = 'did NOT generate a ReferenceError';
var statusitems = [];
var actualvalues = [];
var expectedvalues = [];
var status = summary;
var actual = ERR_REF_NO;
var expect= ERR_REF_YES;


try
{
  var f = function sum(){};
  print(sum);
}
catch (e)
{
  status = 'Section 1 of test';
  actual = e instanceof ReferenceError;
  expect = true;
  addThis();


  /*
   * This test is more literal, and one day may not be valid.
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}


// converts a Boolean result into a textual result -
function isReferenceError(bResult)
{
  return bResult? ERR_REF_YES : ERR_REF_NO;
}
