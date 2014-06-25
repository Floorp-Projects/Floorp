/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 412467;
var summary = 'Iterator values in array comprehension';
var actual = '';
var expect = 'typeof(iterand) == undefined, ';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function g() { yield 1; yield 2; }

  var a = [iterand for (iterand in g())];

  expect = true;
  actual = typeof iterand == 'undefined';
  reportCompare(expect, actual, summary + ': typeof iterand == \'undefined\'');

  expect = true;
  actual = a.length == 2 && a.toString() == '1,2';
  reportCompare(expect, actual, summary + ': a.length == 2 && a.toString() == \'1,2\'');

  exitFunc ('test');
}
