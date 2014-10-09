// Test retrieving the log multiple times.

const root = newGlobal();
const dbg = new Debugger();
dbg.addDebuggee(root)

root.eval([
  "this.allocs = [];",
  "this.doFirstAlloc = " + function () {
    this.allocs.push({});           this.firstAllocLine = Error().lineNumber;
  },
  "this.doSecondAlloc = " + function () {
    this.allocs.push(new Object()); this.secondAllocLine = Error().lineNumber;
  }
].join("\n"));

dbg.memory.trackingAllocationSites = true;

root.doFirstAlloc();
let allocs1 = dbg.memory.drainAllocationsLog();
root.doSecondAlloc();
let allocs2 = dbg.memory.drainAllocationsLog();

let allocs1Lines = allocs1.map(x => x.frame.line);
assertEq(allocs1Lines.indexOf(root.firstAllocLine) != -1, true);
assertEq(allocs1Lines.indexOf(root.secondAllocLine) == -1, true);

let allocs2Lines = allocs2.map(x => x.frame.line);
assertEq(allocs2Lines.indexOf(root.secondAllocLine) != -1, true);
assertEq(allocs2Lines.indexOf(root.firstAllocLine) == -1, true);
