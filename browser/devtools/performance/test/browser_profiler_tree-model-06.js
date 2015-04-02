/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when constructing FrameNodes, if optimization data is available,
 * the FrameNodes have the correct optimization data after iterating over samples.
 */

let time = 1;

let samples = [{
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A", optsIndex: 0 },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A", optsIndex: 0 },
    { location: "D" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A", optsIndex: 1 },
    { location: "E", optsIndex: 2 },
    { location: "C" }
  ],
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "F" }
  ]
}];

// Array of OptimizationSites
let gOpts = [{
  line: 12,
  column: 2,
  types: [{ mirType: "Object", site: "A (http://foo/bar/bar:12)", types: [
    { keyedBy: "constructor", name: "Foo", location: "A (http://foo/bar/baz:12)" },
    { keyedBy: "primitive", location: "self-hosted" }
  ]}],
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Inlined", strategy: "SomeGetter3" },
  ]
}, {
  line: 34,
  types: [{ mirType: "Int32", site: "Receiver" }], // use no types
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "Failure3", strategy: "SomeGetter3" },
  ]
}, {
  line: 78,
  types: [{ mirType: "Object", site: "A (http://foo/bar/bar:12)", types: [
    { keyedBy: "constructor", name: "Foo", location: "A (http://foo/bar/baz:12)" },
    { keyedBy: "primitive", location: "self-hosted" }
  ]}],
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure2", strategy: "SomeGetter2" },
    { outcome: "GenericSuccess", strategy: "SomeGetter3" },
  ]
}];

function test() {
  let { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");

  let root = new ThreadNode(samples, { optimizations: gOpts });

  let A = root.calls.A;

  let opts = A.getOptimizations();
  let sites = opts.getOptimizationSites();
  is(sites.length, 2, "Frame A has two optimization sites.");
  is(sites[0].samples, 2, "first opt site has 2 samples.");
  is(sites[1].samples, 1, "second opt site has 1 sample.");

  let E = A.calls.E;
  opts = E.getOptimizations();
  sites = opts.getOptimizationSites();
  is(sites.length, 1, "Frame E has one optimization site.");
  is(sites[0].samples, 1, "first opt site has 1 samples.");

  let D = A.calls.D;
  ok(!D.getOptimizations(),
    "frames that do not have any opts data do not have JITOptimizations instances.");

  finish();
}
