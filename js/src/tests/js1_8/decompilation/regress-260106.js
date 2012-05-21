/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 260106;
var summary = 'Elisions in array literals should not create properties';
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
 
  var g = (function f(a,b,c,d)[(a,b),(c,d)]);

  expect = 'function f(a,b,c,d)[(a,b),(c,d)]';
  actual = g + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
