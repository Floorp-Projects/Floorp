// Optional expression can be part of a class heritage expression.

var a = {b: null};

class C extends a?.b {}

assertEq(Object.getPrototypeOf(C.prototype), null);

if (typeof reportCompare === "function")
  reportCompare(true, true);
