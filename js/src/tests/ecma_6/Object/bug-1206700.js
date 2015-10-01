var x = {};
Reflect.set(x, "prop", 5, Object.prototype);
var y = {};
Reflect.set(y, "prop", 6, Object.prototype);
assertEq(x.hasOwnProperty("prop"), false);
assertEq(y.hasOwnProperty("prop"), false);
assertEq(Object.prototype.hasOwnProperty("prop"), true);
assertEq(Object.prototype.prop, 6);

reportCompare(0, 0, "ok");
