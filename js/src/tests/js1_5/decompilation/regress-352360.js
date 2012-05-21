/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352360;
var summary = 'Decompilation of negative 0';
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
 
  var f = function() { return -0 };
  expect = 'function() { return -0; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = -Infinity;
  actual = 8 / f();
  reportCompare(expect, actual, summary + ': 8 / f()');

  expect = -Infinity;
  actual = 8 / eval('(' + f + ')')();
  reportCompare(expect, actual, summary + ': 8 / eval("" + f)()');

  exitFunc ('test');
}
