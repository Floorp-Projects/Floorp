// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 417131;
var summary = 'stress test for cache';
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

  function f(N)
  {
    for (var i = 0; i != N; ++i) {
      var obj0 = {}, obj1 = {}, obj2 = {};
      obj1['a'+i] = 0;
      obj2['b'+i] = 0;
      obj2['b'+(i+1)] = 1;
      for (var repeat = 0;repeat != 2; ++repeat) {
        var count = 0;
        for (var j in obj1) {
          if (j !== 'a'+i)
            throw "Bad:"+j;
          for (var k in obj2) {
            if (i == Math.floor(N/3) || i == Math.floor(2*N/3))
              gc();
            var expected;
            switch (count) {
            case 0: expected='b'+i; break;
            case 1: expected='b'+(i+1); break;
            default:
              throw "Bad count: "+count;
            }
            if (expected != k)
              throw "Bad k, expected="+expected+", actual="+k;
            for (var l in obj0)
              ++count;
            ++count;
          }
        }
        if (count !== 2)
          throw "Bad count: "+count;
      }
    }
  }

  var array = [function() { f(10); },
               function() { f(50); },
               function() { f(200); },
               function() { f(400); }
    ];

  if (typeof scatter  == "function") {
    scatter(array);
  } else {
    for (var i = 0; i != array.length; ++i)
      array[i]();
  }

 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
