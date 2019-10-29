function has(k) {
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
    assertEq(k in x, false);
  }
}

// Make sure we use a distinct function for each test.
function test(fn) {
  return Function(`return ${fn};`)();
}

// TypedArray index representable as an Int32.
test(has)(100);
test(has)("100");

// TypedArray index not representable as an Int32.
test(has)(4294967296);
test(has)("4294967296");

// Non-finite TypedArray indices.
test(has)(Infinity);
test(has)("Infinity");

test(has)(-Infinity);
test(has)("-Infinity");

test(has)(NaN);
test(has)("NaN");

// TypedArray index with fractional parts.
test(has)(1.1);
test(has)("1.1");

// TypedArray index with exponent parts.
test(has)(1e+25);
test(has)("1e+25");
