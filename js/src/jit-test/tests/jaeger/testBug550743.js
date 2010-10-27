expected = '';

function g(code) {
  f = Function(code);
  gen = f();
  gen.next();
  try { gen.next(); } catch (ex) { expected = ex.toString() }
}

g("\
  yield this.__defineGetter__('x', function(){ return z }); \
  let z = new String('hi'); \
  ");

eval();
gc();

str = x;

assertEq(expected, "[object StopIteration]");
assertEq(str.toString(), "hi");
