function wrapper(values) {
  function test(values) {
    var expected = values.map(v => !!v);

    for (var i = 0; i < 100; ++i) {
      var ix = i % values.length;
      var val = values[ix];
      var actual = Boolean(val);
      assertEq(actual, expected[ix]);
    }
  }

  for (var i = 0; i < 2; ++i) {
    test(values);
  }
}

function makeTest() {
  // Create a copy to avoid type pollution.
  return Function(`return ${wrapper}`)();
}

// Use a new compartment to create a wrapper.
var g = newGlobal({newCompartment: true});

var testValues = {
  boolean: [true, false],
  int32: [-2147483648, -1, 0, 1, 2147483647],
  double: [-Infinity, -1.5, -1, -0.5, -0, +0, +0.5, +1, +1.5, Infinity, NaN],
  string: ["", "true", "false", "0", "1", "hello"],
  symbol: [Symbol(), Symbol("desc"), Symbol.iterator],
  bigint: [
    -(2n ** 1000n),
    -18446744073709551616n, // -(2n**64n)
    -9223372036854775808n, // -(2n**63n)
    -4294967296n, // -(2**32)
    -2147483648n, // -(2**31)
    -1n, 0n, 1n,
    2147483648n, // 2**31
    4294967296n, // 2**32
    9223372036854775808n, // 2n**63n
    18446744073709551616n, // 2n**64n
    2n ** 1000n,
  ],
  object: [{}, [], function(){}, new Proxy({}, {}), createIsHTMLDDA(), g.eval("createIsHTMLDDA()")],
  null: [null],
  undefined: [undefined],
};

for (var values of Object.values(testValues)) {
  makeTest()(values);
}

// boolean and int32
makeTest()([].concat(testValues.boolean, testValues.int32));

// int32 and double
makeTest()([].concat(testValues.int32, testValues.double));

// null and undefined
makeTest()([].concat(testValues.null, testValues.undefined));

// null, undefined, and object
makeTest()([].concat(testValues.null, testValues.undefined, testValues.object));

// all values
makeTest()(Object.values(testValues).flat());
