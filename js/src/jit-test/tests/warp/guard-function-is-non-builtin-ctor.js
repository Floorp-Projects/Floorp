function test() {
  for (var i = 0; i <= 200; ++i) {
    // Create a fresh function in each iteration.
    var values = [function(){}, () => {}];

    // Use an arrow function in the last iteration.
    var useArrowFn = (i === 200);

    // No conditional (?:) so we don't trigger a cold-code bailout.
    var value = values[0 + useArrowFn];

    // Set or create the "prototype" property.
    value.prototype = null;

    // The "prototype" is configurable iff the function is an arrow function.
    var desc = Object.getOwnPropertyDescriptor(value, "prototype");
    assertEq(desc.configurable, useArrowFn);
  }
}
test();
