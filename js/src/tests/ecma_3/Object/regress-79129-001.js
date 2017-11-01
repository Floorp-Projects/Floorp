/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 06 May 2001
 *
 * SUMMARY: Regression test: we shouldn't crash on this code
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=79129
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 79129;
var summary = "Regression test: we shouldn't crash on this code";

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  tryThis();
  reportCompare('No Crash', 'No Crash', 'Should not crash');
}


function tryThis()
{
  obj={};
  obj.a = obj.b = obj.c = 1;
  delete obj.a;
  delete obj.b;
  delete obj.c;
  obj.d = obj.e = 1;
  obj.a=1;
  obj.b=1;
  obj.c=1;
  obj.d=1;
  obj.e=1;
}
