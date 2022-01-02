/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 618572;
var summary = 'Do not assert when ungetting a Unicode char sequence';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'SyntaxError';

  try
  {
    eval("var a\\0021 = 3;");
  }
  catch(ex)
  {
    actual = ex.constructor.name;
  }

  reportCompare(expect, actual, summary);
}
