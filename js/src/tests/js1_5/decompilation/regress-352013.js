/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352013;
var summary = 'decompilation of new parenthetic expressions';
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

  var f, g, h;
  var x = Function;
  var z = 'actual += arguments[0];';
  var w = 42;

  f = function() { new (x(z))(w) }
  expect = 'function() { new (x(z))(w); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  g = function () { new x(z)(w); }
  expect = 'function () { (new x(z))(w); }';
  actual = g + '';
  compareSource(expect, actual, summary);

  expect = '4242';
  actual = '';
  f();
  g();
  reportCompare(expect, actual, summary);

  h = function () { new (x(y)(z));  }
  expect = 'function () { new (x(y)(z)); }';
  actual = h + '';
  compareSource(expect, actual, summary);

  h = function () { new (x(y).z);   }
  expect = 'function () { new (x(y).z); }';
  actual = h + '';
  compareSource(expect, actual, summary);

  h = function () { new x(y).z;   }
  expect = 'function () { (new x(y)).z; }';
  actual = h + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
