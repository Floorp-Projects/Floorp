// uneval works on objects with no callable .toSource method.

var obj = Object.create(null);
assertEq(uneval(obj), "({})");
assertEq(Function.prototype.toSource.call(obj), "({})");
obj.x = 1;
obj.y = 2;
assertEq(uneval(obj), "({x:1, y:2})");

var d = new Date();
delete Date.prototype.toSource;
assertEq(uneval(d), "({})");

delete Object.prototype.toSource;
assertEq(uneval({p: 2+2}), "({p:4})");

assertEq(uneval({toSource: [0]}), "({toSource:[0]})");
