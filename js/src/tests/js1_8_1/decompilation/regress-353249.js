/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 353249;
var summary = 'regression test for bug 353249';
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
 
  var f = (function () { let (x) <x/>.(1) < let (z) eval('3');
	     for (x in this) {} });

  expect = 'function () { let (x) <x/>.((1)) < (let (z) eval("3")); ' +
    'for (x in this) {} }';
  actual = f + '';
  compareSource(expect, actual, summary);

  // do not crash()
  f();
  exitFunc ('test');
}
