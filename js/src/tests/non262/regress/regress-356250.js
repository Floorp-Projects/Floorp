/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 356250;
var summary = 'Do not assert: !fp->fun || !(fp->fun->flags & JSFUN_HEAVYWEIGHT) || fp->callobj';
var actual = 'No Crash';
var expect = 'No Crash';

(function() { eval("(function() { })"); })();
reportCompare(expect, actual, summary + ': nested 0');

//-----------------------------------------------------------------------------
test1();
test2();
//-----------------------------------------------------------------------------

function test1()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  (function() { eval("(function() { })"); })();

  reportCompare(expect, actual, summary + ': nested 1');
}

function test2()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  (function () {(function() { eval("(function() { })"); })();})();

  reportCompare(expect, actual, summary + ': nested 2');
}
