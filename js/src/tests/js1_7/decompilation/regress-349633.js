/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349633;
var summary = 'Decompilation of increment/decrement on let bound variable';
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

  f = function () { let (x = 3) { x-- } }
  expect = 'function () {\n    let (x = 3) {\n        x--;\n    }\n}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function () { let (x = 3) { x--; x--; } }
  expect = 'function () {\n    let (x = 3) {\n        x--;\n        x--;\n    }\n}'
    actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
