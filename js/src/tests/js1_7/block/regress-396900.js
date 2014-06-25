/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 396900;
var summary = 'Destructuring bind in a let';
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
 
  expect = '3, 4';
  let ([x, y] = [3, 4]) { actual = x + ', ' + y}
  reportCompare(expect, actual, summary);

  expect = 'undefined, undefined';
  actual = typeof x + ', ' + typeof y;
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
