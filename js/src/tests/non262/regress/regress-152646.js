/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    08 July 2002
 * SUMMARY: Testing expressions with large numbers of parentheses
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=152646
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 152646;
var summary = 'Testing expressions with large numbers of parentheses';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Just seeing that we don't crash when compiling this assignment -
 *
 * We will form an eval string to set the result-variable |actual|.
 * To get a feel for this, suppose N were 3. Then the eval string is
 * 'actual = (((0)));' The expected value for this after eval() is 0.
 */
status = inSection(1);

var sLeft = '((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((';
sLeft += sLeft;
sLeft += sLeft;
sLeft += sLeft;
sLeft += sLeft;
sLeft += sLeft;

var sRight = '))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))';
sRight += sRight;
sRight += sRight;
sRight += sRight;
sRight += sRight;
sRight += sRight;

var sEval = 'actual = ' + sLeft + '0' + sRight;
try
{
  eval(sEval);
}
catch(e)
{
  /*
   * An exception during this eval is OK, as the runtime can throw one
   * in response to too deep recursion. We haven't crashed; good!
   */
  actual = 0;
}
expect = 0;
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
