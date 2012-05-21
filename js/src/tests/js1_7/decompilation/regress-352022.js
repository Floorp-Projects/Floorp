// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352022;
var summary = 'decompilation of let, delete and parens';
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

  f = function() { g(h) = (delete let (y) 3); }
  actual = f + '';
  expect = 'function () {\n    g(h) = ((let (y) 3), true);\n}';
  compareSource(expect, actual, summary + ': 1');

  f = function () {    g(h) = ((let (y) 3), true);}
  actual = f + '';
  expect = 'function () {\n    g(h) = ((let (y) 3), true);\n}';
  compareSource(expect, actual, summary + ': 2');

  exitFunc ('test');
}
