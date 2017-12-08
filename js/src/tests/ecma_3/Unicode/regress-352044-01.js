/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352044;
var summary = 'issues with Unicode escape sequences in JavaScript source code';
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

  try
  {
    var i = 1;
    eval('i \\u002b= 1'); 
    print(i);
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);
}
