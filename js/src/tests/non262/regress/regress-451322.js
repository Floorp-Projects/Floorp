// |reftest| skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 451322;
var summary = 'Do not crash with OOM in LirBufWriter';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  function f() {
    for (var i = 0; i < 200000; i++) {
      var m = new Function("var k = 0; for (var j = 0; j < 5; j++) { k += j * 2 + 8 / (j+3) * k} return k;");
      m();
    }
  }
  f();


  reportCompare(expect, actual, summary);
}
