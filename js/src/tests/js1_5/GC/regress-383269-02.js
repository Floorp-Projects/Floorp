// |reftest| skip -- unreliable - based on GC timing
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 383269;
var summary = 'Leak related to arguments object';
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
 
  function generate_big_object_graph()
  {
    var root = {};
    f(root, 17);
    return root;
    function f(parent, depth) {
      if (depth == 0)
        return;
      --depth;
      f(parent.a = {}, depth);
      f(parent.b = {}, depth);
    }
  }

  function f(obj) {
    with (obj)
      return arguments;
  }

  function timed_gc()
  {
    var t1 = Date.now();
    gc();
    return Date.now() - t1;
  }

  var x = f({});
  x = null;
  gc();
  var base_time = timed_gc();

  x = f(generate_big_object_graph());
  x = null;
  gc();
  var time = timed_gc();

  if (time > (base_time + 10) * 3)
    actual = "generate_big_object_graph() leaked, base_gc_time="+base_time+", last_gc_time="+time;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
