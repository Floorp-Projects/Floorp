/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352624;
var summary = 'Do not crash with |let (map)| in WAY_TOO_MUCH_GC builds';
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
  printStatus('This bug can only be verified with a WAY_TOO_MUCH_GC build');
 
  let (x = [].map(function () {})) { x; }
  reportCompare(expect, actual, summary + ': 1');

  let (x = [].map(function () {})) 3
    reportCompare(expect, actual, summary + ': 2');

  var g = function() {};
  (function() { let x = [].map(function () {}); g(x); })()
    reportCompare(expect, actual, summary + ': 3');

  for(let x in [1].map(function () { })) { }
  reportCompare(expect, actual, summary + ': 4');

  exitFunc ('test');
}
