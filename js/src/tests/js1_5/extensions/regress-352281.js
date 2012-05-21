// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352281;
var summary = 'decompilation of |while| and function declaration';
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
 
  var f, g;
  f = function() { { while(0) function t() {  } } }
  expect = 'function() { while(0) { function t() {  } }}';
  actual = f + '';
  compareSource(expect, actual, summary);

  g = eval(uneval(actual));
  actual = g + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
