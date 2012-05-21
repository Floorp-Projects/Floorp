/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 422286;
var summary = 'Array slice when array\'s length is assigned';
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
 
  Array(10000).slice(1);
  a = Array(1); 
  a.length = 10000; 
  a.slice(1);
  a = Array(1); 
  a.length = 10000; 
  a.slice(-1);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
