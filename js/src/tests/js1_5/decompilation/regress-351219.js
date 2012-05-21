/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351219;
var summary = 'Decompilation of immutable infinity, NaN';
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
 
  var f = function () { return 2e308 };
  expect = 'function () { return 1 / 0; }';
  actual = f + '';
  compareSource(expect, actual, summary + ' decompile Infinity as 1/0');

  f = function () { var NaN = 1 % 0; return NaN };
  expect = 'function () { var NaN = 0 / 0; return NaN; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': decompile NaN as 0/0');

  exitFunc ('test');
}
