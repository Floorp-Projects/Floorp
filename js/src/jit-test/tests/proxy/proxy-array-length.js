var a = [1, 2, 3];
var p = new Proxy(a, {});
assertEq(p.length, 3);
assertEq(JSON.stringify(p), "[1,2,3]");
