function set(k) {
  // Different typed array types to ensure we emit a SetProp IC.
  var xs = [
    new Int32Array(10),
    new Int16Array(10),
  ];

  for (var i = 0; i < 100; ++i) {
    var x = xs[i & 1];
    x[k] = 0;
  }

  assertEq(xs[0][k], undefined);
  assertEq(xs[1][k], undefined);
}

// Make sure we use a distinct function for each test.
function test(fn) {
  return Function(`return ${fn};`)();
}

// Negative numbers values aren't int32 indices.
test(set)(-1);
test(set)(-2147483648);
test(set)(-2147483649);

// Int32 indices must be less-or-equal to max-int32.
test(set)(2147483648);

// Int32 indices must not have fractional parts.
test(set)(1.5);
test(set)(-1.5);

// Non-finite numbers aren't int32 indices.
test(set)(NaN);
test(set)(-Infinity);
test(set)(Infinity);
