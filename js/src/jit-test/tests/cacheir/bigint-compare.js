// Test various combinations of (BigInt R BigInt) and ensures the result is consistent with
// (BigInt R⁻¹ BigInt). This test doesn't aim to cover the overall correctness of (BigInt R BigInt),
// but merely ensures the possible combinations are properly handled in CacheIR.

var xs = [
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
];

function testLooseEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isZero = j === xs.length >> 1;

    assertEq(x == x, true);
    assertEq(x == 0n, isZero);
    assertEq(0n == x, isZero);
  }
}
testLooseEqual();

function testStrictEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isZero = j === xs.length >> 1;

    assertEq(x === x, true);
    assertEq(x === 0n, isZero);
    assertEq(0n === x, isZero);
  }
}
testStrictEqual();

function testLooseNotEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isZero = j === xs.length >> 1;

    assertEq(x != x, false);
    assertEq(x != 0n, !isZero);
    assertEq(0n != x, !isZero);
  }
}
testLooseNotEqual();

function testStrictNotEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isZero = j === xs.length >> 1;

    assertEq(x !== x, false);
    assertEq(x !== 0n, !isZero);
    assertEq(0n !== x, !isZero);
  }
}
testStrictNotEqual();

function testLessThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isNegative = j < (xs.length >> 1);
    var isPositive = j > (xs.length >> 1);

    assertEq(x < x, false);
    assertEq(x < 0n, isNegative);
    assertEq(0n < x, isPositive);
  }
}
testLessThan();

function testLessThanEquals() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isNegativeOrZero = j <= (xs.length >> 1);
    var isPositiveOrZero = j >= (xs.length >> 1);

    assertEq(x <= x, true);
    assertEq(x <= 0n, isNegativeOrZero);
    assertEq(0n <= x, isPositiveOrZero);
  }
}
testLessThanEquals();

function testGreaterThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isNegative = j < (xs.length >> 1);
    var isPositive = j > (xs.length >> 1);

    assertEq(x > x, false);
    assertEq(x > 0n, isPositive);
    assertEq(0n > x, isNegative);
  }
}
testGreaterThan();

function testGreaterThanEquals() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var isNegativeOrZero = j <= (xs.length >> 1);
    var isPositiveOrZero = j >= (xs.length >> 1);

    assertEq(x >= x, true);
    assertEq(x >= 0n, isPositiveOrZero);
    assertEq(0n >= x, isNegativeOrZero);
  }
}
testGreaterThanEquals();
