/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 14 October 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 104584
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=104584
 *
 * Testing that we don't crash on this code. The idea is to
 * call F,G WITHOUT providing an argument. This caused a crash
 * on the second call to obj.toString() or print(obj) below -
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 104584;
var summary = "Testing that we don't crash on this code -";

printBugNumber(BUGNUMBER);
printStatus (summary);

F();
G();

reportCompare('No Crash', 'No Crash', '');

function F(obj)
{
  if(!obj)
    obj = {};
  obj.toString();
  gc();
  obj.toString();
}


function G(obj)
{
  if(!obj)
    obj = {};
  print(obj);
  gc();
  print(obj);
}
