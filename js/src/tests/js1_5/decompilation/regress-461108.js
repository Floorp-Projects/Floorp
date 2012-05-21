/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 461108;
var summary = 'Decompilation of for (i = 0; a = as[i]; ++i)';
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

  var f = (function() {for (i = 0; attr = attrs[i]; ++i) {} });

  expect = 'function() {for (i = 0; attr = attrs[i]; ++i) {} }';
  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
