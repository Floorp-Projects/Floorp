/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479487;
var summary = 'js_Array_dense_setelem can call arbitrary JS code';
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


  Array.prototype[1] = 2;

  Array.prototype.__defineSetter__(32, function() { print("Hello from arbitrary JS");});
  Array.prototype.__defineGetter__(32, function() { return 11; });

  function f()
  {
    var a = [];
    for (var i = 0; i != 10; ++i) {
      a[1 << i] = 9999;
    }
    return a;
  }

  f();


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
