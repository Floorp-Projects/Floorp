// Test that multiple Debuggers behave reasonably. Since we're not keeping a
// per-compartment count of how many Debuggers have requested allocation
// tracking, assert that attempts to request allocation tracking from multiple
// debuggers throws.

load(libdir + "asserts.js");

let root1 = newGlobal();
let root2 = newGlobal();

let dbg1 = new Debugger();
let dbg2 = new Debugger();

let d1r1 = dbg1.addDebuggee(root1);
let d2r1 = dbg2.addDebuggee(root1);

let wrappedObj, allocationSite;

function isTrackingAllocations(global, dbgObj) {
  const site = dbgObj.makeDebuggeeValue(global.eval("({})")).allocationSite;
  if (site) {
    assertEq(typeof site, "object");
  }
  return !!site;
}

// Can't track allocations if a different debugger is already tracking them.
dbg1.memory.trackingAllocationSites = true;
assertThrowsInstanceOf(() => dbg2.memory.trackingAllocationSites = true,
                       Error);

// Removing root as a debuggee from dbg1 should disable the allocation hook.
dbg1.removeDebuggee(root1);
assertEq(isTrackingAllocations(root1, d1r1), false);

// Tracking allocations in dbg2 should work now that dbg1 isn't debugging root1.
dbg2.memory.trackingAllocationSites = true;
assertEq(isTrackingAllocations(root1, d2r1), true);

// Adding root back as a debuggee in dbg1 should fail now because it will
// attempt to track allocations in root, but dbg2 is already doing that.
assertThrowsInstanceOf(() => dbg1.addDebuggee(root1),
                       Error);
assertEq(dbg1.hasDebuggee(root1), false);

// Adding a new debuggee to a debugger that is tracking allocations should
// enable the hook for the new debuggee.
dbg2.removeDebuggee(root1);
d1r1 = dbg1.addDebuggee(root1);
assertEq(isTrackingAllocations(root1, d1r1), true);

// Setting trackingAllocationSites to true should throw if the debugger cannot
// install the allocation hooks for *every* debuggee.
dbg1.memory.trackingAllocationSites = true;
dbg1.addDebuggee(root1);
dbg2.memory.trackingAllocationSites = false;
let d2r2 = dbg2.addDebuggee(root2);
dbg2.addDebuggee(root1);
assertThrowsInstanceOf(() => dbg2.memory.trackingAllocationSites = true,
                       Error);

// And after it throws, its trackingAllocationSites accessor should reflect that
// allocation site tracking is still disabled in that Debugger.
assertEq(isTrackingAllocations(root2, d2r2), false);
