/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that JITOptimizations track optimization sites and create
 * an OptimizationSiteProfile when adding optimization sites, like from the
 * FrameNode, and the returning of that data is as expected.
 */

function test() {
  let { JITOptimizations } = devtools.require("devtools/shared/profiler/jit");

  let jit = new JITOptimizations(gOpts);

  jit.addOptimizationSite(1);
  jit.addOptimizationSite(1);
  jit.addOptimizationSite(0);
  jit.addOptimizationSite(0);
  jit.addOptimizationSite(1);
  jit.addOptimizationSite(2);

  let sites = jit.getOptimizationSites();

  let [first, second, third] = sites;

  is(first.id, 1, "Ordered by samples count, descending");
  is(first.samples, 3, "first OptimizationSiteProfile has correct sample count");
  is(first.data, gOpts[1], "includes OptimizationSite as reference under `data`");
  is(second.id, 0, "Ordered by samples count, descending");
  is(second.samples, 2, "second OptimizationSiteProfile has correct sample count");
  is(second.data, gOpts[0], "includes OptimizationSite as reference under `data`");
  is(third.id, 2, "Ordered by samples count, descending");
  is(third.samples, 1, "third OptimizationSiteProfile has correct sample count");
  is(third.data, gOpts[2], "includes OptimizationSite as reference under `data`");

  finish();
}

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
