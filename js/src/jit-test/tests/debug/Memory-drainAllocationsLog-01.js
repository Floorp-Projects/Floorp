// Test basic usage of drainAllocationsLog()

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)
dbg.memory.trackingAllocationSites = true;

root.eval("(" + function immediate() {
  this.tests = [
    ({}),
    [],
    /(two|2)\s*problems/,
    new function Ctor(){},
    new Object(),
    new Array(),
    new Date(),
  ];
} + "());");

const allocs = dbg.memory.drainAllocationsLog();
print(allocs.join("\n--------------------------------------------------------------------------\n"));
print("Total number of allocations logged: " + allocs.length);

let idx = -1;
for (let object of root.tests) {
  let wrappedObject = wrappedRoot.makeDebuggeeValue(object);
  let allocSite = wrappedObject.allocationSite;
  let newIdx = allocs.map(x => x.frame).indexOf(allocSite);
  assertEq(newIdx > idx, true);
  idx = newIdx;
}
