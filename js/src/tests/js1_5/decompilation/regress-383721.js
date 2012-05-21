/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 383721;
var summary = 'decompiling Tabs';
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

  var f = function () {return "\t"};
  expect = 'function () {return "\\t";}';
  actual = f + '';
  compareSource(expect, actual, summary + ': toString');

  expect = '(' + expect + ')';
  actual = uneval(f);
  compareSource(expect, actual, summary + ': uneval');

  exitFunc ('test');
}
