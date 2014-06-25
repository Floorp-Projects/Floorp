/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 480244;
var summary = 'Do not assert: isInt32(*p)';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  function outer() {
    var v = 10.234;
    for (var i = 0; i < 0xff; ++i) {
      inner(v);
    }
  }

  var g = 0;
  var h = 0;

  function inner() {
    var v = 10;
    for (var k = 0; k < 0xff; ++k) {
      g++;
      if (g & 0xff == 0xff)
        h++;
    }
    return h;
  }

  outer();
  print("g=" + g + " h=" + h);

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
