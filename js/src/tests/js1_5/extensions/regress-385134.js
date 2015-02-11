/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 385134;
var summary = 'Do not crash with setter, watch, uneval';
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
 
  if (typeof this.__defineSetter__ != 'undefined' && 
      typeof this.watch != 'undefined' &&
      typeof uneval != 'undefined')
  {
    try {
      this.__defineSetter__(0, function(){});
    } catch (exc) {
      // In the browser, this fails. Ignore the error.
    }
    this.watch(0, function(){});
    uneval(this);
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
