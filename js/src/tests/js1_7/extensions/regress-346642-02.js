/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346642;
var summary = 'decompilation of destructuring assignment';
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

  f = function f() { [z] = 3 }
  expect = 'function f() { [z] = 3; }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = actual = 'No Crash';
  print(uneval(f));
  reportCompare(expect, actual, 'print(uneval(f))');
  exitFunc ('test');
}
