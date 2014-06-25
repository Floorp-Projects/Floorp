/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352797;
var summary = 'Do not assert: OBJ_GET_CLASS(cx, obj) == &js_BlockClass';
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
 
  if (typeof Script == 'undefined')
  {
    print('Test skipped. Script not defined.');
  }
  else
  {
    (function(){let x = 'fafafa'.replace(/a/g, new Script(''))})();
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
