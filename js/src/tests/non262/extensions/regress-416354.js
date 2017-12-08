// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 416354;
var summary = 'GC hazard due to missing SAVE_SP_AND_PC';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f(a, b, c)
  {
    return (-a) * ((-b) * (-c));
  }

  if (typeof gczeal == 'function')
  {
    expect = f(1.5, 1.25, 1.125);
    gczeal(2);
    actual = f(1.5, 1.25, 1.125);
  }
  else
  {
    expect = actual = 'Test requires gczeal, skipped.';
  }

  if (typeof gczeal == 'function')
  {
    gczeal(0);
  }

  reportCompare(expect, actual, summary);
}
