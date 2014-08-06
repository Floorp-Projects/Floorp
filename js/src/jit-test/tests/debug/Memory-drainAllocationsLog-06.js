// Test doing a GC while we have a non-empty log.

const root = newGlobal();
const dbg = new Debugger();
dbg.addDebuggee(root)
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

gc();

const allocs = dbg.memory.drainAllocationsLog();
assertEq(allocs.length >= root.tests.length, true);
