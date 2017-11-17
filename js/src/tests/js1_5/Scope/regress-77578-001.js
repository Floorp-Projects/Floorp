/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-11
 *
 * SUMMARY: Testing eval scope inside a function.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=77578
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 77578;
var summary = 'Testing eval scope inside a function';
var cnEquals = '=';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

// Note contrast with local variables i,j,k defined below -
var i = 999;
var j = 999;
var k = 999;


//--------------------------------------------------
test();
//--------------------------------------------------


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  testA();
  testB();
  testC();

  // Compare actual values to expected values -
  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}


function testA()
{
  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable i
  sTestScript += "status = 'Section A of test';";
  sTestScript += "var i=1;";
  sTestScript += "actual = eval('i');";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('i');";

  eval(sTestScript);
}


function testB()
{
  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local for-loop iterator j
  sTestScript += "status = 'Section B of test';";
  sTestScript += "for(var j=1; j<2; j++)";
  sTestScript += "{";
  sTestScript += "  actual = eval('j');";
  sTestScript += "};";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('j');";

  eval(sTestScript);
}


function testC()
{
  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable k in a try-catch block -
  sTestScript += "status = 'Section C of test';";
  sTestScript += "try";
  sTestScript += "{";
  sTestScript += "  var k=1;";
  sTestScript += "  actual = eval('k');";
  sTestScript += "}";
  sTestScript += "catch(e)";
  sTestScript += "{";
  sTestScript += "};";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('k');";

  eval(sTestScript);
}


function captureThis(varName)
{
  statusitems[UBound] = status;
  actualvalues[UBound] = varName + cnEquals + actual;
  expectedvalues[UBound] = varName + cnEquals + expect;
  UBound++;
}
