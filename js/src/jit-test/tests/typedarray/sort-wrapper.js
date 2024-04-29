function test() {
  var g = newGlobal({ newCompartment: true });
  var wrapped = g.evaluate("new Int32Array([1, 3, 2, 0])");
  var wrappedOther = g.evaluate("({})");
  var unwrapped = new Int32Array(10);

  var ex;
  try {
    unwrapped.sort.call(wrappedOther);
  } catch (e) {
    ex = e;
  }
  assertEq(ex instanceof TypeError, true);

  assertEq(unwrapped.sort.call(wrapped), wrapped);
  assertEq(wrapped.toString(), "0,1,2,3");

  assertEq(unwrapped.sort.call(wrapped, (a, b) => b - a), wrapped);
  assertEq(wrapped.toString(), "3,2,1,0");
}
test();
