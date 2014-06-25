/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    03 February 2003
 * SUMMARY: Testing script with huge number of comments
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=191633
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 191633;
var summary = 'Testing script with huge number of comments';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
actual = false; // initialize to failure
var s = repeat_str("//\n", 40000); // Build a string of 40000 lines of comments
eval(s + "actual = true;");
expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function repeat_str(str, repeat_count)
{
  var arr = new Array(repeat_count);

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
