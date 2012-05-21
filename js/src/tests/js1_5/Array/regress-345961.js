/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  enterFunc ('test');
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

  exitFunc ('test');
}
