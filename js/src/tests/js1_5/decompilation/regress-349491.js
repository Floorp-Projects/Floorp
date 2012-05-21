/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349491;
var summary = 'Incorrect decompilation due to assign to const';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var a;

  a = function () { const z = 3; g = 7; g = z += 1; return g };
  expect = a();
  actual = (eval('(' + a + ')'))();
  reportCompare(expect, actual, summary);

  a = function () { const z = 3; return z += 2 };
  expect = a();
  actual = (eval('(' + a + ')'))();
  reportCompare(expect, actual, summary);

  expect = 'function () {\n    const z = 3;\n}';
  a  = function () { const z = 3; z = 4; }
  actual = a.toString();
  compareSource(expect, actual, summary);

  expect = 'function () {\n    const z = 3;\n}';
  a  = function () { const z = 3; 4; }
  actual = a.toString();
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
