/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// HeapSnapshot.prototype.takeCensus behaves plausibly as we allocate objects.
//
// Exact object counts vary in ways we can't predict. For example,
// BaselineScripts can hold onto "template objects", which exist only to hold
// the shape and type for newly created objects. When BaselineScripts are
// discarded, these template objects go with them.
//
// So instead of expecting precise counts, we expect counts that are at least as
// many as we would expect given the object graph we've built.
//
// Ported from js/src/jit-tests/debug/Memory-takeCensus-02.js

function run_test() {
  // A Debugger with no debuggees had better not find anything.
  const dbg = new Debugger();
  const census0 = saveHeapSnapshotAndTakeCensus(dbg);
  Census.walkCensus(census0, "census0", Census.assertAllZeros);

  function newGlobalWithDefs() {
    const g = newGlobal();
    g.eval(`
           function times(n, fn) {
             var a=[];
             for (var i = 0; i<n; i++)
               a.push(fn());
             return a;
           }
           `);
    return g;
  }

  // Allocate a large number of various types of objects, and check that census
  // finds them.
  const g = newGlobalWithDefs();
  dbg.addDebuggee(g);

  g.eval("var objs = times(100, () => ({}));");
  g.eval("var rxs  = times(200, () => /foo/);");
  g.eval("var ars  = times(400, () => []);");
  g.eval("var fns  = times(800, () => () => {});");

  const census1 = dbg.memory.takeCensus(dbg);
  Census.walkCensus(census1, "census1",
                    Census.assertAllNotLessThan({
                      "objects": {
                        "Object": { count: 100 },
                        "RegExp": { count: 200 },
                        "Array": { count: 400 },
                        "Function": { count: 800 }
                      }
                    }));

  do_test_finished();
}
