// Test inlining parseInt with a Double input.

const doubleValues = [
  // Values around INT32_MIN.
  -2147483648.5,
  -2147483647.5,
  -2147483646.5,

  // Negative values.
  -65536.1, -65535.2, -256.3, -255.4, -100.5, -50.6, -10.7,

  // Values around zero.
  -2.1, -1.1, -0, +0, 0.1, 1.1, 2.1,

  // Positive values.
  10.7, 50.6, 100.5, 255.4, 256.3, 65535.2, 65536.1,

  // Values around INT32_MAX.
  2147483645.5,
  2147483646.5,
  2147483647.5,
];

// Test double input without an explicit radix.
function testRadixAbsent() {
  for (let i = 0; i < 200; ++i) {
    let x = doubleValues[i % doubleValues.length];
    let y = x|0;

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testRadixAbsent();

// Test double input with radix=10.
function testRadixTen() {
  for (let i = 0; i < 200; ++i) {
    let x = doubleValues[i % doubleValues.length];

    let r = Number.parseInt(x, 10);
    assertEq(r, x|0);
  }
}
for (let i = 0; i < 2; ++i) testRadixTen();

// Test double input in the exclusive range (0, 1.0e-6).
function testBadTooSmallPositive() {
  const goodValues = [
    +0, +0.5, +1.5, +2.5, +3.5, +4.5, +5.5,
    -0,       -1.5, -2.5, -3.5, -4.5, -5.5,
  ];
  const badValues = [
    9.999999999999997e-7, // parseInt(9.999999999999997e-7) is 9.
    1e-7, // parseInt(1e-7) is 1.
  ];

  const values = [
    ...goodValues,
    ...badValues,
  ];

  for (let i = 0; i < 200; ++i) {
    let xs = [goodValues, values][(i >= 150)|0];
    let x = xs[i % xs.length];
    let y;
    if (0 < x && x < 1e-6) {
      y = (String(x).match(/(.*)e.*/)[1])|0;
    } else {
      y = x|0;
    }

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadTooSmallPositive();

// Test double input in the exclusive range (-1.0e-6, -0).
function testBadTooSmallNegative() {
  const goodValues = [
    +0, +0.5, +1.5, +2.5, +3.5, +4.5, +5.5,
    -0,       -1.5, -2.5, -3.5, -4.5, -5.5,
  ];
  const badValues = [
    -9.999999999999997e-7, // parseInt(-9.999999999999997e-7) is -9.
    -1e-7, // parseInt(-1e-7) is -1.
  ];

  const values = [
    ...goodValues,
    ...badValues,
  ];

  for (let i = 0; i < 200; ++i) {
    let xs = [goodValues, values][(i >= 150)|0];
    let x = xs[i % xs.length];
    let y;
    if (-1e-6 < x && x < -0) {
      y = (String(x).match(/(.*)e.*/)[1])|0;
    } else {
      y = x|0;
    }

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadTooSmallNegative();

// Test double input in the exclusive range (-1, -1.0e-6).
function testBadNegativeZero() {
  const goodValues = [
    +0, +0.5, +1.5, +2.5, +3.5, +4.5, +5.5,
    -0,       -1.5, -2.5, -3.5, -4.5, -5.5,
  ];
  const badValues = [
    -0.1, // parseInt(-0.1) is -0.
    -0.5, // parseInt(-0.5) is -0.
    -0.9, // parseInt(-0.9) is -0.
  ];

  const values = [
    ...goodValues,
    ...badValues,
  ];

  for (let i = 0; i < 200; ++i) {
    let xs = [goodValues, values][(i >= 150)|0];
    let x = xs[i % xs.length];
    let y;
    if (-1 < x && x < 0) {
      y = -0;
    } else {
      y = x|0;
    }

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadNegativeZero();

// Test double input with infinity values.
function testBadInfinity() {
  const goodValues = [
    +0, +0.5, +1.5, +2.5, +3.5, +4.5, +5.5,
    -0,       -1.5, -2.5, -3.5, -4.5, -5.5,
  ];
  const badValues = [
    Infinity, // parseInt(Infinity) is NaN
    -Infinity, // parseInt(-Infinity) is NaN
  ];

  const values = [
    ...goodValues,
    ...badValues,
  ];

  for (let i = 0; i < 200; ++i) {
    let xs = [goodValues, values][(i >= 150)|0];
    let x = xs[i % xs.length];
    let y;
    if (!Number.isFinite(x)) {
      y = NaN;
    } else {
      y = x|0;
    }

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadInfinity();

// Test double input with NaN values.
function testBadNaN() {
  const goodValues = [
    +0, +0.5, +1.5, +2.5, +3.5, +4.5, +5.5,
    -0,       -1.5, -2.5, -3.5, -4.5, -5.5,
  ];
  const badValues = [
    NaN, // parseInt(NaN) is NaN
  ];

  const values = [
    ...goodValues,
    ...badValues,
  ];

  for (let i = 0; i < 200; ++i) {
    let xs = [goodValues, values][(i >= 150)|0];
    let x = xs[i % xs.length];
    let y;
    if (!Number.isFinite(x)) {
      y = NaN;
    } else {
      y = x|0;
    }

    let r = Number.parseInt(x);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadNaN();
