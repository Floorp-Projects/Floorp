/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var gTestfile = 'regress-459085.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 459085;
var summary = 'Do not assert with JIT: Should not move data from GPR to XMM';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  
  jit(true);

  var m = new Number(3);
  function foo() { for (var i=0; i<20;i++) m.toString(); } 
  foo();

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
