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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #114 From Gary Kwong [:nth10sd]

  for (var x = 0; x < 3; ++x){ y = function (){} }

// glorp!
// Assertion failed: "Constantly false guard detected": 0 (../nanojit/LIR.cpp:999)
// (note, this is with -j; I don't know what the glorp! message is about.)

// =====
  function y([{x: x, y}]){}

// Assertion failure: UPVAR_FRAME_SKIP(pn->pn_cookie) == (pn->pn_defn ? cg->staticLevel : 0), at ../jsemit.cpp:3547

// =====

  try
  {
    eval("(1.3.__defineGetter__(\"\"));let (y, x) { var z = true, x = 0; }");
  }
  catch(ex)
  {
  }
// Assertion failure: ATOM_IS_STRING(atom), at ../jsinterp.cpp:5691

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
