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

// ------- Comment #139 From Gary Kwong [:nth10sd]

// Does not require -j:
// ===
  try
  {
    (function(){var x = x (x() for each (x in []))})();
  }
  catch(ex)
  {
  }
// Assertion failure: (fun->u.i.script)->upvarsOffset != 0, at ../jsfun.cpp:1541
// Opt crash near null [@ js_NewFlatClosure]
// ===

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
