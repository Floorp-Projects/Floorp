// Test drainAllocationsLog() and byte sizes.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

root.eval(
  `
  function AsmModule(stdlib, foreign, heap) {
    "use asm";

    function test() {
      return 5|0;
    }

    return { test: test };
  }
  const buf = new ArrayBuffer(1024*8);

  function Ctor() {}
  this.tests = [
    { name: "new UInt8Array(256)", fn: () => new Uint8Array(256)         },
    { name: "arguments",           fn: function () { return arguments; } },
    { name: "asm.js module",       fn: () => AsmModule(this, {}, buf)    },
    { name: "/2manyproblemz/g",    fn: () => /2manyproblemz/g            },
    { name: "iterator",            fn: () => [1,2,3][Symbol.iterator]()  },
    { name: "Error()",             fn: () => Error()                     },
    { name: "new Ctor",            fn: () => new Ctor                    },
    { name: "{}",                  fn: () => ({})                        },
    { name: "new Date",            fn: () => new Date                    },
    { name: "[1,2,3]",             fn: () => [1,2,3]                     },
  ];
  `
);

for (let { name, fn } of root.tests) {
  print("Test: " + name);

  dbg.memory.trackingAllocationSites = true;

  fn();

  let entries = dbg.memory.drainAllocationsLog();

  for (let {size} of entries) {
    print("  " + size + " bytes");
    // We should get some kind of byte size. We aren't testing that in depth
    // here, it is tested pretty thoroughly in
    // js/src/jit-test/tests/heap-analysis/byteSize-of-object.js.
    assertEq(typeof size, "number");
    assertEq(size > 0, true);
  }

  dbg.memory.trackingAllocationSites = false;
}
