/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 419152;
var summary = 'Shaver can not contain himself';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var a = [1,2,3];

  a[5] = 6;
  expect = '1,2,3,,,6:6';
  actual = a + ':' + a.length;
  reportCompare(expect, actual, summary + ': 1');

  a = [1,2,3,4];
  expect = 'undefined';
  actual = a[-1] + '';
  reportCompare(expect, actual, summary + ': 2');

  a = [1,2,3];
  a[-1] = 55;

  expect = 3;
  actual = a.length;
  reportCompare(expect, actual, summary + ': 3');

  expect = '1,2,3';
  actual = a + '';
  reportCompare(expect, actual, summary + ': 4');

  expect = 55;
  actual = a[-1];
  reportCompare(expect, actual, summary + ': 5');

  var s = "abcdef";

  expect = 'undefined';
  actual = s[-2] + '';
  reportCompare(expect, actual, summary + ': 6');

  exitFunc ('test');
}
