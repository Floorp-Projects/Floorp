// Different typed array types to ensure we emit a SetProp IC.
var xs = [
  new Float32Array(10),
  new Float64Array(10),
];

for (var i = 0; i < 100; ++i) {
  var ta = xs[i & 1];

  var v = +ta[0];

  // Store with payload-register rhs.
  ta[0] = ~v;
}

assertEq(xs[0][0], 0);
assertEq(xs[1][0], 0);
