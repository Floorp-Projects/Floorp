// |reftest| skip-if(xulRuntime.OS=="Linux"&&!xulRuntime.shell&&!xulRuntime.XPCOMABI.match(/x86_64/)&&isDebugBuild) -- bug 521549
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351116;
var summary = 'formal parameter and inner function have same name';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f = function (s) { function s() { } };

  function g(s) { function s() { } }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
