/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 398485;
var summary = 'Date.prototype.toLocaleString should not clamp year';
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

  var d;
  var y;
  var l;
  var maxms = 8640000000000000;

  d = new Date(-maxms );
  y = d.getFullYear();
  l = d.toLocaleString();
  print(l);

  actual = y;
  expect = -271821;
  reportCompare(expect, actual, summary + ': check year');

  actual = l.match(new RegExp(y)) + '';
  expect = y + '';
  reportCompare(expect, actual, summary + ': check toLocaleString');

  d = new Date(maxms );
  y = d.getFullYear();
  l = d.toLocaleString();
  print(l);

  actual = y;
  expect = 275760;
  reportCompare(expect, actual, summary + ': check year');

  actual = l.match(new RegExp(y)) + '';
  expect = y + '';
  reportCompare(expect, actual, summary + ': check toLocaleString');

  exitFunc ('test');
}
