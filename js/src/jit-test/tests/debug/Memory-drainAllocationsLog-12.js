// Test that disabling the debugger disables allocation tracking.

load(libdir + "asserts.js");

const dbg = new Debugger();
const root = newGlobal({newCompartment: true});
dbg.addDebuggee(root);

dbg.memory.trackingAllocationSites = true;
dbg.enabled = false;

root.eval("this.alloc = {}");

// We shouldn't accumulate allocations in our log while the debugger is
// disabled.
let allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length, 0);
