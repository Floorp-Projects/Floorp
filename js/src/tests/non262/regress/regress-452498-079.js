/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

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

// ------- Comment #79 From Jason Orendorff

  x; var x; function x() { return 0; }

// Assertion failure: !(pn->pn_dflags & flag), at ../jsparse.h:635

  reportCompare(expect, actual, summary);
}
