/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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

// ------- Comment #119 From Gary Kwong [:nth10sd]

// The following additional testcases also do not require -j.

// =====
  function f() {
    var x;
    eval("for(let y in [false]) var x, x = 0");
  }
  f();

// Assertion failure: !JSVAL_IS_PRIMITIVE(regs.sp[-2]), at ../jsinterp.cpp:3243
// Opt crash [@ JS_GetMethodById] near null
// =====
  new Function("for(x1 in ((function (){ yield x } )())){var c, x = []} function x(){} ");

// Assertion failure: pn_used, at ../jsparse.h:401
// Opt crash [@ FindFunArgs] at null
// =====
  uneval(new Function("[(x = x) for (c in []) if ([{} for (x in [])])]"))

// Assertion failure: (uintN)i < ss->top, at ../jsopcode.cpp:2814
// =====
    function f() {
    var x;
    (function(){})();
    eval("if(x|=[]) {var x; }");
  }
  f();

// Opt crash [@ js_ValueToNumber] at 0xc3510424
// Dbg crash [@ js_ValueToNumber] at 0xdadadad8
// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
