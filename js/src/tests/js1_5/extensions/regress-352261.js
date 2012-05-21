/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352261;
var summary = 'Decompilation should preserve right associativity';
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
 
  var g, h;

  g = function(a,b,c) { return a - (b + c) }
  expect = 'function(a,b,c) { return a - (b + c); }';
  actual = g + '';
  compareSource(expect, actual, summary);

  h = eval(uneval(g));
  expect = g(1, 10, 100);
  actual = h(1, 10, 100);
  reportCompare(expect, actual, summary);

  var p, q;

  p = function (a,b,c) { return a + (b - c) }
  expect = 'function (a,b,c) { return a + (b - c);}';
  actual = p + '';
  compareSource(expect, actual, summary);

  q = eval(uneval(p));
  expect = p(3, "4", "5");
  actual = q(3, "4", "5");
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
