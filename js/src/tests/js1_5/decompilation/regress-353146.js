/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 353146;
var summary = 'Decompilation of new expressions revisited';
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

  f = function () { return new (p(2))[1]; };
  expect = 'function() { return new (p(2)[1]); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function () { return new p(2)[1]; };
  expect = 'function () { return (new p(2))[1]; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
