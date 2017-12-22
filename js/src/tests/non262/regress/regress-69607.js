/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 21 Feb 2001
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=69607
 *
 * SUMMARY:  testing that we don't crash on trivial JavaScript
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 69607;
var summary = "Testing that we don't crash on trivial JavaScript";
var var1;
var var2;
var var3;

printBugNumber(BUGNUMBER);
printStatus (summary);

/*
 * The crash this bug reported was caused by precisely these lines
 * placed in top-level code (i.e. not wrapped inside a function) -
 */
if(false)
{
  var1 = 0;
}
else
{
  var2 = 0;
}

if(false)
{
  var3 = 0;
}

reportCompare('No Crash', 'No Crash', '');

