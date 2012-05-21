/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352011;
var summary = 'decompilation of statements that begin with object literals';
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

  f = function() { ({}.y = i); }
  expect = 'function() { ({}.y = i); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { let(x) ({t:x}) }
  expect = 'function() { let(x) ({t:x}); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { (let(x) {y: z}) }
  expect = 'function() { let(x) ({y: z}); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
