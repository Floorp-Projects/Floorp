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

// ------- Comment #51 From Jason Orendorff

// Assertion failure: UPVAR_FRAME_SKIP(uva->vector[i]) == 0
// at ../jsopcode.cpp:2791
//
// when decompiling the eval code, which is:
//
// main:
// 00000:  10  getupvar 0
// 00003:  10  getprop "y"
// 00006:  10  popv
// 00007:  10  stop
  try
  {
    function f() { var x; eval("x.y"); }
    f();
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);
}
