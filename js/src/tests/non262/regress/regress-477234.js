// |reftest| slow
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 477234;
var summary = 'Do not assert: v != JSVAL_ERROR_COOKIE';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

 
  for (iters = 0; iters < 11500; ++iters) {
    for (let x of ['', '', '']){}
    eval("Object.defineProperty(__proto__, 'x', " +
         "{" +
         "  enumerable: true, configurable: true," +
         "  get: function(){}" +
         "});");
    var c = toString;
    delete toString;
    toString = c;
  }


  delete __proto__.x;

  reportCompare(expect, actual, summary);
}
