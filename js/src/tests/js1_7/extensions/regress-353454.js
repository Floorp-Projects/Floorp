/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 353454;
var summary = 'Do not assert with regexp iterator';
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
 
  actual = "no exception";
  try
  {
    expect = 'TypeError';
    var obj = {a: 5}; obj.__iterator__ = /x/g; for(x in y = let (z) obj) { }
  }
  catch(ex)
  {
    actual = ex instanceof TypeError ? 'TypeError' : "" + ex;
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
