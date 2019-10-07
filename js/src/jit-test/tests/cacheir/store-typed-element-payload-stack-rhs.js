// Different typed array types to ensure we emit a SetProp IC.
var xs = [
  new Float32Array(10),
  new Float64Array(10),
];

function f(ta) {
  for (var k = 0;;) {
    // Store with payload-stack rhs.
    ta[k] = k;
    break;
  }
}

for (var i = 0; i < 100; ++i) {
  f(xs[i & 1]);
}

assertEq(xs[0][0], 0);
assertEq(xs[1][0], 0);
