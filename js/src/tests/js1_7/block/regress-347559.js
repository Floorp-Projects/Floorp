/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 347559;
var summary = 'Let declarations should not warn that function does not ' +
  'return a value';
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

  actual = 'No Warning';
  expect = 'No Warning';

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
    eval('function f() { let a = 2; return a; }');
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': 1');

  actual = 'No Warning';
  expect = 'TypeError: function f does not always return a value';

  try
  {
    eval('function f() { if (asdf) return 3; let x; }');
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': 2');

  actual = 'No Warning';
  expect = 'No Warning';

  try
  {
    eval('function f() { if (asdf) return 3; let (x) { return 4; } }');
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': 3');

  exitFunc ('test');
}
