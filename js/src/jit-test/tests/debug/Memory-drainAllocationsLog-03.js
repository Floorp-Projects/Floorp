// Test when there are more allocations than the maximum log length.

const root = newGlobal();
const dbg = new Debugger();
dbg.addDebuggee(root)

dbg.memory.maxAllocationsLogLength = 3;
dbg.memory.trackingAllocationSites = true;

root.eval([
  "this.alloc1 = {};", // line 1
  "this.alloc2 = {};", // line 2
  "this.alloc3 = {};", // line 3
  "this.alloc4 = {};", // line 4
].join("\n"));

const allocs = dbg.memory.drainAllocationsLog();

// Should have stayed at the maximum length.
assertEq(allocs.length, 3);
// Should have kept the most recent allocation.
assertEq(allocs[2].frame.line, 4);
// Should have thrown away the oldest allocation.
assertEq(allocs.map(x => x.frame.line).indexOf(1), -1);
