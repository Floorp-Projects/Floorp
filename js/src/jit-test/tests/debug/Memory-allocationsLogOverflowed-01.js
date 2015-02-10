// Test basic usage of `Debugger.Memory.prototype.allocationsLogOverflowed`.

const root = newGlobal();
const dbg = new Debugger(root);
dbg.memory.trackingAllocationSites = true;
dbg.memory.maxAllocationsLogLength = 1;

root.eval("(" + function immediate() {
  // Allocate more than the max log length.
  this.objs = [{}, {}, {}, {}];
} + "());");

// The log should have overflowed.
assertEq(dbg.memory.allocationsLogOverflowed, true);

// Once drained, the flag should be reset.
const allocs = dbg.memory.drainAllocationsLog();
assertEq(dbg.memory.allocationsLogOverflowed, false);

// If we keep allocations under the max log length, then we shouldn't have
// overflowed.
dbg.memory.maxAllocationsLogLength = 10000;
root.eval("this.objs = [{}, {}, {}, {}];");
assertEq(dbg.memory.allocationsLogOverflowed, false);
