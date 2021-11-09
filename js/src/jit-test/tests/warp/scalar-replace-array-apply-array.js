// |jit-test| --fast-warmup; --no-threads

setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 100);

// Prevent GC from cancelling/discarding Ion compilations.
gczeal(0);

// Create a fresh set of functions for each argument count to avoid type pollution.
function makeTest(count) {
  var args = Array(count).fill(0).join(",");

  return Function(`
    function g() {
      return arguments.length;
    }

    function f(...args) {
      // When |f| is inlined into its caller, the number of arguments is fixed and
      // we can scalar replace the inlined rest array.
      assertRecoveredOnBailout(args, true);
      return g(...args);
    }

    // |f| must be small enough to be inlined into the test function.
    assertEq(isSmallFunction(f), true);

    function test() {
      for (let i = 0; i < 1000; ++i) {
        assertEq(f(${args}), ${count});
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
