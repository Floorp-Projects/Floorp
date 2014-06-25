/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 385729;
var summary = 'uneval(eval(expression closure))';
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
 
  if (typeof eval != 'undefined' && typeof uneval != 'undefined')
  {
    expect = '(function f () /x/g)';
    try
    {
      // mozilla 1.9
      actual = uneval(eval(expect));
    }
    catch(ex)
    {
      // mozilla 1.8
      expect = 'SyntaxError: missing { before function body';
      actual = ex + '';
    }
    compareSource(expect, actual, summary);

    expect = '({get f () /x/g})';
    try
    {
      // mozilla 1.9
      actual = uneval(eval("({get f () /x/g})"));
    }
    catch(ex)
    {
      // mozilla 1.8
      expect = 'SyntaxError: missing { before function body';
      actual = ex + '';
    }
    compareSource(expect, actual, summary);
  }

  exitFunc ('test');
}
