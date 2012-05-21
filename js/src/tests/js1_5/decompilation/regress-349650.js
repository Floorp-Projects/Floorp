/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349650;
var summary = 'Number getting parens replaces last character of identifier in decompilation of array comprehension';
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

  f = function() { [5[7] for (y in window)]; }
  expect = 'function () {\n    [(5)[7] for (y in window)];\n}';
  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
