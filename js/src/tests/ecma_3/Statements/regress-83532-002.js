/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 01 June 2001
 *
 * SUMMARY: Testing that we don't crash on switch case -1...
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=83532
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 83532;
var summary = "Testing that we don't crash on switch case -1";
var sToEval = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // Just testing that we don't crash on these -
  sToEval += 'function f () {switch(1) {case -1:}};';
  sToEval += 'function g(){switch(1){case (-1):}};';
  sToEval += 'var h = function() {switch(1) {case -1:}};'
    sToEval += 'f();';
  sToEval += 'g();';
  sToEval += 'h();';
  eval(sToEval);

  reportCompare('No Crash', 'No Crash', '');
}
