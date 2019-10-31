// Same test as bigint-compare-double, except that the inputs are allowed to be any number, i.e.
// either Int32 or Double.

// Test various combinations of (BigInt R Number) and ensures the result is consistent with
// (Number R⁻¹ BigInt). This test doesn't aim to cover the overall correctness of (BigInt R Number),
// but merely ensures the possible combinations are properly handled in CacheIR.

var xs = [
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

// Compute the Number approximation of the BigInt values.
var ys = xs.map(x => Number(x));

// Compute if the Number approximation of the BigInt values is exact.
// (The larger test values are all powers of two, so we can keep this function simple.)
var zs = xs.map(x => {
  var isNegative = x < 0n;
  if (isNegative) {
    x = -x;
  }
  var s = x.toString(2);
  if (s.length <= 53 || (s.length <= 1024 && /^1+0+$/.test(s))) {
    return 0;
  }
  if (s.length <= 1024 && /^1+$/.test(s)) {
    return isNegative ? -1 : 1;
  }
  if (s.length <= 1024 && /^1+0+1$/.test(s)) {
    return isNegative ? 1 : -1;
  }
  return NaN;
});

function testLooseEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x == y, z === 0);
    assertEq(y == x, z === 0);
  }
}
testLooseEqual();

function testLooseNotEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x != y, z !== 0);
    assertEq(y != x, z !== 0);
  }
}
testLooseNotEqual();

function testLessThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    if (z === 0) {
      assertEq(x < y, false);
      assertEq(y < x, false);
    } else if (z > 0) {
      assertEq(x < y, true);
      assertEq(y < x, false);
    } else if (z < 0) {
      assertEq(x < y, false);
      assertEq(y < x, true);
    } else {
      assertEq(x < y, y > 0);
      assertEq(y < x, y < 0);
    }
  }
}
testLessThan();

function testLessThanEquals() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    if (z === 0) {
      assertEq(x <= y, true);
      assertEq(y <= x, true);
    } else if (z > 0) {
      assertEq(x <= y, true);
      assertEq(y <= x, false);
    } else if (z < 0) {
      assertEq(x <= y, false);
      assertEq(y <= x, true);
    } else {
      assertEq(x <= y, y > 0);
      assertEq(y <= x, y < 0);
    }
  }
}
testLessThanEquals();

function testGreaterThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    if (z === 0) {
      assertEq(x > y, false);
      assertEq(y > x, false);
    } else if (z > 0) {
      assertEq(x > y, false);
      assertEq(y > x, true);
    } else if (z < 0) {
      assertEq(x > y, true);
      assertEq(y > x, false);
    } else {
      assertEq(x > y, y < 0);
      assertEq(y > x, y > 0);
    }
  }
}
testGreaterThan();

function testGreaterThanEquals() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    if (z === 0) {
      assertEq(x >= y, true);
      assertEq(y >= x, true);
    } else if (z > 0) {
      assertEq(x >= y, false);
      assertEq(y >= x, true);
    } else if (z < 0) {
      assertEq(x >= y, true);
      assertEq(y >= x, false);
    } else {
      assertEq(x >= y, y < 0);
      assertEq(y >= x, y > 0);
    }
  }
}
testGreaterThanEquals();
