/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352208;
var summary = 'Do not assert new Function("setter/*\n")';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'SyntaxError: unterminated string literal';
  try
  {
    eval('new Function("setter/*\n");');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'new Function("setter/*\n");');

  try
  {
    eval('new Function("setter/*\n*/");');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, 'new Function("setter/*\n*/");');
  exitFunc ('test');
}
