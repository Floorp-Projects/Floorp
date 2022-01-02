function get(k) {
  var p = {};
  p[k] = 1;

  // TypedArrays intercept any TypedArray indices (canonical numeric indices in the spec),
  // so when reading |k| from |ta| we shouldn't return the inherited property |p[k]|.
  var ta = new Int32Array(10);
  Object.setPrototypeOf(ta, p);

  // Assume sixteen different objects trigger a transition to a megamorphic IC.
  var xs = [
    ta,

    {a:0}, {b:0}, {c:0}, {d:0}, {e:0}, {f:0}, {g:0}, {h:0},
    {j:0}, {k:0}, {l:0}, {m:0}, {n:0}, {o:0}, {p:0},
  ];

  for (var i = 0; i < 100; ++i) {
    var x = xs[i & 15];
    assertEq(x[k], undefined);
  }
}

// Make sure we use a distinct function for each test.
function test(fn) {
  return Function(`return ${fn};`)();
}

// TypedArray index representable as an Int32.
test(get)(100);
test(get)("100");

// TypedArray index not representable as an Int32.
test(get)(4294967296);
test(get)("4294967296");

// Non-finite TypedArray indices.
test(get)(Infinity);
test(get)("Infinity");

test(get)(-Infinity);
test(get)("-Infinity");

test(get)(NaN);
test(get)("NaN");

// TypedArray index with fractional parts.
test(get)(1.1);
test(get)("1.1");

// TypedArray index with exponent parts.
test(get)(1e+25);
test(get)("1e+25");
