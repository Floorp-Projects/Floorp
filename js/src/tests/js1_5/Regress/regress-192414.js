/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    08 February 2003
 * SUMMARY: Parser recursion should check stack overflow
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=192414
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 192414;
var summary = 'Parser recursion should check stack overflow';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
 * We will form an eval string to set the result-variable |actual|.
 * To get a feel for this, suppose N were 3. Then the eval string is
 * 'actual = (1&(1&(1&1)));' The expected value after eval() is 1.
 */
status = inSection(1);
var N = 10000;
var left = repeat_str('(1&', N);
var right = repeat_str(')', N);
var str = 'actual = '.concat(left, '1', right, ';');
try
{
  eval(str);
}
catch (e)
{
  /*
   * An exception during this eval is OK, as the runtime can throw one
   * in response to too deep recursion. We haven't crashed; good!
   */
  actual = 1;
}
expect = 1;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function repeat_str(str, repeat_count)
{
  var arr = new Array(--repeat_count);
  while (repeat_count != 0)
    arr[--repeat_count] = str;
  return str.concat.apply(str, arr);
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
