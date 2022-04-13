// Test inlining natives through Function.prototype.call

function test(fn, expected) {
  for (let i = 0; i < 400; ++i) {
    let r = fn.call(null, 0, 1);
    assertEq(r, expected);
  }
}

for (let i = 0; i < 2; ++i) {
  let fn, expected;
  if (i === 0) {
    fn = Math.min;
    expected = 0;
  } else {
    fn = Math.max;
    expected = 1;
  }
  test(fn, expected);
}
