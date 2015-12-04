// Test that we only sample about allocationSamplingProbability * 100 percent of
// allocations.

const root = newGlobal();

const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

root.eval(`
  objs = [];
  objs.push(new Object);
`);

root.eval("" + function makeSomeAllocations() {
  for (var i = 0; i < 100; i++) {
    objs.push(new Object);
  }
});

function measure(P, expected) {
  root.setSavedStacksRNGState(Number.MAX_SAFE_INTEGER - 1);
  dbg.memory.allocationSamplingProbability = P;
  root.makeSomeAllocations();
  assertEq(dbg.memory.drainAllocationsLog().length, expected);
}

dbg.memory.trackingAllocationSites = true;

// These are the sample counts that were correct when this test was last
// updated; changes to SpiderMonkey may occasionally cause changes
// here. Anything that is within a plausible range for the given sampling
// probability is fine.
measure(0.0, 0);
measure(1.0, 100);
measure(0.1, 11);
measure(0.5, 49);
