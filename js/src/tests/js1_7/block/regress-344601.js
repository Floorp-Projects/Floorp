/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 344601;
var summary = 'Function.prototype.toString should preserve let statements';
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
 
  function f() { var i = 1; let (i = 2) {} return i; }
  var fString = f.toString();

  expect = 'function f() {\n    var i = 1;\n    let (i = 2) {\n    }\n    return i;\n}';
  actual = fString;

  reportCompare(expect, actual, summary);

  expect = f();
  eval(f.toString().replace(/function f/, 'function g'));
  actual = g();
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
