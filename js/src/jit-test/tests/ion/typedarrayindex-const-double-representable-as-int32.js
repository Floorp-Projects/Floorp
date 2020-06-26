function f(n) {
  const ta = new Int32Array(n);

  // When the TypedArray has a zero length, accessing the element at index 0
  // should return undefined. That'll lead to including undefined in the
  // typeset of |ta[k]|, which in turn triggers to use MTypedArrayIndexToInt32
  // for the TypedArray index conversion.
  const r = n === 0 ? undefined : 0;

  // numberToDouble always returns a double number.
  const k = numberToDouble(0);

  for (var i = 0; i < 10; ++i) {
    assertEq(ta[k], r);
  }
}

for (var i = 0; i < 2; ++i) {
  f(i);
}
