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

function testAdd() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = xs[xs.length - 1 - j];

    assertEq(x + 0n, x);
    assertEq(x + y, 0n);
  }
}
testAdd();

function testSub() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = xs[xs.length - 1 - j];

    assertEq(x - 0n, x);
    assertEq(x - (-y), 0n);
  }
}
testSub();

function testMul() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x * 0n, 0n);
    assertEq(x * 1n, x);
    assertEq(x * (-1n), -x);
  }
}
testMul();

function testDiv() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];

    // Don't divide by zero.
    if (j === xs.length >> 1) {
      assertEq(x / 1n, 0n);
      continue;
    }

    assertEq(x / x, 1n);
    assertEq(x / 1n, x);
  }
}
testDiv();

function testMod() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];

    // Don't divide by zero.
    if (j === xs.length >> 1) {
      assertEq(x / 1n, 0n);
      continue;
    }

    assertEq(x % x, 0n);
    assertEq(x % 1n, 0n);
  }
}
testMod();

function testPow() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x ** 0n, 1n);
    assertEq(x ** 1n, x);
  }
}
testPow();

function testBitAnd() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x & x, x);
    assertEq(x & 0n, 0n);
  }
}
testBitAnd();

function testBitOr() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x | x, x);
    assertEq(x | 0n, x);
  }
}
testBitOr();

function testBitXor() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x ^ x, 0n);
    assertEq(x ^ 0n, x);
  }
}
testBitXor();

function testLeftShift() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x << 0n, x);
    assertEq(x << 1n, x * 2n);
    if (x >= 0n || !(x & 1n)) {
      assertEq(x << -1n, x / 2n);
    } else {
      assertEq(x << -1n, (x / 2n) - 1n);
    }
  }
}
testLeftShift();

function testRightShift() {
  for (var i = 0; i < 100; ++i) {
    var x = xs[i % xs.length];

    assertEq(x >> 0n, x);
    if (x >= 0n || !(x & 1n)) {
      assertEq(x >> 1n, x / 2n);
    } else {
      assertEq(x >> 1n, (x / 2n) - 1n);
    }
    assertEq(x >> -1n, x * 2n);
  }
}
testRightShift();
