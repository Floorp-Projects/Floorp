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
    var char = formatcontrolchars[i];

    try
    {
      eval("hi" + char + "there = 'howdie';");
    }
    catch(ex)
    {
      actual = ex + '';
    }

    var hex = char.codePointAt(0).toString(16).toUpperCase().padStart(4, '0');
    reportCompare(`${expect} U+${hex}`, actual, summary + ': ' + i);
  }
}
