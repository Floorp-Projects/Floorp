/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';

//-------  Comment #196  From  Gary Kwong

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// Assertion failure: lexdep->isLet(), at ../jsparse.cpp:1900

  (function (){
    var x;
    eval("var x; (function () { return x; })");
  }
    )();

  reportCompare(expect, actual, summary + ': 2');
}
