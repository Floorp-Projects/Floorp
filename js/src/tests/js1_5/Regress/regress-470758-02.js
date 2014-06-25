/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 470758;
var summary = 'Promote evald initializer into upvar';
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
 
  expect = 5;

  (function(){var x;eval("for (x = 0; x < 5; x++);");print(actual = x);})();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
