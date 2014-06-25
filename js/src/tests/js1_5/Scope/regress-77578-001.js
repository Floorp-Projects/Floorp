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


// various versions of JavaScript -
var JS_VER = [100, 110, 120, 130, 140, 150];

// Note contrast with local variables i,j,k defined below -
var i = 999;
var j = 999;
var k = 999;


//--------------------------------------------------
test();
//--------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // Run tests A,B,C on each version of JS and store results
  for (var n=0; n!=JS_VER.length; n++)
  {
    testA(JS_VER[n]);
  }
  for (var n=0; n!=JS_VER.length; n++)
  {
    testB(JS_VER[n]);
  }
  for (var n=0; n!=JS_VER.length; n++)
  {
    testC(JS_VER[n]);
  }


  // Compare actual values to expected values -
  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


function testA(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable i
  sTestScript += "status = 'Section A of test; JS ' + ver/100;";
  sTestScript += "var i=1;";
  sTestScript += "actual = eval('i');";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('i');";

  eval(sTestScript);
}


function testB(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local for-loop iterator j
  sTestScript += "status = 'Section B of test; JS ' + ver/100;";
  sTestScript += "for(var j=1; j<2; j++)";
  sTestScript += "{";
  sTestScript += "  actual = eval('j');";
  sTestScript += "};";
  sTestScript += "expect = 1;";
  sTestScript += "captureThis('j');";

  eval(sTestScript);
}


function testC(ver)
{
  // Set the version of JS to test -
  if (typeof version == 'function')
  {
    version(ver);
  }

  // eval the test, so it compiles AFTER version() has executed -
  var sTestScript = "";

  // Define a local variable k in a try-catch block -
  sTestScript += "status = 'Section C of test; JS ' + ver/100;";
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
