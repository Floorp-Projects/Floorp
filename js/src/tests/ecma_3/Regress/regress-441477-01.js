/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var gTestfile = 'regress-441477-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 441477-01;
var summary = '';
var actual = 'No Exception';
var expect = 'No Exception';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    for (i = 0; i < 5;) 
    { 
      if (i > 5) 
        throw "bad"; 
      i++; 
      continue; 
    }
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
