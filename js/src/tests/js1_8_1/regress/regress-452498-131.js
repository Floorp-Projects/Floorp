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

// ------- Comment #131 From Gary Kwong [:nth10sd]

// Does not require -j:
// =====
  try
  {
    eval('((__defineGetter__, function (x) { function x(){} }) for');
  }
  catch(ex)
  {
  }
// Assertion failure: pn->pn_cookie == FREE_UPVAR_COOKIE, at ../jsparse.cpp:5698
// =====
  try
  {
    eval('( ""  ? 1.3 : x); *::*; x::x;');
  }
  catch(ex)
  {
  }
// Assertion failure: pn != dn->dn_uses, at ../jsparse.cpp:1171
// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
