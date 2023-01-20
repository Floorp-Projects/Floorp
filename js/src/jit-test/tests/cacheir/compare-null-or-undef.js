// Test relational comparison when one operand is null or undefined.

function test(xs) {
  for (let i = 0; i < 200; ++i) {
    let x = xs[i % xs.length];

    // The result is equal when compared to the result with explicit ToNumber conversions.

    // Test when null-or-undefined is on the right-hand side.
    assertEq(x < nullOrUndef, (+x) < (+nullOrUndef));
    assertEq(x <= nullOrUndef, (+x) <= (+nullOrUndef));
    assertEq(x >= nullOrUndef, (+x) >= (+nullOrUndef));
    assertEq(x > nullOrUndef, (+x) > (+nullOrUndef));

    // Test when null-or-undefined is on the left-hand side.
    assertEq(nullOrUndef < x, (+nullOrUndef) < (+x));
    assertEq(nullOrUndef <= x, (+nullOrUndef) <= (+x));
    assertEq(nullOrUndef >= x, (+nullOrUndef) >= (+x));
    assertEq(nullOrUndef > x, (+nullOrUndef) > (+x));
  }
}

function runTest(inputs) {
  let fNull = Function(`return ${test}`.replaceAll("nullOrUndef", "null"))();
  fNull(inputs);

  let fUndefined = Function(`return ${test}`.replaceAll("nullOrUndef", "undefined"))();
  fUndefined(inputs);
}

// Null inputs.
runTest([
  null,
]);

// Undefined inputs.
runTest([
  undefined,
]);

// Null or undefined inputs.
runTest([
  null,
  undefined,
]);

// Int32 inputs
runTest([
  -0x8000_0000,
  -0x7fff_ffff,
  -0x7fff_fffe,
  -2,
  -1,
  0,
  1,
  2,
  0x7fff_fffe,
  0x7fff_ffff,
]);

// Number inputs
runTest([
  Number.NEGATIVE_INFINITY,
  -Number.MAX_VALUE,
  Number.MIN_SAFE_INTEGER,
  -1.5,
  -0.5,
  -Number.EPSILON,
  -Number.MIN_VALUE,
  -0,
  0,
  Number.MIN_VALUE,
  Number.EPSILON,
  0.5,
  1.5,
  Number.MAX_SAFE_INTEGER,
  Number.MAX_VALUE,
  Number.POSITIVE_INFINITY,
  NaN,
]);

// Boolean inputs.
runTest([
  true, false,
]);

// String inputs
runTest([
  "",
  "0", "-0", "0.0", "0e100",
  "1", "1.5", "-1234", "-1e0",
  "Infinity", "NaN", "not-a-number",
]);
