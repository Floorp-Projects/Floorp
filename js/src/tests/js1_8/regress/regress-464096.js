/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 464096;
var summary = 'TM: Do not assert: tm->recoveryDoublePoolPtr > tm->recoveryDoublePool';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  for (let f in [1,1]);
  Object.prototype.__defineGetter__('x', function() { return gc(); });
  (function() { for (let j of [1,1,1,1,1]) { var y = .2; } })();


  reportCompare(expect, actual, summary);
}
