/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352312;
var summary = 'decompilation of |new| with unary expression';
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
  f = function() { new (-2) }
  expect = 'function() { new (-2); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  var f;
  f = function (){(new x)(y)}
  expect = 'function (){(new x)(y);}';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
