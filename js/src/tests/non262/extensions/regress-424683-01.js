/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 424683;
var summary = 'Throw too much recursion instead of script stack space quota';
var actual = 'No Error';
var expect = 'InternalError: too much recursion';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f() { f(); }

  try
  {
    f();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
}
