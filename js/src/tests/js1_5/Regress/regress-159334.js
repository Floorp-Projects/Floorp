/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    31 Oct 2002
 * SUMMARY: Testing script with at least 64K of different string literals
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=159334
 *
 * Testing that script engine can handle scripts with at least 64K of different
 * string literals. The following will evaluate, via eval(), a script like this:
 *
 *     f('0')
 *     f('1')
 *     ...
 *     f('N - 1')
 *
 * where N is 0xFFFE
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 159334;
var summary = 'Testing script with at least 64K of different string literals';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var N = 0xFFFE;

// Create big string for eval recursively to avoid N*N behavior
// on string concatenation
var long_eval = buildEval_r(0, N);

// Run it
var test_sum = 0;
function f(str) { test_sum += Number(str); }
eval(long_eval);

status = inSection(1);
actual = (test_sum == N * (N - 1) / 2);
expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function buildEval_r(beginLine, endLine)
{
  var count = endLine - beginLine;

  if (count == 0)
    return "";

  if (count == 1)
    return "f('" + beginLine + "')\n";

  var middle = beginLine + (count >>> 1);
  return buildEval_r(beginLine, middle) + buildEval_r(middle, endLine);
}


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
