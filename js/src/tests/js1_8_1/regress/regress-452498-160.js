/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';

//-------  Comment #160  From  Gary Kwong

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// crash [@ js_Interpret]
  (eval("(function(){ this.watch(\"x\", function () { new function ()y } ); const y });"))();
  x = NaN;
  reportCompare(expect, actual, summary + ': 2');

// Assertion failure: JOF_OPTYPE(op) == JOF_ATOM, at ../jsemit.cpp:5916
  ({ set z(v){},  set y(v)--x, set w(v)--w });
  reportCompare(expect, actual, summary + ': 3');

  exitFunc ('test');
}
