// Extensive test for (BigInt R Int32) comparison operations, testing the output
// is correct and consistent with (Int32 R⁻¹ BigInt).

function gcd(a, b) {
  a |= 0;
  b |= 0;
  while (b !== 0) {
    [a, b] = [b, a % b];
  }
  return Math.abs(a);
}

const ITERATIONS = 150;

function assertAllCombinationsTested(xs, ys, n) {
  // If the array lengths are relatively prime and their product is at least
  // |n| long, all possible combinations are tested at least once. Make sure
  // we test each combination at least three times.
  var m = 3;

  assertEq(gcd(xs.length, ys.length), 1);
  assertEq(m * xs.length * ys.length <= n, true);
}

function LessThan(xs, ys, n = ITERATIONS) {
  assertAllCombinationsTested(xs, ys, n);
  for (var i = 0; i < n; ++i) {
    var x = xs[i % xs.length];
    var y = ys[i % ys.length]|0; // Ensure int32 typed

    assertEq(x == y, false);
    assertEq(y == x, false);

    assertEq(x != y, true);
    assertEq(y != x, true);

    assertEq(x < y, true);
    assertEq(y < x, false);

    assertEq(x <= y, true);
    assertEq(y <= x, false);

    assertEq(x > y, false);
    assertEq(y > x, true);

    assertEq(x >= y, false);
    assertEq(y >= x, true);
  }
}

function GreaterThan(xs, ys, n = ITERATIONS) {
  assertAllCombinationsTested(xs, ys, n);
  for (var i = 0; i < n; ++i) {
    var x = xs[i % xs.length];
    var y = ys[i % ys.length]|0; // Ensure int32 typed

    assertEq(x == y, false);
    assertEq(y == x, false);

    assertEq(x != y, true);
    assertEq(y != x, true);

    assertEq(x < y, false);
    assertEq(y < x, true);

    assertEq(x <= y, false);
    assertEq(y <= x, true);

    assertEq(x > y, true);
    assertEq(y > x, false);

    assertEq(x >= y, true);
    assertEq(y >= x, false);
  }
}

function Equal(xs, ys, n = ITERATIONS) {
  assertAllCombinationsTested(xs, ys, n);
  for (var i = 0; i < n; ++i) {
    var x = xs[i % xs.length];
    var y = ys[i % ys.length]|0; // Ensure int32 typed

    assertEq(x == y, true);
    assertEq(y == x, true);

    assertEq(x != y, false);
    assertEq(y != x, false);

    assertEq(x < y, false);
    assertEq(y < x, false);

    assertEq(x <= y, true);
    assertEq(y <= x, true);

    assertEq(x > y, false);
    assertEq(y > x, false);

    assertEq(x >= y, true);
    assertEq(y >= x, true);
  }
}

function test(fn) {
  // Clone the test function to ensure a new function is compiled each time.
  return Function(`return ${fn}`)();
}

const negativeInt32 = [-2147483648, -2147483647, -1];
const zeroInt32 = [0];
const positiveInt32 = [1, 2147483646, 2147483647];
const zeroOrPositiveInt32 = [...zeroInt32, ...positiveInt32];
const anyInt32 = [...negativeInt32, ...zeroInt32, ...positiveInt32];

// Test when the BigInt is too large to be representable as a single BigInt digit.
function testLarge() {
  var xs = [
    2n ** 32n, // exceeds single digit limit on 32-bit
    2n ** 64n, // exceeds single digit limit on 64-bit
    2n ** 96n, // not a single digit on either platform
  ];
  test(GreaterThan)(xs, anyInt32);

  var xs = [
    -(2n ** 32n), // exceeds single digit limit on 32-bit
    -(2n ** 64n), // exceeds single digit limit on 64-bit
    -(2n ** 96n), // not a single digit on either platform
  ];
  test(LessThan)(xs, anyInt32);
}
testLarge();

