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
        assertRecoveredOnBailout(args, true);
        return super(...args);
      }
    }

    function test() {
      // Ensure |C| isn't inlined into its caller.
      with ({});

      for (let i = 0; i < 1000; ++i) {
        let obj = new C(${args});
        assertEq(obj.count, ${count});
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
