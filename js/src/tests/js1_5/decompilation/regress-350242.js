/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350242;
var summary = 'decompilation of delete 0x11.x';
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

  var f;

  f = function () { delete 0x11.x }
  expect = 'function () {\n    delete (17).x;\n}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function () { delete (17).x }
  expect = 'function () {\n    delete (17).x;\n}';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
