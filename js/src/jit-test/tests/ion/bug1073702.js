try {
  let x = ((function f(y) {
    if (y > 0) {
      f(-1)
    }
    x
  })(1))
} catch (e) {
  assertEq(e instanceof ReferenceError, true);
}
