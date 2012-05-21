/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 416705;
var summary = 'throw from xml filter crashes';
var actual = 'No Crash';
var expect = 6;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var g;

  function f()
  {
    try {
      <><a/><b/></>.(let (a=1, b = 2, c = 3)
                     (g = function() { a += b+c; return a; }, xxx));
    } catch (e) {
    }
  }

  f();
  var actual = g();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
