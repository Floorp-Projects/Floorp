/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350666;
var summary = 'decompilation for (;; delete expr)';
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

  f = function () { for(;; delete y.(x)) { } }
  actual = f + '';
  expect = 'function () {\n    for (;; y.(x), true) {\n    }\n}';
  compareSource(expect, actual, summary);

  try
  {
    eval('(' + expect + ')');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  expect = 'No Error';
  reportCompare(expect, actual, summary);

  f = function () { for(;; delete (y+x)) { } }
  actual = f + '';
  expect = 'function () {\n    for (;; y + x, true) {\n    }\n}';
  compareSource(expect, actual, summary);

  try
  {
    eval('(' + expect + ')');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  expect = 'No Error';
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
