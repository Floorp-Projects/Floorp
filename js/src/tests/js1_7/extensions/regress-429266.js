/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 429266;
var summary = 'Do not assert: nuses == 0 || *pcstack[pcdepth - 1] == JSOP_ENTERBLOCK';
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
    function f() { let (a) 1; null.x; }
    if (typeof trap == 'function')
    {
      trap(f, 0, "");
    }
    f();
  }
  catch(ex)
  {
    print(ex + '');
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
