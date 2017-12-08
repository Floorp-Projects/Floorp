/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
// ------- Comment #102 From Gary Kwong [:nth10sd]

// =====

  (function(){function x(){} function x() { return y; }})();

// Assertion failure: JOF_OPTYPE(op) == JOF_ATOM, at ../jsemit.cpp:1710

// =====
  function f() {
    "" + (function(){
        for( ; [function(){}] ; x = 0)
          with({x: ""}) {
            const x = []
           }});
  }
  f();

// Assertion failure: ss->top - saveTop <= 1U, at ../jsopcode.cpp:2156

// =====

  try
  {
    function f() {
      var x;
      eval("const x = [];");
    }
    f();
  }
  catch(ex)
  {
  }
// Assertion failure: regs.sp == StackBase(fp), at ../jsinterp.cpp:2984

// Assertion failure: cg->staticLevel >= level, at ../jsemit.cpp:2014
// Crash [@ BindNameToSlot] in opt without -j

// =====

  reportCompare(expect, actual, summary);
}



