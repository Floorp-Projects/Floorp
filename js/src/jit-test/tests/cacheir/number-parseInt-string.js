// Test inlining parseInt with a String input.

const stringInt32Values = [
  // Values around INT32_MIN.
  "-2147483648",
  "-2147483647",
  "-2147483646",

  // Negative values.
  "-65536", "-65535", "-256", "-255", "-100", "-50", "-10",

  // Values around zero.
  "-2", "-1", "0", "1", "2",

  // Positive values.
  "10", "50", "100", "255", "256", "65535", "65536",

  // Values around INT32_MAX.
  "2147483645",
  "2147483646",
  "2147483647",
];

const stringInt32HexValues = [
  // Values around INT32_MIN.
  "-0x80000000",
  "-0x7fffffff",
  "-0x7ffffffe",

  // Negative values.
  "-0x10000", "-0xffff", "-0x100", "-0xff", "-0x64", "-0x32", "-0xa",

  // Values around zero.
  "-0x2", "-0x1", "0x0", "0x1", "0x2",

  // Positive values.
  "0xa", "0x32", "0x64", "0xff", "0x100", "0xffff", "0x10000",

  // Values around INT32_MAX.
  "0x7ffffffd",
  "0x7ffffffe",
  "0x7fffffff",
];

// Test string-int32 input without an explicit radix.
function testRadixAbsent() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32Values[i % stringInt32Values.length];
    assertEq(+x, x|0, "x is an int32 value");

    let r = Number.parseInt(x);
    assertEq(r, +x);
  }
}
for (let i = 0; i < 2; ++i) testRadixAbsent();

// Test string-int32 hex input without an explicit radix.
function testRadixAbsentHex() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32HexValues[i % stringInt32HexValues.length];

    // String to number conversion doesn't support negative hex-strings, so we
    // have to chop off the leading minus sign manually.
    let y = x;
    let sign = 1;
    if (x.startsWith("-")) {
      y = x.slice(1);
      sign = -1;
    }

    assertEq((+y) * sign, ((+y) * sign)|0, "x is an int32 hex value");

    let r = Number.parseInt(x);
    assertEq(r, (+y) * sign);
  }
}
for (let i = 0; i < 2; ++i) testRadixAbsentHex();

// Test string-int32 input with radix=10.
function testRadixTen() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32Values[i % stringInt32Values.length];
    assertEq(+x, x|0, "x is an int32 value");

    let r = Number.parseInt(x, 10);
    assertEq(r, +x);
  }
}
for (let i = 0; i < 2; ++i) testRadixTen();

// Test string-int32 input with radix=16. (This case isn't currently inlined.)
function testRadixSixteen() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32Values[i % stringInt32Values.length];
    assertEq(+x, x|0, "x is an int32 value");

    let expected = Math.sign(x) * Number("0x" + Math.abs(x).toString(10));

    let r = Number.parseInt(x, 16);
    assertEq(r, expected);
  }
}
for (let i = 0; i < 2; ++i) testRadixSixteen();

// Test string-int32 hex input with radix=16. (This case isn't currently inlined.)
function testRadixSixteenHex() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32HexValues[i % stringInt32HexValues.length];

    // String to number conversion doesn't support negative hex-strings, so we
    // have to chop off the leading minus sign manually.
    let y = x;
    let sign = 1;
    if (x.startsWith("-")) {
      y = x.slice(1);
      sign = -1;
    }

    assertEq((+y) * sign, ((+y) * sign)|0, "x is an int32 hex value");

    let r = Number.parseInt(x, 16);
    assertEq(r, (+y) * sign);
  }
}
for (let i = 0; i < 2; ++i) testRadixSixteenHex();

// Test with variable radix.
function testRadixVariable() {
  for (let i = 0; i < 200; ++i) {
    let x = stringInt32Values[i % stringInt32Values.length];
    assertEq(+x, x|0, "x is an int32 value");

    let radix = [10, 16][(i > 100)|0];

    let expected = +x;
    if (radix === 16) {
      expected = Math.sign(+x) * Number("0x" + Math.abs(+x).toString(10));
    }

    let r = Number.parseInt(x, radix);
    assertEq(r, expected);
  }
}
for (let i = 0; i < 2; ++i) testRadixVariable();
