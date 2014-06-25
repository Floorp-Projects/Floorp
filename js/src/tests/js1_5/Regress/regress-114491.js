/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    10 December 2001
 * SUMMARY: Regression test for bug 114491
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=114491
 *
 * Rhino crashed on this code. It should produce a syntax error, not a crash.
 * Using the () operator after a function STATEMENT is incorrect syntax.
 * Rhino correctly caught the error when there was no |if (true)|.
 * With the |if (true)|, however, Rhino crashed -
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 114491;
var summary = 'Regression test for bug 114491';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
actual = 'Program execution did NOT fall into catch-block';
expect = 'Program execution fell into into catch-block';
try
{
  var sEval = 'if (true) function f(){}()';
  eval(sEval);
}
catch(e)
{
  actual = expect;
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
