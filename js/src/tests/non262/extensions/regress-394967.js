/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 394967;
var summary = 'Do not assert: !vp[1].isPrimitive()';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof evalcx == 'undefined')
  {
    print('Skipping. This test requires evalcx.');
  }
  else
  {
    var sandbox = evalcx(""); 
    try
    {
      evalcx("(1)()", sandbox);
    }
    catch(ex)
    {
    }
  }

  reportCompare(expect, actual, summary);
}
