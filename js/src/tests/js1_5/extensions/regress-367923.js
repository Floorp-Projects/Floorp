/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 367923;
var summary = 'strict warning for variable redeclares argument';
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

  print('This test will fail in Gecko prior to 1.9');

  if (!options().match(/strict/))
  {
    options('strict');
  }
  if (!options().match(/werror/))
  {
    options('werror');
  }

  try
  {
    expect = 'TypeError: variable v redeclares argument';
    // syntax warning, need to eval to catch
    eval("(function (v) { var v; })(1)");
    actual = 'No warning';
  }
  catch(ex)
  {
    actual = ex + '';
  }
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
