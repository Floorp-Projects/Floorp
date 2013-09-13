load(libdir + "eqArrayHelper.js");

function g(a, b, c) {
  this.value = [a, b, c];
  assertEq(Object.getPrototypeOf(this), g.prototype);
  assertEq(arguments.callee, g);
}

assertEqArray(new g(...[1, 2, 3]).value, [1, 2, 3]);
