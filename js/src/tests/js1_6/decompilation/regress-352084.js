/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352084;
var summary = 'decompilation of comma expression lists';
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

  f = function() { h = {x:5, y:(a,b)}}  ;
  expect = 'function() { h = {x:5, y:(a,b)};}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { x(a, (b,c)) };
  expect = 'function() { x(a, (b,c));}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { return [(x, y) for each (z in [])] };
  expect = 'function() { return [(x, y) for each (z in [])];}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { return [(a, b), c]; };
  expect = 'function() { return [(a, b), c]; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
