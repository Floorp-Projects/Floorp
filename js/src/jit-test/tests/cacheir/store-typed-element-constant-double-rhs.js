// Different typed array types to ensure we emit a SetProp IC.
var xs = [
  new Float32Array(10),
  new Float64Array(10),
];

for (var i = 0; i < 100; ++i) {
  var ta = xs[i & 1];

  // Store with constant rhs.
  ta[0] = 0.1;
}

assertEq(xs[0][0], Math.fround(0.1));
assertEq(xs[1][0], 0.1);
