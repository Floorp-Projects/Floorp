/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349648;
var summary = 'Extra "[" in decomilation of nested array comprehensions';
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

  f = function(){ [[0 for (x in [])] for (y in []) ]; };
  expect = 'function () {\n    [[0 for (x in [])] for (y in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':1');

  f = function(){ [[0 for (x in [])] for (y in []) ]; };
  expect = 'function () {\n    [[0 for (x in [])] for (y in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':2');

  f = function(){ [[0 for (x in [])] for (yyyyyyyyyyy in []) ]; }
  expect = 'function () {\n    [[0 for (x in [])] for (yyyyyyyyyyy in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':3');

  f = function(){ [0 for (x in [])]; }
  expect = 'function () {\n    [0 for (x in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':4');

  f = function(){ [[[0 for (x in [])] for (yyyyyyyyyyy in []) ]
                   for (zzz in [])]; }
  expect = 'function () {\n    [[[0 for (x in [])] for (yyyyyyyyyyy in [])]' +
    ' for (zzz in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':5');

  f = function(){ [[0 for (x in [])] for (y in []) ]; }
  expect = 'function () {\n    [[0 for (x in [])] for (y in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':6');

  f = function(){ [[11 for (x in [])] for (y in []) ]; }
  expect = 'function () {\n    [[11 for (x in [])] for (y in [])];\n}';
  actual = f + '';
  reportCompare(expect, actual, summary + ':7');

  exitFunc ('test');
}
