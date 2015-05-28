// |reftest| skip-if(!xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 394709;
var summary = 'Do not leak with object.watch and closure';
var actual = 'No Leak';
var expect = 'No Leak';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  assertEq(finalizeCount(), 0, "invalid initial state");

  runtest();
  gc();
  assertEq(finalizeCount(), 1, "leaked");

  runtest();
  gc();
  assertEq(finalizeCount(), 2, "leaked");

  runtest();
  gc();
  assertEq(finalizeCount(), 3, "leaked");


  function runtest () {
    var obj = { b: makeFinalizeObserver() };
    obj.watch('b', watcher);

    function watcher(id, old, value) {
      ++obj.n;
      return value;
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
