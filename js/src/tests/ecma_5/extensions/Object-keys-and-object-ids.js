var o = { normal:"a" };
Object.defineProperty(o, new AttributeName, { enumerable:true });
var keys = Object.keys(o);
assertEq(keys.length, 1);
assertEq(keys[0], "normal");

var o = {};
Object.defineProperty(o, new AttributeName, { enumerable:true });
var keys = Object.keys(o);
assertEq(keys.length, 0);

reportCompare(true, true);
