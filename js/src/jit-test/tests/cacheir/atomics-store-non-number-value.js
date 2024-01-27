const types = [
  "Int8",
  "Int16",
  "Int32",
  "Uint8",
  "Uint16",
  "Uint32",
];

function convert(type, value) {
  let num = Number(value);
  switch (type) {
    case "Int8":
      return ((num | 0) << 24) >> 24;
    case "Int16":
      return ((num | 0) << 16) >> 16;
    case "Int32":
      return (num | 0);
    case "Uint8":
      return (num >>> 0) & 0xff;
    case "Uint16":
      return (num >>> 0) & 0xffff;
    case "Uint32":
      return (num >>> 0);
    case "Uint8Clamped": {
      if (Number.isNaN(num)) {
        return 0;
      }
      let clamped = Math.max(0, Math.min(num, 255));
      let f = Math.floor(clamped);
      if (clamped < f + 0.5) {
        return f;
      }
      if (clamped > f + 0.5) {
        return f + 1;
      }
      return f + (f & 1);
    }
    case "Float32":
      return Math.fround(num);
    case "Float64":
      return num;
  }
  throw new Error();
}


function runTest(type, initial, values) {
  let expected = values.map(v => convert(type, v));
  assertEq(
    expected.some(e => Object.is(e, initial)),
    false,
    "initial must be different from the expected values"
  );

  // Create a fresh function to ensure ICs are specific to a single TypedArray kind.
  let test = Function("initial, values, expected", `
    let ta = new ${type}Array(1);
    for (let i = 0; i < 200; ++i) {
      ta[0] = initial;
      Atomics.store(ta, 0, values[i % values.length]);
      assertEq(ta[0], expected[i % expected.length]);
    }
  `);
  test(initial, values, expected);
}

const tests = [
  // |null| is coerced to zero.
  {
    initial: 1,
    values: [null],
  },

  // |undefined| is coerced to zero or NaN.
  {
    initial: 1,
    values: [undefined],
  },

  // |false| is coerced to zero and |true| is coerced to one.
  {
    initial: 2,
    values: [false, true],
  },
  
  // Strings without a fractional part.
  {
    initial: 42,
    values: [
      "0", "1", "10", "111", "128", "256", "0x7fffffff", "0xffffffff",
    ],
  },

  // Strings without a fractional part, but a leading minus sign.
  {
    initial: 42,
    values: [
      "-0", "-1", "-10", "-111", "-128", "-256", "-2147483647", "-4294967295",
    ],
  },
  
  // Strings with a fractional number part.
  {
    initial: 42,
    values: [
      "0.1", "1.2", "10.8", "111.9",
      "-0.1", "-1.2", "-10.8", "-111.9",
    ],
  },

  // Special values and strings not parseable as a number.
  {
    initial: 42,
    values: ["Infinity", "-Infinity", "NaN", "foobar"],
  },
];

for (let type of types) {
  for (let {initial, values} of tests) {
    runTest(type, initial, values);
  }
}