// Test when the BigInt is 0n.
function testZero() {
  var xs = [
    0n
  ];

  test(GreaterThan)(xs, negativeInt32);
  test(Equal)(xs, zeroInt32);
  test(LessThan)(xs, positiveInt32);
}
testZero();

// Test when both numbers are negative.
function testNegative() {
  var xs = [
    -(2n ** 64n) - 2n,
    -(2n ** 64n) - 1n, // Max negative using a single BigInt digit on 64-bit.
    -(2n ** 64n),

    -(2n ** 32n) - 2n,
    -(2n ** 32n) - 1n, // Max negative using a single BigInt digit on 32-bit.
    -(2n ** 32n),

    -(2n ** 31n) - 1n, // One past max negative for Int32.
  ];
  test(LessThan)(xs, negativeInt32);

  var xs = [
    -(2n ** 31n), // Max negative for Int32.
  ];
  test(Equal)(xs, [-2147483648]);
  test(LessThan)(xs, [-2147483647, -1]);

  var xs = [
    -(2n ** 31n) + 1n,
  ];
  test(GreaterThan)(xs, [-2147483648]);
  test(Equal)(xs, [-2147483647]);
  test(LessThan)(xs, [-1]);

  var xs = [
    -1n,
  ];
  test(GreaterThan)(xs, [-2147483648, -2147483647]);
  test(Equal)(xs, [-1]);
}
testNegative();

// Test when both numbers are positive (and BigInt strictly positive).
function testPositive() {
  var xs = [
    1n,
  ];
  test(GreaterThan)(xs, [0]);
  test(Equal)(xs, [1]);
  test(LessThan)(xs, [2147483646, 2147483647]);

  var xs = [
    2n ** 31n - 2n,
  ];
  test(GreaterThan)(xs, [0, 1]);
  test(Equal)(xs, [2147483646]);
  test(LessThan)(xs, [2147483647]);

  var xs = [
    2n ** 31n - 1n, // Max positive for Int32.
  ];
  test(GreaterThan)(xs, [0, 1, 2147483646]);
  test(Equal)(xs, [2147483647]);

  var xs = [
    2n ** 31n, // One past max positive for Int32.

    2n ** 32n - 2n,
    2n ** 32n - 1n, // Max positive using a single BigInt digit on 32-bit.
    2n ** 32n,

    2n ** 64n - 2n,
    2n ** 64n - 1n, // Max positive using a single BigInt digit on 64-bit.
    2n ** 64n,
  ];
  test(GreaterThan)(xs, zeroOrPositiveInt32);
}
testPositive();

// Test negative BigInt and positive Int32.
function testNegativePositive() {
  var xs = [
    -(2n ** 64n) - 2n,
    -(2n ** 64n) - 1n, // Max negative using a single BigInt digit on 64-bit.
    -(2n ** 64n),

    -(2n ** 32n) - 2n,
    -(2n ** 32n) - 1n, // Max negative using a single BigInt digit on 32-bit.
    -(2n ** 32n),

    -(2n ** 31n) - 1n,
    -(2n ** 31n), // Max negative for Int32.
    -(2n ** 31n) + 1n,

    -2n, // Extra entry to ensure assertAllCombinationsTested passes.
    -1n,
  ];
  test(LessThan)(xs, zeroOrPositiveInt32);
}
testNegativePositive();

// Test (strictly) positive BigInt and negative Int32.
function testPositiveNegative() {
  var xs = [
    1n,

    2n ** 31n - 2n,
    2n ** 31n - 1n, // Max positive for Int32.
    2n ** 31n,

    2n ** 32n - 2n,
    2n ** 32n - 1n, // Max positive using a single BigInt digit on 32-bit.
    2n ** 32n,

    2n ** 64n - 2n,
    2n ** 64n - 1n, // Max positive using a single BigInt digit on 64-bit.
    2n ** 64n,
  ];
  test(GreaterThan)(xs, negativeInt32);
}
testPositiveNegative();
