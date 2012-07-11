/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469625;
var summary = 'TM: Array prototype and expression closures';
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
 
  expect = 'TypeError: [].__proto__ is not a function';

  jit(true);

  Array.prototype.__proto__ = function () 3; 

  try
  {
    [].__proto__();
  }
  catch(ex)
  {
    print(actual = ex + '');
  }
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
