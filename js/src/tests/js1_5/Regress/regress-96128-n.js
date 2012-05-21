/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 29 Aug 2001
 *
 * SUMMARY: Negative test that JS infinite recursion protection works.
 * We expect the code here to fail (i.e. exit code 3), but NOT crash.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=96128
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 96128;
var summary = 'Testing that JS infinite recursion protection works';


function objRecurse()
{
  /*
   * jband:
   *
   * Causes a stack overflow crash in debug builds of both the browser
   * and the shell. In the release builds this is safely caught by the
   * "too much recursion" mechanism. If I remove the 'new' from the code below
   * this is safely caught in both debug and release builds. The 'new' causes a
   * lookup for the Constructor name and seems to (at least) double the number
   * of items on the C stack for the given interpLevel depth.
   */
  return new objRecurse();
}



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // we expect this to fail (exit code 3), but NOT crash. -
  var obj = new objRecurse();

  exitFunc ('test');
}
