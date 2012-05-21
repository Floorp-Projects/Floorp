/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 421806;
var summary = 'Do not assert: *pcstack[pcdepth - 1] == JSOP_ENTERBLOCK';
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
 
  try 
  { 
    let([] = c) 1; 
  } 
  catch(ex) 
  { 
    try
    {
      this.a.b; 
    }
    catch(ex2)
    {
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
