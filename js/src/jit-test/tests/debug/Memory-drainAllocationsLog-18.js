// Test drainAllocationsLog() entries' inNursery flag.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

dbg.memory.trackingAllocationSites = true;

root.eval(
  `
  for (let i = 0; i < 10; i++)
    allocationMarker({ nursery: true });

  for (let i = 0; i < 10; i++)
    allocationMarker({ nursery: false });
  `
);

let entries = dbg.memory.drainAllocationsLog().filter(e => e.class == "AllocationMarker");

assertEq(entries.length, 20);

for (let i = 0; i < 10; i++)
  assertEq(entries[i].inNursery, true);

for (let i = 10; i < 20; i++)
  assertEq(entries[i].inNursery, false);
