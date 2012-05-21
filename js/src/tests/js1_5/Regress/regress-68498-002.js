/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 15 Feb 2001
 *
 * SUMMARY: create a Deletable local variable using eval
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=68498
 * See http://bugzilla.mozilla.org/showattachment.cgi?attach_id=25251
 *
 * Brendan:
 *
 * "Demonstrate the creation of a Deletable local variable using eval"
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 68498;
var summary = 'Creating a Deletable local variable using eval';
var statprefix = '; currently at expect[';
var statsuffix = '] within test -';
var actual = [ ];
var expect = [ ];


// Capture a reference to the global object -
var self = this;

// This function is the heart of the test -
function f(s) {eval(s); actual[0]=y; return delete y;}


// Set the actual-results array. The next line will set actual[0] and actual[1] in one shot
actual[1] = f('var y = 42');
actual[2] = 'y' in self && y;

// Set the expected-results array -
expect[0] = 42;
expect[1] = true;
expect[2] = false;


//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  for (var i in expect)
  {
    reportCompare(expect[i], actual[i], getStatus(i));
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return (summary  +  statprefix  +  i  +  statsuffix);
}
