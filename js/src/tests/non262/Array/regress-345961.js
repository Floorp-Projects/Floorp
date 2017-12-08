/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 345961;
var summary = 'Array.prototype.shift should preserve holes';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = false;

  var array = new Array(2);
  array.shift();
  actual = array.hasOwnProperty(0);
  reportCompare(expect, actual, summary);

  array=Array(1);
  array.shift(1);
  actual = array.hasOwnProperty(1);
  reportCompare(expect, actual, summary);
}
