/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 356083;
var summary = 'decompilation for (function () {return {set this(v) {}};}) ';
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
 
  var f = function() { return { set this(v) { } }; } ;
  expect = 'function() { return { set this(v) { } }; }';
  actual = f + '';

  compareSource(expect, actual, summary);

  expect = "({ set ''(v) {} })";
  actual = uneval({ set ''(v) {} });
  compareSource(expect, actual, expect);
  exitFunc ('test');
}
