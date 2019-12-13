// |reftest| skip-if(!this.uneval)

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

// ------- Comment #117 From Gary Kwong [:nth10sd]

// The following all do not require -j.

// Assertion failure: pnu->pn_lexdef == dn, at ../jsemit.cpp:1817
// =====
  uneval(function(){arguments = ({ get y(){} }); for(var [arguments] in y ) (x);});

// =====

  reportCompare(expect, actual, summary);
}
