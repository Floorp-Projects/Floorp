const isLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

function fromRawBits(s) {
  let bits = s.split(" ").map(n => parseInt(n, 16));
  assertEq(bits.length, 8);
  if (isLittleEndian) {
    bits.reverse();
  }
  return new Float64Array(new Uint8Array(bits).buffer)[0];
}

function toRawBits(d) {
  let bits = [...new Uint8Array(new Float64Array([d]).buffer)];
  if (isLittleEndian) {
    bits.reverse();
  }
  return bits.map(n => n.toString(16).padStart(2, "0")).join(" ");
}

assertEq(fromRawBits("7f ef ff ff ff ff ff ff"), Number.MAX_VALUE);
assertEq(toRawBits(Number.MAX_VALUE), "7f ef ff ff ff ff ff ff");

assertEq(fromRawBits("00 00 00 00 00 00 00 01"), Number.MIN_VALUE);
assertEq(toRawBits(Number.MIN_VALUE), "00 00 00 00 00 00 00 01");

let values = [
  0, 0.000001, 0.1, 0.125, 1/6, 0.25, 0.3, 1/3, 0.5, 2/3, 0.8, 0.9,
  1, 2, 3, 4, 5, 10, 14, 15, 16,
  100.1, 100.2,

  Number.MAX_SAFE_INTEGER + 4,
  Number.MAX_SAFE_INTEGER + 3,
  Number.MAX_SAFE_INTEGER + 2,
  Number.MAX_SAFE_INTEGER + 1,
  Number.MAX_SAFE_INTEGER,
  Number.MAX_SAFE_INTEGER - 1,
  Number.MAX_SAFE_INTEGER - 2,
  Number.MAX_SAFE_INTEGER - 3,
  Number.MAX_SAFE_INTEGER - 4,

  // Largest normal (Number.MAX_VALUE)
  fromRawBits("7f ef ff ff ff ff ff ff"),
  fromRawBits("7f ef ff ff ff ff ff fe"),
  fromRawBits("7f ef ff ff ff ff ff fd"),
  fromRawBits("7f ef ff ff ff ff ff fc"),
  fromRawBits("7f ef ff ff ff ff ff fb"),
  fromRawBits("7f ef ff ff ff ff ff fa"),
  fromRawBits("7f ef ff ff ff ff ff f9"),
  fromRawBits("7f ef ff ff ff ff ff f8"),
  fromRawBits("7f ef ff ff ff ff ff f7"),
  fromRawBits("7f ef ff ff ff ff ff f6"),
  fromRawBits("7f ef ff ff ff ff ff f5"),
  fromRawBits("7f ef ff ff ff ff ff f4"),
  fromRawBits("7f ef ff ff ff ff ff f3"),
  fromRawBits("7f ef ff ff ff ff ff f2"),
  fromRawBits("7f ef ff ff ff ff ff f1"),
  fromRawBits("7f ef ff ff ff ff ff f0"),

  // Smallest subnormal (Number.MIN_VALUE)
  fromRawBits("00 00 00 00 00 00 00 01"),
  fromRawBits("00 00 00 00 00 00 00 02"),
  fromRawBits("00 00 00 00 00 00 00 03"),
  fromRawBits("00 00 00 00 00 00 00 04"),
  fromRawBits("00 00 00 00 00 00 00 05"),
  fromRawBits("00 00 00 00 00 00 00 06"),
  fromRawBits("00 00 00 00 00 00 00 07"),
  fromRawBits("00 00 00 00 00 00 00 08"),
  fromRawBits("00 00 00 00 00 00 00 09"),
  fromRawBits("00 00 00 00 00 00 00 0a"),
  fromRawBits("00 00 00 00 00 00 00 0b"),
  fromRawBits("00 00 00 00 00 00 00 0c"),
  fromRawBits("00 00 00 00 00 00 00 0d"),
  fromRawBits("00 00 00 00 00 00 00 0e"),
  fromRawBits("00 00 00 00 00 00 00 0f"),

  // Largest subnormal
  fromRawBits("00 0f ff ff ff ff ff ff"),
  fromRawBits("00 0f ff ff ff ff ff fe"),
  fromRawBits("00 0f ff ff ff ff ff fd"),
  fromRawBits("00 0f ff ff ff ff ff fc"),
  fromRawBits("00 0f ff ff ff ff ff fb"),
  fromRawBits("00 0f ff ff ff ff ff fa"),
  fromRawBits("00 0f ff ff ff ff ff f9"),
  fromRawBits("00 0f ff ff ff ff ff f8"),
  fromRawBits("00 0f ff ff ff ff ff f7"),
  fromRawBits("00 0f ff ff ff ff ff f6"),
  fromRawBits("00 0f ff ff ff ff ff f5"),
  fromRawBits("00 0f ff ff ff ff ff f4"),
  fromRawBits("00 0f ff ff ff ff ff f3"),
  fromRawBits("00 0f ff ff ff ff ff f2"),
  fromRawBits("00 0f ff ff ff ff ff f1"),
  fromRawBits("00 0f ff ff ff ff ff f0"),

  // Least positive normal
  fromRawBits("00 10 00 00 00 00 00 00"),
  fromRawBits("00 10 00 00 00 00 00 01"),
  fromRawBits("00 10 00 00 00 00 00 02"),
  fromRawBits("00 10 00 00 00 00 00 03"),
  fromRawBits("00 10 00 00 00 00 00 04"),
  fromRawBits("00 10 00 00 00 00 00 05"),
  fromRawBits("00 10 00 00 00 00 00 06"),
  fromRawBits("00 10 00 00 00 00 00 07"),
  fromRawBits("00 10 00 00 00 00 00 08"),
  fromRawBits("00 10 00 00 00 00 00 09"),
  fromRawBits("00 10 00 00 00 00 00 0a"),
  fromRawBits("00 10 00 00 00 00 00 0b"),
  fromRawBits("00 10 00 00 00 00 00 0c"),
  fromRawBits("00 10 00 00 00 00 00 0d"),
  fromRawBits("00 10 00 00 00 00 00 0e"),
  fromRawBits("00 10 00 00 00 00 00 0f"),

  Infinity,
  NaN,

  Math.E, Math.LN10, Math.LN2, Math.LOG10E, Math.LOG2E,
  Math.PI, Math.SQRT1_2, Math.SQRT2,
];

// Also test with sign bit set.
values = values.concat(values.map(x => -x));

function mod(n, d) {
  with ({}); // disable Ion
  return n % d;
}

function makeTest(divisor) {
  function test() {
    let expected = values.map(x => mod(x, divisor));

    for (let i = 0; i < 2000; ++i) {
      let j = i % values.length;
      assertEq(values[j] % divisor, expected[j]);
    }
  }

  // Create a new function for each divisor to ensure we have proper compile-time constants.
  return Function(`return ${test.toString().replaceAll("divisor", divisor)}`)();
}

// The optimisation is used for power of two values up to 2^31.
for (let i = 0; i <= 31; ++i) {
  let divisor = 2 ** i;
  let f = makeTest(divisor);
  f();
}

// Also cover some cases which don't trigger the optimisation
for (let divisor of [-3, -2, -1, -0.5, 0, 0.5, 3, 5, 10]) {
  let f = makeTest(divisor);
  f();
}
