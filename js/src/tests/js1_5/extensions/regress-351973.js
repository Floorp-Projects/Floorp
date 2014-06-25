/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351973;
var summary = 'GC hazard with unrooted ids in Object.toSource';
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
 
  function removeAllProperties(o)
  {
    for (var prop in o)
      delete o[prop];
    for (var i = 0; i != 50*1000; ++i) {
      var tmp = Math.sqrt(i+0.2);
      tmp = 0;
    }
    if (typeof gc == "function")
      gc();
  }

  function run_test()
  {

    var o = {};
    o.first = { toSource: function() { removeAllProperties(o); } };
    for (var i = 0; i != 10; ++i) {
      o[Math.sqrt(i + 0.1)] = 1;
    }
    return o.toSource();
  }

  print(run_test());

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
