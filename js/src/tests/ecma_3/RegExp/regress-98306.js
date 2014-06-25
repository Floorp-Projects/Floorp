/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 04 September 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 98306
 * "JS parser crashes in ParseAtom for script using Regexp()"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=98306
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 98306;
var summary = "Testing that we don't crash on this code -";
var cnUBOUND = 10;
var re;
var s;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  s = '"Hello".match(/[/]/)';
  tryThis(s);

  s = 're = /[/';
  tryThis(s);

  s = 're = /[/]/';
  tryThis(s);

  s = 're = /[//]/';
  tryThis(s);

  reportCompare('No Crash', 'No Crash', '');
  exitFunc ('test');
}


// Try to provoke a crash -
function tryThis(sCode)
{
  // sometimes more than one attempt is necessary -
  for (var i=0; i<cnUBOUND; i++)
  {
    try
    {
      eval(sCode);
    }
    catch(e)
    {
      // do nothing; keep going -
    }
  }
}
