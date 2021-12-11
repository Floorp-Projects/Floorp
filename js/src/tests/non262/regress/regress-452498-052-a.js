// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

// ------- Comment #52 From Jason Orendorff


// Assertion failure: pn_arity == PN_FUNC || pn_arity == PN_NAME, at ../jsparse.h:444
// Here the function node has been morphed into a JSOP_TRUE node, but we're in
// FindFunArgs poking it anyway.
  if (typeof timeout == 'function')
  {
    expectExitCode(6);
    timeout(3);
    while(function(){});
  }

  reportCompare(expect, actual, summary);
}
