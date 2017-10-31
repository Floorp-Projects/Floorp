/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 344711;
var summary = 'Do not crash compiling when peeking over a newline';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof window == 'undefined')
  {
    // exclude from browser as the crash only occurs in shell
    // and attempting to trap the error prevents the crash.
    var a1 = {abc2 : 1, abc3 : 3};
    var j = eval('a1\\\n.abc2;');
  }

  reportCompare(expect, actual, summary);
}
