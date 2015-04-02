/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that JITOptimizations create OptimizationSites, and the underlying
 * OptimizationSites methods work as expected.
 */

function test() {
  let { JITOptimizations, OptimizationSite } = devtools.require("devtools/shared/profiler/jit");

  let jit = new JITOptimizations(gOpts);

  jit.addOptimizationSite(1);
  jit.addOptimizationSite(1);
  jit.addOptimizationSite(0);
  jit.addOptimizationSite(0);
  jit.addOptimizationSite(1);
  jit.addOptimizationSite(2);

  let sites = jit.getOptimizationSites();

  let [first, second, third] = sites;

  /* hasSuccessfulOutcome */
  is(first.hasSuccessfulOutcome(), false, "optSite.hasSuccessfulOutcome() returns expected (1)");
  is(second.hasSuccessfulOutcome(), true, "optSite.hasSuccessfulOutcome() returns expected (2)");
  is(third.hasSuccessfulOutcome(), true, "optSite.hasSuccessfulOutcome() returns expected (3)");

  /* getAttempts */
  is(first.getAttempts().length, 2, "optSite.getAttempts() has the correct amount of attempts (1)");
  is(second.getAttempts().length, 5, "optSite.getAttempts() has the correct amount of attempts (2)");
  is(third.getAttempts().length, 3, "optSite.getAttempts() has the correct amount of attempts (3)");

  /* getIonTypes */
  is(first.getIonTypes().length, 1, "optSite.getIonTypes() has the correct amount of IonTypes (1)");
  is(second.getIonTypes().length, 2, "optSite.getIonTypes() has the correct amount of IonTypes (2)");
  is(third.getIonTypes().length, 1, "optSite.getIonTypes() has the correct amount of IonTypes (3)");

  finish();
}

let gOpts = [{
  line: 12,
  column: 2,
  types: [{ mirType: "Object", site: "A (http://foo/bar/bar:12)", types: [
    { keyedBy: "constructor", name: "Foo", location: "A (http://foo/bar/baz:12)" },
    { keyedBy: "constructor", location: "A (http://foo/bar/baz:12)" }
  ]}, { mirType: "Int32", site: "A (http://foo/bar/bar:12)", types: [
    { keyedBy: "primitive", location: "self-hosted" }
  ]}],
  attempts: [
    { outcome: "Failure1", strategy: "SomeGetter1" },
    { outcome: "Failure1", strategy: "SomeGetter1" },
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
