/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 396684;
var summary = 'Function call with stack arena exhausted';
var actual = '';
var expect = '';

/*
  The test case builds a function containing f(0,0,...,0,Math.atan2()) with
  enough zero arguments to exhaust the current stack arena fully. Thus, when
  Math.atan2 is loaded into the stack, there would be no room for the extra 2
  args required by Math.atan2 and args will be allocated from the new arena.
*/


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function f() {
  return arguments[arguments.length - 1];
}

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'PASS';

  var src = "return f(" +Array(10*1000).join("0,")+"Math.atan2());";

  var result = new Function(src)();

  if (typeof result != "number" || !isNaN(result))
    actual = "unexpected result: " + result;
  else
    actual = 'PASS';

  reportCompare(expect, actual, summary);
}
