/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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


  Array.prototype.__proto__ = function () 3; 

  try
  {
    [].__proto__();
  }
  catch(ex)
  {
    print(actual = ex + '');
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
