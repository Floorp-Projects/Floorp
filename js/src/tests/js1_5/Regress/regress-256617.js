/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// should fail with syntax error. won't run in browser...
var BUGNUMBER = 256617;
var summary = 'throw statement: eol should not be allowed';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'syntax error';

try
{
  eval('throw\n1;');
  actual = 'throw ignored';
}
catch(e)
{
  if (e instanceof SyntaxError)
  {
    actual = 'syntax error';
  }
  else
  {
    actual = 'no error';
  }
}
 
reportCompare(expect, actual, summary);
