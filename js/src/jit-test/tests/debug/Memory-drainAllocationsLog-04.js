// Test that when we shorten the maximum log length, we won't get a longer log
// than that new maximum afterwards.

const root = newGlobal();
const dbg = new Debugger();
dbg.addDebuggee(root)

dbg.memory.trackingAllocationSites = true;

root.eval([
  "this.alloc1 = {};", // line 1
  "this.alloc2 = {};", // line 2
  "this.alloc3 = {};", // line 3
  "this.alloc4 = {};", // line 4
].join("\n"));

dbg.memory.maxAllocationsLogLength = 1;
const allocs = dbg.memory.drainAllocationsLog();

// Should have trimmed down to the new maximum length.
assertEq(allocs.length, 1);
