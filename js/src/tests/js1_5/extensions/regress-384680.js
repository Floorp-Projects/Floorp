/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 384680;
var summary = 'Round-trip change in decompilation with paren useless expression';
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

  expect = 'function() {}';

  var f = (function() { (3); });
  actual = f + '';
  compareSource(expect, actual, summary + ': f');

  var g = eval('(' + f + ')');
  actual = g + '';
  compareSource(expect, actual, summary + ': g');

  exitFunc ('test');
}
