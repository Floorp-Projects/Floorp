// Test that multiple Debuggers behave reasonably.

load(libdir + "asserts.js");

let dbg1, dbg2, root1, root2;

function isTrackingAllocations(global, dbgObj) {
  const site = dbgObj.makeDebuggeeValue(global.eval("({})")).allocationSite;
  if (site) {
    assertEq(typeof site, "object");
  }
  return !!site;
}

function test(name, fn) {
  print();
  print(name);

  // Reset state.
  root1 = newGlobal({newCompartment: true});
  root2 = newGlobal({newCompartment: true});
  dbg1 = new Debugger;
  dbg2 = new Debugger;

  // Run the test.
  fn();

  print("  OK");
}

test("Can track allocations even if a different debugger is already tracking " +
     "them.",
     () => {
       let d1r1 = dbg1.addDebuggee(root1);
       let d2r1 = dbg2.addDebuggee(root1);
       dbg1.memory.trackingAllocationSites = true;
       dbg2.memory.trackingAllocationSites = true;
       assertEq(isTrackingAllocations(root1, d1r1), true);
       assertEq(isTrackingAllocations(root1, d2r1), true);
     });

test("Removing root1 as a debuggee from all debuggers should disable the " +
     "allocation hook.",
     () => {
       dbg1.memory.trackingAllocationSites = true;
       let d1r1 = dbg1.addDebuggee(root1);
       dbg1.removeAllDebuggees();
       assertEq(isTrackingAllocations(root1, d1r1), false);
     });

test("Adding a new debuggee to a debugger that is tracking allocations should " +
     "enable the hook for the new debuggee.",
     () => {
       dbg1.memory.trackingAllocationSites = true;
       let d1r1 = dbg1.addDebuggee(root1);
       assertEq(isTrackingAllocations(root1, d1r1), true);
     });

test("Setting trackingAllocationSites to true should throw if the debugger " +
     "cannot install the allocation hooks for *every* debuggee.",
     () => {
       let d1r1 = dbg1.addDebuggee(root1);
       let d1r2 = dbg1.addDebuggee(root2);

       // Can't install allocation hooks for root2 with this set.
       root2.enableShellAllocationMetadataBuilder();

       assertThrowsInstanceOf(() => dbg1.memory.trackingAllocationSites = true,
                              Error);

       // And after it throws, its trackingAllocationSites accessor should reflect that
       // allocation site tracking is still disabled in that Debugger.
       assertEq(dbg1.memory.trackingAllocationSites, false);
       assertEq(isTrackingAllocations(root1, d1r1), false);
       assertEq(isTrackingAllocations(root2, d1r2), false);
     });

test("A Debugger isn't tracking allocation sites when disabled.",
     () => {
       dbg1.memory.trackingAllocationSites = true;
       let d1r1 = dbg1.addDebuggee(root1);

       assertEq(isTrackingAllocations(root1, d1r1), true);
       dbg1.enabled = false;
       assertEq(isTrackingAllocations(root1, d1r1), false);
     });

test("Re-enabling throws an error if we can't reinstall allocations tracking " +
     "for all debuggees.",
     () => {
       dbg1.enabled = false
       dbg1.memory.trackingAllocationSites = true;
       let d1r1 = dbg1.addDebuggee(root1);
       let d1r2 = dbg1.addDebuggee(root2);

       // Can't install allocation hooks for root2 with this set.
       root2.enableShellAllocationMetadataBuilder();

       assertThrowsInstanceOf(() => dbg1.enabled = true,
                              Error);

       assertEq(dbg1.enabled, false);
       assertEq(isTrackingAllocations(root1, d1r1), false);
       assertEq(isTrackingAllocations(root2, d1r2), false);
     });
