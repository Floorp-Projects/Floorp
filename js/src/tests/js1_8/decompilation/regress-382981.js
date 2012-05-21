/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 382981;
var summary = 'decompilation of expcio body with delete ++x';
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

  expect = 'function () (++x, true)';
  var f = (function () delete ++x);
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = 'function () (*, true)';
  var f = (function () delete *) ;
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
