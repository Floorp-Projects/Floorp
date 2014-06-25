/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 274152;
var summary = 'Do not ignore unicode format-control characters';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'SyntaxError: illegal character';

  var formatcontrolchars = ['\u200E',
                            '\u0600', 
                            '\u0601', 
                            '\u0602', 
                            '\u0603', 
                            '\u06DD', 
                            '\u070F'];

  for (var i = 0; i < formatcontrolchars.length; i++)
  {
    try
    {
      eval("hi" + formatcontrolchars[i] + "there = 'howdie';");
    }
    catch(ex)
    {
      actual = ex + '';
    }

    reportCompare(expect, actual, summary + ': ' + i);
  }

  exitFunc ('test');
}
