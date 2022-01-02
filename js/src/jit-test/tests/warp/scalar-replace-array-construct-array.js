// |jit-test| --fast-warmup; --no-threads

setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 100);

// Prevent GC from cancelling/discarding Ion compilations.
gczeal(0);

// Create a fresh set of functions for each argument count to avoid type pollution.
function makeTest(count) {
  var args = Array(count).fill(0).join(",");

  return Function(`
    class Base {
      constructor() {
        this.count = arguments.length;
      }
    }
  
    class C extends Base {
      constructor(...args) {
        // When |C| is inlined into its caller, the number of arguments is fixed and
        // we can scalar replace the inlined rest array.
        assertRecoveredOnBailout(args, true);
        return super(...args);
      }
    }

    // |C| must be small enough to be inlined into the test function.
    assertEq(isSmallFunction(C), true);

    function test() {
      for (let i = 0; i < 1000; ++i) {
        let obj = new C(${args});
        assertEq(obj.count, ${count});
      }
    }

    return test;
  `)();
}

// Limited by gc::CanUseFixedElementsForArray(), see also WarpBuilder::build_Rest().
const maxRestArgs = 14;

for (let i = 0; i <= maxRestArgs; ++i) {
  makeTest(i)();
}
