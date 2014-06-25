/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 294191;
var summary = 'Do not crash with nested function and "delete" op';
var actual = 'No Crash';
var expect = 'No Crash';

enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);
 
function f()
{
  function x()
  {
    x;
  }
}

f.z=0;

delete f.x;

f();

reportCompare(expect, actual, summary);
