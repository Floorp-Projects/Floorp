/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 369696;
var summary = 'Do not assert: map->depth > 0" in js_LeaveSharpObject';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var x = [[[ { toSource: function() { gc();  }}]]];

  var a = [];
  a[0] = a;
  a.toSource = a.toString;
  Array.prototype.toSource.call(a);

//cx->sharpObjectMap.depth == -2

  (function() {
    var tmp = [];
    for (var i = 0; i != 30*1000; ++i) {
      var tmp2 = [];
      tmp.push(tmp2);
      tmp2.toSource();
    }
  })();

  gc();
  x.toSource();

  reportCompare(expect, actual, summary);
}
