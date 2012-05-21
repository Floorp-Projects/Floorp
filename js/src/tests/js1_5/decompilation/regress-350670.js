/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350670;
var summary = 'decompilation of (for(z() in x)';
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

  f = function() { for(z() in x) { } }
  expect = 'function () {\n    for (z() in x) {\n    }\n}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function() { for(z(12345678) in x) { } }
  expect = 'function () {\n    for (z(12345678) in x) {\n    }\n}';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
