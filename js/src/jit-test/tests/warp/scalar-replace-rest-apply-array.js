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
      assertRecoveredOnBailout(args, true);
      return g(...args);
    }

    function test() {
      // Ensure |f| isn't inlined into its caller.
      with ({});

      for (let i = 0; i < 1000; ++i) {
        assertEq(f(${args}), ${count});
      }
    }

    return test;
  `)();
}

// Inline rest arguments are limited to 14 elements, cf.
// gc::CanUseFixedElementsForArray() in WarpBuilder::build_Rest().
// There isn't such limit when the function isn't inlined.
const maxRestArgs = 20;

for (let i = 0; i <= maxRestArgs; ++i) {
  makeTest(i)();
}
