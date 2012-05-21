/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // Just testing that we don't crash on these -
  function f () {switch(1) {case -1:}}
  function g(){switch(1){case (-1):}}
  var h = function() {switch(1) {case -1:}}
  f();
  g();
  h();
  reportCompare('No Crash', 'No Crash', '');

  exitFunc ('test');
}
