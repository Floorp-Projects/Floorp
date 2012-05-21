/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352015;
var summary = 'decompilation of yield expressions with parens';
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
 
  var f;

  f = function() { (yield).a }
  actual = f + '';
  expect = 'function () {\n    (yield).a;\n}';
  compareSource(expect, actual, summary);

  f = function() { 3 + (yield 4) }
  actual = f + '';
  expect = 'function () {\n    3 + (yield 4);\n}';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
