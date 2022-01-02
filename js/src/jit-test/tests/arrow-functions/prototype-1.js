// The prototype of an arrow function is Function.prototype.

assertEq(Object.getPrototypeOf(a => a), Function.prototype);
assertEq(Object.getPrototypeOf(() => {}), Function.prototype);
