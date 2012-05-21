// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 373678;
var summary = 'Missing quotes around string in decompilation, with for..in and do..while ';
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
 
  var f = function() { do {for(a.b in []) { } } while("c\\d"); };
  expect = 'function() { do {for(a.b in []) { } } while("c\\\\d"); }';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
