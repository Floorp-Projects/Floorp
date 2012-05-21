/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352079;
var summary = 'decompilation of various operators';
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

  f = function() { f(let (y = 3) 4)++; }
  expect = 'function() { f(let (y = 3) 4)++; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { f(4)++; } 
  expect = 'function() { f(4)++; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { ++f(4); }
  expect = 'function() { ++f(4); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { delete(p(3)) } 
  expect = 'function() { p(3), true; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
