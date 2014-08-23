/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 365869;
var summary = 'strict warning for object literal with duplicate propery names';
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

  if (!options().match(/strict/))
  {
    options('strict');
  }
  if (!options().match(/werror/))
  {
    options('werror');
  }

  print('test crash from bug 371292 Comment 9');

  try
  {
    expect = "TypeError: can't redefine non-configurable property '5'";
    "012345".__defineSetter__(5, function(){});
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);


  exitFunc ('test');
}
