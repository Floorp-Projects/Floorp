// Test inlining parseInt with an Int32 input.

const int32Values = [
  // Values around INT32_MIN.
  -2147483648,
  -2147483647,
  -2147483646,

  // Negative values.
  -65536, -65535, -256, -255, -100, -50, -10,

  // Values around zero.
  -2, -1, 0, 1, 2,

  // Positive values.
  10, 50, 100, 255, 256, 65535, 65536,

  // Values around INT32_MAX.
  2147483645,
  2147483646,
  2147483647,
];

// Test int32 input without an explicit radix.
function testRadixAbsent() {
  for (let i = 0; i < 200; ++i) {
    let x = int32Values[i % int32Values.length];
    assertEq(x, x|0, "x is an int32 value");

    let r = Number.parseInt(x);
    assertEq(r, x);
  }
}
for (let i = 0; i < 2; ++i) testRadixAbsent();

// Test int32 input with radix=10.
function testRadixTen() {
  for (let i = 0; i < 200; ++i) {
    let x = int32Values[i % int32Values.length];
    assertEq(x, x|0, "x is an int32 value");

    let r = Number.parseInt(x, 10);
    assertEq(r, x);
  }
}
for (let i = 0; i < 2; ++i) testRadixTen();

// Test int32 input with radix=16. (This case isn't currently inlined.)
function testRadixSixteen() {
  for (let i = 0; i < 200; ++i) {
    let x = int32Values[i % int32Values.length];
    assertEq(x, x|0, "x is an int32 value");

    let expected = Math.sign(x) * Number("0x" + Math.abs(x).toString(10));

    let r = Number.parseInt(x, 16);
    assertEq(r, expected);
  }
}
for (let i = 0; i < 2; ++i) testRadixSixteen();

// Test with variable radix.
function testRadixVariable() {
  for (let i = 0; i < 200; ++i) {
    let x = int32Values[i % int32Values.length];
    assertEq(x, x|0, "x is an int32 value");

    let radix = [10, 16][(i > 100)|0];

    let expected = x;
    if (radix === 16) {
      expected = Math.sign(x) * Number("0x" + Math.abs(x).toString(10));
    }

    let r = Number.parseInt(x, radix);
    assertEq(r, expected);
  }
}
for (let i = 0; i < 2; ++i) testRadixVariable();

// Test with int32 and double inputs.
function testBadInput() {
  for (let i = 0; i < 200; ++i) {
    let x = int32Values[i % int32Values.length];
    assertEq(x, x|0, "x is an int32 value");

    let y = [x, NaN][(i > 150)|0];

    let r = Number.parseInt(y, 10);
    assertEq(r, y);
  }
}
for (let i = 0; i < 2; ++i) testBadInput();
