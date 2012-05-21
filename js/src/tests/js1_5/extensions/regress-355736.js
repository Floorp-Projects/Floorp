/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355736;
var summary = 'Decompilation of "[reserved]" has extra quotes';
var actual = '';
var expect = '';
var f;

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  f = function() { [implements] = q; };
  expect = 'function() { [implements] = q; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 1');

  f = function() { return { get implements() { } } };
  expect = 'function() { return { get implements() { } }; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 2');

  f = function() { [goto] = a };
  expect = 'function() { [goto] = a; }';
  actual = f + '';
  compareSource(expect, actual, summary + ': 3');

  exitFunc ('test');
}
