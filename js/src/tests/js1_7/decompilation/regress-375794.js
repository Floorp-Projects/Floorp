/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375794;
var summary = 'Decompilation of array comprehension with catch guard';
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
 
  var f = function() { try { } catch(a if [b for each (c in [])]) { } } ;
  expect = 'function() { try { } catch(a if [b for each (c in [])]) { } }';
  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
