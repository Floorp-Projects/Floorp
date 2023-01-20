// Test relational comparison when one operand is null or undefined.

function test(xs) {
  for (let i = 0; i < 200; ++i) {
    let x = xs[i % xs.length];

    // The result is equal when compared to the result with explicit ToNumber conversions.

    // Test when null-or-undefined is on the right-hand side.
    assertEq(x < nullOrUndef, x < (+nullOrUndef));
    assertEq(x <= nullOrUndef, x <= (+nullOrUndef));
    assertEq(x >= nullOrUndef, x >= (+nullOrUndef));
    assertEq(x > nullOrUndef, x > (+nullOrUndef));

    // Test when null-or-undefined is on the left-hand side.
    assertEq(nullOrUndef < x, (+nullOrUndef) < x);
    assertEq(nullOrUndef <= x, (+nullOrUndef) <= x);
    assertEq(nullOrUndef >= x, (+nullOrUndef) >= x);
    assertEq(nullOrUndef > x, (+nullOrUndef) > x);
  }
}

function runTest(inputs) {
  let fNull = Function(`return ${test}`.replaceAll("nullOrUndef", "null"))();
  fNull(inputs);

  let fUndefined = Function(`return ${test}`.replaceAll("nullOrUndef", "undefined"))();
  fUndefined(inputs);
}

// BigInt inputs
runTest([
  // Definitely heap digits.
  -(2n ** 1000n),

  // -(2n**64n)
  -18446744073709551617n,
  -18446744073709551616n,
  -18446744073709551615n,

  // -(2n**63n)
  -9223372036854775809n,
  -9223372036854775808n,
  -9223372036854775807n,

  // -(2**32)
  -4294967297n,
  -4294967296n,
  -4294967295n,

  // -(2**31)
  -2147483649n,
  -2147483648n,
  -2147483647n,

  -1n,
  0n,
  1n,

  // 2**31
  2147483647n,
  2147483648n,
  2147483649n,

  // 2**32
  4294967295n,
  4294967296n,
  4294967297n,

  // 2n**63n
  9223372036854775807n,
  9223372036854775808n,
  9223372036854775809n,

  // 2n**64n
  18446744073709551615n,
  18446744073709551616n,
  18446744073709551617n,

  // Definitely heap digits.
  2n ** 1000n,
]);
