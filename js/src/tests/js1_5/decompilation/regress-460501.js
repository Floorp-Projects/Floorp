/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 460501;
var summary = 'Decompilation of constant folding with && and valueOf, eval';
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

  var f = (function() { if ((1 && w.valueOf())) {} });

  expect = 'function() { if (w.valueOf()) {} }';
  actual = f + '';

  compareSource(expect, actual, summary);

  var f = (function() { if ((1 && w.eval())) {} });

  expect = 'function() { if (w.eval()) {} }';
  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
