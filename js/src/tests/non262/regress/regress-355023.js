/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355023;
var summary = 'destructuring assignment optimization';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  Array.prototype[0] = 1024;

  expect = (function(){ var a=[],[x]=a; return x; })();
  actual = (function(){ var [x]=[]; return x; })();

  reportCompare(expect, actual, summary);
}
