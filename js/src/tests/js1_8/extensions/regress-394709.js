/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 394709;
var summary = 'Do not leak with object.watch and closure';
var actual = 'No Leak';
var expect = 'No Leak';

if (typeof countHeap == 'undefined')
{
  countHeap = function () { 
    print('This test requires countHeap which is not supported'); 
    return 0;
  };
}

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // Ensure that we flush all values so that gc() collects all objects that
  // the user cannot reach from JS.
  eval();

  runtest();
  gc();
  var count1 = countHeap();
  runtest();
  gc();
  var count2 = countHeap();
  runtest();
  gc();
  var count3 = countHeap();
  /* Try to be tolerant of conservative GC noise: we want a steady leak. */
  if (count1 < count2 && count2 < count3)
    throw "A leaky watch point is detected";

  function runtest () {
    var obj = { b: 0 };
    obj.watch('b', watcher);

    function watcher(id, old, value) {
      ++obj.n;
      return value;
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
