/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 344262;
var summary = 'Variables bound by let statement/expression';
var actual = '';
var expect = 0;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f() {
    var a = [];
    for (var i = 0; i < 10; i++) {
      a[i] = let (j = i) function () { return j; };
      var b = [];
      for (var k = 0; k <= i; k++)
        b.push(a[k]());
      print(b.join());
    }
    actual = a[0]();
    print(actual);
  }
  f();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
