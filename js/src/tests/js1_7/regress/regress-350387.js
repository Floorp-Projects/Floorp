// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350387;
var summary = 'Var declaration and let with same name';
var actual = '';
var expect = '';

expect = undefined + '';
actual = '';
let (x = 2)
{
  var x;
}
actual = x + '';
reportCompare(expect, actual, summary + ': 1');

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = undefined + '';
  actual = '';
  (function () { let (x = 2) { var x; } actual = x + ''; })(); 
  reportCompare(expect, actual, summary + ': 2');

  exitFunc ('test');
}
