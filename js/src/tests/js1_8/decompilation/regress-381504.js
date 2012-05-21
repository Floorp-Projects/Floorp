/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 381504;
var summary = 'Decompilation of dynamic member access if some parts are local variables ';
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
 
  function _1() { return foo[bar]; }
  expect = 'function _1() { return foo[bar]; }';
  actual = _1 + '';
  compareSource(expect, actual, summary);

  function _2() { var bar; return foo[bar]; }
  expect = 'function _2() { var bar; return foo[bar]; }';
  actual = _2 + '';
  compareSource(expect, actual, summary);

  function _3() { return foo[bar.baz]; }
  expect = 'function _3() { return foo[bar.baz]; }';
  actual = _3 + '';
  compareSource(expect, actual, summary);

  function _4() { var bar; return foo[bar.baz]; }
  expect = 'function _4() { var bar; return foo[bar.baz]; }';
  actual = _4 + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
