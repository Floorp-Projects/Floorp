/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350268;
var summary = 'new Function with unbalanced braces';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var f;

  try
  {
    expect = 'SyntaxError';
    actual = 'No Error';
    f = new Function("}");
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ": }");

  try
  {
    expect = 'SyntaxError';
    actual = 'No Error';
    f = new Function("}}}}}");
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ": }}}}}");

  try
  {
    expect = 'SyntaxError';
    actual = 'No Error';
    f = new Function("alert(6); } alert(5);");
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ": alert(6); } alert(5);");

  try
  {
    expect = 'SyntaxError';
    actual = 'No Error';
    f = new Function("} {");
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ": } {");
}
