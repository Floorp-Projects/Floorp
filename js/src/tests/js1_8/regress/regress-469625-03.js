/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469625;
var summary = 'Do not assert: script->objectsOffset != 0';
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
 
  function f(x) {
    var [a, b, [c0, c1]] = [x, x, x];
  }

  var ITERATOR = JS_HAS_SYMBOLS ? "Symbol.iterator" : "'@@iterator'";
  expect = `TypeError: (intermediate value)[${ITERATOR}](...).next(...).value is null`;
  actual = 'No Error';
  try
  {
    f(null);
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
  
  exitFunc ('test');
}
