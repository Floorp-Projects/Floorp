/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 06 Mar 2001
 *
 * SUMMARY: Propagate heavyweightness back up the function-nesting
 * chain. See http://bugzilla.mozilla.org/show_bug.cgi?id=71107
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 71107;
var summary = 'Propagate heavyweightness back up the function-nesting chain.';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var actual = outer()()();  //call the return of calling the return of outer()
  var expect = 5;
  reportCompare(expect, actual, summary);
}


function outer () {
  var outer_var = 5;

  function inner() {
    function way_inner() {
      return outer_var;
    }
    return way_inner;
  }
  return inner;
}
