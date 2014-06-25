/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 410852;
var summary = 'Valgrind errors in jsemit.cpp';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  print('Note: You must run this test under valgrind to determine if it passes');

  try
  {
    eval('function(){if(t)');
  }
  catch(ex)
  {
    assertEq(ex instanceof SyntaxError, true, "wrong error: " + ex);
  }

  reportCompare(true, true, summary);

  exitFunc ('test');
}
