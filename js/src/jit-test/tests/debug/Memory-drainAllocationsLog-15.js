// Test drainAllocationsLog() and [[Class]] names.
if (!('Promise' in this))
    quit(0);

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)

root.eval(
  `
  this.tests = [
    { expected: "Object",    test: () => new Object    },
    { expected: "Array",     test: () => []            },
    { expected: "Date",      test: () => new Date      },
    { expected: "RegExp",    test: () => /problems/    },
    { expected: "Int8Array", test: () => new Int8Array },
    { expected: "Promise",   test: () => new Promise(function (){})},
  ];
  `
);


for (let { expected, test } of root.tests) {
  print(expected);

  dbg.memory.trackingAllocationSites = true;
  test();
  let allocs = dbg.memory.drainAllocationsLog();
  dbg.memory.trackingAllocationSites = false;

  assertEq(allocs.some(a => a.class === expected), true);
}

