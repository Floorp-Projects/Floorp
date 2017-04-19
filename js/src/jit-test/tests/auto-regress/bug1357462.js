var p = Object.freeze(["old-value"]);
var a = Object.setPrototypeOf([], p);

assertEq(p[0], "old-value");
assertEq(Reflect.set(a, 0, "new-value", p), false);
assertEq(p[0], "old-value");
