/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361617;
var summary = 'Do not crash with getter, watch and gc';
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
 
  (function() {
    Object.defineProperty(this, "x", { get: function(){}, enumerable: true, configurable: true });
  })();
  this.watch('x', print);
  Object.defineProperty(this, "x", { get: function(){}, enumerable: true, configurable: true });
  gc();
  this.unwatch('x');
  x;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
