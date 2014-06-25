/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 15 Feb 2001
 *
 * SUMMARY: calling obj.eval(str)
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=68498
 * See http://bugzilla.mozilla.org/showattachment.cgi?attach_id=25251
 *
 * Brendan:
 *
 * "Backward compatibility: support calling obj.eval(str), which evaluates
 *   str using obj as the scope chain and variable object."
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 68498;
var summary = 'Testing calling obj.eval(str)';
var statprefix = '; currently at expect[';
var statsuffix = '] within test -';
var actual = [ ];
var expect = [ ];


// Capture a reference to the global object -
var self = this;

// This function is the heart of the test -
function f(s) {self.eval(s); return y;}


// Set the actual-results array -
actual[0] = f('var y = 43');
actual[1] = 'y' in self && y;
actual[2] = delete y;
actual[3] = 'y' in self;

// Set the expected-results array -
expect[0] = 43;
expect[1] = 43;
expect[2] = true;
expect[3] = false;


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
