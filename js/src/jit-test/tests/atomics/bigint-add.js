// |jit-test| test-join=--spectre-mitigations=off

// These do not test atomicity, just that code generation for BigInt values
// works correctly.

const bigIntValues = [
  // Definitely heap digits.
  -(2n ** 2000n),
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
  2n ** 2000n,
];

function testAdd() {
  const int64 = new BigInt64Array(2);
  const uint64 = new BigUint64Array(2);

  // Test with constant index.
  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < bigIntValues.length; ++j) {
      let value = bigIntValues[j];

      assertEq(Atomics.add(int64, 0, value), 0n);
      assertEq(int64[0], BigInt.asIntN(64, value));

      assertEq(Atomics.add(uint64, 0, value), 0n);
      assertEq(uint64[0], BigInt.asUintN(64, value));

      assertEq(Atomics.add(int64, 0, -value), BigInt.asIntN(64, value));
      assertEq(int64[0], 0n);

      assertEq(Atomics.add(uint64, 0, -value), BigInt.asUintN(64, value));
      assertEq(uint64[0], 0n);
    }
  }

  // Test with variable index.
  for (let i = 0; i < 20; ++i) {
    for (let j = 0; j < bigIntValues.length; ++j) {
      let value = bigIntValues[j];
      let idx = j & 1;

      assertEq(Atomics.add(int64, idx, value), 0n);
      assertEq(int64[idx], BigInt.asIntN(64, value));

      assertEq(Atomics.add(uint64, idx, value), 0n);
      assertEq(uint64[idx], BigInt.asUintN(64, value));

      assertEq(Atomics.add(int64, idx, -value), BigInt.asIntN(64, value));
      assertEq(int64[idx], 0n);

      assertEq(Atomics.add(uint64, idx, -value), BigInt.asUintN(64, value));
      assertEq(uint64[idx], 0n);
    }
  }
}
for (let i = 0; i < 2; ++i) testAdd();
