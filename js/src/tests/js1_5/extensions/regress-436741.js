/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 436741;
var summary = 'Do not assert: OBJ_IS_NATIVE(obj)';
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
 
  if (typeof window == 'undefined')
  {
    print('This test is only meaningful in the browser.');
  }
  else
  {
    try { window.__proto__.__proto__ = [{}]; } catch (e) {}
    for (var j in window);
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
