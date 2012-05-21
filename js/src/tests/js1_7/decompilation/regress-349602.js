// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349602;
var summary = 'decompilation of let with e4x literal';
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

  expect = 'function () {\n    (let (a = 3) <x/>);\n}';
  try
  {
    var f = eval('(function () { let (a = 3) <x/>; })');
    actual = f + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
